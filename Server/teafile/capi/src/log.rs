use slog::{Drain, OwnedKVList, Record, KV, o, Error, Level};
use std::fmt::Arguments;
use lazy_static::lazy_static;
use std::os::raw::c_char;
use crate::CAPI_GLOBAL_DATA;
use std::ops::Deref;

struct CustomSerializer {
    client_ip: Option<String>,
    buffer: String
}

impl slog::Serializer for CustomSerializer {
    fn emit_arguments(&mut self, key: &'static str, val: &Arguments) -> slog::Result {
        if key == "client" {
            self.client_ip = Some(format!("{}", val));
        } else {
            self.buffer.push_str(format!(", {}={}", key, val).as_str());
        }
        Ok(())
    }
}

pub struct CApiLog;
impl CApiLog {
    fn new() -> Self {
        CApiLog { }
    }
}

impl Drain for CApiLog {
    type Ok = ();
    type Err = Error;

    fn log(&self, record: &Record, values: &OwnedKVList) -> Result<Self::Ok, Self::Err> {
        let mut serializer = CustomSerializer { buffer: String::new(), client_ip: None };
        record.kv().serialize(record, &mut serializer)?;
        values.serialize(record, &mut serializer)?;

        let message = {
            if let Some(client_ip) = serializer.client_ip {
                format!("[{}] {}{}\n", client_ip, record.msg(), &serializer.buffer)
            } else {
                format!("{}{}\n", record.msg(), &serializer.buffer)
            }
        };

        let level = match record.level() {
            Level::Trace => 0u8,
            Level::Debug => 1,
            Level::Info => 2,
            Level::Warning => 3,
            Level::Error => 4,
            Level::Critical => 5
        };

        (CAPI_GLOBAL_DATA.callbacks().log)(level, message.as_ptr() as *const c_char, message.len() as u32);
        Ok(())
    }
}

fn init_capi_logger() -> slog::Logger {
    slog::Logger::root(slog::Fuse(CApiLog::new()), o!())
}

lazy_static!{
    static ref GLOBAL_LOGGER: slog::Logger = init_capi_logger();
}

pub fn capi_logger() -> &'static slog::Logger {
    GLOBAL_LOGGER.deref()
}