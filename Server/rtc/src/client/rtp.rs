use std::sync::{Weak, Mutex, Arc};
use std::task::{Poll, Context};
use std::os::raw::{c_void, c_char};
use crate::exports::{ GLOBAL_DATA };
use webrtc_lib::rtc::{PeerConnectionEvent, RtcDescriptionType, RemoteDescriptionApplyError, CreateAnswerError, PeerConnectionBuilder};
use futures::{StreamExt, FutureExt};
use std::collections::{VecDeque};
use futures::task::Waker;
use webrtc_sdp::media_type::SdpMediaValue;
use std::ptr;
use webrtc_sdp::{SdpSession, SdpBandwidth};
use std::cell::RefCell;
use webrtc_lib::media::MediaReceiver;
use webrtc_sdp::attribute_type::{SdpAttributeCandidate, SdpAttributeExtmap};
use webrtc_lib::transport::RTCTransportICECandidateAddError;
use crate::threads::{execute_task, enter_tasks};
use crate::client::{
    ClientData, ClientAudioSender, ClientVideoSender,
    RtpClientMediaSender, RtpClientAudioSender, RtpClientAudioSource,
    RtpClientVideoSender, RtpClientVideoSource, InternalMediaReceiver,
    InternalMediaSender, LOCAL_EXT_ID_AUDIO_LEVEL, OPUS_CODEC_STEREO,
    OPUS_CODEC_MONO, h264_codec, H264_PAYLOAD_TYPE,
    VP8_CODEC,
    InternalMediaSenderState
};
use crate::{MediaType};
use crate::channel::{ClientAudioSource, ClientVideoSource};
use crate::log::client_logger;
use std::ops::DerefMut;

pub struct RTCConnection {
    logger: slog::Logger,

    ref_self: Weak<Mutex<RTCConnection>>,
    client_id: u32,
    client_data: ClientData,

    /// Maximum video bitrate per video channel
    max_video_bitrate: u32,

    waker_peer_poll: Option<Waker>,
    waker_registered_receiver_poll: Option<Waker>,

    peer: webrtc_lib::rtc::PeerConnection,
    media_sender_id_index: u32,
    media_receiver_id_index: u32,

    /// A list of all registered sender
    registered_sender: VecDeque<InternalMediaSender>,

    /// A map containing the unused media receivers
    registered_receivers: VecDeque<InternalMediaReceiver>,
}

impl RTCConnection {
    pub fn new(logger: slog::Logger, client_id: u32, context: glib::MainContext, client_data: ClientData) -> Result<Arc<Mutex<RTCConnection>>, String> {
        let mut peer = webrtc_lib::rtc::PeerConnection::builder();
        peer.event_loop(context)
            .logger(client_logger(client_data.clone()));
        //peer.nice_compatibility(libnice::ffi::NiceCompatibility::RFC5245);
        //peer.nice_flags(libnice::sys::NiceAgentOption_NICE_AGENT_OPTION_ICE_TRICKLE | libnice::sys::NiceAgentOption_NICE_AGENT_OPTION_REGULAR_NOMINATION);

        let result = (GLOBAL_DATA.callbacks().rtc_configure)(client_data.as_ptr(), &mut peer as *mut PeerConnectionBuilder as *mut c_void);
        if result != 0 {
            return Err(format!("rtc_configure failed with {}", result));
        }

        let result = Arc::new(Mutex::new(RTCConnection {
            logger,
            ref_self: Weak::new(),

            client_id,
            client_data,

            waker_peer_poll: None,
            waker_registered_receiver_poll: None,

            max_video_bitrate: 10_000_000,

            peer: peer.create()?,
            media_sender_id_index: 1,
            media_receiver_id_index: 1,

            registered_sender: VecDeque::new(),
            registered_receivers: VecDeque::new(),
        }));
        result.lock().unwrap().ref_self = Arc::downgrade(&result);

        {
            let session = Arc::downgrade(&result);
            execute_task(async move {
                if let Some(session) = session.upgrade() {
                    if let Ok(session) = session.lock() {
                        session.execute_peer();
                    }
                }
            });
        }

        Ok(result)
    }

    fn execute_task<T>(&self, task: T)
        where T: 'static + Fn(&mut RTCConnection, &mut Context) -> bool + Send
    {
        let ref_self = self.ref_self.clone();
        tokio::spawn(tokio::future::poll_fn(move |cx| {
            if let Some(session) = ref_self.upgrade() {
                if let Ok(mut session) = session.lock() {
                    if task(session.deref_mut(), cx) {
                        return Poll::Ready(());
                    }

                    return Poll::Pending
                }
            }

            Poll::Ready(())
        }));
    }

