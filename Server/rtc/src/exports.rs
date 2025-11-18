use std::os::raw::{c_char, c_void};
use std::ffi::{CString, CStr};
use lazy_static::lazy_static;
use crate::{version, AudioCodec};
use std::ops::{Deref, DerefMut};
use std::cell::{UnsafeCell};
use std::ptr;
use crate::server::{Server};
use webrtc_lib::rtc::RtcDescriptionType;
use webrtc_sdp::attribute_type::{SdpAttribute};
use std::str::FromStr;
use crate::client::{NativeAudioSourceSupplier};
use parking_lot::{ RwLock };
use crate::threads::{execute_task, enter_tasks};
use std::sync::Arc;
use crate::log::global_logger;
use slog::slog_crit;
use std::process::abort;
use crate::broadcast::{BroadcastStartResult, VideoBroadcastMode, VideoBroadcastConfigureError, VideoBroadcastOptions};
use crate::channel::VideoBroadcastJoinResult;

const NATIVE_CALLBACKS_VERSION: u32 = 7;

#[repr(C)]
pub struct BroadcastInfo {
    pub broadcasting_client_id: u32,
    pub broadcasting_client_data: *const c_void,
    pub broadcast_type: u8,
}

#[repr(C)]
pub struct NativeCallbacks {
    version: u32,

    pub log: extern "C" fn(u8, *const c_void, *const c_char, u32),
    pub free_client_data: extern "C" fn(*mut c_void),

    pub rtc_configure: extern "C" fn(callback_data: *const c_void, configure_data: *mut c_void) -> u32,

    /// Callback if a client stream has been changed.
    /// If the `target_client_callback_data` equals null it indicates that the stream is not unused any more.
    pub client_stream_assignment: extern "C" fn(callback_data: *const c_void, stream_id: u32, media_type: u8, source_client_data: *const c_void),
    pub client_offer_generated: extern "C" fn(callback_data: *const c_void, offer: *const c_char, offer_length: u32),
    pub client_ice_candidate: extern "C" fn(callback_data: *const c_void, media_line: u32, candidate: *const c_char, candidate_length: u32),

    pub client_stream_start: extern "C" fn(callback_data: *const c_void, stream_id: u32, source_client_data: *const c_void),
    pub client_stream_stop: extern "C" fn(callback_data: *const c_void, stream_id: u32, source_client_data: *const c_void),

    pub client_video_join: extern "C" fn(callback_data: *const c_void, stream_id: u32, target_client_data: *const c_void),
    pub client_video_leave: extern "C" fn(callback_data: *const c_void, stream_id: u32, target_client_data: *const c_void),

    /// Send the clients broadcast info about broadcasts they can receive.
    pub client_video_broadcast_info: extern "C" fn(callback_data_array: *const *const c_void, callback_data_length: u32, broadcasts: *const BroadcastInfo, broadcast_count: u32),

    /// We're receiving audio.
    /// Mode 0 means normal channel audio. A value of 1 means whisper.
    pub client_audio_sender_data: extern "C" fn(callback_data: *const c_void, source_client_data: *const c_void, mode: u8, seq_no: u16, codec: u8, data: *const c_void, length: u32),

    pub client_whisper_session_reset: extern "C" fn(callback_data: *const c_void),
}

#[repr(C)]
#[derive(Debug)]
pub struct RtpClientConfigureOptions {
    min_port: u16,
    max_port: u16,

    ice_tcp: bool,
    ice_udp: bool,
    ice_upnp: bool,

    stun_host: *const c_char,
    stun_port: u16
}

pub struct WrappedCallbacks(*const NativeCallbacks);
impl Deref for WrappedCallbacks {
    type Target = NativeCallbacks;

    fn deref(&self) -> &Self::Target {
        unsafe { &*self.0 }
    }
}

pub struct GlobalData {
    version: *const c_char,
    callbacks: UnsafeCell<*const NativeCallbacks>,
}
unsafe impl Send for GlobalData {}
unsafe impl Sync for GlobalData {}

impl GlobalData {
    pub fn callbacks(&self) -> WrappedCallbacks {
        WrappedCallbacks(unsafe { *self.callbacks.get() })
    }
}

