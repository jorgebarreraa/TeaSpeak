#![feature(btree_drain_filter)]
#![feature(drain_filter)]
#![feature(box_syntax)]
#![feature(backtrace)]
/* Used for the panic handler breakpoint */
#![feature(core_intrinsics)]
#![feature(array_methods)]

#![allow(dead_code)]

mod exports;
mod client;
mod server;
mod threads;
mod channel;
mod extension;
mod sdp;
mod broadcast;
mod utils;
pub mod log;

use build_timestamp::build_time;

#[derive(Debug, Copy, Clone, Eq, PartialEq, Ord, PartialOrd)]
pub enum MediaType {
    Audio,
    AudioWhisper,
    Video,
    VideoScreen
}

#[derive(Debug, Copy, Clone, Eq, PartialEq, Ord, PartialOrd)]
pub enum AudioCodec {
    Opus,
    OpusMusic
}

#[derive(Debug, Copy, Clone, Eq, PartialEq, Ord, PartialOrd)]
pub enum VideoCodec {
    H264,
    VP8
}

impl AudioCodec {
    fn from(value: u8) -> Option<Self> {
        match value {
            0x04 => Some(AudioCodec::Opus),
            0x05 => Some(AudioCodec::OpusMusic),
            _ => None
        }
    }
}

impl Into<u8> for AudioCodec {
    fn into(self) -> u8 {
        match self {
            AudioCodec::Opus => 0x04,
            AudioCodec::OpusMusic => 0x05
        }
    }
}

build_time!("%c");
pub fn version() -> &'static str {
    BUILD_TIME
}