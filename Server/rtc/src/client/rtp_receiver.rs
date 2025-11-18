use futures::task::{Poll, Context};
use futures::{StreamExt, FutureExt, Future};
use std::pin::Pin;
use futures::channel::oneshot;
use tokio::sync::mpsc;
use tokio::stream::Stream;
use webrtc_lib::media::{MediaReceiverEvent, MediaReceiver};
use crate::channel::{ClientAudioSource, AudioSourceEvent, ClientVideoSource, VideoSourceEvent};
use unchecked_unwrap::UncheckedUnwrap;
use crate::{AudioCodec, VideoCodec};
use super::{OPUS_VOICE_PAYLOAD_TYPE, OPUS_MUSIC_PAYLOAD_TYPE};
use tokio::sync::mpsc::error::TrySendError;
use webrtc_lib::utils::rtcp::RtcpPacket;
use webrtc_lib::utils::rtcp::packets::{RtcpPacketPayloadFeedback, RtcpPayloadFeedback};
use crate::client::{H264_PAYLOAD_TYPE, LOCAL_EXT_ID_AUDIO_LEVEL, VP8_PAYLOAD_TYPE, ClientData};
use webrtc_lib::utils::rtp::ParsedRtpPacket;
use webrtc_sdp::media_type::SdpMediaValue;
use crate::threads::execute_task;
use crate::extension::{OneByteExtension, TwoByteExtension, GeneralExtension};
use crate::utils::{h264_is_keyframe, VP8PayloadDescriptor};
use crate::exports::GLOBAL_DATA;
use std::io::Cursor;
use byteorder::{ReadBytesExt, BigEndian, LittleEndian};
use tokio::io::ErrorKind;

type OptionalMediaReceiver = Option<Box<webrtc_lib::media::MediaReceiver>>;
pub struct InternalMediaReceiver {
    logger: slog::Logger,

    receiver_id: u32,
    stream_id: u32,
    media_id: u32,
    media_type: SdpMediaValue,

    pub revoke_signal_sender: Option<oneshot::Sender<()>>,
    pub return_channel: (tokio::sync::mpsc::Sender<OptionalMediaReceiver>, tokio::sync::mpsc::Receiver<OptionalMediaReceiver>),

    pub next_receiver: Option<oneshot::Sender<Box<webrtc_lib::media::MediaReceiver>>>
}

/// Creates a new internal, borrowable media receiver.
/// The future needs to be polled to make any progress (e. g. passing receivers).
/// If the future terminated it means the underlying receiver terminated.
impl InternalMediaReceiver {
    /// Create a new media receiver from the given parameters
    pub fn new(logger: slog::Logger, receiver_id: u32, stream: MediaReceiver, media_type: SdpMediaValue) -> Self {
        let mut internal_receiver = InternalMediaReceiver{
            logger,

            receiver_id,
            stream_id: stream.ssrc(),
            media_id: stream.media_line(),
            media_type,

            revoke_signal_sender: None,
            return_channel: mpsc::channel(2),
            next_receiver: None,
        };

        internal_receiver.void_receiver_handle(Box::new(stream));
        internal_receiver
    }

    /// Returns the internal and unique media receiver id
    pub fn receiver_id(&self) -> u32 {
        self.receiver_id
    }

    /// Returns the rtp stream id (ssrc)
    pub fn stream_id(&self) -> u32 {
        self.stream_id
    }

    /// Returns the webrtc_lib internal media id used
    /// to identify the corresponding media line.
    pub fn media_id(&self) -> u32 {
        self.media_id
    }

    /// Returns the media type of the receiver
    pub fn media_type(&self) -> SdpMediaValue {
        self.media_type.clone()
    }

    /// Get the rtp media receiver (will destroy the old one)
    pub fn get_receiver(&mut self) -> RtpMediaReceiver {
        self.get_receiver_internal(false)
    }

