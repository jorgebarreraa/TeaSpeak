use tokio::io::{AsyncWrite, AsyncRead, ErrorKind, Error};
use crate::request::util::{BufferStreamReader, send_http_response, StreamStatistics, write_generic_http_header, TransferHttpHeader, HttpMethod};
use crate::request::{TransferRequest, NetworkTransferDownloadSink, TransferExecuteError, NetworkTransferUploadSink, RequestClientInfo, NetworkTransferSink};
use std::sync::Arc;
use crate::request_handler::{FileTransferInfo, FT_KEY_SIZE};
use tokio::macros::support::{Pin, Future, Poll};
use crate::request::accept::{AcceptStepResult, AcceptStepFutureClose};
use std::io::{Cursor, Write};
use futures::task::Context;
use futures::{FutureExt};
use std::convert::TryFrom;
use tokio::time::Duration;

pub struct TransferRequestRaw<S: AsyncWrite + AsyncRead + Unpin + Send> {
    transfer_key: [u8; FT_KEY_SIZE],
    connection: Arc<RequestClientInfo>,
    socket: BufferStreamReader<S>,
}

impl<S: AsyncWrite + AsyncRead + Unpin + Send> TransferRequestRaw<S> {
    pub fn new(transfer_key: [u8; FT_KEY_SIZE], connection: Arc<RequestClientInfo>, socket: BufferStreamReader<S>) -> Self {
        TransferRequestRaw{
            transfer_key,
            connection,
            socket
        }
    }
}

impl<S: AsyncWrite + AsyncRead + Unpin + Send> TransferRequest for TransferRequestRaw<S> {
    fn transfer_key(&self) -> &[u8; FT_KEY_SIZE] {
        &self.transfer_key
    }

    fn execute_download(&mut self, _request_info: Arc<FileTransferInfo>) -> Result<Box<dyn NetworkTransferDownloadSink>, TransferExecuteError> {
        Err(TransferExecuteError::NotSupported)
    }

    fn execute_upload(&mut self, _request_info: Arc<FileTransferInfo>) -> Result<Box<dyn NetworkTransferUploadSink>, TransferExecuteError> {
        Err(TransferExecuteError::NotSupported)
    }

    fn close_request(self: Box<Self>, _reason: &str) -> Option<Pin<Box<dyn Future<Output=()> + Send>>> {
        /* Just close the socket and we're ready to go. We don't need to shutdown anything. */
        None
    }
}

struct TransferRequestHttpInner<S: AsyncWrite + AsyncRead + Unpin + Send> {
    socket: BufferStreamReader<S>,
    http_header: TransferHttpHeader,
}

pub struct TransferRequestHttp<S: AsyncWrite + AsyncRead + Unpin + Send> {
    transfer_key: [u8; FT_KEY_SIZE],
    connection: Arc<RequestClientInfo>,

    inner: Option<TransferRequestHttpInner<S>>
}

