use std::sync::{Arc, Weak, Mutex};
use std::task::{Poll, Context};
use parking_lot::RwLock;
use crate::client::{Client, ClientData};
use std::collections::{BTreeMap};
use crate::{AudioCodec, MediaType, VideoCodec};
use crate::threads::execute_task;
use tokio::time::{Interval, Duration};
use std::time::{Instant};
use futures::{StreamExt};
use std::ops::DerefMut;
use crate::broadcast::{VideoBroadcastMode, BroadcastStartResult, ClientBroadcaster, execute_broadcast, ClientAudioBroadcaster, ClientVideoBroadcaster, AudioBroadcastMode, VideoBroadcastOptions};
use unchecked_unwrap::UncheckedUnwrap;
use std::os::raw::c_void;
use crate::exports::BroadcastInfo;
use crate::exports::{ GLOBAL_DATA };
use slog::o;

pub enum AudioSourceEvent {
    EndSignal,
    Packet{
        timestamp: u32,
        sequence: u16,
        codec: AudioCodec,
        marked: bool,
        payload: Vec<u8>,
        level: Option<u8>
    }
}

pub trait ClientAudioSource: Send + Sync {
    fn stream_id(&self) -> u32;
    fn poll_next(&mut self, cx: &mut Context) -> Poll<Option<AudioSourceEvent>>;
}

pub enum VideoSourceEvent {
    /// Video source has been terminated
    Ended,
    /// Current video sequence has been finished
    SequenceEndSignal,
    /// Video packet
    Packet{
        payload: Vec<u8>,
        timestamp: u32,
        marked: bool,
        key_frame: bool,
        sequence: u16,
        codec: VideoCodec
    }
}

pub trait ClientVideoSource: Send + Sync {
    fn request_pli(&mut self);
    fn set_bitrate(&mut self, bitrate_limit: Option<u32>);
    fn bitrate(&self) -> Option<u32>;

    fn poll_next(&mut self, cx: &mut Context) -> Poll<VideoSourceEvent>;

    fn notify_client_join(&mut self, client_id: u32, client_data: &ClientData);
    fn notify_client_leave(&mut self, client_id: u32, client_data: &ClientData);
}

pub enum VideoBroadcastJoinResult {
    Success,
    InvalidClient,
    InvalidBroadcast
}

struct VideoBroadcast {
    broadcaster: Arc<Mutex<ClientVideoBroadcaster>>,
    source_client_data: ClientData
}

pub struct Channel {
    id: u32,
    ref_self: Weak<RwLock<Channel>>,

    clients: BTreeMap<u32, Arc<Mutex<Client>>>,

    audio_broadcasts: BTreeMap<u32, Arc<Mutex<ClientAudioBroadcaster>>>,
    video_broadcasts: BTreeMap<(VideoBroadcastMode, u32), VideoBroadcast>,

    stream_timeout_poll: Option<Interval>,
    video_broadcast_update_pending: bool
}

impl Channel {
    pub(crate) fn new(id: u32) -> Arc<RwLock<Channel>> {
        let channel = Arc::new(RwLock::new(Channel{
            id,
            ref_self: Weak::new(),

            clients: BTreeMap::new(),

            audio_broadcasts: BTreeMap::new(),
            video_broadcasts: BTreeMap::new(),

            stream_timeout_poll: None,
            video_broadcast_update_pending: false
        }));

        channel.write().ref_self = Arc::downgrade(&channel);

        channel
    }

    pub(crate) fn execute(&self) {
        /* Stream timeouts */
        let ref_self = self.ref_self.clone();
        tokio::spawn(tokio::future::poll_fn(move |cx| {
            if let Some(channel) = ref_self.upgrade() {
                {
                    let mut channel = channel.write();
                    if channel.stream_timeout_poll.is_none() {
                        channel.stream_timeout_poll = Some(tokio::time::interval(Duration::from_millis(500)));
                    }

                    let poll = channel.stream_timeout_poll.as_mut().unwrap();
                    if let Poll::Pending = poll.poll_next_unpin(cx) {
                        return Poll::Pending;
                    }

                    while let Poll::Ready(_) = poll.poll_next_unpin(cx) {
                        /* consume all events */
                    }
                }

                let now = Instant::now();
                for broadcast in { let values = channel.read().audio_broadcasts.values().map(|e| e.clone()).collect::<Vec<_>>(); values } {
                    let mut broadcast = broadcast.lock().unwrap();
                    broadcast.test_timeout(&now);
                }

                /* require only all 7500ms something */
                for broadcast in { let values = channel.read().video_broadcasts.values().map(|e| e.broadcaster.clone()).collect::<Vec<_>>(); values } {
                    let mut broadcast = broadcast.lock().unwrap();
                    broadcast.test_timeout(&now);
                }
                Poll::Pending
            } else {
                Poll::Ready(())
            }
        }));
    }