fn callback_void<T>() -> T {
    extern "C" fn callback() { }

    unsafe {
        let fn_ptr = std::mem::transmute::<extern "C" fn (), *const ()>(callback);
        std::mem::transmute_copy(&fn_ptr)
    }
}

extern "C" fn callback_rtc_configure_dummy(_: *const c_void, _: *mut c_void) -> u32 {
    1
}

lazy_static! {
    pub static ref GLOBAL_DATA: GlobalData = GlobalData{
        version: CString::new(version()).unwrap().into_raw(),
        callbacks: UnsafeCell::new(&*VOID_CALLBACKS)
    };

    static ref VOID_CALLBACKS: NativeCallbacks = NativeCallbacks {
        version: NATIVE_CALLBACKS_VERSION,

        log: callback_void(),
        free_client_data: callback_void(),

        rtc_configure: callback_rtc_configure_dummy,

        client_offer_generated: callback_void(),
        client_ice_candidate: callback_void(),

        client_stream_assignment: callback_void(),
        client_stream_start: callback_void(),
        client_stream_stop: callback_void(),
        client_audio_sender_data: callback_void(),
        client_video_broadcast_info: callback_void(),

        client_video_join: callback_void(),
        client_video_leave: callback_void(),

        client_whisper_session_reset: callback_void()
    };
}

unsafe fn ptr_to_server(ptr: *const c_void) -> &'static Arc<RwLock<Server>> {
    &*(ptr as *const Arc<RwLock<Server>>)
}

/// Returns a pointer to the library version.
/// Do NOT free the target pointer!
#[no_mangle]
pub extern "C" fn librtc_version() -> *const c_char {
    GLOBAL_DATA.version
}

/// Free a library allocated string.
#[no_mangle]
pub extern "C" fn librtc_free_str(ptr: *mut c_char) {
    unsafe { CString::from_raw(ptr) };
}

/// Initialize the library and setup the callbacks.
/// On error a pointer to the message will be returned.
/// You've to free the string by calling `librtc_free_str`.
#[no_mangle]
pub extern "C" fn librtc_init(callbacks: *mut NativeCallbacks, callbacks_length: usize) -> *const c_char {
    std::panic::set_hook(Box::new(|panic| {
        slog_crit!(global_logger(), "{:?}", panic);
        slog_crit!(global_logger(), "{:?}", std::backtrace::Backtrace::capture());
        eprintln!("{:?}", panic);
        eprintln!("{:?}", std::backtrace::Backtrace::capture());

        unsafe { std::intrinsics::breakpoint() };
        abort();
    }));

    if callbacks_length != std::mem::size_of::<NativeCallbacks>() {
        return CString::new("invalid callback size").unwrap().into_raw();
    }
    if unsafe { &*callbacks }.version != NATIVE_CALLBACKS_VERSION {
        return CString::new("invalid callback version").unwrap().into_raw();
    }

    unsafe { *UnsafeCell::get(&GLOBAL_DATA.callbacks) = callbacks; }
    enter_tasks(|| {
        webrtc_lib::initialize_webrtc(global_logger().clone());
    });
    std::ptr::null()
}

#[no_mangle]
pub extern "C" fn librtc_create_server() -> *mut c_void {
    let server = Box::new(Arc::new(RwLock::new(Server::new())));

    Box::into_raw(server) as *mut c_void
}

#[no_mangle]
pub extern "C" fn librtc_destroy_server(server: *mut c_void) {
    let _server = unsafe { Box::from_raw(server as *mut Arc<RwLock<Server>>) };
    /* TODO: Shutdown things? */
}

#[no_mangle]
pub extern "C" fn librtc_create_client(server: *mut c_void, user_data: *mut c_void) -> u32 {
    let server = unsafe { ptr_to_server(server) };
    let (id, _) = server.write().create_client(user_data);
    id
}

#[no_mangle]
pub extern "C" fn librtc_initialize_rtc_connection(server: *mut c_void, client_id: u32) -> *const c_char {
    let server = unsafe { ptr_to_server(server) };
    if let Some(client) = server.read().find_client(client_id) {
        if let Ok(mut client) = client.lock() {
            if let Err(reason) = client.initialize_rtc_connection() {
                CString::new(reason).unwrap().into_raw()
            } else {
                ptr::null()
            }
        } else {
            CString::new("failed to lock client").unwrap().into_raw()
        }
    } else {
        CString::new("invalid client handle").unwrap().into_raw()
    }
}