    pub(crate) fn execute_peer(&self) {
        self.execute_task(|peer, cx| {
            peer.waker_peer_poll = Some(cx.waker().clone());

            while let Poll::Ready(event) = peer.peer.poll_next_unpin(cx) {
                if event.is_none() { return true; }

                peer.handle_peer_event(event.unwrap());
            }

            for internal_sender in peer.registered_sender.iter_mut() {
                if let InternalMediaSenderState::Owned(sender) = &mut internal_sender.state {
                    /* sender not in use; void all events */
                    while let Poll::Ready(event) = sender.poll_next_unpin(cx) {
                        if event.is_none() {
                            /* sender has been terminated. Removing him. */
                            internal_sender.sender_id = 0;
                            break;
                        }
                    }
                }
            }

            peer.registered_sender.retain(|sender| sender.sender_id > 0);

            false
        });

        self.execute_task(|peer, cx| {
            peer.waker_registered_receiver_poll = Some(cx.waker().clone());
            peer.registered_receivers.retain_mut(|receiver| {
                receiver.poll_unpin(cx) == Poll::Pending
            });

            false
        });
    }

    pub fn reset(&mut self) {
        self.peer.reset();
        self.registered_sender.clear();
        self.registered_receivers.clear();
    }

    pub fn apply_remote_description(&mut self, description: &SdpSession, mode: &RtcDescriptionType) -> Result<(), RemoteDescriptionApplyError> {
        if mode == &RtcDescriptionType::Offer {
            if description.media.len() > 32 {
                return Err(RemoteDescriptionApplyError::InternalError { detail: String::from("too many media lines")});
            }
        }
        self.peer.set_remote_description(description, mode)?;

        if mode == &RtcDescriptionType::Offer {
            enter_tasks(|| {
                while self.peer.free_send_media_lines(SdpMediaValue::Audio) > 0 {
                    self.create_media_sender(SdpMediaValue::Audio);
                }

                while self.peer.free_send_media_lines(SdpMediaValue::Video) > 0 {
                    self.create_media_sender(SdpMediaValue::Video);
                }
            });
        }
        Ok(())
    }

    pub fn add_remote_ice_candidate(&mut self, candidate: Option<&SdpAttributeCandidate>, media_line: usize) -> Result<(), RTCTransportICECandidateAddError> {
        self.peer.add_remote_ice_candidate(media_line, candidate)
    }

    pub fn generate_local_description(&mut self) -> Result<SdpSession, CreateAnswerError> {
        for line in self.peer.media_lines() {
            let mut line = RefCell::borrow_mut(line);
            match line.media_type {
                SdpMediaValue::Application => {},
                SdpMediaValue::Video => {
                    if line.local_codecs().is_empty() {
                        line.register_local_codec(VP8_CODEC.clone())
                            .expect("failed to register VP8 codec");
                    }

                    if line.allow_setting_change() {
                        let _ = line.unregister_local_codec(H264_PAYLOAD_TYPE);

                        /* TODO: These limits configurable via permissions? Is the frame rate even testable? */
                        let codec = h264_codec(30);
                        line.register_local_codec(codec.clone()).expect("failed to register local codec");
                        /* TODO: For FF set AS bandwidth; Currently the web client does that */
                    }
                },
                SdpMediaValue::Audio => {
                    if line.local_codecs().is_empty() {
                        line.register_local_extension(SdpAttributeExtmap{
                            id: LOCAL_EXT_ID_AUDIO_LEVEL as u16,
                            url: String::from("urn:ietf:params:rtp-hdrext:ssrc-audio-level"),
                            direction: None,
                            extension_attributes: None
                        }).expect("failed to register audio level extension");

                        line.register_local_codec(OPUS_CODEC_MONO.clone()).expect("failed to register local codec");
                        line.register_local_codec(OPUS_CODEC_STEREO.clone()).expect("failed to register local codec");
                    } else {
                        /* we don't reconfigure the audio channels */
                    }
                }
            }
        }

        let mut description = self.peer.create_local_description().map(|sdp| {
            sdp
        })?;

        for media_line in description.media.iter_mut() {
            media_line.add_bandwidth(SdpBandwidth::Tias(self.max_video_bitrate));
        }

        Ok(description)
    }

