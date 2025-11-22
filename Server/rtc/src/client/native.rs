use crate::client::{ClientData, ClientAudioSender, AudioStopReason};
use crate::{MediaType, AudioCodec};
use crate::exports::{ GLOBAL_DATA };
use std::os::raw::c_void;
use std::sync::{Arc};
use futures::task::Context;
use tokio::macros::support::Poll;
use crate::channel::{ClientAudioSource, AudioSourceEvent};
use parking_lot::RwLock;
use tokio::sync::mpsc;
use futures::StreamExt;

pub struct NativeClientAudioSender {
    pub own_client_id: u32,
    pub own_client_data: ClientData,

    pub mode: u8,
    pub source_client_data: ClientData,

    pub last_send_codec: u8,
    pub stop_packet_count: u16,
    pub last_sequence_id: u16
}

impl ClientAudioSender for NativeClientAudioSender {
    fn owner_id(&self) -> u32 {
        self.own_client_id
    }

    fn send_start(&mut self) { }

    fn send(&mut self, data: &[u8], seq_no: u16, _marked: bool, _timestamp: u32, codec: AudioCodec, _level: Option<u8>) {
        self.last_send_codec = codec.into();
        self.last_sequence_id = seq_no;

        /*
         * The seq_no is continuous, starting anywhere, but it's not a problem.
         * We require all native clients to handle resetting seq ids from the same client id.
         * We only need to add the stop packets, since they affect the sequence.
         */

        (GLOBAL_DATA.callbacks().client_audio_sender_data)(
            self.own_client_data.as_ptr(),
            self.source_client_data.as_ptr(),
            self.mode,
            self.last_sequence_id.wrapping_add(self.stop_packet_count),
            self.last_send_codec,
            data.as_ptr() as *const c_void,
            data.len() as u32
        );
    }

    fn send_stop(&mut self, _reason: AudioStopReason) {
        self.stop_packet_count = self.stop_packet_count.wrapping_add(1);

        (GLOBAL_DATA.callbacks().client_audio_sender_data)(
            self.own_client_data.as_ptr(),
            self.source_client_data.as_ptr(),
            self.mode,
            self.last_sequence_id.wrapping_add(self.stop_packet_count),
            self.last_send_codec,
            std::ptr::null(),
            0
        );
    }

    fn poll_event(&mut self, _cx: &mut Context) -> Poll<Option<()>> {
        Poll::Pending
    }
}

pub struct NativeClientAudioSource {
    pub stream_id: u32,
    pub receiver: mpsc::UnboundedReceiver<AudioSourceEvent>
}

impl ClientAudioSource for NativeClientAudioSource {
    fn stream_id(&self) -> u32 {
        self.stream_id
    }

    fn poll_next(&mut self, cx: &mut Context) -> Poll<Option<AudioSourceEvent>> {
        self.receiver.poll_next_unpin(cx)
    }
}

pub struct NativeAudioSourceSupplier {
    pub sender: Arc<RwLock<mpsc::UnboundedSender<AudioSourceEvent>>>,
    pub stop_packet_count: u16
}

impl NativeAudioSourceSupplier {
    pub fn send(&mut self, buffer: &[u8], seq_no: u16, marked: bool, timestamp: u32, codec: AudioCodec) {
        /*
         * We don't need to map the seq_no to any virtual seq no since.
         * We simply assume that the seq_no is continuous over the whole session the client sends audio.
         * The only mapping we need is to subtract all stop sequence packet ids.
         * Our system do not count them into the voice sequence.
         */

        let _ = self.sender.read().send(AudioSourceEvent::Packet {
            codec,
            marked,
            timestamp,
            sequence: seq_no.wrapping_sub(self.stop_packet_count),
            payload: buffer.to_vec(),
            level: None
        });
    }

    pub fn send_stop(&mut self) {
        self.stop_packet_count = self.stop_packet_count.wrapping_add(1);
        let _ = self.sender.read().send(AudioSourceEvent::EndSignal);
    }
}

pub struct NativeConnection {
    audio_events_sender: Arc<RwLock<mpsc::UnboundedSender<AudioSourceEvent>>>,
    audio_whisper_events_sender: Arc<RwLock<mpsc::UnboundedSender<AudioSourceEvent>>>,
}

impl NativeConnection {
    pub fn new() -> Box<Self> {
        let (audio_sender, _) = mpsc::unbounded_channel();
        let (audio_whisper_sender, _) = mpsc::unbounded_channel();

        Box::new(NativeConnection{
            audio_events_sender: Arc::new(RwLock::new(audio_sender)),
            audio_whisper_events_sender: Arc::new(RwLock::new(audio_whisper_sender)),
        })
    }

    pub fn create_audio_sender(&mut self, own_client_id: u32, own_client_data: ClientData, sender_type: MediaType, _source_client_id: u32, source_client_data: &ClientData) -> Box<dyn ClientAudioSender> {
        let mut sender = NativeClientAudioSender{
            own_client_id,
            own_client_data,

            source_client_data: source_client_data.clone(),

            mode: match sender_type {
                MediaType::Audio => 0,
                MediaType::AudioWhisper => 1,
                _ => panic!()
            },
            last_send_codec: 4,
            last_sequence_id: 0,
            stop_packet_count: 0
        };

        /*
         * Sending a second stop signal, just to ensure that the client resets his locally tracked sequence.
         */
        sender.send_stop(AudioStopReason::InternalEnding);
        Box::new(sender)
    }

    pub fn create_audio_source_supplier(&self, stream_id: u32) -> NativeAudioSourceSupplier {
        NativeAudioSourceSupplier {
            sender: match stream_id {
                1 => self.audio_events_sender.clone(),
                2 => self.audio_whisper_events_sender.clone(),
                _ => self.audio_events_sender.clone()
            },
            stop_packet_count: 0
        }
    }

    pub fn create_audio_source(&mut self, stream_id: u32) -> Option<Box<dyn ClientAudioSource>> {
        match stream_id {
            1 => {
                let (sender, receiver) = mpsc::unbounded_channel();
                *self.audio_events_sender.write() = sender;

                Some(Box::new(NativeClientAudioSource{ receiver, stream_id }))
            },
            2 => {
                let (sender, receiver) = mpsc::unbounded_channel();
                *self.audio_whisper_events_sender.write() = sender;

                Some(Box::new(NativeClientAudioSource{ receiver, stream_id }))
            },
            _ => None
        }
    }
}