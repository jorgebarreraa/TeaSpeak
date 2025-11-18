use tokio::io::{AsyncRead, AsyncWrite, ReadBuf, ErrorKind};
use std::task::{Context, Poll};
use std::pin::Pin;
use std::cmp::min;
use tokio::net::TcpStream;
use tokio::time::Duration;
use std::sync::atomic::{AtomicU64, Ordering};
use std::io::{Result, Read, Write, Cursor};
use std::sync::{Arc, Weak};
use tokio::io::AsyncWriteExt;
use crate::version::{server_version, os_type};

pub struct BufferStreamReader<S: AsyncRead + Unpin> {
    source: S,

    buffer: Vec<u8>,
    buffered_bytes: usize,

    eof_received: bool
}

impl<S: AsyncRead + Unpin> BufferStreamReader<S> {
    pub fn new(stream: S) -> Self {
        BufferStreamReader{
            source: stream,
            buffer: Vec::with_capacity(128),
            buffered_bytes: 0,
            eof_received: false
        }
    }

    pub fn into_inner(mut self) -> (Vec<u8>, S) {
        self.buffer.truncate(self.buffered_bytes);
        (self.buffer, self.source)
    }

    pub fn filled(&self) -> bool {
        self.buffered_bytes == self.buffer.len()
    }

    pub fn poll_buffered(&mut self, cx: &mut Context<'_>, target_bytes: usize) -> Poll<Result<()>> {
        if self.buffer.len() < target_bytes {
            self.buffer.resize(target_bytes, 0);
        }

        if target_bytes > self.buffered_bytes {
            let mut reader = ReadBuf::new(&mut self.buffer[self.buffered_bytes..target_bytes]);

            while let Poll::Ready(result) = AsyncRead::poll_read(Pin::new(&mut self.source), cx, &mut reader) {
                result?;

                if reader.filled().len() == 0 {
                    self.eof_received = true;
                }
            }

            let bytes_read = reader.filled().len();
            if bytes_read == 0 && self.eof_received {
                return Poll::Ready(Err(std::io::Error::new(ErrorKind::UnexpectedEof, "eof while buffering")));
            }

            self.buffered_bytes += bytes_read;
            if self.buffered_bytes < target_bytes {
                return Poll::Pending;
            }
        }

        return Poll::Ready(Ok(()));
    }

    pub fn filled_buffer(&self) -> &[u8] {
        &self.buffer[0..self.buffered_bytes]
    }

    pub fn consume_buffer(&mut self, bytes: usize) {
        if bytes == self.buffered_bytes {
            self.buffered_bytes = 0;
        } else {
            assert!(self.buffered_bytes > bytes);
            self.buffer.copy_within(bytes..self.buffered_bytes, 0);
            self.buffered_bytes -= bytes;
        }
    }
}

impl<S: AsyncRead + Unpin> AsyncRead for BufferStreamReader<S> {
    fn poll_read(mut self: Pin<&mut Self>, cx: &mut Context<'_>, buf: &mut ReadBuf<'_>) -> Poll<Result<()>> {
        if self.buffered_bytes > 0 {
            let bytes = min(buf.remaining(), self.buffered_bytes);
            buf.put_slice(&self.buffer[0..bytes]);
            self.consume_buffer(bytes);
            return Poll::Ready(Ok(()));
        } else {
            AsyncRead::poll_read(Pin::new(&mut self.source), cx, buf)
        }
    }
}

impl<S: AsyncRead + AsyncWrite + Unpin> AsyncWrite for BufferStreamReader<S> {
    fn poll_write(mut self: Pin<&mut Self>, cx: &mut Context<'_>, buf: &[u8]) -> Poll<Result<usize>> {
        AsyncWrite::poll_write(Pin::new(&mut self.source), cx, buf)
    }