#[no_mangle]
pub extern "C" fn librtc_initialize_native_connection(server: *mut c_void, client_id: u32) -> *const c_char {
    let server = unsafe { ptr_to_server(server) };
    if let Some(client) = server.read().find_client(client_id) {
        if let Ok(mut client) = client.lock() {
            client.initialize_native_connection();
            ptr::null()
        } else {
            CString::new("failed to lock client").unwrap().into_raw()
        }
    } else {
        CString::new("invalid client handle").unwrap().into_raw()
    }
}

#[no_mangle]
pub extern "C" fn librtc_rtc_configure(callback_data: *mut c_void, config: *const RtpClientConfigureOptions, config_length: usize) -> *const c_char {
    if config_length != std::mem::size_of::<RtpClientConfigureOptions>() {
        return CString::new("invalid callback size").unwrap().into_raw();
    }

    let config = unsafe { &*config };
    let builder = unsafe { &mut *(callback_data as *mut webrtc_lib::rtc::PeerConnectionBuilder) };
    builder.ice_tcp(config.ice_tcp);
    builder.ice_udp(config.ice_udp);
    builder.ice_upnp(config.ice_upnp);

    slog::slog_trace!(global_logger(), "Configuring nice agent: {:?}", &config);
    if config.min_port > 0 && config.max_port > 0 {
        builder.ice_port_range((config.min_port.min(config.max_port), config.min_port.max(config.max_port)));
    }
    if config.stun_port > 0 && !config.stun_host.is_null() {
        builder.stun((unsafe { CStr::from_ptr(config.stun_host) }.to_string_lossy().into_owned(), config.stun_port));
    }
    ptr::null()
}

#[no_mangle]
pub extern "C" fn librtc_destroy_client(server: *mut c_void, client_id: u32) {
    let server = unsafe { ptr_to_server(server) }.clone();

    /* since we allow librtc_destroy_client to get called within the callbacks we've to ensure nothing has been locked */
    execute_task(async move {
        server.write().destroy_client(client_id);
    });
}

#[no_mangle]
pub extern "C" fn librtc_reset_rtp_session(server: *mut c_void, client_id: u32) {
    let server = unsafe { ptr_to_server(server) }.read();
    if let Some(client_ref) = server.find_client(client_id) {
        let mut client = client_ref.lock().unwrap();
        if let Some(mut connection) = client.rtc_connection() {
            connection.reset();
        } else {
            return;
        }

        drop(client);
        if let Some(channel) = server.get_channel(client_id) {
            let mut channel = channel.write();
            channel.unregister_client(client_id, true);
            channel.register_client(client_ref.clone(), client_ref.lock().unwrap().deref_mut());
        };
    }
}

#[no_mangle]
pub extern "C" fn librtc_apply_remote_description(server: *mut c_void, client_id: u32, mode: u32, description: *const c_char) -> *const c_char {
    let description = unsafe { CStr::from_ptr(description).to_string_lossy() };
    let description = description.into_owned(); /* if we're not doing this it panics... */
    let description = webrtc_sdp::parse_sdp(description.as_str(), false);
    if description.is_err() {
        return CString::new(format!("failed to parse sdp: {:?}", description.err().unwrap())).unwrap().into_raw();
    }
    let description = description.unwrap();

    let session_type = match mode {
        1 => RtcDescriptionType::Offer,
        2 => RtcDescriptionType::Answer,
        _ => return CString::new("invalid mode").unwrap().into_raw()
    };

    let server = unsafe { ptr_to_server(server) };
    if let Some(client) = server.read().find_client(client_id) {
        let mut client = client.lock().unwrap();
        if let Some(mut client) = client.rtc_connection() {
            if let Err(error) = client.apply_remote_description(&description, &session_type) {
                return CString::new(format!("{:?}", error)).unwrap().into_raw();
            }
        } else {
            return CString::new("missing rtc connection").unwrap().into_raw();
        };
    } else {
        return CString::new("invalid client handle").unwrap().into_raw();
    }

    ptr::null()
}

