use std::sync::{Mutex, Weak};
use crate::client::{
    ClientData, ClientAudioSender, AudioStopReason, ClientVideoSenderEvent, ClientVideoSender, VideoStopReason,
    OPUS_VOICE_PAYLOAD_TYPE, OPUS_MUSIC_PAYLOAD_TYPE, LOCAL_EXT_ID_AUDIO_LEVEL,
    RTCConnection};
use crate::threads::execute_task;
use crate::{AudioCodec, MediaType, VideoCodec};
use webrtc_lib::utils::rtcp::packets::{RtcpPacketBye, RtcpPayloadFeedback};
use webrtc_lib::utils::rtcp::RtcpPacket;
use crate::client::GLOBAL_DATA;
use futures::task::Context;
use tokio::macros::support::Poll;
use unchecked_unwrap::UncheckedUnwrap;
use futures::StreamExt;
use webrtc_lib::media::MediaSenderEvent;
use std::ops::{Deref, DerefMut};
use webrtc_sdp::media_type::SdpMediaValue;

/* TODO: Support transport-cc within webrtc_lib in order to improve memory footprint or packet resending and caching the packets on broadcast level not for each individual client. */
pub enum InternalMediaSenderState {
    /// Owned and currently not in use
    Owned(Box<webrtc_lib::media::MediaSender>),
    /// Borrowed in order to send media.
    /// (owning client id, transferring media type)
    Borrowed(u32, MediaType)
}

pub struct InternalMediaSender {
    pub logger: slog::Logger,

    pub sender_id: u32,
    pub sender_type: SdpMediaValue,
    pub sender_source_id: u32,

    pub state: InternalMediaSenderState,
}

struct TimeTranslator {
    base_no: Option<u16>,
    virtual_no: u16,
}

pub struct RtpClientMediaSender {
    pub logger: slog::Logger,
    pub sender_id: u32,

    pub owning_session: Weak<Mutex<RTCConnection>>,
    pub owning_client_id: u32,
    pub owning_client_data: ClientData,

    pub sender_source_id: u32,
    pub source_client_data: ClientData,

    pub base_seq: Option<u16>,
    pub base_tsp: Option<u32>,

    pub virtual_base_seq: u16,
    pub virtual_base_tsp: u32,

    pub sender: Option<Box<webrtc_lib::media::MediaSender>>,
}

impl RtpClientMediaSender {
    fn send_transmission_end(&mut self, native_callback: bool) {
        if native_callback {
            (GLOBAL_DATA.callbacks().client_stream_stop)(self.owning_client_data.as_ptr(), self.sender_source_id, self.source_client_data.as_ptr());
        }

        let ssrc = vec![self.sender_source_id];
        let sender = unsafe { self.sender.as_mut().unchecked_unwrap() };
        let _ = sender.send_control(RtcpPacket::Bye(RtcpPacketBye{ reason: None, src: ssrc }));
    }

    fn send_stream_start(&mut self) {
        /* Just to ensure that the client got that the stream has been ended */
        self.send_transmission_end(false);
        (GLOBAL_DATA.callbacks().client_stream_start)(self.owning_client_data.as_ptr(), self.sender_source_id, self.source_client_data.as_ptr());
    }

    fn send(&mut self, data: &[u8], virt_seq_no: u16, marked: bool, virt_timestamp: u32, extension: Option<(u16, &[u8])>) {
        let sender = unsafe { self.sender.as_mut().unchecked_unwrap() };

        let seq_no;
        if let Some(base_seq_no) = self.base_seq {
            seq_no = base_seq_no.wrapping_add(virt_seq_no.wrapping_sub(self.virtual_base_seq));
            *sender.current_sequence_no_mut() = seq_no.wrapping_add(1);
        } else {
            /*
             * We assume this is the first packet in the transmission from this client.
             * We've ensured, that for the whole transmission virt_seq_no will be sequential.
             * Even though the RTP sender currently starts all virt_seq_no with zero, it's not ensured
             * and we have to set a virtual seq no base as well.
             */
            self.base_seq = Some(sender.current_sequence_no());
            self.virtual_base_seq = virt_seq_no;
            seq_no = sender.current_sequence_no();
        }

        let packet_tsp;
        if let Some(base_tsp) = self.base_tsp {
            packet_tsp = base_tsp.wrapping_add(virt_timestamp.wrapping_sub(self.virtual_base_tsp));
        } else {
            self.base_tsp = Some(sender.last_send_timestamp());
            self.virtual_base_tsp = virt_timestamp;
            packet_tsp = sender.last_send_timestamp();
        }

        //println!("[RTP] Mapped virt seq {} to {}, virt base {}, stream base {}", virt_seq_no, seq_no, self.virtual_base_no, self.base_seq_no.unwrap());

        sender.send_seq(data, seq_no, marked, packet_tsp, extension);
    }

