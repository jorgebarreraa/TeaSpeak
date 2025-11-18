use slog::{Drain, OwnedKVList, Record, KV, o, Error, Level};
use std::ops::{Deref};
use crate::client::ClientData;
use lazy_static::lazy_static;
use crate::exports::{ GLOBAL_DATA };
use std::os::raw::c_char;
use std::fmt::Arguments;

struct TeaSerializer {
    buffer: String
}

impl slog::Serializer for TeaSerializer {
    fn emit_arguments(&mut self, key: &'static str, val: &Arguments) -> slog::Result {
        self.buffer.push_str(format!(", {}={}", key, val).as_str());
        Ok(())
    }
}

pub struct TeaLog {
    client_data: ClientData,
}

impl TeaLog {
    fn new(client_data: ClientData) -> Self {
        TeaLog {
            client_data
        }
    }
}

impl Drain for TeaLog {
    type Ok = ();
    type Err = Error;

    fn log(&self, record: &Record, values: &OwnedKVList) -> Result<Self::Ok, Self::Err> {
        let mut serializer = TeaSerializer{ buffer: String::new() };
        record.kv().serialize(record, &mut serializer)?;
        values.serialize(record, &mut serializer)?;

        let message = format!("{}{}", record.msg(), &serializer.buffer);
        //println!("RTC - {}", message); // FIXME: Callback to native with the appropriate level

        let level = match record.level() {
            Level::Trace => 0u8,
            Level::Debug => 1,
            Level::Info => 2,
            Level::Warning => 3,
            Level::Error => 4,
            Level::Critical => 5
        };

        (GLOBAL_DATA.callbacks().log)(level, self.client_data.as_ptr(), message.as_ptr() as *const c_char, message.len() as u32);
        Ok(())
    }
}

fn init_global_logger() -> slog::Logger {
    slog::Logger::root(slog::Fuse(TeaLog::new(ClientData::new_null())), o!())
}

lazy_static!{
    static ref GLOBAL_LOGGER: slog::Logger = init_global_logger();
}

pub fn global_logger() -> &'static slog::Logger {
    GLOBAL_LOGGER.deref()
}

pub fn client_logger(callback_data: ClientData) -> slog::Logger {
    slog::Logger::root(slog::Fuse(TeaLog::new(callback_data)), o!())
}