impl<S: AsyncWrite + AsyncRead + Unpin + Send + 'static> TransferRequestHttp<S> {
    pub fn parse(connection: Arc<RequestClientInfo>, socket: BufferStreamReader<S>, http_header: TransferHttpHeader) -> AcceptStepResult {
        let mut result = Box::new(TransferRequestHttp{
            transfer_key: [0; FT_KEY_SIZE],
            connection,

            inner: Some(TransferRequestHttpInner{
                socket,
                http_header
            })
        });

        let http_header = result.http_header().unwrap();
        if http_header.method == HttpMethod::Options {
            slog::debug!(result.connection.logger, "Received CORS preflight request. Sending response.");
            return AcceptStepResult::NextStep(Box::new(AcceptStepFutureClose::new(
                result.http_close((200, "Ok"), None)
            )))
        }

        let transfer_key = match http_header.url_parameters.iter().find(|(key,_)| key == "transfer-key") {
            Some((_, value)) => value,
            None => return AcceptStepResult::NextStep(Box::new(AcceptStepFutureClose::new(
                result.http_close((400, "Bad Request"), Some("missing transfer key".to_string()))
            )))
        };

        if transfer_key.len() != FT_KEY_SIZE {
            return AcceptStepResult::NextStep(Box::new(AcceptStepFutureClose::new(
                result.http_close((400, "Bad Request"), Some("invalid transfer key length".to_string()))
            )));
        }

        let transfer_key = transfer_key.clone();
        result.transfer_key.copy_from_slice(transfer_key.as_bytes());
        AcceptStepResult::Finished(result)
    }

    fn http_header(&self) -> Option<&TransferHttpHeader> {
        self.inner.as_ref().map(|e| &e.http_header)
    }

    fn http_close(mut self: Box<Self>, status: (u16, &'static str), body: Option<String>) -> Pin<Box<dyn Future<Output=()> + Send>> {
        let mut inner = {
            if let Some(inner) = self.inner.take() {
                inner
            } else {
                return Box::pin(async {})
            }
        };

        let logger = self.connection.logger.clone();
        Box::pin(async move {
            if let Err(error) = send_http_response(&mut inner.socket, status, body, inner.http_header.http_headers).await {
                match error.kind() {
                    ErrorKind::BrokenPipe |
                    ErrorKind::UnexpectedEof |
                    ErrorKind::NotConnected => {
                        return;
                    },
                    _ => {
                        slog::warn!(logger, "Failed to send/generate http error response: {}", error);
                    }
                }
            }
        })
    }

    /*
    fn create_http_download_header(&self, content_length: usize) -> std::io::Result<Vec<u8>> {
        let mut buffer: [u8; 1024 * 2] = unsafe { std::mem::MaybeUninit::uninit().assume_init() };
        let mut writer = Cursor::new(&mut buffer[..]);

        write!(writer, "HTTP/1.1 {} {}\r\n", status.0, status.1)?;
        write_generic_http_header(&mut writer, &self.http_headers)?;
        write!(writer, "Content-Type: text/html\r\n")?;
        write!(writer, "Content-Length: {}\r\n", content_length)?;
        write!(writer, "\r\n")?;


        let written = writer.position() as usize;
        Ok(buffer[0..written].to_vec())
    }
    */
}

impl<S: AsyncWrite + AsyncRead + Unpin + Send + 'static> TransferRequest for TransferRequestHttp<S> {
    fn transfer_key(&self) -> &[u8; 32] {
        &self.transfer_key
    }

    fn execute_download(&mut self, request_info: Arc<FileTransferInfo>) -> Result<Box<dyn NetworkTransferDownloadSink>, TransferExecuteError> {
        let inner = {
            if let Some(inner) = self.inner.take() {
                inner
            } else {
                return Err(TransferExecuteError::InternalError);
            }
        };

        return Ok(Box::new(TransferHttpDownload::new(
            self.connection.clone(),
            request_info,
            inner.socket,
            inner.http_header
        )));
    }

    fn execute_upload(&mut self, _request_info: Arc<FileTransferInfo>) -> Result<Box<dyn NetworkTransferUploadSink>, TransferExecuteError> {
        Err(TransferExecuteError::NotSupported)
    }

    fn close_request(self: Box<Self>, reason: &str) -> Option<Pin<Box<dyn Future<Output=()> + Send>>> {
        Some(self.http_close((503, "Service Unavailable"), Some(reason.to_owned())))
    }
}

const MAX_MEDIA_BYTES_LENGTH: usize = 32;
enum TransferHttpDownloadState {
    AwaitingMediaBytes{
        bytes: [u8; MAX_MEDIA_BYTES_LENGTH],
        /// Total bytes which should be buffered.
        /// This will be the `min` between the actual file size and the standard
        /// media byte length.
        bytes_total: usize,
        bytes_buffered: usize,
    },
    SendingHttp{
        buffer: Vec<u8>,
        written: usize
    },
    SendingContent,
    Borrowed
}

struct TransferHttpDownload<S: AsyncWrite + AsyncRead + Unpin + Send + 'static> {
    connection: Arc<RequestClientInfo>,
    transfer_info: Arc<FileTransferInfo>,
    /// The request http header.
    /// Might be `None` if a http response header has already been send.
    http_header: Option<TransferHttpHeader>,

    socket: S,
    status: TransferHttpDownloadState
}