    /// Get the rtp media receiver with optional checking of a valid revoke signal sender.
    fn get_receiver_internal(&mut self, initial_request: bool) -> RtpMediaReceiver {
        let (recv_tx, recv_rx) = oneshot::channel();
        let (revoke_tx, revoke_rx) = oneshot::channel();

        let receiver = RtpMediaReceiver {
            logger: self.logger.clone(),

            stream_id: self.stream_id,
            state: RtpMediaReceiverState::Receiving(recv_rx),
            return_channel: self.return_channel.0.clone(),
            revoke_signal_receiver: revoke_rx,
            base_sequence: None
        };

        self.next_receiver = Some(recv_tx);
        if let Some(revoke_signal_sender) = self.revoke_signal_sender.replace(revoke_tx) {
            let _ = revoke_signal_sender.send(());
        } else if !initial_request {
            panic!("Missing receiver revoke signal sender");
        }

        receiver
    }

    fn void_receiver_handle(&mut self, stream: Box<MediaReceiver>) {
        let receiver = self.get_receiver_internal(true);
        let _ = self.next_receiver.take()
            .expect("missing receiver receiver")
            .send(stream);

        execute_task(VoidMediaReceiver::new(receiver));
    }
}

impl Future for InternalMediaReceiver {
    type Output = ();

    fn poll(mut self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Self::Output> {
        while let Poll::Ready(media_receiver) = self.return_channel.1.poll_next_unpin(cx) {
            let media_receiver = media_receiver.unwrap();
            if media_receiver.is_none() {
                slog::slog_trace!(self.logger, "Media receiver gone");
            } else if self.next_receiver.is_some() {
                slog::slog_trace!(self.logger, "Media receiver passed");
                if let Err(media_receiver) = self.next_receiver.take()
                    .unwrap().send(media_receiver.unwrap()) {
                    slog::slog_warn!(self.logger, "Failed to forward media receiver to next stream");
                    if let Err(_) = self.return_channel.0.try_send(Some(media_receiver)) {
                        slog::slog_error!(self.logger, "Failed to pass forwarded failed receiver back to the stream. Dropping it");
                    }
                }
            } else {
                slog::slog_trace!(self.logger, "Channel gets voided again");
                self.void_receiver_handle(media_receiver.unwrap());
            }
        }

        Poll::Pending
    }
}

enum RtpMediaReceiverState {
    Receiving(oneshot::Receiver<Box<webrtc_lib::media::MediaReceiver>>),
    Owned(Box<webrtc_lib::media::MediaReceiver>),
    Destroyed
}

pub struct RtpMediaReceiver {
    logger: slog::Logger,
    stream_id: u32,
    state: RtpMediaReceiverState,
    revoke_signal_receiver: oneshot::Receiver<()>,
    return_channel: mpsc::Sender<Option<Box<webrtc_lib::media::MediaReceiver>>>,
    base_sequence: Option<u16>
}

#[derive(Debug)]
pub enum RtpMediaReceiverEvent {
    ReceiverReceived,
    MessageReceived(MediaReceiverEvent),
    DataReceived(ParsedRtpPacket, u16)
}

impl RtpMediaReceiver {
    fn destroy(&mut self) {
        if let RtpMediaReceiverState::Owned(receiver) = std::mem::replace(&mut self.state, RtpMediaReceiverState::Destroyed) {
            if let Err(error) = self.return_channel.try_send(Some(receiver)) {
                match error {
                    TrySendError::Closed(_) => slog::slog_error!(self.logger, "Failed to send back local media receiver (pipe closed)"),
                    TrySendError::Full(_) => slog::slog_error!(self.logger, "Failed to send back local media receiver (pipe full)")
                }
            }
        }
    }

    fn get_receiver_mut(&mut self) -> Option<&mut Box<MediaReceiver>> {
        if let RtpMediaReceiverState::Owned(receiver) = &mut self.state {
            Some(receiver)
        } else {
            None
        }
    }
}

impl Stream for RtpMediaReceiver {
    type Item = RtpMediaReceiverEvent;

