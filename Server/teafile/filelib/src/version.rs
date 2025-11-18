use lazy_static;
use build_timestamp::build_time;

const SERVER_VERSION: &'static str = env!("CARGO_PKG_VERSION");

lazy_static::lazy_static!{
    static ref OS_TYPE: String = sys_info::os_type().unwrap_or("Unknown".to_owned());
}

build_time!("%c");
pub fn build_timestamp() -> &'static str {
    BUILD_TIME
}


pub fn server_version() -> &'static str {
    SERVER_VERSION
}

pub fn os_type() -> &'static str {
    &OS_TYPE
}