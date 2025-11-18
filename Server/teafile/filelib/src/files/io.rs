use std::path::PathBuf;
use std::collections::VecDeque;
use std::sync::{Arc, RwLock};
use futures::task::{AtomicWaker, Context};
use crate::files::{UploadTarget, DownloadTarget, UploadWriteResult};
use tokio::macros::support::{Pin, Poll};
use tokio::io::{ErrorKind};
use slog::*;
use std::cmp::{min};
use std::fs::File;
use std::io::{Write, Read};
use std::convert::TryFrom;
use std::fmt::{ Formatter, Debug };

pub struct UploadingFileInternals {
    logger: slog::Logger,
    file_path: PathBuf,

    uploaded_bytes: u64,
    expected_bytes: u64,

    aborted: bool,

    advance_notifies: VecDeque<Arc<AtomicWaker>>,
}

pub struct DefaultUploadTarget {
    upload_handle: Arc<RwLock<UploadingFileInternals>>,
    bytes_left: u64,
    file_handle: Option<File>
}

impl DefaultUploadTarget {
    pub fn new(logger: slog::Logger, file_path: PathBuf, expected_bytes: u64) -> std::io::Result<Self> {
        if let Some(parents) = file_path.parent() {
            std::fs::create_dir_all(parents)?;
        }

        let file_handle = File::with_options()
            .read(false)
            .write(true)
            .create(true)
            .open(&file_path)?;

        file_handle.set_len(expected_bytes)?;

        let upload_handle = Arc::new(RwLock::new(UploadingFileInternals{
            logger,
            file_path,

            uploaded_bytes: 0,
            expected_bytes,

            aborted: false,
            advance_notifies: VecDeque::with_capacity(2)
        }));

        Ok(DefaultUploadTarget{
            upload_handle,
            file_handle: Some(file_handle),
            bytes_left: expected_bytes
        })
    }

    pub fn upload_handle(&self) -> &Arc<RwLock<UploadingFileInternals>> {
        &self.upload_handle
    }
}

impl Debug for DefaultUploadTarget {
    fn fmt(&self, f: &mut Formatter<'_>) -> std::fmt::Result {
        let handle = self.upload_handle.read().unwrap();

        f.debug_struct("DefaultUploadTarget")
            .field("expected_bytes", &handle.expected_bytes)
            .field("uploaded_bytes", &handle.uploaded_bytes)
            .field("aborted", &handle.aborted)
            .field("file", &handle.file_path)
            .finish()
    }
}

impl UploadTarget for DefaultUploadTarget {
    fn file_path(&self) -> PathBuf {
        self.upload_handle.read().unwrap().file_path.clone()
    }

    fn total_expected_bytes(&self) -> u64 {
        self.upload_handle.read().unwrap().expected_bytes
    }

    fn current_bytes(&self) -> u64 {
        self.upload_handle.read().unwrap().uploaded_bytes
    }

    fn write(&mut self, buf: &[u8]) -> UploadWriteResult {
        let write_length = min(
            usize::try_from(self.bytes_left).unwrap_or(usize::max_value()),
            buf.len());

        let file_handle = match self.file_handle.as_mut() {
            Some(handle) => handle,
            None => return UploadWriteResult::TransferFinished
        };

        match file_handle.write(&buf[0..(write_length as usize)]) {
            Ok(length) => {
                let mut internal = self.upload_handle.write().unwrap();
                internal.uploaded_bytes += length as u64;
                self.bytes_left -= length as u64;

                if self.bytes_left == 0 && write_length != buf.len() {
                    internal.advance_notifies.iter().for_each(|notify| notify.wake());
                    UploadWriteResult::WriteExceedsSize(length, buf.len() - length)
                } else {
                    internal.advance_notifies.iter().for_each(|notify| notify.wake());
                    UploadWriteResult::Success(length)
                }
            },
            Err(error) => UploadWriteResult::IoError(error)
        }
    }

    fn finish_upload(&mut self) {
        {
            let mut internal = self.upload_handle.write().unwrap();
            if internal.uploaded_bytes != internal.expected_bytes {
                slog::warn!(internal.logger, "Received upload finish but we did not received the expected bytes"; slog::o!(
                    "expected" => internal.expected_bytes,
                    "uploaded" => internal.uploaded_bytes
                ));

                internal.uploaded_bytes = internal.expected_bytes;
            }
            internal.advance_notifies.iter().for_each(|notify| notify.wake());
        }

        let mut file = self.file_handle.take().expect("missing internal file handle");
        if let Err(error) = file.flush() {
            warn!(self.upload_handle.read().unwrap().logger, "Failed to flush file upload for {}: {}", self.file_path().to_string_lossy(), error);
        }
    }