impl<S: AsyncWrite + AsyncRead + Unpin + Send + 'static> TransferHttpDownload<S> {
    fn generate_http_response(&mut self, media_bytes: &[u8]) -> std::io::Result<Vec<u8>> {
        let http_header = self.http_header.take().expect("missing http header");

        let mut buffer: [u8; 1024 * 2 + MAX_MEDIA_BYTES_LENGTH] = unsafe { std::mem::MaybeUninit::uninit().assume_init() };
        let mut writer = Cursor::new(&mut buffer[..]);

        /*
        const auto download_name = request.findHeader("download-name");
        response.setHeader("Content-Length", { std::to_string(client->transfer->expected_file_size - client->transfer->file_offset) });

        response.setHeader("Content-type", {"application/octet-stream; "});
        response.setHeader("Content-Transfer-Encoding", {"binary"});

        response.setHeader("Content-Disposition", {
            "attachment; filename=\"" + http::encode_url(request.parameters.count("download-name") > 0 ? request.parameters.at("download-name") : client->transfer->file_name) + "\""
        });

        response.setHeader("X-media-bytes", { base64::encode((char*) client->file.media_bytes, client->file.media_bytes_length) });
        client->networking.http_state = FileClient::HTTP_STATE_DOWNLOADING;
        goto send_response_exit;
         */

        write!(writer, "HTTP/1.1 200 OK\r\n")?;
        write_generic_http_header(&mut writer, &http_header.http_headers)?;
        write!(writer, "Content-Type: application/octet-stream; \r\n")?;
        write!(writer, "Content-Transfer-Encoding: binary\r\n")?;
        write!(writer, "Content-Length: {}\r\n", self.transfer_info.file_size())?;
        write!(writer, "Content-Disposition: attachment; filename=\"{}\"\r\n", "FileNameTODO__")?;
        /* TODO: Download name */

        /* TODO: Encode media bytes */
        write!(writer, "X-media-bytes: SomeBytes\r\n")?;

        write!(writer, "\r\n")?;
        writer.write_all(media_bytes)?;

        let written = writer.position() as usize;
        Ok(buffer[0..written].to_vec())
    }

    fn new(connection: Arc<RequestClientInfo>, transfer_info: Arc<FileTransferInfo>, socket: S, http_header: TransferHttpHeader) -> Self {
        let media_bytes = MAX_MEDIA_BYTES_LENGTH.min(usize::try_from(transfer_info.file_size()).unwrap_or(usize::max_value()));
        TransferHttpDownload{
            connection,
            transfer_info,
            socket,
            http_header: Some(http_header),
            status: TransferHttpDownloadState::AwaitingMediaBytes {
                bytes: unsafe { std::mem::MaybeUninit::uninit().assume_init() },
                bytes_total: media_bytes,
                bytes_buffered: 0
            }
        }
    }

    fn flush_sending_http(&mut self, cx: &mut Context<'_>) -> Poll<std::io::Result<()>> {
        let mut status = std::mem::replace(&mut self.status, TransferHttpDownloadState::Borrowed);
        if let TransferHttpDownloadState::SendingHttp { buffer, written } = &mut status {
            let mut bytes_required = buffer.len() - *written;
            while bytes_required > 0 {
                match AsyncWrite::poll_write(Pin::new(&mut self.socket), cx, &buffer[*written..]) {
                    Poll::Pending => {
                        self.status = status;
                        return Poll::Pending;
                    },
                    Poll::Ready(Err(error)) => {
                        self.status = status;
                        return Poll::Ready(Err(error))
                    },
                    Poll::Ready(Ok(length)) => {
                        bytes_required -= length;
                        *written += length;
                    }
                }
            }

            debug_assert!(bytes_required == 0);
            self.status = TransferHttpDownloadState::SendingContent;

            cx.waker().wake_by_ref();
            Poll::Pending
        } else {
            self.status = status;
            Poll::Ready(Ok(()))
        }
    }
}

