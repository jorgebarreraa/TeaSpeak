#![feature(backtrace)]
#![feature(core_intrinsics)]
#![feature(vec_into_raw_parts)]
#![feature(into_future)]

mod log;
mod threads;

use std::sync::{Arc};
use parking_lot::{Mutex, RwLock};
use std::ffi::{CString, CStr};
use lazy_static::lazy_static;
use std::ops::Deref;
use std::os::raw::{c_char};
use std::ptr;
use std::process::abort;
use std::cell::UnsafeCell;
use slog::*;
use teaspeak_filelib::server::FileServer;
use teaspeak_filelib::request_handler::server::FileServerInstance;
use crate::log::capi_logger;
use teaspeak_filelib::version::build_timestamp;
use teaspeak_filelib::files::{FilePath, PathQueryResult, PathInfo, DirectoryQueryResult, PathDeleteResult, DirectoryCreateResult, PathRenameResult};
use std::time::SystemTime;
use tokio::time;
use tokio::time::Duration;
use crate::threads::{runtime_enter_guard};

const NATIVE_CALLBACKS_VERSION: u32 = 1;

#[repr(C)]
pub struct NativeCallbacks {
    version: u32,

    pub log: extern "C" fn(u8, *const c_char, u32),
}

#[repr(C)]
pub struct CFilePath {
    /// `0` channel file
    /// `1` icon
    /// `2` avatar
    file_type: u32,
    channel_id: u64,
    path: *const c_char
}

impl CFilePath {
    pub fn as_rust(&self) -> Option<FilePath> {
        let rel_path = if !self.path.is_null() {
            unsafe { CStr::from_ptr(self.path).to_string_lossy().to_string() }
        } else {
            "".to_string()
        };
        match self.file_type {
            0 => Some(FilePath::Channel(self.channel_id, rel_path)),
            1 => Some(FilePath::Icon(rel_path)),
            2 => Some(FilePath::Avatar(rel_path)),
            _ => None
        }
    }
}

#[repr(C)]
pub struct CPathInfo {
    /// Path query result.
    /// `-1` end of info array
    /// `0` success
    /// `1` invalid path
    /// `2` path does not exists
    /// `3` unknown files type (path info only)
    /// `4` path is not a directory (path query only)
    /// `5` io error (path query and delete only)
    /// `6` file is locked (delete only)
    query_result: i32,

    /// Target file type.
    /// `1` directory
    /// `2` file
    file_type: u32,

    /// Target file/directory name.
    /// In case of file delete this might hold error detail.
    name: *const c_char,

    /// Target file/directory modify timestamp in seconds since UNIX epoc
    modify_timestamp: u64,

    /// Target file size
    file_size: u64,

    /// If the target directory is empty
    directory_empty: bool,
}

impl CPathInfo {
    pub fn end() -> Self {
        CPathInfo{
            query_result: -1,
            file_type: 0,
            name: ptr::null(),
            modify_timestamp: 0,
            file_size: 0,
            directory_empty: false
        }
    }

    pub fn query_error(error: i32) -> Self {
        CPathInfo{
            query_result: error,
            file_type: 0,
            name: ptr::null(),
            modify_timestamp: 0,
            file_size: 0,
            directory_empty: false
        }
    }
}

impl From<PathQueryResult> for CPathInfo {
    fn from(result: PathQueryResult) -> Self {
        match result {
            PathQueryResult::Success(info) => info.into(),
            PathQueryResult::InvalidPath => CPathInfo::query_error(1),
            PathQueryResult::PathDoesNotExists => CPathInfo::query_error(2),
            PathQueryResult::UnknownFileType => CPathInfo::query_error(3),
        }
    }
}

impl From<PathInfo> for CPathInfo {
    fn from(info: PathInfo) -> Self {
        match info {
            PathInfo::File(info) => CPathInfo{
                query_result: 0,
                file_type: 2,
                name: CString::new(info.name).unwrap().into_raw(),
                modify_timestamp: info.modify_timestamp.duration_since(SystemTime::UNIX_EPOCH).unwrap().as_secs() as u64,
                file_size: info.size,
                directory_empty: false
            },
            PathInfo::Directory(info) => CPathInfo{
                query_result: 0,
                file_type: 2,
                name: CString::new(info.name).unwrap().into_raw(),
                modify_timestamp: info.modify_timestamp.duration_since(SystemTime::UNIX_EPOCH).unwrap().as_secs() as u64,
                file_size: 0,
                directory_empty: info.empty
            }
        }
    }
}