    fn poll_flush(mut self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Result<()>> {
        AsyncWrite::poll_flush(Pin::new(&mut self.source), cx)
    }

    fn poll_shutdown(mut self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Result<()>> {
        AsyncWrite::poll_shutdown(Pin::new(&mut self.source), cx)
    }
}

pub struct StreamStatistics {
    parent: Weak<StreamStatistics>,
    bytes_read: AtomicU64,
    bytes_write: AtomicU64,
}

impl StreamStatistics {
    pub fn new() -> Self {
        StreamStatistics{
            parent: Weak::new(),
            bytes_read: AtomicU64::new(0),
            bytes_write: AtomicU64::new(0)
        }
    }

    pub fn new_parent(parent: &Arc<StreamStatistics>) -> Self {
        StreamStatistics{
            parent: Arc::downgrade(parent),
            bytes_read: AtomicU64::new(0),
            bytes_write: AtomicU64::new(0)
        }
    }

    pub fn bytes_read(&self) -> u64 {
        self.bytes_read.load(Ordering::Relaxed)
    }

    pub fn bytes_write(&self) -> u64 {
        self.bytes_write.load(Ordering::Relaxed)
    }

    fn increase_read(&self, bytes: u64) {
        self.bytes_read.fetch_add(bytes, Ordering::Relaxed);

        if let Some(parent) = self.parent.upgrade() {
            parent.increase_read(bytes);
        }
    }

    fn increase_write(&self, bytes: u64) {
        self.bytes_write.fetch_add(bytes, Ordering::Relaxed);

        if let Some(parent) = self.parent.upgrade() {
            parent.increase_write(bytes);
        }
    }
}

pub struct StatisticsStream<S: Unpin> {
    stream: S,
    statistics: Arc<StreamStatistics>
}

impl<S: Unpin> StatisticsStream<S> {
    pub fn new(stream: S) -> Self {
        StatisticsStream{
            stream,
            statistics: Arc::new(StreamStatistics{
                parent: Weak::new(),
                bytes_write: Default::default(),
                bytes_read: Default::default()
            })
        }
    }

    pub fn new_with_statistics(stream: S, statistics: Arc<StreamStatistics>) -> Self {
        StatisticsStream{
            stream,
            statistics
        }
    }

    pub fn statistics(&self) -> &Arc<StreamStatistics> {
        &self.statistics
    }
}

impl<S: Read + Unpin> Read for StatisticsStream<S> {
    fn read(&mut self, buf: &mut [u8]) -> Result<usize> {
        match self.stream.read(buf) {
            Ok(length) => {
                self.statistics.increase_read(length as u64);
                Ok(length)
            },
            result => result
        }
    }
}

impl<S: AsyncRead + Unpin> AsyncRead for StatisticsStream<S> {
    fn poll_read(mut self: Pin<&mut Self>, cx: &mut Context<'_>, buf: &mut ReadBuf<'_>) -> Poll<Result<()>> {
        let fill_offset = buf.filled().len();
        match AsyncRead::poll_read(Pin::new(&mut self.stream), cx, buf) {
            Poll::Ready(Ok(_)) => {
                let bytes_read = buf.filled().len() - fill_offset;
                self.statistics.increase_read(bytes_read as u64);
                return Poll::Ready(Ok(()));
            },
            result => result
        }
    }
}

impl<S: Write + Unpin> Write for StatisticsStream<S> {
    fn write(&mut self, buf: &[u8]) -> Result<usize> {
        match self.stream.write(buf) {
            Ok(length) => {
                self.statistics.increase_write(length as u64);
                Ok(length)
            },
            result => result
        }
    }

    fn flush(&mut self) -> Result<()> {
        self.stream.flush()
    }
}

impl<S: AsyncWrite + Unpin> AsyncWrite for StatisticsStream<S> {
    fn poll_write(mut self: Pin<&mut Self>, cx: &mut Context<'_>, buf: &[u8]) -> Poll<Result<usize>> {
        match Pin::new(&mut self.stream).poll_write(cx, buf) {
            Poll::Ready(Ok(length)) => {
                self.statistics.increase_write(length as u64);
                Poll::Ready(Ok(length))
            },
            result => result
        }
    }

    fn poll_flush(mut self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Result<()>> {
        Pin::new(&mut self.stream).poll_flush(cx)
    }

    fn poll_shutdown(mut self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Result<()>> {
        Pin::new(&mut self.stream).poll_shutdown(cx)
    }
}

// If Tokio's `TcpSocket` gains support for setting the
// keepalive timeout, it would be nice to use that instead of socket2,
// and avoid the unsafe `into_raw_fd`/`from_raw_fd` dance...
#[allow(unused_variables)]
pub fn set_keepalive(socket: &mut TcpStream, time: Option<Duration>, interval: Option<Duration>, retries: Option<u32>) ->  Result<()> {
    // Convert the Tokio `TcpStream` into a `socket2` socket
    // so we can call `set_keepalive`.
    #[cfg(unix)]
        let socket: socket2::Socket = unsafe {
        // Safety: `socket2`'s socket will try to close the
        // underlying fd when it's dropped. However, we
        // can't take ownership of the fd from the tokio
        // TcpStream, so instead we will call `into_raw_fd`
        // on the socket2 socket before dropping it. This
        // prevents it from trying to close the fd.
        use std::os::unix::io::{AsRawFd, FromRawFd};
        socket2::Socket::from_raw_fd(socket.as_raw_fd())
    };
    #[cfg(windows)]
        let socket: socket2::Socket = unsafe {
        // Safety: `socket2`'s socket will try to close the
        // underlying SOCKET when it's dropped. However, we
        // can't take ownership of the SOCKET from the tokio
        // TcpStream, so instead we will call `into_raw_socket`
        // on the socket2 socket before dropping it. This
        // prevents it from trying to close the SOCKET.
        use std::os::windows::io::{AsRawSocket, FromRawSocket};
        socket2::Socket::from_raw_socket(socket.as_raw_socket())
    };

    // Actually set the TCP keepalive timeout.
    let mut timeout = socket2::TcpKeepalive::new();
    if let Some(time) = time { timeout = timeout.with_time(time); }
    if let Some(interval) = interval { timeout = timeout.with_interval(interval); }
    #[cfg(any(
        target_os = "freebsd",
        target_os = "fuchsia",
        target_os = "linux",
        target_os = "netbsd",
        target_vendor = "apple",
    ))]
    if let Some(retries) = retries { timeout = timeout.with_retries(retries); }

    let result = socket.set_tcp_keepalive(&timeout);

    // Take ownershop of the fd/socket back from the socket2
    // `Socket`, so that socket2 doesn't try to close it
    // when it's dropped.
    #[cfg(unix)]
    drop(std::os::unix::io::IntoRawFd::into_raw_fd(socket));

    #[cfg(windows)]
    drop(std::os::windows::io::IntoRawSocket::into_raw_socket(socket));

    result
}

#[derive(Debug, Copy, Clone, Ord, PartialOrd, Eq, PartialEq)]
pub enum HttpMethod {
    Get,
    Post,
    Options
}

#[derive(Debug, Clone)]
pub struct TransferHttpHeader {
    pub method: HttpMethod,
    pub url_parameters: Vec<(String, String)>,
    pub http_headers: Vec<(String, String)>,
}

pub fn write_generic_http_header(writer: &mut Cursor<&mut [u8]>, http_headers: &[(String, String)]) -> std::io::Result<()> {
    write!(writer, "Server: TeaSpeak-FileServer/{} ({})\r\n", server_version(), os_type())?;
    write!(writer, "Connection: close\r\n")?;

    /* CORS header */
    write!(writer, "Access-Control-Allow-Methods: GET, POST\r\n")?;
    write!(writer, "Access-Control-Allow-Origin: *\r\n")?;
    write!(writer, "Access-Control-Max-Age: 86400\r\n")?;

    if let Some((_, value)) = http_headers.iter()
        .find(|(key, _)| key == "Access-Control-Request-Headers") {
        write!(writer, "Access-Control-Allow-Headers: {}\r\n", value)?;
    } else {
        write!(writer, "Access-Control-Allow-Headers: *\r\n")?;
    }

    Ok(())
}

pub async fn send_http_response<S: AsyncWrite + AsyncRead + Unpin + Send + 'static>(stream: &mut S, status: (u16, &'static str), body: Option<String>, http_headers: Vec<(String, String)>) -> std::io::Result<()> {
    let mut buffer: [u8; 1024 * 2] = unsafe { std::mem::MaybeUninit::uninit().assume_init() };
    let mut writer = Cursor::new(&mut buffer[..]);

    write!(writer, "HTTP/1.1 {} {}\r\n", status.0, status.1)?;
    write_generic_http_header(&mut writer, &http_headers)?;
    write!(writer, "Content-Type: text/html\r\n")?;
    write!(writer, "Content-Length: {}\r\n", body.as_ref().map_or(0, |body| body.len()))?;
    write!(writer, "\r\n")?;

    let written = writer.position() as usize;
    stream.write_all(&buffer[0..written]).await?;
    if let Some(body) = &body {
        stream.write_all(body.as_bytes()).await?;
    }

    Ok(())
}