impl<S: AsyncWrite + AsyncRead + Unpin + Send + 'static> AsyncWrite for TransferHttpDownload<S> {
    fn poll_write(mut self: Pin<&mut Self>, cx: &mut Context<'_>, buf: &[u8]) -> Poll<Result<usize, Error>> {
        match &mut self.status {
            TransferHttpDownloadState::AwaitingMediaBytes { bytes, bytes_buffered, bytes_total } => {
                let write_bytes = (*bytes_total - *bytes_buffered).min(buf.len());
                bytes[*bytes_buffered..(*bytes_buffered + write_bytes)].copy_from_slice(&buf[0..write_bytes]);
                *bytes_buffered += write_bytes;

                if *bytes_buffered == *bytes_total {
                    /* We have our media bytes. Send HTTP response header */
                    let media_bytes = bytes[0..*bytes_total].to_vec();
                    match self.generate_http_response(&media_bytes) {
                        Ok(buffer) => {
                            self.status = TransferHttpDownloadState::SendingHttp {
                                written: 0,
                                buffer
                            };
                        },
                        Err(error) => return Poll::Ready(Err(error))
                    }
                }

                /* We've successfully written these bytes */
                Poll::Ready(Ok(write_bytes))
            },
            TransferHttpDownloadState::SendingHttp { .. } => {
                match self.flush_sending_http(cx) {
                    Poll::Ready(Ok(_)) => { },
                    Poll::Ready(Err(error)) => return Poll::Ready(Err(error)),
                    Poll::Pending => return Poll::Pending
                };

                self.status = TransferHttpDownloadState::SendingContent;
                cx.waker().wake_by_ref();
                Poll::Pending
            },
            TransferHttpDownloadState::SendingContent => {
                self.status = TransferHttpDownloadState::SendingContent;
                AsyncWrite::poll_write(Pin::new(&mut self.socket), cx, buf)
            },
            TransferHttpDownloadState::Borrowed => return Poll::Ready(Err(Error::new(ErrorKind::InvalidData, "TransferHttpDownloadState::Borrowed")))
        }
    }

    fn poll_flush(mut self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Result<(), Error>> {
        if matches!(&self.status, TransferHttpDownloadState::SendingHttp { .. }) {
            match self.flush_sending_http(cx) {
                Poll::Ready(Ok(_)) => {
                    self.status = TransferHttpDownloadState::SendingContent;
                    cx.waker().wake_by_ref();
                    Poll::Pending
                },
                Poll::Ready(Err(error)) => Poll::Ready(Err(error)),
                Poll::Pending => Poll::Pending
            }
        } else {
            AsyncWrite::poll_flush(Pin::new(&mut self.socket), cx)
        }
    }

    fn poll_shutdown(mut self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Result<(), Error>> {
        if matches!(&self.status, TransferHttpDownloadState::SendingHttp { .. }) {
            match self.flush_sending_http(cx) {
                Poll::Ready(Ok(_)) => {
                    self.status = TransferHttpDownloadState::SendingContent;
                    cx.waker().wake_by_ref();
                    Poll::Pending
                },
                Poll::Ready(Err(error)) => Poll::Ready(Err(error)),
                Poll::Pending => Poll::Pending
            }
        } else {
            AsyncWrite::poll_shutdown(Pin::new(&mut self.socket), cx)
        }
    }
}

impl<S: AsyncWrite + AsyncRead + Unpin + Send + 'static> NetworkTransferSink for TransferHttpDownload<S> {
    fn network_statistics(&self) -> Arc<StreamStatistics> {
        self.connection.network_statistics.clone()
    }

    fn close_connection(mut self: Box<Self>, _reason: Option<&str>) -> Option<Box<dyn Future<Output=()> + Send>> {
        /* TODO: If we haven't send an HTTP response yet, do it now. */

        let future = tokio::time::timeout(Duration::from_secs(5), async move {
            tokio::io::AsyncWriteExt::shutdown(&mut self).await?;
            Ok::<(), std::io::Error>(())
        });

        let future = future.then(|_result| async {
            /* TODO: Log if everything succeeded */
        });

        Some(box future)
    }
}

impl<S: AsyncWrite + AsyncRead + Unpin + Send + 'static> NetworkTransferDownloadSink for TransferHttpDownload<S> { }