impl From<PathDeleteResult> for CPathInfo {
    fn from(result: PathDeleteResult) -> Self {
        match result {
            PathDeleteResult::Success => CPathInfo::query_error(0),
            PathDeleteResult::InvalidPath => CPathInfo::query_error(1),
            PathDeleteResult::PathDoesNotExists => CPathInfo::query_error(2),
            PathDeleteResult::IoError => CPathInfo::query_error(5),
            PathDeleteResult::PathLocked => CPathInfo::query_error(6),
        }
    }
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
    internals: UnsafeCell<Option<ApiInternals>>,
}

impl GlobalData {
    pub fn callbacks(&self) -> WrappedCallbacks {
        WrappedCallbacks(unsafe { *self.callbacks.get() })
    }

    pub fn file_server(&self) -> Option<Arc<Mutex<FileServer>>> {
        let internals = unsafe { &*self.internals.get() }.as_ref();
        internals.map(|internals| internals.file_server.clone())
    }

    pub fn file_instance(&self) -> Option<Arc<RwLock<FileServerInstance>>> {
        let internals = unsafe { &*self.internals.get() }.as_ref();
        internals.map(|internals| internals.file_instance.clone())
    }
}
unsafe impl Send for GlobalData {}
unsafe impl Sync for GlobalData {}

struct ApiInternals {
    file_server: Arc<Mutex<FileServer>>,
    file_instance: Arc<RwLock<FileServerInstance>>,
}

lazy_static!{
    pub static ref CAPI_GLOBAL_DATA: GlobalData = GlobalData{
        version: CString::new(build_timestamp()).unwrap().into_raw(),
        callbacks: UnsafeCell::new(&*VOID_CALLBACKS),
        internals: UnsafeCell::new(None)
    };

    static ref VOID_CALLBACKS: NativeCallbacks = NativeCallbacks {
        version: NATIVE_CALLBACKS_VERSION,

        log: callback_void(),
    };
}

fn callback_void<T>() -> T {
    extern "C" fn callback() { }

    unsafe {
        let fn_ptr = std::mem::transmute::<extern "C" fn (), *const ()>(callback);
        std::mem::transmute_copy(&fn_ptr)
    }
}

/// Returns a pointer to the library version.
/// Do NOT free the target pointer!
#[no_mangle]
pub extern "C" fn libteaspeak_file_version() -> *const c_char {
    CAPI_GLOBAL_DATA.version
}

/// Free a library allocated string.
#[no_mangle]
pub extern "C" fn libteaspeak_free_str(ptr: *mut c_char) {
    unsafe { CString::from_raw(ptr); }
}

/// Free a library allocated string.
#[no_mangle]
pub extern "C" fn libteaspeak_free_path_info(ptr: *mut c_char) {
    unsafe { CString::from_raw(ptr); }
}

