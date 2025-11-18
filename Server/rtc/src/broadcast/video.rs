use std::sync::{Arc, Mutex, Weak};
use std::task::{Context, Poll, Waker};
use std::time::{Instant, Duration};
use tokio::sync::mpsc::{ self, UnboundedReceiver, UnboundedSender };

use futures::{FutureExt, StreamExt};
use unchecked_unwrap::UncheckedUnwrap;

use crate::broadcast::ClientBroadcaster;
use crate::channel::{ClientVideoSource, VideoSourceEvent};
use crate::client::{Client, ClientData, ClientVideoSender, ClientVideoSenderEvent, VideoStopReason};
use crate::MediaType;
use crate::threads::{enter_tasks, execute_task};
use crate::exports::GLOBAL_DATA;

#[derive(Ord, PartialOrd, Eq, PartialEq, Debug, Copy, Clone)]
pub enum VideoBroadcastMode {
    Camera,
    Screen
}

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct VideoBroadcastOptions {
    /// A mask which fields should be updated (Only set when changing anything)
    /// 0x01: Update the broadcast bitrate
    /// 0x02: Update the keyframe interval
    pub update_mask: u16,

    /// A value of zero means unlimited
    pub bitrate: u32,

    /// Interval of force requesting a keyframe in ms.
    /// A value of zero means that no keyframe gets requested.
    pub keyframe_interval: u32,
}

impl VideoBroadcastOptions {
    pub fn bitrate(&self) -> Option<u32> {
        if (self.update_mask & 0x01) > 0 {
            Some(self.bitrate)
        } else {
            None
        }
    }

    pub fn keyframe_interval(&self) -> Option<u32> {
        if (self.update_mask & 0x02) > 0 {
            Some(self.keyframe_interval)
        } else {
            None
        }
    }
}

struct FpsCounter {
    time_base: Instant,
    index: usize,
    counter: [u8; 32]
}

impl FpsCounter {
    pub fn new() -> Self {
        FpsCounter {
            time_base: Instant::now(),
            index: 0,
            counter: [0; 32]
        }
    }

    pub fn reset(&mut self) {}

    pub fn log_frame(&mut self) {
        let index = Instant::now().duration_since(self.time_base)
            .as_secs() & 0x1F;
        let index = index as usize;

        while index != self.index {
            self.index += 1;
            self.index &= 0x1F;

            self.counter[self.index] = 0;
        }

        /* just in case */
        self.counter[self.index] = self.counter[self.index].wrapping_add(1);
    }

    pub fn fps(&self) -> u8 {
        let index = Instant::now().duration_since(self.time_base)
            .as_secs() & 0x1F;

        if index == 0 {
            self.counter.as_slice()[(self.counter.len() - 1) as usize]
        } else {
            self.counter.as_slice()[(index - 1) as usize]
        }
    }

    pub fn average_fps(&self) -> f32 {
        let mut count = 0u32;
        let mut sum = 0u32;
        self.counter.iter().for_each(|fps| {
            if *fps > 0 {
                count += 1;
                sum += *fps as u32;
            }
        });

        if count == 0 {
            0f32
        } else {
            sum as f32 / count as f32
        }
    }
}

pub struct ClientVideoBroadcaster {
    logger: slog::Logger,

    media_type: MediaType,
    ref_self: Weak<Mutex<ClientVideoBroadcaster>>,

    source: Box<dyn ClientVideoSource>,
    source_waker: Option<Waker>,

    last_received_packet: Option<Instant>,
    last_received_keyframe: Option<Instant>,

    auto_keyframe_interval: Duration,

    sender: Vec<Box<dyn ClientVideoSender>>,
    sender_waker: Option<Waker>,

    pli_request_pending: bool,
    pli_request_timestamp: Instant,
    pli_request_timer: Option<tokio::time::Delay>,

    source_client_id: u32,
    source_client_data: ClientData,

    fps: FpsCounter,
    fps_last_message: Instant
}

#[derive(Debug, Copy, Clone)]
pub enum VideoBroadcastConfigureError {
    /* TODO? */
}

impl ClientVideoBroadcaster {
    pub fn new(logger: slog::Logger, media_type: MediaType, source: Box<dyn ClientVideoSource>, client_id: u32, client_data: ClientData) -> Arc<Mutex<Self>> {
        let broadcaster = Arc::new(Mutex::new(ClientVideoBroadcaster {
            logger,

            media_type,
            ref_self: Weak::new(),

            sender: Vec::new(),

            source,
            source_client_id: client_id,
            source_client_data: client_data,

            pli_request_timestamp: Instant::now(),
            pli_request_timer: None,
            pli_request_pending: false,

            last_received_packet: None,
            last_received_keyframe: None,

            auto_keyframe_interval: Duration::from_secs(7),

            source_waker: None,
            sender_waker: None,

            fps: FpsCounter::new(),
            fps_last_message: Instant::now()
        }));

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

        broadcaster.lock().unwrap().ref_self = Arc::downgrade(&broadcaster);
        broadcaster
    }

    pub fn configure(&mut self, options: &VideoBroadcastOptions) -> Result<(), VideoBroadcastConfigureError> {
        if let Some(bitrate) = options.bitrate() {
            self.source.set_bitrate(if bitrate == 0 { None } else { Some(bitrate) })
        }

        if let Some(keyframe_interval) = options.keyframe_interval() {
            self.auto_keyframe_interval = Duration::from_millis(keyframe_interval as u64);
        }

        Ok(())
    }

    pub fn config(&self) -> VideoBroadcastOptions {
        VideoBroadcastOptions {
            update_mask: 0,

            keyframe_interval: self.auto_keyframe_interval.as_millis() as u32,
            bitrate: self.source.bitrate().unwrap_or(0),
        }
    }
}