    fn abort_upload(&mut self) {
        let mut internal = self.upload_handle.write().unwrap();
        internal.aborted = true;
        internal.advance_notifies.iter().for_each(|notify| notify.wake());

        let file_path = internal.file_path.clone();
        let logger = internal.logger.clone();
        drop(internal);

        self.file_handle.take();

        if let Err(error) = std::fs::remove_file(&file_path) {
            warn!(logger, "Failed to delete aborted file upload for {}: {}", file_path.to_string_lossy(), error);
        }
    }
}

#[cfg(debug_assertions)]
impl Drop for DefaultUploadTarget {
    fn drop(&mut self) {
        if self.file_handle.is_none() {
            return;
        }

        let internal = self.upload_handle.read().unwrap();
        error!(internal.logger, "File upload {} dropped without being finished properly.", internal.file_path.to_string_lossy());
    }
}

pub struct StreamingDownloadTarget {
    upload_handle: Arc<RwLock<UploadingFileInternals>>,
    registered_waker: Arc<AtomicWaker>,
    file_handle: File,
    file_offset: u64
}

impl StreamingDownloadTarget {
    pub fn new(upload_handle: Arc<RwLock<UploadingFileInternals>>) -> std::io::Result<Self> {
        let waker = Arc::new(AtomicWaker::new());
        let mut locked_handle = upload_handle.write().unwrap();
        locked_handle.advance_notifies.push_back(waker.clone());
        let file_path = locked_handle.file_path.clone();
        drop(locked_handle);

        let file = File::with_options()
            .read(true)
            .write(false)
            .open(&file_path)?;

        Ok(StreamingDownloadTarget{
            file_handle: file,
            file_offset: 0,

            registered_waker: waker,
            upload_handle
        })
    }


    pub fn try_read(&mut self, buf: &mut [u8]) -> Option<std::io::Result<usize>> {
        let upload_handle = self.upload_handle.read().unwrap();
        if upload_handle.aborted {
            return Some(Err(std::io::Error::new(ErrorKind::NotFound, "upload has been aborted")));
        }

        if self.file_offset < upload_handle.uploaded_bytes {
            let bytes_available = upload_handle.uploaded_bytes - self.file_offset;
            drop(upload_handle);

            let max_bytes = min(usize::try_from(bytes_available).unwrap_or(usize::max_value()), buf.len());
            match self.file_handle.read(&mut buf[0..max_bytes]) {
                Ok(bytes_read) => {
                    self.file_offset += bytes_read as u64;
                    Some(Ok(bytes_read))
                },
                Err(error) => Some(Err(error))
            }
        } else {
            None
        }
    }
}

impl Debug for StreamingDownloadTarget {
    fn fmt(&self, f: &mut Formatter<'_>) -> std::fmt::Result {
        let handle = self.upload_handle.read().unwrap();

        f.debug_struct("StreamingDownloadTarget")
            .field("expected_bytes", &handle.expected_bytes)
            .field("uploaded_bytes", &handle.uploaded_bytes)
            .field("aborted", &handle.aborted)
            .field("file", &handle.file_path)
            .field("file_offset", &self.file_offset)
            .finish()
    }
}

impl DownloadTarget for StreamingDownloadTarget {
    fn file_path(&self) -> PathBuf {
        self.upload_handle.read().unwrap().file_path.clone()
    }

    fn current_bytes(&self) -> u64 {
        self.file_offset
    }

    fn total_expected_bytes(&self) -> u64 {
        self.upload_handle.read().unwrap().expected_bytes
    }

    fn poll_read(mut self: Pin<&mut Self>, cx: &mut Context<'_>, buf: &mut [u8]) -> Poll<std::io::Result<usize>> {
        match self.try_read(buf) {
            Some(result) => Poll::Ready(result),
            None => {
                self.registered_waker.register(cx.waker());
                Poll::Pending
            }
        }
    }
}

impl Drop for StreamingDownloadTarget {
    fn drop(&mut self) {
        let mut upload_handle = self.upload_handle.write().unwrap();
        upload_handle.advance_notifies.retain(|entry| entry.as_ref() as *const _ != self.registered_waker.as_ref() as *const _);
    }
}

pub struct DownloadFileInternals {
    logger: slog::Logger,
    file_path: PathBuf,
    file_size: u64
}

pub struct NormalDownloadTarget {
    internals: Arc<DownloadFileInternals>,
    file_handle: File,
    byte_offset: u64
}