#[no_mangle]
pub extern "C" fn librtc_add_ice_candidate(server: *mut c_void, client_id: u32, media_line: usize, candidate: *const c_char) -> *const c_char {
    let server = unsafe { ptr_to_server(server) };
    if let Some(client) = server.read().find_client(client_id) {
        let mut client = client.lock().unwrap();

        if let Some(mut connection) = client.rtc_connection() {
            if candidate.is_null() {
                if let Err(error) = connection.add_remote_ice_candidate(None, media_line) {
                    return CString::new(format!("{:?}", error)).unwrap().into_raw();
                }
            } else {
                let candidate = SdpAttribute::from_str(unsafe { CStr::from_ptr(candidate) }.to_string_lossy().as_ref());
                if candidate.is_err() { return CString::new("candidate parsing failed").unwrap().into_raw(); }
                let candidate = { if let SdpAttribute::Candidate(c) = candidate.unwrap() { Ok(c) } else { Err(String::from("invalid candidate value")) } };
                if candidate.is_err() { return CString::new(candidate.err().unwrap()).unwrap().into_raw(); }

                if let Err(error) = connection.add_remote_ice_candidate(Some(&candidate.unwrap()), media_line) {
                    return CString::new(format!("{:?}", error)).unwrap().into_raw();
                }
            }
            return ptr::null()
        } else {
            return CString::new("missing rtc connection").unwrap().into_raw();
        };
    } else {
        return CString::new("invalid client handle").unwrap().into_raw();
    }
}

#[no_mangle]
pub extern "C" fn librtc_generate_local_description(server: *mut c_void, client_id: u32, description: *mut *const c_char) -> *const c_char {
    let server = unsafe { ptr_to_server(server) };
    if let Some(client) = server.read().find_client(client_id) {
        let mut client = client.lock().unwrap();

        if let Some(mut connection) = client.rtc_connection() {
            return match connection.generate_local_description() {
                Ok(sdp) => {
                    unsafe { *description = CString::new(sdp.to_string()).unwrap().into_raw(); };
                    ptr::null()
                },
                Err(error) => {
                    CString::new(format!("{:?}", error)).unwrap().into_raw()
                }
            }
        } else {
            return CString::new("client isn't a rtp client").unwrap().into_raw();
        };
    } else {
        CString::new("invalid client handle").unwrap().into_raw()
    }
}

#[no_mangle]
pub extern "C" fn librtc_client_video_stream_count(server: *mut c_void, client_id: u32, camera_count: *mut u32, screen_count: *mut u32) -> u32 {
    let server = unsafe { ptr_to_server(server) }.read();
    if let Some(client) = server.find_client(client_id) {
        let mut client = client.lock().unwrap();

        let (camera, screen) = if let Some(connection) = client.rtc_connection() {
            connection.current_stream_count()
        } else {
            (0, 0)
        };

        unsafe {
            *camera_count = camera as u32;
            *screen_count = screen as u32;
        }

        0
    } else {
        1
    }
}

/// Error code list:
/// `0x00` - Success
/// `0x01` - Invalid client
/// `0x02` - Client has no channel
/// `0x03` - (obsolete)
/// `0x04` - Invalid stream id
/// `0x05` - Invalid config
#[no_mangle]
pub extern "C" fn librtc_client_broadcast_audio(server: *mut c_void, client_id: u32, stream_id: u32) -> u32 {
    let server = unsafe { ptr_to_server(server) }.read();
    if let Some(channel) = server.get_channel(client_id) {
        let result = channel.write().broadcast_client_audio(client_id, stream_id);
        match result {
            BroadcastStartResult::ClientHasNoSource => if stream_id == 0 { 0 } else { 4 },
            BroadcastStartResult::InvalidClient => 2,
            BroadcastStartResult::Succeeded => 0,
            BroadcastStartResult::ConfigError(_) => 0x05
        }
    } else {
        2
    }
}