#[no_mangle]
pub extern "C" fn libteaspeak_file_initialize(callbacks: *mut NativeCallbacks, callbacks_length: usize) -> *const c_char {
    if unsafe { &*CAPI_GLOBAL_DATA.internals.get() }.is_some() {
        return CString::new("api already initialized").unwrap().into_raw();
    }

    std::panic::set_hook(Box::new(|panic| {
        slog_crit!(capi_logger(), "{:?}", panic);
        slog_crit!(capi_logger(), "{:?}", std::backtrace::Backtrace::capture());
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

    let _ = runtime_enter_guard();
    let file_server: Arc<Mutex<FileServer>> = Arc::new(Mutex::new(FileServer::new(capi_logger().clone())));
    let file_instance = {
        let file_server = file_server.lock();
        let mut transfer_handler = file_server.transfer_handler().write();
        transfer_handler.create_server_instance("__internal".to_owned(), (0, 0))
    };

    unsafe {
        *CAPI_GLOBAL_DATA.internals.get() = Some(ApiInternals{
            file_server,
            file_instance
        });
    }

    ptr::null()
}

#[no_mangle]
pub extern "C" fn libteaspeak_file_finalize() {
    let data = unsafe {
        let internals = &CAPI_GLOBAL_DATA.internals;
        (*internals.get()).take()
    };
    let data = match data {
        Some(data) => data,
        None => return
    };

    let _ = runtime_enter_guard();
    let mut file_server = data.file_server.lock();
    let result = time::timeout(Duration::from_secs(5), file_server.shutdown());
    let result = tokio::runtime::Handle::current().block_on(result);
    if result.is_err() {
        warn!(capi_logger(), "Failed to shutdown server within 5 seconds (some clients may not be disconnected properly).");
    }
}

#[no_mangle]
pub extern "C" fn libteaspeak_file_free_file_info(ptr: *const CPathInfo) {
    if ptr.is_null() {
        return;
    }

    let data = unsafe {
        let mut length = 0;
        let mut ptr = ptr.clone();
        while (*ptr).query_result != -1 {
            ptr = ptr.add(1);
            length += 1;
        }
        Vec::from_raw_parts(ptr as *mut CPathInfo, length, length)
    };

    for entry in data {
        if !entry.name.is_null() {
            unsafe { CString::from_raw(entry.name as *mut _) };
        }
    }
}

#[no_mangle]
pub extern "C" fn libteaspeak_file_system_register_server(server_unique_id: *const c_char) {
    let server_unique_id = unsafe { CStr::from_ptr(server_unique_id).to_string_lossy().to_string() };
    let file_server = CAPI_GLOBAL_DATA.file_instance().expect("missing file instance");
    let mut file_server = file_server.write();
    file_server.create_virtual_server(server_unique_id, (0, 0));
}

#[no_mangle]
pub extern "C" fn libteaspeak_file_system_unregister_server(server_unique_id: *const c_char, delete_files: bool) {
    let server_unique_id = unsafe { CStr::from_ptr(server_unique_id).to_string_lossy().to_string() };
    let file_server = CAPI_GLOBAL_DATA.file_instance().expect("missing file instance");
    let mut file_server = file_server.write();

    file_server.delete_virtual_server(&server_unique_id, delete_files);
}

/// Query several files.
/// result must be freed with `libteaspeak_file_free_file_info`.
#[no_mangle]
pub extern "C" fn libteaspeak_file_system_query_file_info(server_unique_id: *const c_char, cpaths: *const CFilePath, path_count: usize, result: *mut *const CPathInfo) -> *const c_char {
    let server_unique_id = unsafe { CStr::from_ptr(server_unique_id).to_string_lossy() };
    let cpaths = unsafe { std::slice::from_raw_parts(cpaths, path_count) };
    let mut paths = Vec::with_capacity(cpaths.len());
    for path in cpaths.iter() {
        if let Some(path) = path.as_rust() {
            paths.push(path);
        } else {
            return CString::new("invalid path type entry").unwrap().into_raw();
        }
    }

    let file_server = CAPI_GLOBAL_DATA.file_instance().expect("missing file instance");
    let virtual_server = match file_server.read().find_virtual_server(server_unique_id.deref()) {
        Some(server) => server.clone(),
        None => return CString::new("unknown server").unwrap().into_raw()
    };

    let file_system = virtual_server.read().file_system().clone();
    let file_system = file_system.read();

    let mut results = Vec::with_capacity(cpaths.len() + 1);
    for result in file_system.query_path_info(&paths) {
        results.push(result.into());
    }
    results.push(CPathInfo::end());
    results.shrink_to_fit();

    unsafe { *result = results.into_raw_parts().0; };
    ptr::null()
}

/// Query all files within a directory
/// result must be freed with `libteaspeak_file_free_file_info`.
#[no_mangle]
pub extern "C" fn libteaspeak_file_system_query_directory(server_unique_id: *const c_char, path: *const CFilePath, result: *mut *const CPathInfo) -> *const c_char {
    let server_unique_id = unsafe { CStr::from_ptr(server_unique_id).to_string_lossy() };
    let path = match unsafe { &*path }.as_rust() {
        Some(path) => path,
        None => return CString::new("invalid path").unwrap().into_raw()
    };

    let file_server = CAPI_GLOBAL_DATA.file_instance().expect("missing file instance");
    let virtual_server = match file_server.read().find_virtual_server(server_unique_id.deref()) {
        Some(server) => server.clone(),
        None => return CString::new("unknown server").unwrap().into_raw()
    };

    let file_system = virtual_server.read().file_system().clone();
    let file_system = file_system.read();

    let mut results = Vec::with_capacity(2);
    match file_system.query_directory_entries(&path) {
        DirectoryQueryResult::Success(entries) => {
            results.reserve(entries.len());
            for entry in entries {
                results.push(entry.into());
            }
        },
        DirectoryQueryResult::InvalidPath => results.push(CPathInfo::query_error(1)),
        DirectoryQueryResult::PathDoesNotExists => results.push(CPathInfo::query_error(2)),
        DirectoryQueryResult::PathIsNotADirectory => results.push(CPathInfo::query_error(4)),
        DirectoryQueryResult::IoError => results.push(CPathInfo::query_error(5)),
    }
    results.push(CPathInfo::end());
    results.shrink_to_fit();

    unsafe { *result = results.into_raw_parts().0; };
    ptr::null()
}

#[no_mangle]
pub extern "C" fn libteaspeak_file_system_delete_files(server_unique_id: *const c_char, cpaths: *const CFilePath, path_count: usize, result: *mut *const CPathInfo) -> *const c_char {
    let server_unique_id = unsafe { CStr::from_ptr(server_unique_id).to_string_lossy() };
    let paths = {
        let cpaths = unsafe { std::slice::from_raw_parts(cpaths, path_count) };
        let mut paths = Vec::with_capacity(cpaths.len());
        for path in cpaths.iter() {
            if let Some(path) = path.as_rust() {
                paths.push(path);
            } else {
                return CString::new("invalid path type entry").unwrap().into_raw();
            }
        }
        paths
    };

    let file_server = CAPI_GLOBAL_DATA.file_instance().expect("missing file instance");
    let virtual_server = match file_server.read().find_virtual_server(server_unique_id.deref()) {
        Some(server) => server.clone(),
        None => return CString::new("unknown server").unwrap().into_raw()
    };

    let file_system = virtual_server.read().file_system().clone();
    let file_system = file_system.read();

    let mut results = Vec::with_capacity(paths.len() + 1);
    for result in file_system.delete_file(&paths) {
        results.push(result.into());
    }
    results.push(CPathInfo::end());
    results.shrink_to_fit();

    unsafe { *result = results.into_raw_parts().0; };
    ptr::null()
}

/// Return codes:
/// `1` Server unique not found
/// `2` Invalid path
/// `3` Path already exists
/// `4` IO Error
#[no_mangle]
pub extern "C" fn libteaspeak_file_system_create_channel_directory(server_unique_id: *const c_char, channel_id: u64, path: *const c_char) -> u32 {
    let server_unique_id = unsafe { CStr::from_ptr(server_unique_id).to_string_lossy() };
    let path = unsafe { CString::from_raw(path as *mut c_char).to_string_lossy().to_string() };

    let file_server = CAPI_GLOBAL_DATA.file_instance().expect("missing file instance");
    let virtual_server = match file_server.read().find_virtual_server(server_unique_id.deref()) {
        Some(server) => server.clone(),
        None => return 1
    };

    let file_system = virtual_server.read().file_system().clone();
    let file_system = file_system.read();

    let result = file_system.create_directories(&[
        FilePath::Channel(channel_id, path.to_string())
    ]).pop().expect("missing result");

    match result {
        DirectoryCreateResult::Success => 0,
        DirectoryCreateResult::InvalidPath => 2,
        DirectoryCreateResult::PathAlreadyExists => 3,
        DirectoryCreateResult::IoError => 4,
    }
}

/// Return codes:
/// `1` Server unique not found
/// `2` Path invalid type
/// `3` Invalid source path
/// `4` Invalid target path
/// `5` Source not found
/// `6` Source locked
/// `7` Target already exists
/// `8` IO error
#[no_mangle]
pub extern "C" fn libteaspeak_file_system_rename_channel_file(server_unique_id: *const c_char, old_channel_id: u64, old_path: *const c_char, new_channel_id: u64, new_path: *const c_char) -> u32 {
    let server_unique_id = unsafe { CStr::from_ptr(server_unique_id).to_string_lossy().to_string() };
    let old_path = unsafe { CString::from_raw(old_path as *mut c_char).to_string_lossy().to_string() };
    let new_path = unsafe { CString::from_raw(new_path as *mut c_char).to_string_lossy().to_string() };

    let file_server = CAPI_GLOBAL_DATA.file_instance().expect("missing file instance");
    let virtual_server = match file_server.read().find_virtual_server(server_unique_id.deref()) {
        Some(server) => server.clone(),
        None => return 1
    };

    let file_system = virtual_server.read().file_system().clone();
    let file_system = file_system.read();

    let result = file_system.rename_file(&FilePath::Channel(old_channel_id, old_path), &FilePath::Channel(new_channel_id, new_path));

    match result {
        PathRenameResult::Success => 0,
        PathRenameResult::PathInvalidType => 2,
        PathRenameResult::InvalidSourcePath => 3,
        PathRenameResult::InvalidTargetPath => 4,
        PathRenameResult::SourceNotFound => 5,
        PathRenameResult::SourcePathLocked => 6,
        PathRenameResult::TargetPathAlreadyExists => 7,
        PathRenameResult::IoError => 8
    }
}