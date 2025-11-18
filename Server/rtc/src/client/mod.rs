use std::sync::{Arc, MutexGuard, Mutex, Weak};
use std::ffi::c_void;
use crate::{MediaType, exports::GLOBAL_DATA, AudioCodec, VideoCodec};
use std::task::{Poll};
use futures::task::Context;
use crate::channel::{ClientAudioSource, ClientVideoSource};
use std::ptr::null_mut;
use std::ops::DerefMut;
use unchecked_unwrap::UncheckedUnwrap;
use crate::broadcast::WhisperSession;
use crate::threads::{enter_tasks, execute_task};
use futures::FutureExt;

mod rtp;
pub use rtp::*;

mod rtp_codec;
pub use rtp_codec::*;

mod rtp_sender;
pub use rtp_sender::*;

mod rtp_receiver;
pub use rtp_receiver::*;

mod native;
pub use native::*;
use crate::log::client_logger;

#[derive(Clone)]
pub struct ClientData {
    inner: Arc<InternalClientData>
}

impl ClientData {
    pub fn as_ptr(&self) -> *const c_void {
        self.inner.inner
    }

    pub fn new(ptr: *mut c_void) -> Self {
        ClientData{
            inner: Arc::new(InternalClientData::new_from_ptr(ptr))
        }
    }

    pub fn new_null() -> Self {
        ClientData{
            inner: Arc::new(InternalClientData{ inner: null_mut() })
        }
    }
}

struct InternalClientData {
    inner: *mut c_void
}
impl InternalClientData {
    fn new_from_ptr(ptr: *mut c_void) -> InternalClientData {
        InternalClientData{ inner: ptr }
    }
}
impl Drop for InternalClientData {
    fn drop(&mut self) {
        if self.inner.is_null() {
            /* nothing to do */
        } else {
            (GLOBAL_DATA.callbacks().free_client_data)(self.inner);
        }
    }
}

unsafe impl Send for InternalClientData {}
unsafe impl Sync for InternalClientData {}

#[derive(Clone, Copy, Debug, Eq, PartialEq, Ord, PartialOrd)]
pub enum AudioStopReason {
    User,
    Timeout,
    InternalEnding /* do not send any stop notified */
}

pub trait ClientAudioSender: Send + Sync {
    fn owner_id(&self) -> u32;

    fn send_start(&mut self);
    fn send(&mut self, data: &[u8], virtual_seq_no: u16, marked: bool, timestamp: u32, codec: AudioCodec, level: Option<u8>);
    fn send_stop(&mut self, reason: AudioStopReason);

    fn poll_event(&mut self, cx: &mut Context) -> Poll<Option<()>>;
}

pub enum ClientVideoSenderEvent {
    RequestPLI
}

#[derive(Clone, Copy, Debug, Eq, PartialEq, Ord, PartialOrd)]
pub enum VideoStopReason {
    User,
    Timeout,
    InternalEnding /* do not send any stop notified */
}

pub trait ClientVideoSender: Send + Sync {
    fn owner_id(&self) -> u32;
    fn owner_data(&self) -> &ClientData;

    fn send_start(&mut self);
    fn send(&mut self, data: &[u8], codec: VideoCodec, virtual_seq_no: u16, marked: bool, timestamp: u32);
    fn send_stop(&mut self, reason: VideoStopReason);

    fn poll_event(&mut self, cx: &mut Context) -> Poll<Option<ClientVideoSenderEvent>>;
}


pub struct Client {
    logger: slog::Logger,

    ref_self: Weak<Mutex<Client>>,

    client_id: u32,
    client_data: ClientData,

    rtc_event_context: glib::MainContext,
    rtc_connection: Option<Arc<Mutex<RTCConnection>>>,
    native_connection: Option<Box<NativeConnection>>,

    whisper_session: Option<Arc<Mutex<WhisperSession>>>,
    whisper_session_id: u32,
}

impl Client {
    pub fn new(client_id: u32, context: glib::MainContext, client_data: *mut c_void) -> Arc<Mutex<Client>> {
        let client_data = ClientData::new(client_data);

        let client = Arc::new(Mutex::new(Client {
            logger: client_logger(client_data.clone()),
            ref_self: Weak::new(),

            client_id,
            client_data,

            rtc_connection: None,
            rtc_event_context: context,
            native_connection: None,

            whisper_session: None,
            whisper_session_id: 0,
        }));

        {
            let mut owned_client = client.lock().unwrap();
            owned_client.ref_self = Arc::downgrade(&client);
        }

        client
    }

    pub fn logger(&self) -> &slog::Logger {
        &self.logger
    }

    pub fn client_id(&self) -> u32 {
        self.client_id
    }

    pub fn client_data(&self) -> &ClientData {
        &self.client_data
    }

