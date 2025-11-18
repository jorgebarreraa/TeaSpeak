use std::task::{Context, Poll};
use crate::client::{Client};
use std::sync::{Mutex, Arc};
use crate::threads::execute_task;

mod audio;
pub use audio::*;

mod video;
pub use video::*;

mod whisper;
pub use whisper::*;

use std::time::Instant;

#[derive(Debug)]
pub enum BroadcastStartResult {
    Succeeded,
    InvalidClient,
    ClientHasNoSource,
    ConfigError(VideoBroadcastConfigureError)
}

pub trait ClientBroadcaster {
    fn source_client_id(&self) -> u32;

    fn client_ids(&self) -> Vec<u32>;
    fn contains_client(&self, client_id: u32) -> bool;

    /// Returns `true` when ended
    fn poll_source(&mut self, cx: &mut Context) -> bool;

    fn test_timeout(&mut self, now: &Instant);

    fn register_client(&mut self, client: &mut Client);
    fn remove_client(&mut self, client_id: u32) -> bool;
}

pub fn execute_broadcast<T: 'static, CallbackCreate, CallbackShutdown: 'static>(
    client: &mut Client,
    create_callback: CallbackCreate,
    callback_shutdown: CallbackShutdown) -> Result<Arc<Mutex<T>>, BroadcastStartResult>
    where
        T: ClientBroadcaster + Send + Sync,
        CallbackCreate: Fn(&mut Client) -> Option<Arc<Mutex<T>>>,
        CallbackShutdown: Fn(/* client id */ u32) -> () + Sync + Send
{
    let broadcast = create_callback(client);

    if !broadcast.is_some() {
        return Err(BroadcastStartResult::ClientHasNoSource);
    }

    let broadcast = broadcast.unwrap();

    /* the broadcast executor */
    let broadcast_ref = Arc::<Mutex<T>>::downgrade(&broadcast);
    let client_id = client.client_id();
    execute_task(tokio::future::poll_fn(move |cx| {
        if let Some(broadcast) = broadcast_ref.upgrade() {
            let mut broadcast = broadcast.lock().unwrap();

            if broadcast.poll_source(cx) {
                drop(broadcast);
                callback_shutdown(client_id);
                return Poll::Ready(())
            }

            Poll::Pending
        } else {
            Poll::Ready(())
        }
    }));

    Ok(broadcast)
}