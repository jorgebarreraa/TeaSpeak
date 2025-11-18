use std::sync::Arc;
use crate::request_handler::FileTransferInfo;
use futures::task::{Context, Poll, Waker};
use crate::files::DownloadTarget;
use crate::request::NetworkTransferDownloadSink;
use std::io::{BufRead};
use vmap::io::{ SeqRead, SeqWrite };
use std::convert::TryFrom;
use tokio::macros::support::{Pin};
use std::ops::{DerefMut, Deref};
use tokio::time::{Interval, Duration};
use tokio::io::ErrorKind;
use futures::{Future, FutureExt};
use slog::*;

#[derive(Copy, Clone, Debug)]
pub enum FileTransferResult {
    Success,
    Aborted,

    UnexpectedClientDisconnect,
    NetworkIoError,
    DiskIoError,

    NetworkTimeout,

    FileDeleted
}

pub struct TransferStatistics {
    /* TODO! */
}

pub trait RunningFileTransfer : Send {
    fn transfer_info(&self) -> &Arc<FileTransferInfo>;
    fn transferred_bytes(&self) -> u64;

    /* TODO: Statistics */
    fn abort(&mut self, reason: &str);

    /// Returns (transfer_status, finish_future)
    fn poll_process(&mut self, cx: &mut Context<'_>) -> Poll<FileTransferResult>;
}

struct RingBuffer(vmap::io::Ring);

impl Deref for RingBuffer {
    type Target = vmap::io::Ring;

    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

impl DerefMut for RingBuffer {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.0
    }
}

unsafe impl Send for RingBuffer {}

struct TimeoutWatcher {
    alive_checker: Option<Interval>,
    timeout_ticks: u16,
    inactive_ticks: u16
}

impl TimeoutWatcher {
    pub fn new(seconds: u16) -> Self {
        TimeoutWatcher{
            alive_checker: None,
            timeout_ticks: seconds,
            inactive_ticks: 0
        }
    }

    pub fn reset_timeout(&mut self) {
        self.inactive_ticks = 0;
    }
}

impl Future for TimeoutWatcher {
    type Output = ();

    fn poll(mut self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Self::Output> {
        if let Some(checker) = &mut self.alive_checker {
            let poll_result = checker.poll_tick(cx);
            while let Poll::Ready(_) = poll_result {
                if self.inactive_ticks > self.timeout_ticks {
                    return Poll::Ready(());
                }

                self.inactive_ticks = self.inactive_ticks.wrapping_add(1);
            }
        } else {
            let mut interval = tokio::time::interval(Duration::from_secs(1));
            while let Poll::Ready(_) = interval.poll_tick(cx) {}
            self.alive_checker = Some(interval);
        }

        Poll::Pending
    }
}

enum DownloadTransferState {
    Transferring,
    Flushing,
    Finished
}

pub struct RunningDownloadFileTransfer {
    logger: slog::Logger,
    transfer_info: Arc<FileTransferInfo>,

    source: Box<dyn DownloadTarget>,
    target: Box<dyn NetworkTransferDownloadSink>,

    buffer: RingBuffer,
    read_bytes_left: u64,

    write_byte_count: u64,

    transfer_result: Option<FileTransferResult>,
    waker: Option<Waker>,

    timeout: TimeoutWatcher
}

impl RunningDownloadFileTransfer {
    pub fn new(logger: slog::Logger, transfer_info: Arc<FileTransferInfo>, source: Box<dyn DownloadTarget>, target: Box<dyn NetworkTransferDownloadSink>) -> Self {
        let read_bytes_left = transfer_info.file_size;
        RunningDownloadFileTransfer{
            logger,
            transfer_info,

            source,
            target,

            buffer: RingBuffer(vmap::io::Ring::new(8 * 1024).expect("failed to create ring buffer")),
            read_bytes_left,

            write_byte_count: 0,

            transfer_result: None,
            waker: None,

            timeout: TimeoutWatcher::new(10)
        }
    }

    fn set_transfer_result(&mut self, result: FileTransferResult) -> Poll<FileTransferResult> {
        self.transfer_result = Some(result);
        return Poll::Ready(result);
    }

