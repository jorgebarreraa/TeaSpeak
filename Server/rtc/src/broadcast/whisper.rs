use crate::broadcast::{ClientAudioBroadcaster, ClientBroadcaster, AudioBroadcastMode, execute_broadcast};
use std::sync::{Mutex, Arc};
use crate::client::{Client};
use std::future::Future;
use futures::task::{Context, Poll};
use tokio::macros::support::Pin;
use std::time::{Instant};
use tokio::time::{Interval, Duration};
use futures::StreamExt;
use unchecked_unwrap::UncheckedUnwrap;
use std::sync::atomic::{AtomicU32, Ordering};

static WHISPER_SESSION_ID_INDEX: AtomicU32 = AtomicU32::new(1);

pub struct WhisperSession {
    session_id: u32,
    stream_id: u32,

    broadcast: Arc<Mutex<ClientAudioBroadcaster>>,
    targets: Vec<u32>,

    session_tick: Option<Interval>
}

impl WhisperSession {
    pub fn new(client: &mut Client, stream_id: u32) -> Result<Self, String> {
        let broadcast = execute_broadcast(
            client,
            move |client| {
                if let Some(source) = client.create_audio_source(stream_id) {
                    Some(ClientAudioBroadcaster::new(AudioBroadcastMode::Whisper, source, client.client_id(), client.client_data().clone()))
                } else {
                    None
                }
            },
            |_client_id| {
                /* FIXME: Reset whisper session */
                //channel.shutdown_broadcast_audio(client_id);
            }
        ).map_err(|err| format!("{:?}", err))?;

        let mut session_id;
        loop {
            session_id = WHISPER_SESSION_ID_INDEX.fetch_add(1, Ordering::Relaxed);
            if session_id != 0 { break; }
        }

        Ok(WhisperSession {
            session_id,

            stream_id,
            broadcast,

            targets: Vec::with_capacity(32),
            session_tick: None
        })
    }

    pub fn session_id(&self) -> u32 { self.session_id }

    pub fn stream_id(&self) -> u32 {
        self.stream_id
    }

    /// Returns a list of not containing clients which should be added to the session.
    /// Every client id which is not contained in the new list will be removed from the session.
    pub fn update_target_clients(&mut self, client_ids: &[u32]) -> Vec<u32> {
        let dropped_clients = self.targets.drain_filter(|client_id| {
            !client_ids.contains(client_id)
        }).collect::<Vec<_>>();

        {
            let mut broadcast = self.broadcast.lock().unwrap();
            for client_id in dropped_clients {
                broadcast.remove_client(client_id);
            }
        }

        client_ids.iter().filter(|client_id| !self.targets.contains(*client_id)).map(|e| *e).collect::<Vec<_>>()
    }

    /// Register a new listener for the whisper session
    pub fn register_client(&mut self, client: &mut Client) {
        let mut broadcast = self.broadcast.lock().unwrap();
        broadcast.register_client(client);
        self.targets.push(client.client_id());
    }
}

impl Future for WhisperSession {
    type Output = ();

    fn poll(mut self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Self::Output> {
        if self.session_tick.is_none() {
            self.session_tick = Some(tokio::time::interval(Duration::from_millis(500)));
        }

        while let Poll::Ready(_) = unsafe { self.session_tick.as_mut().unchecked_unwrap() }.poll_next_unpin(cx) {
            let mut broadcast = self.broadcast.lock().unwrap();
            broadcast.test_timeout(&Instant::now());

            if broadcast.last_activity().elapsed().as_secs() > 10 {
                /* Whisper session finished. We don't expect any data */
                return Poll::Ready(());
            }
        }

        Poll::Pending
    }
}