    fn sender(&mut self) -> &mut webrtc_lib::media::MediaSender {
        unsafe { self.sender.as_mut().unchecked_unwrap() }.deref_mut()
    }
}

impl Drop for RtpClientMediaSender {
    fn drop(&mut self) {
        let ref_owner = self.owning_session.clone();
        let sender = self.sender.take().unwrap();
        let sender_id = self.sender_id;

        /* the drop may occur while our client mutex is still locked, that's why we're spawning a task for later */
        execute_task(async move {
            if let Some(session) = ref_owner.upgrade() {
                if let Ok(mut session) = session.lock() {
                    session.handle_media_sender_released(sender_id, sender);
                }
            }
        });
    }
}

pub struct RtpClientAudioSender {
    base: RtpClientMediaSender,

    opus_voice: u8,
    opus_music: u8
}

impl RtpClientAudioSender {
    pub fn new(base: RtpClientMediaSender) -> Self {
        let mut sender = RtpClientAudioSender{ base, opus_voice: 0, opus_music: 0 };

        sender.update_payload_format_ids();
        sender
    }

    fn update_payload_format_ids(&mut self) {
        let logger = self.base.logger.clone();
        let sender = self.base.sender();

        /* fallback if we don't yet know the remotes payload type for opus voice */
        self.opus_voice = OPUS_VOICE_PAYLOAD_TYPE;
        self.opus_music = OPUS_MUSIC_PAYLOAD_TYPE;

        if let Some(codecs) = sender.remote_codecs().deref() {
            let codec_opus_mono = codecs.iter().find(|e| e.codec_name == "opus" && e.parameters.as_ref().map_or(false, |p| p.stereo) == false);
            let codec_opus_stereo = codecs.iter().find(|e| e.codec_name == "opus" && e.parameters.as_ref().map_or(false, |p| p.stereo) == true);

            if codecs.len() == 1 {
                /* firefox I assume */
                if let Some(codec) = codec_opus_mono {
                    slog::slog_debug!(logger, "Remote host offered only opus mono. Using it to transmit mono and stereo...");
                    self.opus_voice = codec.payload_type;
                    self.opus_music = codec.payload_type;
                } else if let Some(codec) = codec_opus_stereo {
                    slog::slog_debug!(logger, "Remote host offered only opus stereo. Using it to transmit mono and stereo...");
                    self.opus_voice = codec.payload_type;
                    self.opus_music = codec.payload_type;
                } else {
                    slog::slog_debug!(logger, "Missing remote audio codec. Supplied codec is not opus mono nor opus stereo.");
                }
            } else {
                if let Some(codec) = codec_opus_mono {
                    slog::slog_debug!(logger, "Received remote audio codec {} (Opus Mono/Opus Voice)", codec.payload_type);

                    self.opus_voice = codec.payload_type;
                } else {
                    slog::slog_debug!(logger, "Missing remote audio codec (Opus Mono/Opus Voice). Using fallback");
                }

                if let Some(codec) = codec_opus_stereo {
                    slog::slog_debug!(logger, "Received remote audio codec {} (Opus Stereo/Opus Music)", codec.payload_type);

                    self.opus_music = codec.payload_type;
                } else {
                    slog::slog_debug!(logger, "Missing remote audio codec (Opus Stereo/Opus Music). Using fallback");
                }
            }
        }
    }
}

impl ClientAudioSender for RtpClientAudioSender {
    fn owner_id(&self) -> u32 {
        self.base.owning_client_id
    }

    fn send_start(&mut self) {
        self.base.send_stream_start();
    }

    fn send(&mut self, data: &[u8], virt_seq_no: u16, marked: bool, timestamp: u32, codec: AudioCodec, level: Option<u8>) {
        {
            let sender = self.base.sender();
            match codec {
                AudioCodec::Opus => {
                    *sender.payload_type_mut() = self.opus_voice;
                },
                AudioCodec::OpusMusic => {
                    *sender.payload_type_mut() = self.opus_music;
                }
            }
        }

        /* four bytes since extensions must have a length of a multiple of 32bits  */
        let mut extension_buffer = [0u8; 4];
        let mut extension = None;
        if let Some(level) = level {
            extension_buffer[0] = (LOCAL_EXT_ID_AUDIO_LEVEL << 4) as u8;
            extension_buffer[1] = level;
            extension = Some((0xBEDE, extension_buffer.as_slice()));
        }

        self.base.send(data, virt_seq_no, marked, timestamp, extension);
    }

    fn send_stop(&mut self, reason: AudioStopReason) {
        self.base.send_transmission_end(reason != AudioStopReason::InternalEnding);
    }