    pub fn reference(&self) -> Arc<Mutex<Client>> {
        self.ref_self.upgrade().expect("invalid self ref")
    }

    pub fn weak_reference(&self) -> Weak<Mutex<Client>> {
        self.ref_self.clone()
    }

    /// Initialize the rtc connection backend.
    /// This is required for video and (for the webclient) audio.
    pub fn initialize_rtc_connection(&mut self) -> Result<(), String> {
        if self.rtc_connection.is_some() {
            return Err("session already initialized".to_owned());
        }

        let session = RTCConnection::new(self.logger.clone(), self.client_id, self.rtc_event_context.clone(), self.client_data.clone())?;
        self.rtc_connection = Some(session);
        Ok(())
    }

    /// Initialize the native connection backend.
    pub fn initialize_native_connection(&mut self) {
        if self.native_connection.is_some() {
            return;
        }

        self.native_connection = Some(NativeConnection::new());
    }

    pub fn rtc_connection(&mut self) -> Option<MutexGuard<RTCConnection>> {
        self.rtc_connection
            .as_ref()
            .map(|e| e.lock().unwrap())
    }

    pub fn native_connection(&mut self) -> Option<&mut NativeConnection> {
        self.native_connection
            .as_mut()
            .map(|e| e.deref_mut())
    }

    pub fn create_audio_sender(&mut self, sender_type: MediaType, source_client_id: u32, source_client_data: &ClientData) -> Option<Box<dyn ClientAudioSender>> {
        if self.native_connection.is_some() {
            /* prefer the native audio connection over the WebRTC one */
            let client_id = self.client_id;
            let client_data = self.client_data.clone();

            let connection = unsafe { self.native_connection.as_mut().unchecked_unwrap() };
            Some(connection.create_audio_sender(client_id, client_data, sender_type, source_client_id, source_client_data))
        } else if let Some(mut connection) = self.rtc_connection() {
            Some(connection.create_audio_sender(sender_type, source_client_id, source_client_data))
        } else {
            None
        }
    }

    pub fn create_video_sender(&mut self, sender_type: MediaType, source_client_id: u32, source_client_data: &ClientData) -> Option<Box<dyn ClientVideoSender>> {
        if let Some(mut connection) = self.rtc_connection() {
            Some(connection.create_video_sender(sender_type, source_client_id, source_client_data))
        } else {
            None
        }
    }

    pub fn create_audio_source(&mut self, stream_id: u32) -> Option<Box<dyn ClientAudioSource>> {
        if let Some(connection) = self.native_connection() {
            /* prefer the native audio connection over the WebRTC one */
            connection.create_audio_source(stream_id)
        } else if let Some(mut connection) = self.rtc_connection() {
            connection.create_audio_source(stream_id)
        } else {
            None
        }
    }

    pub fn create_video_source(&mut self, stream_id: u32) -> Option<Box<dyn ClientVideoSource>> {
        if let Some(mut connection) = self.rtc_connection() {
            connection.create_video_source(stream_id)
        } else {
            None
        }
    }

    fn get_whisper_session(&self) -> Option<&Arc<Mutex<WhisperSession>>> {
        self.whisper_session.as_ref()
    }

    pub fn reset_whisper_session(&mut self) {
        self.whisper_session = None;
    }

    pub fn get_create_whisper_session(&mut self, stream_id: u32) -> Result<&Arc<Mutex<WhisperSession>>, String> {
        if let Some(session) = &self.whisper_session {
            if session.lock().unwrap().stream_id() == stream_id {
                return Ok(self.whisper_session.as_ref().unwrap());
            }
        }

        let session = WhisperSession::new(self, stream_id)?;
        let session_id = session.session_id();
        let session = Arc::new(Mutex::new(session));

        let weak_session = Arc::downgrade(&session);
        let weak_client = self.ref_self.clone();
        enter_tasks(move || {
            execute_task(tokio::future::poll_fn(move |cx| {
                if let Some(session) = weak_session.upgrade() {
                    let mut session = session.lock().unwrap();
                    if let Poll::Ready(_) = session.poll_unpin(cx) {
                        let session_id = session.session_id();
                        drop(session);

                        if let Some(client) = weak_client.upgrade() {
                            let mut client = client.lock().unwrap();
                            if client.whisper_session_id == session_id {
                                client.whisper_session = None;
                                client.whisper_session_id = 0;
                                (GLOBAL_DATA.callbacks().client_whisper_session_reset)(client.client_data.as_ptr());
                            }
                        }

                        Poll::Ready(())
                    } else {
                        Poll::Pending
                    }
                } else {
                    Poll::Ready(())
                }
            }));
        });

        self.whisper_session_id = session_id;
        self.whisper_session = Some(session);
        Ok(self.whisper_session.as_ref().unwrap())
    }
}