impl NormalDownloadTarget {
    pub fn new(logger: slog::Logger, file_path: PathBuf) -> std::io::Result<Self> {
        let file_handle = File::with_options()
            .read(true)
            .write(false)
            .open(&file_path)?;

        let file_size = file_handle.metadata()?.len();

        let internals = Arc::new(DownloadFileInternals{
            logger,
            file_path,
            file_size
        });

        Ok(NormalDownloadTarget {
            internals,
            file_handle,
            byte_offset: 0
        })
    }

    pub fn new_from_handle(handle: Arc<DownloadFileInternals>) -> std::io::Result<Self> {
        let file_handle = File::with_options()
            .read(true)
            .write(false)
            .open(&handle.file_path)?;

        let file_size = file_handle.metadata()?.len();
        if file_size != handle.file_size {
            return Err(std::io::Error::new(ErrorKind::InvalidInput, "file size miss match"));
        }

        Ok(NormalDownloadTarget {
            internals: handle,
            file_handle,
            byte_offset: 0
        })
    }

    pub fn download_handle(&self) -> &Arc<DownloadFileInternals> {
        &self.internals
    }
}

impl Debug for NormalDownloadTarget {
    fn fmt(&self, f: &mut Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("NormalDownloadTarget")
            .field("byte_offset", &self.byte_offset)
            .field("file", &self.internals.file_path)
            .finish()
    }
}

impl DownloadTarget for NormalDownloadTarget {
    fn file_path(&self) -> PathBuf {
        self.internals.file_path.clone()
    }

    fn current_bytes(&self) -> u64 {
        self.byte_offset
    }

    fn total_expected_bytes(&self) -> u64 {
        self.internals.file_size
    }

    fn poll_read(mut self: Pin<&mut Self>, _cx: &mut Context<'_>, buf: &mut [u8]) -> Poll<std::io::Result<usize>> {
        Poll::Ready(self.file_handle.read(buf))
    }
}


#[cfg(test)]
mod test {
    use std::path::{Path};
    use std::fs::File;
    use std::io::{Write, Read};
    use crate::files::io::{DefaultUploadTarget, StreamingDownloadTarget};
    use crate::files::{UploadTarget};
    use tokio::io::ErrorKind;
    use crate::log::create_test_logger;

    #[test]
    fn test_delete_on_read() {
        let file_path = Path::new("test_file.txt");

        let mut write = File::with_options()
            .read(false)
            .write(true)
            .truncate(true)
            .create(true)
            .open(&file_path).expect("failed to open file");

        let mut read = File::with_options()
            .read(true)
            .write(false)
            .open(&file_path).expect("failed to open file");

        write.write_all(b"Hello World").expect("failed to write");

        let mut buffer = [0u8; 11];
        read.read_exact(&mut buffer).expect("failed to read");

        assert_eq!(&buffer, b"Hello World");
    }

    #[test]
    fn test_streaming() {
        let logger = create_test_logger();
        let mut buffer = [0u8; 10];

        let mut upload = DefaultUploadTarget::new(logger.clone(), "test.txt".into(), 10).expect("failed to create upload");
        let mut download_a = StreamingDownloadTarget::new(upload.upload_handle().clone()).expect("failed to create download");
        let mut download_b = StreamingDownloadTarget::new(upload.upload_handle().clone()).expect("failed to create download");
        let mut download_c = StreamingDownloadTarget::new(upload.upload_handle().clone()).expect("failed to create download");

        match download_a.try_read(&mut buffer) {
            None => {},
            result => panic!("Unexpected read result: {:?}", result)
        }

        upload.write(b"Hello");

        match download_a.try_read(&mut buffer) {
            Some(Ok(length)) => {
                assert_eq!(length, 5);
                assert_eq!(&buffer[0..5], b"Hello");
            },
            result => panic!("Unexpected read result: {:?}", result)
        }

        upload.write(b"World");

        match download_a.try_read(&mut buffer) {
            Some(Ok(length)) => {
                assert_eq!(length, 5);
                assert_eq!(&buffer[0..5], b"World");
            },
            result => panic!("Unexpected read result: {:?}", result)
        }

        match download_a.try_read(&mut buffer) {
            None => {},
            result => panic!("Unexpected read result: {:?}", result)
        }

        match download_b.try_read(&mut buffer) {
            Some(Ok(length)) => {
                assert_eq!(length, 10);
                assert_eq!(&buffer[0..10], b"HelloWorld");
            },
            result => panic!("Unexpected read result: {:?}", result)
        }

        upload.abort_upload();

        match download_c.try_read(&mut buffer) {
            Some(Err(error)) => {
                assert_eq!(error.kind(), ErrorKind::NotFound);
            },
            result => panic!("Unexpected read result: {:?}", result)
        }
    }
}