    fn poll_next(mut self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Option<Self::Item>> {
        match &mut self.state {
            RtpMediaReceiverState::Receiving(receiver) => {
                match receiver.poll_unpin(cx) {
                    Poll::Ready(Ok(receiver)) => {
                        self.state = RtpMediaReceiverState::Owned(receiver);
                        Poll::Ready(Some(RtpMediaReceiverEvent::ReceiverReceived))
                    },
                    Poll::Ready(Err(_)) => {
                        /* We'll never get the stream */
                        Poll::Ready(None)
                    },
                    Poll::Pending => Poll::Pending
                }
            },
            RtpMediaReceiverState::Owned(receiver) => {
                loop {
                    match receiver.poll_next_unpin(cx) {
                        Poll::Ready(Some(event)) => {
                            return match event {
                                MediaReceiverEvent::DataReceived(packet) => {
                                    let mut virt_seq_no: u16 = packet.sequence_number().into();
                                    if let Some(base_seq) = self.base_sequence {
                                        virt_seq_no = virt_seq_no.wrapping_sub(base_seq);
                                    } else {
                                        /*
                                         * We're assuming this is the first packet of the transmission.
                                         * If this is already a OOO packet, we'll probably lose the earlier send packets.
                                         */
                                        self.base_sequence = Some(virt_seq_no);
                                        virt_seq_no = 0;
                                    }

                                    Poll::Ready(Some(RtpMediaReceiverEvent::DataReceived(packet, virt_seq_no)))
                                },
                                event => Poll::Ready(Some(RtpMediaReceiverEvent::MessageReceived(event)))
                            }
                        },
                        Poll::Ready(None) => {
                            /* receiver ended */
                            let _ = self.return_channel.send(None);
                            self.state = RtpMediaReceiverState::Destroyed;
                            return Poll::Ready(None);
                        },
                        Poll::Pending => {
                            break;
                        }
                    }
                }

                match self.revoke_signal_receiver.poll_unpin(cx) {
                    Poll::Ready(Ok(_))=> {
                        /* receiver has been requested back */
                        self.destroy();
                        return Poll::Ready(None);
                    },
                    Poll::Ready(Err(_)) => {
                        /* stream has been dropped */
                        return Poll::Ready(None);
                    },
                    Poll::Pending => { }
                }

                Poll::Pending
            },
            RtpMediaReceiverState::Destroyed => Poll::Ready(None)
        }
    }
}

impl Drop for RtpMediaReceiver {
    fn drop(&mut self) {
        self.destroy();
    }
}

/// Receiver which just drops all received data
struct VoidMediaReceiver {
    receiver: RtpMediaReceiver
}

impl VoidMediaReceiver {
    pub fn new(receiver: RtpMediaReceiver) -> Self {
        VoidMediaReceiver{ receiver }
    }
}

impl Future for VoidMediaReceiver {
    type Output = ();

    fn poll(mut self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Self::Output> {
        while let Poll::Ready(event) = self.receiver.poll_next_unpin(cx) {
            if event.is_none() {
                return Poll::Ready(());
            }
        }

        Poll::Pending
    }
}



pub struct RtpClientAudioSource {
    receiver: RtpMediaReceiver
}

impl RtpClientAudioSource {
    pub fn new(receiver: RtpMediaReceiver) -> Self {
        RtpClientAudioSource{ receiver }
    }

    pub fn get_audio_extension(packet: &ParsedRtpPacket) -> Option<(bool, u8)> {
        let extension = packet.extension();
        if !extension.is_some() {
            return None;
        }

        let (ext_id, extension) = extension.unwrap();
        match ext_id {
            0xBEDE => {
                let extension = OneByteExtension::new(extension);
                let value = extension.value(LOCAL_EXT_ID_AUDIO_LEVEL);

                /* all extensions are at least one byte wide */
                value.map(|e| ((e[0] & 0x80) > 0, e[0] & 0x7F))
            },
            0x100 => {
                let extension = TwoByteExtension::new(extension);
                let value = extension.value(LOCAL_EXT_ID_AUDIO_LEVEL);

                /* all extensions are at least one byte wide */
                value.map(|e| ((e[0] & 0x80) > 0, e[0] & 0x7F))
            },
            _ => None
        }
    }
}

impl ClientAudioSource for RtpClientAudioSource {
    fn stream_id(&self) -> u32 {
        self.receiver.stream_id
    }