    fn poll_event(&mut self, cx: &mut Context) -> Poll<Option<()>> {
        let sender = self.base.sender();
        while let Poll::Ready(event) = sender.poll_next_unpin(cx) {
            if event.is_none() {
                return Poll::Ready(None);
            }

            match event.as_ref().unwrap() {
                MediaSenderEvent::RemoteCodecsUpdated => {
                    slog::slog_debug!(self.base.logger, "Remote audio codecs changed!");
                    self.update_payload_format_ids();
                    /* reschedule event poll */
                    cx.waker().wake_by_ref();
                    return Poll::Pending;
                },
                _ => {
                    /* we're currently not interested in that event */
                }
            }
        }

        return Poll::Pending;
    }
}

impl Drop for RtpClientAudioSender {
    fn drop(&mut self) {
        self.send_stop(AudioStopReason::InternalEnding);
    }
}

pub struct RtpClientVideoSender {
    base: RtpClientMediaSender,

    video_codec_supported: bool,
    last_video_codec: Option<VideoCodec>,

    h264_codec: Option<u8>,
    vp8_codec: Option<u8>
}

impl RtpClientVideoSender {
    pub fn new(base: RtpClientMediaSender) -> Self {
        let mut sender = RtpClientVideoSender{
            base,

            video_codec_supported: false,
            last_video_codec: None,

            h264_codec: None,
            vp8_codec: None
        };
        sender.update_video_codec();
        sender
    }

    fn update_video_codec(&mut self) {
        let logger = self.base.logger.clone();
        let sender = self.base.sender();

        self.h264_codec = None;
        self.vp8_codec = None;

        if let Some(codecs) = sender.remote_codecs().deref() {
            if let Some(codec) = codecs.iter().find(|e| e.codec_name == "H264") {
                self.h264_codec = Some(codec.payload_type);
            }

            if let Some(codec) = codecs.iter().find(|e| e.codec_name == "VP8") {
                self.vp8_codec = Some(codec.payload_type);
            }
        }

        slog::slog_debug!(logger, "Updated remote video codecs: (H264: {:?}, VP8: {:?})", &self.h264_codec, &self.vp8_codec);
        self.update_payload_type();
    }

    fn update_payload_type(&mut self) {
        if let Some(codec) = &self.last_video_codec {
            let payload_type = match codec {
                &VideoCodec::VP8 => self.vp8_codec.clone(),
                &VideoCodec::H264 => self.h264_codec.clone()
            };

            if let Some(payload_type) = payload_type {
                self.video_codec_supported = true;
                let sender = self.base.sender();
                *sender.payload_type_mut() = payload_type;
            } else {
                self.video_codec_supported = false;
                slog::slog_info!(self.base.logger, "Client does not support the video codec {:?}. Client will not receive any data", codec);

                /* TODO: Some kind of notification? */
            }
        } else {
            /* We're not sending any video. No update needed */
        }
    }
}

impl ClientVideoSender for RtpClientVideoSender {
    fn owner_id(&self) -> u32 {
        self.base.owning_client_id
    }

    fn owner_data(&self) -> &ClientData {
        &self.base.owning_client_data
    }

    fn send_start(&mut self) {
        self.base.send_stream_start();
    }

    fn send(&mut self, data: &[u8], codec: VideoCodec, virtual_seq_no: u16, marked: bool, virtual_timestamp: u32) {
        if self.last_video_codec.is_none() || unsafe { self.last_video_codec.unchecked_unwrap() } != codec {
            slog::slog_trace!(self.base.logger, "Having codec change from {:?} to {:?}.", &self.last_video_codec, codec);
            self.last_video_codec = Some(codec);

            self.update_payload_type();
        }

        if self.video_codec_supported {
            self.base.send(data, virtual_seq_no, marked, virtual_timestamp, None);
        }
    }

    fn send_stop(&mut self, reason: VideoStopReason) {
        self.base.send_transmission_end(reason != VideoStopReason::InternalEnding);
    }

    fn poll_event(&mut self, cx: &mut Context) -> Poll<Option<ClientVideoSenderEvent>> {
        let sender = unsafe { self.base.sender.as_mut().unchecked_unwrap() };
        while let Poll::Ready(event) = sender.poll_next_unpin(cx) {
            if event.is_none() {
                /* We've been closed */
                return Poll::Ready(None);
            }

            match event.as_ref().unwrap() {
                MediaSenderEvent::PayloadFeedbackReceived(fb) => {
                    if fb == &RtcpPayloadFeedback::PictureLossIndication {
                        return Poll::Ready(Some(ClientVideoSenderEvent::RequestPLI));
                    } else {
                        /* not really anything to do ;) */
                    }
                },
                MediaSenderEvent::RemoteCodecsUpdated => {
                    slog::slog_debug!(self.base.logger, "Remote video codecs changed!");
                    self.update_video_codec();

                    /* reschedule event poll */
                    cx.waker().wake_by_ref();
                    return Poll::Pending;
                },
                _ => {}
            }
        }

        Poll::Pending
    }
}
impl Drop for RtpClientVideoSender {
    fn drop(&mut self) {
        self.send_stop(VideoStopReason::InternalEnding);
    }
}