/// Error code list:
/// `0x00` - Success
/// `0x01` - Invalid client
/// `0x02` - Client has no channel
/// `0x03` - Invalid broadcast type
/// `0x04` - Invalid stream id
/// `0x05` - Invalid config
#[no_mangle]
pub extern "C" fn librtc_client_broadcast_video(server: *mut c_void, client_id: u32, btype: u8, stream_id: u32, config: *const VideoBroadcastOptions) -> u32 {
    let config = unsafe { &*config };
    let btype = match btype {
        0 => VideoBroadcastMode::Camera,
        1 => VideoBroadcastMode::Screen,
        _ => return 0x03 /* invalid broadcast type */
    };

    let server = unsafe { ptr_to_server(server) }.read();
    if let Some(channel) = server.get_channel(client_id) {
        let result = channel.write().broadcast_client_video(client_id, stream_id, btype, config);

        /* FIXME: Apply the video config! */

        match result {
            BroadcastStartResult::ClientHasNoSource => if stream_id == 0 { 0 } else { 4 },
            BroadcastStartResult::InvalidClient => 2,
            BroadcastStartResult::Succeeded => 0,
            BroadcastStartResult::ConfigError(_) => 0x05
        }
    } else {
        2
    }
}

/// Returns an u32 which indicates whatever updating has succeeded.
/// `0x00` Success
/// `0x01` Invalid video broadcast type
/// `0x02` Missing client
/// `0x03` Client isn't broadcasting
#[no_mangle]
pub extern "C" fn librtc_client_broadcast_video_configure(server: *mut c_void, client_id: u32, btype: u8, config: *const VideoBroadcastOptions) -> u32 {
    let config = unsafe { &*config };
    let server = unsafe { ptr_to_server(server) }.read();

    let btype = match btype {
        0 => VideoBroadcastMode::Camera,
        1 => VideoBroadcastMode::Screen,
        _ => return 1 /* invalid broadcast type */
    };

    if let Some(channel) = server.get_channel(client_id) {
        if let Some(broadcast) = channel.read()
            .get_video_broadcast(client_id, btype)
            .cloned() {

            let mut broadcast = broadcast.lock().unwrap();
            match broadcast.configure(config) {
                Ok(()) => 0,
                Err(_error) => {
                    /* FIXME: Currently we don't have any configure errors */
                    unreachable!();
                }
            }
        } else {
            3 /* not broadcasting */
        }
    } else {
        2 /* Missing client */
    }
}

#[no_mangle]
pub extern "C" fn librtc_client_broadcast_video_config(server: *mut c_void, client_id: u32, btype: u8, config: *mut VideoBroadcastOptions) -> u32 {
    let config = unsafe { &mut *config };
    let server = unsafe { ptr_to_server(server) }.read();

    let btype = match btype {
        0 => VideoBroadcastMode::Camera,
        1 => VideoBroadcastMode::Screen,
        _ => return 1 /* invalid broadcast type */
    };

    if let Some(channel) = server.get_channel(client_id) {
        if let Some(broadcast) = channel.read()
            .get_video_broadcast(client_id, btype)
            .cloned() {

            let broadcast = broadcast.lock().unwrap();
            *config = broadcast.config();
            0
        } else {
            3 /* not broadcasting */
        }
    } else {
        2 /* Missing client */
    }
}

/// Returns the created channel id
#[no_mangle]
pub extern "C" fn librtc_create_channel(server: *mut c_void) -> u32 {
    let server = unsafe { ptr_to_server(server) };
    let channel = server.write().create_channel();
    channel.0
}

#[no_mangle]
pub extern "C" fn librtc_assign_channel(server: *mut c_void, client_id: u32, channel_id: u32) -> u32 {
    let server = unsafe { ptr_to_server(server) };
    let result = { server.write().assign_channel(client_id, channel_id) };
    result.into()
}

#[no_mangle]
pub extern "C" fn librtc_video_broadcast_join(server: *mut c_void, client_id: u32, target_client_id: u32, broadcast_type: u8) -> u32 {
    let broadcast_type = match broadcast_type {
        0 => VideoBroadcastMode::Camera,
        1 => VideoBroadcastMode::Screen,
        _ => return 1 /* invalid broadcast mode */
    };

    let server = unsafe { ptr_to_server(server) };
    if let Some(channel) = server.read().get_channel(client_id) {
        let mut channel = channel.write();

        match channel.join_video_broadcast(client_id, target_client_id, broadcast_type) {
            VideoBroadcastJoinResult::Success => 0,
            VideoBroadcastJoinResult::InvalidBroadcast => 3,
            VideoBroadcastJoinResult::InvalidClient => 2
        }
    } else {
        /* client has no channel or is not known */
        2
    }
}