    fn poll_next(&mut self, cx: &mut Context) -> Poll<Option<AudioSourceEvent>> {
        while let Poll::Ready(event) = self.receiver.poll_next_unpin(cx) {
            if event.is_none() {
                return Poll::Ready(None);
            }

            match unsafe { event.as_ref().unchecked_unwrap() } {
                RtpMediaReceiverEvent::MessageReceived(event) => {
                    match event {
                        MediaReceiverEvent::ByeSignalReceived(_) => {
                            slog::slog_trace!(self.receiver.logger, "Received audio bye signal");
                            return Poll::Ready(Some(AudioSourceEvent::EndSignal));
                        },
                        MediaReceiverEvent::DataReceived(_) => panic!("this branch should never be reached"),
                        _ => {}
                    }
                },
                RtpMediaReceiverEvent::DataReceived(packet, virtual_seq_no) => {
                    let audio_level = RtpClientAudioSource::get_audio_extension(&packet);
                    let audio_level_val: u8;
                    if let Some((vad_activated, level)) = audio_level {
                        //println!("Packet audio volume: {}-{} Stream: {}", vad_activated, level, packet.payload_type());
                        if level == 127 && !vad_activated {
                            /* silence, don't broadcast that */
                            continue;
                        }

                        audio_level_val = level;
                    } else {
                        slog::slog_trace!(self.receiver.logger, "Received audio packet without audio extension. Dropping packet.");
                        continue;
                    }

                    let codec = match packet.payload_type() {
                        OPUS_MUSIC_PAYLOAD_TYPE => AudioCodec::OpusMusic,
                        OPUS_VOICE_PAYLOAD_TYPE => AudioCodec::Opus,
                        _ => {
                            slog::slog_trace!(self.receiver.logger, "Received data on RTP receiver which isn't the expected audio codecs. Expected: {} or {}, Received: {}",
                                     OPUS_VOICE_PAYLOAD_TYPE, OPUS_MUSIC_PAYLOAD_TYPE, packet.payload_type());
                            continue;
                        }
                    };

                    /* TODO: Callback if we should handle voice/whisper! */
                    return Poll::Ready(Some(AudioSourceEvent::Packet {
                        payload: packet.payload().to_vec(),
                        sequence: virtual_seq_no.clone(),
                        timestamp: packet.timestamp(),
                        marked: packet.mark(),
                        codec,
                        level: Some(audio_level_val)
                    }));
                },
                RtpMediaReceiverEvent::ReceiverReceived => {
                    /* nothing to do except be happy that we've received the receiver */
                }
            }
        }

        Poll::Pending
    }
}



pub struct RtpClientVideoSource {
    receiver: RtpMediaReceiver,
    bandwidth_limit: Option<u32>,
    own_client_data: ClientData,
}

impl RtpClientVideoSource {
    pub fn new(receiver: RtpMediaReceiver, own_client_data: ClientData) -> Self {
        RtpClientVideoSource{
            receiver,
            own_client_data,
            bandwidth_limit: None,
        }
    }

    fn vp8_is_keyframe(&self, payload: &[u8]) -> std::io::Result<bool> {
        let payload_descriptor = VP8PayloadDescriptor::parse(payload)?;

        if payload_descriptor.partition_start && payload_descriptor.partition_index == 0 {
            /* new partition */
            let mut cursor = Cursor::new(payload_descriptor.payload);
            let flags = cursor.read_u24::<BigEndian>()?;

            /* Debugging code */
            if (flags & 0x010000) == 0 {
                let magic = cursor.read_u24::<BigEndian>()?;
                if magic != 0x9D012A {
                    return Err(std::io::Error::new(ErrorKind::InvalidData, format!("invalid magic bytes ({:x})", magic)))
                }

                let widths = cursor.read_u16::<LittleEndian>()?;
                let width = widths & 0x3FFF;
                let source_width = widths >> 14;

                let heights = cursor.read_u16::<LittleEndian>()?;
                let height = heights & 0x3FFF;
                let source_height = heights >> 14;

                slog::trace!(self.receiver.logger, "Received VP8 key frame: {}x{} (Original: {}x{})", width, height, source_width, source_height);
                Ok(true)
            } else {
                Ok(false)
            }

            /* P bit isn't set -> key frame! */
            //Ok((flags & 0x01000000) == 0)
        } else {
            Ok(false)
        }
    }
}

impl ClientVideoSource for RtpClientVideoSource {
    fn request_pli(&mut self) {
        if let Some(receiver) = self.receiver.get_receiver_mut() {
            let _ = receiver.send_control(&RtcpPacket::PayloadFeedback(RtcpPacketPayloadFeedback{
                ssrc: 1, /* we've no own "real" source */
                media_ssrc: receiver.ssrc(),
                feedback: RtcpPayloadFeedback::PictureLossIndication
            }));
        } else {
            /* We're not owning the receiver. We just simply ignore this request. We'll send a PLI anyway as soon we get it. */
        }
    }