    /* FIXME: Only leak fn to rtp_sender */
    pub fn handle_media_sender_released(&mut self, sender_id: u32, sender: Box<webrtc_lib::media::MediaSender>) {
        if let Some(internal_sender) = self.registered_sender.iter_mut().find(|e| e.sender_id == sender_id) {
            internal_sender.state = InternalMediaSenderState::Owned(sender);

            if let Some(waker) = &self.waker_peer_poll {
                waker.wake_by_ref();
            }
            (GLOBAL_DATA.callbacks().client_stream_assignment)(self.client_data.as_ptr(), internal_sender.sender_source_id, 0, ptr::null());
        }
    }

    fn create_media_sender(&mut self, mtype: SdpMediaValue) {
        let sender = self.peer.create_media_sender(mtype.clone());
        let sender_source_id = sender.id();

        let sender = Box::new(sender);
        let internal_sender_id = self.media_sender_id_index;

        self.media_sender_id_index = self.media_sender_id_index.wrapping_add(1);
        self.registered_sender.push_back(InternalMediaSender{
            logger: self.logger.new(slog::o!("sender" => sender_source_id)),
            state: InternalMediaSenderState::Owned(sender),
            sender_id: internal_sender_id,
            sender_type: mtype,
            sender_source_id,
        });
        if let Some(waker) = &self.waker_peer_poll {
            waker.wake_by_ref();
        }
    }

    fn get_create_media_sender(&mut self, sender_type: MediaType, source_client_id: u32, source_client_data: &ClientData) -> RtpClientMediaSender {
        let sdp_media = match sender_type {
            MediaType::Audio |
            MediaType::AudioWhisper => SdpMediaValue::Audio,
            MediaType::Video |
            MediaType::VideoScreen => SdpMediaValue::Video
        };

        let media_type = match sender_type {
            MediaType::Audio => 0u8,
            MediaType::AudioWhisper => 1,
            MediaType::Video => 2,
            MediaType::VideoScreen => 3
        };

        let callback_data = self.client_data.as_ptr();
        let internal_sender = {
            if let Some(sender) = self.registered_sender.iter_mut()
                .find(|sender| sender.sender_type == sdp_media && matches!(sender.state, InternalMediaSenderState::Owned(_))) {
                sender
            } else {
                /* create six sender at once so we don't have to allocate new one that often */
                for _ in 0..6 {
                    self.create_media_sender(sdp_media.clone());
                }
                self.registered_sender.back_mut().unwrap()
            }
        };

        let sender = if let InternalMediaSenderState::Owned(sender) =
            std::mem::replace(&mut internal_sender.state, InternalMediaSenderState::Borrowed(source_client_id, sender_type)) {
            sender
        } else {
            panic!("queried sender should be owned!");
        };

        (GLOBAL_DATA.callbacks().client_stream_assignment)(callback_data, internal_sender.sender_source_id, media_type, source_client_data.as_ptr());

        RtpClientMediaSender {
            logger: internal_sender.logger.clone(),

            owning_session: self.ref_self.clone(),
            owning_client_id: self.client_id,
            owning_client_data: self.client_data.clone(),

            sender: Some(sender),
            sender_id: internal_sender.sender_id,
            sender_source_id: internal_sender.sender_source_id,

            base_tsp: None,
            base_seq: None,

            virtual_base_seq: 0,
            virtual_base_tsp: 0,

            source_client_data: source_client_data.clone(),
        }
    }

    fn execute_negotiation(&mut self) {
        match self.generate_local_description() {
            Ok(sdp) => {
                let offer = sdp.to_string();
                (GLOBAL_DATA.callbacks().client_offer_generated)(self.client_data.as_ptr(), offer.as_ptr() as *const c_char, offer.len() as u32);
            },
            Err(error) => {
                /* TODO: Error handing, the session cant be kept alive! */
                slog::slog_error!(self.logger, "Failed to generate a local description for offer: {:?}", error);
            }
        }

        slog::slog_debug!(self.logger, "PeerConnectionEvent NegotiationNeeded");
    }