    fn handle_write_error(&mut self, error: std::io::Error) -> Poll<FileTransferResult> {
        return match error.kind() {
            ErrorKind::ConnectionReset |
            ErrorKind::UnexpectedEof |
            ErrorKind::BrokenPipe => {
                self.set_transfer_result(FileTransferResult::UnexpectedClientDisconnect)
            },
            _ => {
                warn!(self.logger, "Encountered a network error. Closing remote connection and aborting transfer: {}", error);
                self.set_transfer_result(FileTransferResult::NetworkIoError)
            }
        }
    }
}

impl RunningFileTransfer for RunningDownloadFileTransfer {
    fn transfer_info(&self) -> &Arc<FileTransferInfo> {
        &self.transfer_info
    }

    fn transferred_bytes(&self) -> u64 {
        self.write_byte_count
    }

    fn abort(&mut self, _reason: &str) {
        if self.transfer_result.is_some() {
            /* Transfer isn't running any more */
            return;
        }

        /* Nothing to clean up. */
        /* TODO: Is this needed? self.target.close_connection(Some(reason)); */
        self.transfer_result = Some(FileTransferResult::Aborted);
    }

    fn poll_process(&mut self, cx: &mut Context<'_>) -> Poll<FileTransferResult> {
        if let Some(result) = &self.transfer_result {
            return Poll::Ready(result.clone());
        }

        if let Poll::Ready(_) = self.timeout.poll_unpin(cx) {
            return self.set_transfer_result(FileTransferResult::NetworkTimeout);
        }

        let mut read_bytes_left = usize::try_from(self.read_bytes_left).unwrap_or(usize::max_value());
        let mut some_would_block = false;

        loop {
            let free_buffer = self.buffer.as_write_slice(read_bytes_left);
            if free_buffer.len() > 0 {
                let source = Pin::new(self.source.deref_mut());
                match source.poll_read(cx, free_buffer) {
                    Poll::Ready(Err(error)) => {
                        return match error.kind() {
                            ErrorKind::NotFound => {
                                /* Special code for streaming upload targets */
                                self.set_transfer_result(FileTransferResult::FileDeleted)
                            },
                            _ => {
                                warn!(self.logger, "Failed to read from source. Aborting transfer: {}", error);
                                self.set_transfer_result(FileTransferResult::DiskIoError)
                            }
                        }
                    },
                    Poll::Ready(Ok(length)) => {
                        self.buffer.feed(length);
                        self.read_bytes_left -= length as u64;
                        read_bytes_left -= length;
                    },
                    Poll::Pending => {
                        some_would_block = true;
                        break;
                    }
                }
            } else {
                break;
            }
        }

        'write: loop {
            let full_buffer = self.buffer.as_read_slice(usize::max_value());

            if full_buffer.is_empty() {
                if self.read_bytes_left == 0 {
                    /* TODO: Use a separate timeout here? */
                    /* Transfer finished. Flushing the connection. */
                    match Pin::new(self.target.deref_mut()).poll_flush(cx) {
                        Poll::Pending => {
                            some_would_block = true;
                            break 'write;
                        },
                        Poll::Ready(Ok(_)) => {
                            return Poll::Ready(FileTransferResult::Success);
                        },
                        Poll::Ready(Err(error)) => {
                            return self.handle_write_error(error);
                        }
                    }
                } else {
                    /* Buffer empty. Awaiting new data. */
                }
                break;
            }

            match Pin::new(self.target.deref_mut()).poll_write(cx, full_buffer) {
                Poll::Pending => {
                    some_would_block = true;
                    break;
                },
                Poll::Ready(Err(error)) => {
                    return self.handle_write_error(error);
                },
                Poll::Ready(Ok(length)) => {
                    self.buffer.consume(length);
                    self.write_byte_count += length as u64;
                    self.timeout.reset_timeout();
                }
            }
        }

        if !some_would_block {
            /* schedule us again */
            cx.waker().wake_by_ref();
        } else {
            self.waker = Some(cx.waker().clone());
        }

        Poll::Pending
    }
}