#[no_mangle]
pub extern "C" fn librtc_video_broadcast_leave(server: *mut c_void, client_id: u32, target_client_id: u32, broadcast_type: u8) {
    let broadcast_type = match broadcast_type {
        0 => VideoBroadcastMode::Camera,
        1 => VideoBroadcastMode::Screen,
        _ => return /* invalid broadcast mode */
    };

    let server = unsafe { ptr_to_server(server) };
    if let Some(channel) = server.read().get_channel(client_id) {
        let mut channel = channel.write();
        channel.leave_video_broadcast(client_id, target_client_id, broadcast_type);
    }
}

#[no_mangle]
pub extern "C" fn librtc_create_audio_source_supplier(server: *mut c_void, client_id: u32, stream_id: u32) -> *mut c_void {
    let server = unsafe { ptr_to_server(server) };
    if let Some(client) = server.read().find_client(client_id) {
        let mut client = client.lock().unwrap();

        if let Some(connection) = client.native_connection() {
            Box::into_raw(Box::new(connection.create_audio_source_supplier(stream_id))) as *mut c_void
        } else {
            ptr::null_mut()
        }
    } else {
        ptr::null_mut()
    }
}


#[no_mangle]
pub extern "C" fn librtc_audio_source_supply(sender: *mut c_void, seq_no: u16, marked: bool, timestamp: u32, codec: u8, data: *const c_void, len: u32) {
    let supplier = unsafe { &mut *(sender as *mut NativeAudioSourceSupplier) };

    if let Some(codec) = AudioCodec::from(codec) {
        if data.is_null() {
            supplier.send_stop();
        } else {
            supplier.send(
                unsafe { std::slice::from_raw_parts(data as *const u8, len as usize) },
                seq_no,
                marked,
                timestamp,
                codec
            );
        }
    } else {
        /* We don't support that codec */
    }
}

#[no_mangle]
pub extern "C" fn librtc_destroy_audio_source_supplier(sender: *mut c_void) {
    unsafe { Box::from_raw(sender as *mut NativeAudioSourceSupplier) };
}

#[no_mangle]
pub extern "C" fn librtc_destroy_channel(server: *mut c_void, channel: u32) {
    let server = unsafe { ptr_to_server(server) };
    server.write().destroy_channel(channel);
}

#[no_mangle]
pub extern "C" fn librtc_whisper_configure(server: *mut c_void, client_id: u32, source_stream: u32, client_ids: *const u32, client_id_count: u32) -> *const c_char {
    let clients = unsafe { std::slice::from_raw_parts(client_ids, client_id_count as usize) };
    let server = unsafe { ptr_to_server(server) }.read();

    if let Some(client) = server.find_client(client_id) {
        let mut client = client.lock().unwrap();

        match client.get_create_whisper_session(source_stream) {
            Ok(session) => {
                let session = session.clone();
                let mut session = session.lock().unwrap();

                let new_clients = session.update_target_clients(clients);
                for participant_client_id in new_clients {
                    if participant_client_id == client_id {
                        session.register_client(client.deref_mut());
                    } else if let Some(client) = server.find_client(participant_client_id) {
                        session.register_client(client.lock().unwrap().deref_mut());
                    } else {
                        /* client not found, but we will not return an error here */
                    }
                }

                ptr::null()
            },
            Err(error) => {
                CString::new(error).unwrap().into_raw()
            }
        }
    } else {
        CString::new("invalid client handle").unwrap().into_raw()
    }
}

#[no_mangle]
pub extern "C" fn librtc_whisper_reset(server: *mut c_void, client_id: u32) {
    let server = unsafe { ptr_to_server(server) }.read();

    if let Some(client) = server.find_client(client_id) {
        let mut client = client.lock().unwrap();
        client.reset_whisper_session();
    }
}