    fn set_bitrate(&mut self, bitrate_limit: Option<u32>) {
        if let Some(receiver) = self.receiver.get_receiver_mut() {
            receiver.set_bandwidth_limit(bitrate_limit.clone());
        }

        self.bandwidth_limit = bitrate_limit;
    }

    fn bitrate(&self) -> Option<u32> {
        self.bandwidth_limit
    }

    fn poll_next(&mut self, cx: &mut Context) -> Poll<VideoSourceEvent> {
        while let Poll::Ready(event) = self.receiver.poll_next_unpin(cx) {
            if event.is_none() {
                slog::slog_trace!(self.receiver.logger, "Received video EOF");
                return Poll::Ready(VideoSourceEvent::Ended);
            }


            match unsafe { event.as_ref().unchecked_unwrap() } {
                RtpMediaReceiverEvent::MessageReceived(event) => {
                    match event {
                        MediaReceiverEvent::ByeSignalReceived(_) => {
                            slog::slog_trace!(self.receiver.logger, "Received bye signal");
                            return Poll::Ready(VideoSourceEvent::SequenceEndSignal);
                        },
                        MediaReceiverEvent::DataReceived(_) => panic!("this branch should never be reached"),
                        MediaReceiverEvent::BandwidthLimitViolation(bytes_per_second) => {
                            if let Some(limit) = self.receiver.get_receiver_mut().map(|e| e.bandwidth_limit().clone()).flatten() {
                                if limit < 1_500_000 {
                                    /* Sometimes the receiver must send more data */
                                    continue;
                                }
                                slog::slog_info!(self.receiver.logger, "Remote violates the bandwidth limits of our receiver with sending {} bits/second. Allowed: {} bits/second.", *bytes_per_second * 8 as f64, limit);
                                /* TODO: Count violations and close the stream after n once */
                            }
                        },
                        MediaReceiverEvent::ReceiverActivated => {},
                        _ => {
                            slog::slog_trace!(self.receiver.logger, "Unknown MediaReceiverEvent event {:?}", event);
                        }
                    }
                },
                RtpMediaReceiverEvent::DataReceived(packet, virtual_seq_no) => {
                    let codec: VideoCodec;
                    let key_frame: bool;

                    match packet.payload_type() {
                        H264_PAYLOAD_TYPE => {
                            codec = VideoCodec::H264;
                            key_frame = h264_is_keyframe(packet.payload()).unwrap_or(false);
                        },
                        VP8_PAYLOAD_TYPE => {
                            codec = VideoCodec::VP8;

                            key_frame = match self.vp8_is_keyframe(packet.payload()) {
                                Ok(key_frame) => key_frame,
                                Err(error) => {
                                    /* TODO: Drop packet and don't spam in here */
                                    slog::slog_warn!(self.receiver.logger, "Failed to parse vp8 header: {:?}", error);
                                    false
                                }
                            };
                        },
                        codec => {
                            slog::slog_trace!(self.receiver.logger, "Received data on RTP receiver with an invalid payload type: {}", codec);
                            continue;
                        }
                    };

                    /* TODO: Extensions? */
                    return Poll::Ready(VideoSourceEvent::Packet {
                        payload: packet.payload().to_vec(),
                        timestamp: packet.timestamp(),
                        marked: packet.mark(),
                        sequence: virtual_seq_no.clone(),
                        codec,
                        key_frame
                    });
                },
                RtpMediaReceiverEvent::ReceiverReceived => {
                    slog::slog_trace!(self.receiver.logger, "Request initial pli (received received)");
                    self.request_pli();


                    if let Some(receiver) = self.receiver.get_receiver_mut() {
                        receiver.set_bandwidth_limit(self.bandwidth_limit);
                    } else {
                        unreachable!();
                    }
                }
            }
        }

        Poll::Pending
    }

    fn notify_client_join(&mut self, _client_id: u32, client_data: &ClientData) {
        (GLOBAL_DATA.callbacks().client_video_join)(self.own_client_data.as_ptr(), self.receiver.stream_id, client_data.as_ptr());
    }

    fn notify_client_leave(&mut self, _client_id: u32, client_data: &ClientData) {
        (GLOBAL_DATA.callbacks().client_video_leave)(self.own_client_data.as_ptr(), self.receiver.stream_id, client_data.as_ptr());
    }
}