    pub(crate) fn register_client(&mut self, client: Arc<Mutex<Client>>, owned_client: &mut Client) {
        self.clients.insert(owned_client.client_id(), client.clone());

        /* we're not ensure that we're in a tokio runtime right now, but this is required when creating new senders */
        let channel = self.ref_self.clone();
        let client = Arc::downgrade(&client);
        execute_task(async move {
            if let Some(channel) = channel.upgrade() {
                let channel = channel.read();

                if let Some(client) = client.upgrade() {
                    for broadcast in channel.audio_broadcasts.values() {
                        let mut broadcast = broadcast.lock().unwrap();
                        let mut client = client.lock().unwrap();

                        if broadcast.source_client_id() == client.client_id() {
                            continue;
                        }

                        broadcast.register_client(client.deref_mut());
                    }

                    channel.send_video_broadcast_update( client.lock().unwrap().deref_mut());
                }
            }
        });
    }

    pub fn shutdown_broadcast_audio(&mut self, client_id: u32) {
        if let Some(_broadcast) = self.audio_broadcasts.remove(&client_id) {
        }
    }

    pub fn shutdown_broadcast_video(&mut self, client_id: u32, broadcast_type: VideoBroadcastMode) {
        if let Some(_broadcast) = self.video_broadcasts.remove(&(broadcast_type, client_id)) {
            self.broadcast_video_broadcast_update();
        }
    }

    pub fn get_video_broadcast(&self, client_id: u32, broadcast_type: VideoBroadcastMode) -> Option<&Arc<Mutex<ClientVideoBroadcaster>>> {
        self.video_broadcasts.get(&(broadcast_type, client_id))
            .map(|e| &e.broadcaster)
    }

    pub fn is_client_audio_broadcasting(&self, client_id: u32) -> bool {
        self.audio_broadcasts.contains_key(&client_id)
    }

    pub(crate) fn broadcast_client_audio(&mut self, client_id: u32, stream_id: u32) -> BroadcastStartResult {
        let client = self.clients.get(&client_id).cloned();
        if client.is_none() { return BroadcastStartResult::InvalidClient; }

        self.shutdown_broadcast_audio(client_id);

        let client = unsafe { client.unchecked_unwrap() };
        let mut client = client.lock().unwrap();

        let channel = self.ref_self.clone();
        let broadcast = match execute_broadcast(client.deref_mut(), |client| {
            client.create_audio_source(stream_id)
                .map(|source| ClientAudioBroadcaster::new(AudioBroadcastMode::Channel, source, client_id, client.client_data().clone()))
        }, move |client_id| {
            if let Some(channel) = channel.upgrade() {
                channel.write().shutdown_broadcast_audio(client_id);
            }
        }) {
            Ok(broadcast) => broadcast,
            Err(error) => return error
        };

        /* Adding all participants within this channel */
        let channel = self.ref_self.clone();
        let broadcast_ref = Arc::downgrade(&broadcast);
        execute_task(async move {
            if let Some(channel) = channel.upgrade() {
                let channel = channel.read();

                if let Some(broadcast) = broadcast_ref.upgrade() {
                    let mut broadcast = broadcast.lock().unwrap();

                    for (client_id, client) in channel.clients.iter() {
                        if *client_id == broadcast.source_client_id() { continue; }

                        let mut client = client.lock().unwrap();
                        broadcast.register_client(client.deref_mut());
                    }
                }
            }
        });

        self.audio_broadcasts.insert(client_id, broadcast.clone());
        BroadcastStartResult::Succeeded
    }

    pub(crate) fn broadcast_client_video(&mut self, client_id: u32, stream_id: u32, video_type: VideoBroadcastMode, config: &VideoBroadcastOptions) -> BroadcastStartResult {
        let client = self.clients.get(&client_id).cloned();
        if client.is_none() { return BroadcastStartResult::InvalidClient; }

        self.shutdown_broadcast_video(client_id, video_type);

        let media_type = match video_type {
            VideoBroadcastMode::Camera => MediaType::Video,
            VideoBroadcastMode::Screen => MediaType::VideoScreen
        };

        let media_type_str = match video_type {
            VideoBroadcastMode::Camera => "camera",
            VideoBroadcastMode::Screen => "screen"
        };

        let client = unsafe { client.unchecked_unwrap() };
        let mut client = client.lock().unwrap();

        let channel = self.ref_self.clone();
        let broadcast = match execute_broadcast(client.deref_mut(), |client| {
            client.create_video_source(stream_id)
                .map(|source| ClientVideoBroadcaster::new(client.logger().new(o!("video" => media_type_str)), media_type, source, client_id, client.client_data().clone()))
        }, move |client_id| {
            if let Some(channel) = channel.upgrade() {
                channel.write().shutdown_broadcast_audio(client_id);
            }
        }) {
            Ok(broadcast) => broadcast,
            Err(error) => return error
        };

        match broadcast.lock().unwrap().configure(config) {
            Ok(()) => {},
            Err(error) => return BroadcastStartResult::ConfigError(error)
        };

        self.video_broadcasts.insert((video_type, client_id), VideoBroadcast {
            source_client_data: client.client_data().clone(),
            broadcaster: broadcast
        });

        self.broadcast_video_broadcast_update();
        BroadcastStartResult::Succeeded
    }

