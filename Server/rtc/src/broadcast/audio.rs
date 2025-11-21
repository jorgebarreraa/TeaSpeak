use std::sync::{Mutex, Weak, Arc};
use crate::channel::{ClientAudioSource, AudioSourceEvent};
use std::task::{Waker, Context, Poll};
use crate::client::{ClientAudioSender, ClientData, AudioStopReason, Client};
use crate::broadcast::ClientBroadcaster;
use unchecked_unwrap::UncheckedUnwrap;
use std::time::{Instant};
use crate::MediaType;
use crate::threads::execute_task;

#[derive(Ord, PartialOrd, Eq, PartialEq, Debug, Copy, Clone)]
pub enum AudioBroadcastMode {
    Channel,
    Whisper
}

pub struct ClientAudioBroadcaster {
    mode: AudioBroadcastMode,
    ref_self: Weak<Mutex<ClientAudioBroadcaster>>,

    last_activity: Instant,

    source: Box<dyn ClientAudioSource>,
    last_send_packet: Option<Instant>,

    /* TODO: DTS */
    sender: Vec<Box<dyn ClientAudioSender>>,
    sender_waker: Option<Waker>,

    source_client_id: u32,
    source_client_data: ClientData,
}

impl ClientAudioBroadcaster {
    pub fn new(mode: AudioBroadcastMode, source: Box<dyn ClientAudioSource>, client_id: u32, client_data: ClientData) -> Arc<Mutex<Self>> {
        let broadcaster = Arc::new(Mutex::new(ClientAudioBroadcaster {
            mode,
            source,
            source_client_id: client_id,
            source_client_data: client_data,

            sender: Vec::new(),
            last_send_packet: None,
            last_activity: Instant::now(),
            sender_waker: None,
            ref_self: Weak::new()
        }));

        broadcaster.lock().unwrap().ref_self = Arc::downgrade(&broadcaster);

        execute_task({
            let broadcast = Arc::downgrade(&broadcaster);
            futures::future::poll_fn(move |cx| {
                let broadcast = match broadcast.upgrade() {
                    Some(broadcast) => broadcast,
                    None => return Poll::Ready(())
                };

                let mut broadcast = broadcast.lock().unwrap();
                broadcast.poll_senders(cx);
                Poll::Pending
            })
        });

        broadcaster
    }

    pub fn last_activity(&self) -> &Instant {
        &self.last_activity
    }

    fn stop_stream(&mut self, reason: AudioStopReason) {
        if self.last_send_packet.is_some() {
            self.last_send_packet = None;

            for sender in self.sender.iter_mut() {
                sender.send_stop(reason);
            }
        }
    }

    fn poll_senders(&mut self, cx: &mut Context) {
        self.sender.retain_mut(|sender| {
            while let Poll::Ready(event) = sender.poll_event(cx) {
                if event.is_none() {
                    return false; // Remover este sender
                }
            }

            true // Mantener este sender
        });

        self.sender_waker = Some(cx.waker().clone());
    }
}

impl ClientBroadcaster for ClientAudioBroadcaster {
    fn source_client_id(&self) -> u32 {
        self.source_client_id
    }

    fn client_ids(&self) -> Vec<u32> {
        self.sender.iter().map(|e| e.owner_id()).collect()
    }

    fn contains_client(&self, client_id: u32) -> bool {
        self.sender.iter().find(|e| e.owner_id() == client_id).is_some()
    }

    fn poll_source(&mut self, cx: &mut Context) -> bool {
        while let Poll::Ready(event) = self.source.poll_next(cx) {
            if event.is_none() {
                //println!("Audio broadcast has been ended for {}", self.source_client_id);
                /* stream has been ended */
                return true;
            }

            match unsafe { event.unchecked_unwrap() } {
                AudioSourceEvent::EndSignal => {
                    self.stop_stream(AudioStopReason::User);
                    self.last_activity = Instant::now();
                },
                AudioSourceEvent::Packet {timestamp, payload, sequence, marked, codec, level} => {
                    if self.last_send_packet.is_none() {
                        self.sender.iter_mut().for_each(|sender| sender.send_start());
                    }

                    self.last_activity = Instant::now();
                    self.last_send_packet = Some(self.last_activity.clone());

                    for sender in self.sender.iter_mut() {
                        sender.send(&*payload, sequence, marked, timestamp, codec, level);
                    }
                }
            }
        }
        false
    }

    fn test_timeout(&mut self, now: &Instant) {
        if let Some(last_received_packet) = &self.last_send_packet {
            /* The now timestamp might be before the last packet we received */
            if now.checked_duration_since(*last_received_packet).map_or(0, |d| d.as_millis()) > 500 {
                self.stop_stream(AudioStopReason::Timeout);
            }
        }

    }

    fn register_client(&mut self, client: &mut Client) {
        if self.contains_client(client.client_id()) { return; }

        if let Some(mut sender) = client.create_audio_sender(
            match self.mode {
                AudioBroadcastMode::Channel => MediaType::Audio,
                AudioBroadcastMode::Whisper => MediaType::AudioWhisper
            },
            self.source_client_id,
            &self.source_client_data
        ) {
            if self.last_send_packet.is_some() {
                sender.send_start();
            }
            self.sender.push(sender);

            if let Some(waker) = &self.sender_waker {
                waker.wake_by_ref();
            }
        }
    }

    fn remove_client(&mut self, client_id: u32) -> bool {
        let initial_len = self.sender.len();
        self.sender.retain(|e| e.owner_id() != client_id);
        self.sender.len() < initial_len
    }
}