    fn handle_peer_event(&mut self, event: PeerConnectionEvent) {
        match event {
            PeerConnectionEvent::NegotiationNeeded => {
                self.execute_negotiation();
            },
            PeerConnectionEvent::LocalIceCandidate(candidate, mline) => {
                if let Some(candidate) = &candidate {
                    let candidate = candidate.to_string();
                    (GLOBAL_DATA.callbacks().client_ice_candidate)(self.client_data.as_ptr(), mline as u32, candidate.as_ptr() as *const c_char, candidate.len() as u32);
                } else {
                    (GLOBAL_DATA.callbacks().client_ice_candidate)(self.client_data.as_ptr(), mline as u32, ptr::null(), 0);
                }

                slog::slog_trace!(self.logger, "Having a candidate on {}: {:?}", mline, candidate.map(|e| e.to_string()));
            },
            PeerConnectionEvent::ReceivedDataChannel(channel) => {
                slog::slog_trace!(self.logger, "The remote created a data channel ({}). Ignoring it.", channel.label());
            },
            PeerConnectionEvent::ReceivedRemoteStream(stream) => {
                self.register_remote_stream(stream);
            },
            _ => {}
        }
    }

    fn register_remote_stream(&mut self, mut stream: MediaReceiver) {
        let media_line = self.peer.media_line_by_unique_id(stream.media_line()).expect("missing remote streams assigned media line");

        if RefCell::borrow(&media_line).media_type == SdpMediaValue::Video {
            /* TODO: Permission related! */
            stream.set_bandwidth_limit(Some(self.max_video_bitrate));

            let resend_requester = stream.resend_requester_mut();
            /* We've bandwidths over 10MBit/sec which does not fit the default frame size of 64 */
            /* TODO: Set this dynamically according to the bandwidth limit */
            resend_requester.set_frame_size(1024);
            resend_requester.set_nack_delay(20);
            resend_requester.set_resend_interval(80);
        }

        let receiver_id = stream.ssrc();
        let internal_receiver = InternalMediaReceiver::new(
            self.logger.new(slog::o!("receiver" => receiver_id)),
            self.media_receiver_id_index,
            stream,
            RefCell::borrow(&media_line).media_type.clone()
        );
        self.media_receiver_id_index = self.media_receiver_id_index.wrapping_add(1);
        self.registered_receivers.push_back(internal_receiver);

        if let Some(waker) = &self.waker_registered_receiver_poll {
            waker.wake_by_ref();
        }

        slog::slog_trace!(self.logger, "Having {} receivers on hold", self.registered_receivers.len());
    }

    pub fn create_audio_sender(&mut self, sender_type: MediaType, source_client_id: u32, source_client_data: &ClientData) -> Box<dyn ClientAudioSender> {
        Box::new(RtpClientAudioSender::new(
            self.get_create_media_sender(sender_type, source_client_id, source_client_data)
        ))
    }

    pub fn create_video_sender(&mut self, sender_type: MediaType, source_client_id: u32, source_client_data: &ClientData) -> Box<dyn ClientVideoSender> {
        Box::new(RtpClientVideoSender::new(
            self.get_create_media_sender(sender_type, source_client_id, source_client_data)
        ))
    }

    pub fn create_audio_source(&mut self, stream_id: u32) -> Option<Box<dyn ClientAudioSource>> {
        if let Some(stream) = self.registered_receivers.iter_mut().find(|e| e.stream_id() == stream_id) {
            if stream.media_type() != SdpMediaValue::Audio {
                return None;
            }

            slog::slog_debug!(self.logger, "create_audio_source for stream {}", stream.stream_id());
            let source = RtpClientAudioSource::new(stream.get_receiver());
            Some(Box::new(source))
        } else {
            None
        }
    }


    pub fn create_video_source(&mut self, stream_id: u32) -> Option<Box<dyn ClientVideoSource>> {
        if let Some(stream) = self.registered_receivers.iter_mut().find(|e| e.stream_id() == stream_id) {
            if stream.media_type() != SdpMediaValue::Video {
                return None;
            }

            slog::slog_debug!(self.logger, "create_video_source for stream {}", stream.stream_id());
            let source = RtpClientVideoSource::new(stream.get_receiver(), self.client_data.clone());
            Some(Box::new(source))
        } else {
            None
        }
    }

    /// Returns the number of streams the client receives.
    /// The own streams are not included.
    /// (camera, screen)
    pub fn current_stream_count(&self) -> (usize, usize) {
        let mut camera = 0;
        let mut screen = 0;

        for sender in self.registered_sender.iter() {
            if let InternalMediaSenderState::Borrowed(client_id, media_type) = &sender.state {
                if *client_id == self.client_id {
                    continue;
                }

                match media_type {
                    &MediaType::VideoScreen => screen += 1,
                    &MediaType::Video => camera += 1,
                    _ => {}
                }
            }
        }

        (camera, screen)
    }
}