    /// `update_video_broadcaster_list` is false the video broadcast list will not be updated.
    /// There is no need when a user leaves the channel for example. The client already knows he's
    /// not broadcasting any more.
    pub(crate) fn unregister_client(&mut self, client_id: u32, mut update_video_broadcaster_list: bool) {
        self.clients.remove(&client_id);

        if !update_video_broadcaster_list && !self.video_broadcast_update_pending {
            self.video_broadcast_update_pending = true;
            update_video_broadcaster_list = true;
        } else {
            update_video_broadcaster_list = false;
        }

        self.shutdown_broadcast_audio(client_id);
        self.shutdown_broadcast_video(client_id, VideoBroadcastMode::Camera);
        self.shutdown_broadcast_video(client_id, VideoBroadcastMode::Screen);

        if update_video_broadcaster_list {
            self.video_broadcast_update_pending = false
        }

        for (_, broadcast) in self.audio_broadcasts.iter_mut() {
            let mut broadcast = broadcast.lock().unwrap();
            broadcast.remove_client(client_id);
        }

        for (_, broadcast) in self.video_broadcasts.iter_mut() {
            let mut broadcast = broadcast.broadcaster.lock().unwrap();
            broadcast.remove_client(client_id);
        }
    }

    /* Firstly lock the target broadcast and than the client! */
    pub fn join_video_broadcast(&mut self, client_id: u32, target_client_id: u32, broadcast_type: VideoBroadcastMode) -> VideoBroadcastJoinResult {
        let client = self.clients.get(&client_id).cloned();
        if client.is_none() { return VideoBroadcastJoinResult::InvalidClient; }

        let broadcast = self.video_broadcasts.get(&(broadcast_type, target_client_id));
        if broadcast.is_none() { return VideoBroadcastJoinResult::InvalidBroadcast; }

        let broadcast = unsafe { broadcast.unchecked_unwrap() };
        let mut broadcast = broadcast.broadcaster.lock().unwrap();

        let client = unsafe { client.unchecked_unwrap() };
        let mut client = client.lock().unwrap();
        broadcast.register_client(client.deref_mut());

        VideoBroadcastJoinResult::Success
    }

    pub fn leave_video_broadcast(&mut self, client_id: u32, target_client_id: u32, broadcast_type: VideoBroadcastMode) {
        let broadcast = self.video_broadcasts.get(&(broadcast_type, target_client_id));
        if broadcast.is_none() { return; }

        let broadcast = unsafe { broadcast.unchecked_unwrap() };
        let mut broadcast = broadcast.broadcaster.lock().unwrap();

        broadcast.remove_client(client_id);
    }

    fn video_broadcast_info(&self) -> Vec<BroadcastInfo> {
        let mut broadcasts = Vec::<BroadcastInfo>::with_capacity(self.video_broadcasts.len());
        for ((btype, client_id), broadcast) in self.video_broadcasts.iter() {
            broadcasts.push(BroadcastInfo {
                broadcast_type: match btype {
                    VideoBroadcastMode::Camera => 0,
                    VideoBroadcastMode::Screen => 1
                },
                broadcasting_client_data: broadcast.source_client_data.as_ptr(),
                broadcasting_client_id: *client_id
            });
        }
        broadcasts
    }

    fn broadcast_video_broadcast_update(&mut self) {
        if self.video_broadcast_update_pending {
            return;
        }

        self.video_broadcast_update_pending = true;

        let ref_channel = self.ref_self.clone();
        execute_task(async move {
            if let Some(channel) = ref_channel.upgrade() {
                let mut channel = channel.write();
                channel.video_broadcast_update_pending = false;

                let mut client_datas = Vec::<*const c_void>::with_capacity(channel.clients.len());
                for client in channel.clients.values() {
                    let client = client.lock().unwrap();
                    client_datas.push(client.client_data().as_ptr());
                }

                let broadcasts = channel.video_broadcast_info();
                (GLOBAL_DATA.callbacks().client_video_broadcast_info)(client_datas.as_ptr(), client_datas.len() as u32, broadcasts.as_ptr(), broadcasts.len() as u32);
            }
        });
    }

    fn send_video_broadcast_update(&self, client: &mut Client) {
        if self.video_broadcast_update_pending {
            return;
        }

        let broadcasts = self.video_broadcast_info();
        (GLOBAL_DATA.callbacks().client_video_broadcast_info)(&client.client_data().as_ptr(), 1, broadcasts.as_ptr(), broadcasts.len() as u32);
    }
}