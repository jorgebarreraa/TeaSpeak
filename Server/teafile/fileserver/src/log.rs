use slog::{Drain, OwnedKVList, Record, KV, o, Error, Level};
use std::io::{Write};
use std::time::SystemTime;
use std::fs::File;
use std::sync::Mutex;
use std::path::Path;
use std::fmt::Arguments;
use chrono::{DateTime, Local};
use crate::terminal;

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

pub struct FileLog(Mutex<File>);
impl FileLog {
    fn new(file: File) -> Self {
        FileLog(Mutex::new(file))
    }
}

impl Drain for FileLog {
    type Ok = ();
    type Err = Error;

    fn log(&self, record: &Record, values: &OwnedKVList) -> Result<Self::Ok, Self::Err> {
        let mut serializer = CustomSerializer { buffer: String::new(), client_ip: None };
        record.kv().serialize(record, &mut serializer)?;
        values.serialize(record, &mut serializer)?;

        let system_time = SystemTime::now();
        let datetime: DateTime<Local> = system_time.into();
        let time = datetime.format("%d/%m %T%.3f");

        let level = match record.level() {
            Level::Trace => "TRACE",
            Level::Debug => "DEBUG",
            Level::Info => "INFO ",
            Level::Warning => "WARN ",
            Level::Error => "ERROR",
            Level::Critical => "CRITICAL"
        };

        let message = {
            if let Some(client_ip) = serializer.client_ip {
                format!("[{}] [{}] [{}] {}{}\n", time, level, client_ip, record.msg(), &serializer.buffer)
            } else {
                format!("[{}] [{}] {}{}\n", time, level, record.msg(), &serializer.buffer)
            }
        };

        self.0.lock().unwrap().write(message.as_bytes())?;
        Ok(())
    }
}

impl Drop for FileLog {
    fn drop(&mut self) {
        let _ = self.0.lock().unwrap().flush();
    }
}

pub struct TerminalLog;
impl TerminalLog {
    fn new() -> Self { TerminalLog{} }
}

impl Drain for TerminalLog {
    type Ok = ();
    type Err = Error;

    fn log(&self, record: &Record, values: &OwnedKVList) -> Result<Self::Ok, Self::Err> {
        let mut serializer = CustomSerializer { buffer: String::new(), client_ip: None };
        record.kv().serialize(record, &mut serializer)?;
        values.serialize(record, &mut serializer)?;

        match &serializer.client_ip {
            Some(ip) => {
                terminal::println(record.level(), &format!("[{}] {}{}", ip, record.msg(), &serializer.buffer));
            },
            None => {
                terminal::println(record.level(), &format!("{}{}", record.msg(), &serializer.buffer));
            }
        }
        Ok(())
    }
}

fn current_log_file_name() -> String {
    let system_time = SystemTime::now();
    let datetime: DateTime<Local> = system_time.into();

    return format!("logs/log_{}.txt", datetime.format("%y_%m_%d_%d_%H_%M_%S"));
}

pub fn create_file_logger() -> slog::Logger {
    let path = current_log_file_name();
    let path = Path::new(&path);
    if let Some(path) = path.parent() {
        std::fs::create_dir_all(path).expect("failed to create logging directories");
    }

    let file = std::fs::File::with_options()
        .write(true)
        .truncate(false)
        .create(true)
        .open(current_log_file_name())
        .expect("failed to open log file");

    let terminal_drain = TerminalLog::new().fuse();
    let file_drain = FileLog::new(file).fuse();

    let drain = slog::Duplicate(terminal_drain, file_drain).fuse();
    let drain = slog_async::Async::new(drain).build().fuse();

    slog::Logger::root(drain, o!())
}