const PLI_MIN_REQUEST_INTERVAL: Duration = Duration::from_millis(500);
impl ClientVideoBroadcaster {
    fn request_pli(&mut self, force: bool) {
        let elapsed = self.pli_request_timestamp.elapsed();
        if force || elapsed > PLI_MIN_REQUEST_INTERVAL {
            self.pli_request_timestamp = Instant::now();
            self.pli_request_pending = false;
            self.pli_request_timer = None;
            self.source.request_pli();

            slog::slog_trace!(self.logger, "Sending PLI request");
        } else if self.pli_request_timer.is_none() {
            let remaining_time = PLI_MIN_REQUEST_INTERVAL.checked_sub(elapsed).unwrap_or(Duration::from_secs(0));
            slog::slog_trace!(self.logger, "Scheduling a PLI request in {}ms", remaining_time.as_millis());

            self.pli_request_timer = Some(tokio::time::delay_until((self.pli_request_timestamp + PLI_MIN_REQUEST_INTERVAL).into()));
            if let Some(waker) = &self.source_waker {
                waker.wake_by_ref();
            }
        }
    }

    fn stop_stream(&mut self, reason: VideoStopReason) {
        if self.last_received_packet.is_some() {
            self.last_received_packet = None;
            self.pli_request_pending = false;

            for sender in self.sender.iter_mut() {
                sender.send_stop(reason);
            }

            self.fps.reset();
        } else {
            /* We never send a packet to the target. We don't have to stop the stream */
        }

        self.last_received_keyframe = None;
    }

    fn poll_senders(&mut self, cx: &mut Context) {
        let mut request_pli = false;

        let logger = self.logger.clone();
        let sender = self.sender.drain_filter(|sender| {
            while let Poll::Ready(event) = sender.poll_event(cx) {
                if event.is_none() {
                    return true;
                }

                match unsafe { event.unchecked_unwrap() } {
                    ClientVideoSenderEvent::RequestPLI => {
                        request_pli = true;
                        slog::slog_trace!(logger, "Receiver {} send a PLI request", sender.owner_id());
                    }
                }
            }

            false
        }).collect::<Vec<_>>();

        for sender in sender {
            self.source.notify_client_leave(sender.owner_id(), sender.owner_data());
        }

        if request_pli {
            self.request_pli(false);
        }

        self.sender_waker = Some(cx.waker().clone());
    }
}

impl ClientBroadcaster for ClientVideoBroadcaster {
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
            match event {
                VideoSourceEvent::Ended => {
                    /* stream has been ended */
                    slog::slog_debug!(self.logger, "Video broadcast has ended");
                    return true;
                },
                VideoSourceEvent::SequenceEndSignal => {
                    self.stop_stream(VideoStopReason::User);
                },
                VideoSourceEvent::Packet {timestamp, payload, marked, sequence, codec, key_frame} => {
                    if self.last_received_packet.is_none() {
                        self.sender.iter_mut().for_each(|sender| sender.send_start());
                    }

                    if marked {
                        self.fps.log_frame();
                        let now = Instant::now();

                        if now.duration_since(self.fps_last_message).as_secs() >= 5 {
                            self.fps_last_message = now;
                            slog::slog_trace!(self.logger, "FPS: Current: {} Average: {}", self.fps.fps(), self.fps.average_fps());
                        }
                    }

                    if key_frame {
                        slog::slog_trace!(self.logger, "Received a KeyFrame");
                        self.last_received_keyframe = Some(Instant::now());
                    } else if let Some(last_key_frame) = &self.last_received_keyframe {
                        if !self.auto_keyframe_interval.is_zero() && last_key_frame.elapsed() >= self.auto_keyframe_interval {
                            slog::slog_trace!(self.logger, "Requesting a keyframe (Auto interval {}s)", self.auto_keyframe_interval.as_secs());

                            self.request_pli(false);
                            self.last_received_keyframe = Some(Instant::now());
                        }
                    }

                    self.last_received_packet = Some(Instant::now());

                    for sender in self.sender.iter_mut() {
                        sender.send(&*payload, codec, sequence, marked, timestamp);
                    }
                }
            }
        }

        if let Some(pli_timer) = &mut self.pli_request_timer {
            if let Poll::Ready(_) = pli_timer.poll_unpin(cx) {
                self.pli_request_timer = None;
                self.request_pli(true);
            }
        }

        self.source_waker = Some(cx.waker().clone());
        false
    }

    fn test_timeout(&mut self, now: &Instant) {
        if let Some(last_received_packet) = &self.last_received_packet {
            if now.checked_duration_since(*last_received_packet).map_or(0, |d| d.as_millis()) > 5000 {
                self.stop_stream(VideoStopReason::Timeout);
            }
        }
    }

    fn register_client(&mut self, client: &mut Client) {
        if self.contains_client(client.client_id()) { return; }
        if let Some(mut sender) = client.create_video_sender(self.media_type, self.source_client_id, &self.source_client_data) {
            if self.last_received_packet.is_some() {
                sender.send_start();
            }

            self.source.notify_client_join(sender.owner_id(), sender.owner_data());
            self.sender.push(sender);
            self.request_pli(true);

            if let Some(waker) = &self.sender_waker {
                waker.wake_by_ref();
            }
        }
    }

    fn remove_client(&mut self, client_id: u32) -> bool {
        let removed_clients = self.sender.drain_filter(|e| e.owner_id() == client_id).collect::<Vec<_>>();
        if removed_clients.is_empty() {
            return false;
        }

        /* removed_clients should only be 1 */
        for client in removed_clients {
            self.source.notify_client_leave(client.owner_id(), client.owner_data());
        }
        return true;
    }
}