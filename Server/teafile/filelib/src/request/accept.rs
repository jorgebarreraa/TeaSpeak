use std::future::Future;
use tokio::net::TcpStream;
use futures::task::Context;
use tokio::macros::support::{Pin, Poll};
use tokio::io::{AsyncRead, AsyncWrite, ErrorKind};
use slog::*;
use tokio_openssl;
use openssl::ssl::{Ssl};
use crate::ssl::CERTIFICATE_CACHE;
use std::sync::Arc;
use crate::request::{TransferRequest, RequestClientInfo};
use crate::request::util::{BufferStreamReader, StatisticsStream, send_http_response, HttpMethod, TransferHttpHeader};
use crate::request_handler::FT_KEY_SIZE;
use crate::request::request::{TransferRequestRaw, TransferRequestHttp};

#[derive(Debug)]
pub enum AcceptError {
    /// User provided invalid input data and has been denied.
    Denied,
    /// Internal IO error
    IoError(std::io::Error),
    /// Internal ssl error
    SslError(openssl::ssl::Error),
    /// The client has rejected our server certificate
    SslCertificateRejected,
    /// Unknown internal error
    CustomError(String),
    /// When the accept got aborted by somewhere else
    Aborted
}

pub type AbortFuture = Pin<Box<dyn Future<Output=()> + Send>>;
pub trait AcceptStep : Send {
    fn abort(self: Box<Self>, reason: &str) -> Option<AbortFuture>;
    fn advance(self: Box<Self>, cx: &mut Context<'_>) -> AcceptStepResult;
}

pub enum AcceptStepResult {
    Pending(Box<dyn AcceptStep>),
    NextStep(Box<dyn AcceptStep>),
    Errored(AcceptError),
    Finished(Box<dyn TransferRequest>)
}

impl Into<AcceptStepResult> for AcceptError {
    fn into(self) -> AcceptStepResult {
        AcceptStepResult::Errored(self)
    }
}

/// Try to identify the protocol the target client uses.
/// Supported protocols:
/// - HTTP
/// - SSL
/// - Raw (Raw file transfer)
pub struct AcceptStepProtocolIdentify<S: AsyncWrite + AsyncRead + Unpin + Send> {
    connection: Arc<RequestClientInfo>,
    socket: BufferStreamReader<S>,
}

impl<S: AsyncWrite + AsyncRead + Unpin + Send> AcceptStepProtocolIdentify<S> {
    pub fn new(connection: Arc<RequestClientInfo>, socket: BufferStreamReader<S>) -> Self {
        AcceptStepProtocolIdentify{
            connection,
            socket,
        }
    }
}

impl<S: AsyncWrite + AsyncRead + Unpin + Send + 'static> AcceptStep for AcceptStepProtocolIdentify<S> {
    fn abort(self: Box<Self>, _reason: &str) -> Option<AbortFuture> {
        /* We can only drop the client */
        None
    }

    fn advance(mut self: Box<Self>, cx: &mut Context<'_>) -> AcceptStepResult {
        match self.socket.poll_buffered(cx, 8) {
            Poll::Ready(Ok(_)) => {},
            Poll::Ready(Err(error)) =>  return AcceptStepResult::Errored(AcceptError::IoError(error)),
            Poll::Pending => return AcceptStepResult::Pending(self)
        }

        let buffer = self.socket.filled_buffer();
        if &buffer[0..3] == b"raw" {
            trace!(self.connection.logger, "Client using raw transfer protocol");
            return AcceptStepResult::NextStep(Box::new(AcceptStepRaw::new(self.connection, self.socket)));
        }

        if &buffer[0..3] == b"GET" || &buffer[0..4] == b"POST" || &buffer[0..4] == b"HEAD" || &buffer[0..4] == b"OPTIONS" {
            trace!(self.connection.logger, "Client using HTTP transfer protocol");

            return AcceptStepResult::NextStep(Box::new(AcceptStepHttp::new(self.connection, self.socket)));
        }

        if buffer[0] == 0x16 && (buffer[1] >= 1 && buffer[1] <= 3) && (buffer[2] >= 1 && buffer[2] <= 3) {
            trace!(self.connection.logger, "Client using ssl encryption.");
            return AcceptStepResult::NextStep(Box::new(AcceptStepSsl::new(self.connection, self.socket)));
        }

        return AcceptError::IoError(std::io::Error::new(ErrorKind::InvalidData, "unknown header bytes")).into();
    }
}

pub struct AcceptStepRaw<S: AsyncWrite + AsyncRead + Unpin + Send> {
    connection: Arc<RequestClientInfo>,
    socket: BufferStreamReader<S>,
}

impl<S: AsyncWrite + AsyncRead + Unpin + Send> AcceptStepRaw<S> {
    pub fn new(connection: Arc<RequestClientInfo>, socket: BufferStreamReader<S>) -> Self {
        AcceptStepRaw{
            connection,
            socket
        }
    }
}

impl<S: AsyncWrite + AsyncRead + Unpin + Send + 'static> AcceptStep for AcceptStepRaw<S> {
    fn abort(self: Box<Self>, _reason: &str) -> Option<AbortFuture> {
        /* We've no other chance than dropping the client */
        None
    }

    fn advance(mut self: Box<Self>, cx: &mut Context<'_>) -> AcceptStepResult {
        match self.socket.poll_buffered(cx, FT_KEY_SIZE) {
            Poll::Pending => AcceptStepResult::Pending(self),
            Poll::Ready(Ok(_)) => {
                let mut transfer_key = [0u8; FT_KEY_SIZE];
                transfer_key.copy_from_slice(&self.socket.filled_buffer()[0..FT_KEY_SIZE]);
                self.socket.consume_buffer(FT_KEY_SIZE);

                info!(self.connection.logger, "Received transfer key: {}", String::from_utf8_lossy(&transfer_key));
                AcceptStepResult::Finished(Box::new(TransferRequestRaw::new(transfer_key, self.connection, self.socket)))
            },
            Poll::Ready(Err(error)) => AcceptError::IoError(error).into()
        }
    }
}

enum SslSocket<S: AsyncWrite + AsyncRead + Unpin + Send> {
    Uninitialized(BufferStreamReader<S>),
    Initialized(tokio_openssl::SslStream<BufferStreamReader<S>>)
}

pub struct AcceptStepSsl<S: AsyncWrite + AsyncRead + Unpin + Send> {
    connection: Arc<RequestClientInfo>,
    socket: Option<SslSocket<S>>,
}

impl<S: AsyncWrite + AsyncRead + Unpin + Send> AcceptStepSsl<S> {
    pub fn new(connection: Arc<RequestClientInfo>, socket: BufferStreamReader<S>) -> Self {
        AcceptStepSsl{
            connection,
            socket: Some(SslSocket::Uninitialized(socket))
        }
    }
}

impl<S: AsyncWrite + AsyncRead + Unpin + Send + 'static> AcceptStep for AcceptStepSsl<S> {
    fn abort(self: Box<Self>, _reason: &str) -> Option<AbortFuture> {
        /* Handshake not yet finished. We've no other chance than just dropping the connection. */
        None
    }

    fn advance(mut self: Box<Self>, cx: &mut Context<'_>) -> AcceptStepResult {
        match self.socket.take().expect("missing underlying socket") {
            SslSocket::Uninitialized(socket) => {
                let binding_info = self.connection.binding.info();

                let key_file = match &binding_info.private_key {
                    Some(value) => value.as_str(),
                    None => return AcceptError::CustomError("missing private key config for binding".to_owned()).into()
                };

                let certificate_file = match &binding_info.certificate {
                    Some(value) => value.as_str(),
                    None => return AcceptError::CustomError("missing certificate config for binding".to_owned()).into()
                };

                let mut certificate_cache = CERTIFICATE_CACHE.lock().unwrap();
                let context = match certificate_cache.get_context(key_file, certificate_file) {
                    Some(context) => context,
                    None => return AcceptError::CustomError("could not load private key or certificate".to_owned()).into()
                };
                let ssl = Ssl::new(&context);
                drop(certificate_cache);

                let mut ssl = ssl.expect("failed to create ssl instance");
                ssl.set_accept_state();

                let stream = tokio_openssl::SslStream::new(ssl, socket).expect("failed to create ssl stream");
                self.socket = Some(SslSocket::Initialized(stream));

                cx.waker().wake_by_ref();
                AcceptStepResult::Pending(self)
            },
            SslSocket::Initialized(mut socket) => {
                if let Poll::Ready(result) = Pin::new(&mut socket).poll_do_handshake(cx) {
                    if let Err(error) = result {
                        if let Some(ssl_error) = error.ssl_error() {
                            if let Some(first_error) = ssl_error.errors().first() {
                                /* 336151574 := Certificate invalid (Chrome) 14094418 := Alert bad (Curl). 14094412 := Alert unknown Firefox */
                                if first_error.code() == 336151574 || first_error.code() == 14094418 || first_error.code() == 14094412 {
                                    return AcceptError::SslCertificateRejected.into();
                                }
                            }
                        }

                        AcceptError::SslError(error).into()
                    } else {
                        trace!(self.connection.logger, "Successfully executed ssl handshake");
                        AcceptStepResult::NextStep(Box::new(AcceptStepHttp::new(self.connection, BufferStreamReader::new(socket))))
                    }
                } else {
                    self.socket = Some(SslSocket::Initialized(socket));
                    AcceptStepResult::Pending(self)
                }
            }
        }
    }
}

/// Abort the connect attempt.
/// Awaiting full close and then drop the connection.
pub struct AcceptStepFutureClose {
    inner: AbortFuture
}

impl AcceptStepFutureClose {
    pub fn new(future: AbortFuture) -> Self {
        AcceptStepFutureClose{
            inner: future
        }
    }
}

impl AcceptStep for AcceptStepFutureClose {
    fn abort(self: Box<Self>, _reason: &str) -> Option<AbortFuture> {
        Some(self.inner)
    }

    fn advance(mut self: Box<Self>, cx: &mut Context<'_>) -> AcceptStepResult {
        let future = &mut self.inner;
        if let Poll::Ready(_) = Future::poll(future.as_mut(), cx) {
            AcceptError::Denied.into()
        } else {
            AcceptStepResult::Pending(self)
        }
    }
}

/// Accept the client via HTTPS
pub struct AcceptStepHttp<S: AsyncWrite + AsyncRead + Unpin + Send> {
    connection: Arc<RequestClientInfo>,
    socket: BufferStreamReader<S>,
    buffer_scan_index: usize,
    buffer_max_size: usize
}

impl<S: AsyncWrite + AsyncRead + Unpin + Send + 'static> AcceptStepHttp<S> {
    pub fn new(connection: Arc<RequestClientInfo>, socket: BufferStreamReader<S>) -> Self {
        AcceptStepHttp{
            connection,
            socket,
            buffer_scan_index: 0,
            buffer_max_size: 1024 * 8
        }
    }

    fn handle_http_data(self: Box<Self>, http_buffer: Vec<u8>) -> AcceptStepResult {
        let mut headers = [httparse::EMPTY_HEADER; 64];
        let mut request = httparse::Request::new(&mut headers);

        match request.parse(&http_buffer) {
            Ok(_) => {},
            Err(error) => {
                return AcceptStepResult::NextStep(Box::new(AcceptStepFutureClose::new(
                    self.abort_http((500, "failed to parse header"), Some(format!("Error:<br/>{}", error)))
                )));
            }
        };

        let url = url::Url::options()
            .base_url(Some(&url::Url::parse("file://temp_url/").unwrap()))
            .parse(request.path.unwrap_or("/"));

        let url = match url {
            Err(error) => {
                return AcceptStepResult::NextStep(Box::new(AcceptStepFutureClose::new(
                    self.abort_http((400, "Bad Request"), Some(format!("URL parse error: {}", error)))
                )));
            },
            Ok(url) => url
        };

        let method = match request.method.unwrap_or("UNKNOWN") {
            "GET" => HttpMethod::Get,
            "POST" => HttpMethod::Post,
            "OPTIONS" => HttpMethod::Options,
            _ => return AcceptStepResult::NextStep(Box::new(AcceptStepFutureClose::new(
                self.abort_http((400, "Bad Request"), Some("unsupported http method".to_string()))
            )))
        };

        let url_parameters = url.query_pairs();
        let url_parameters = url_parameters.into_owned().collect::<Vec<_>>();

        let http_headers = headers.iter()
            .filter(|header| !header.name.is_empty())
            .map(|header| (header.name.to_owned(), String::from_utf8_lossy(header.value).to_string()))
            .collect::<Vec<_>>();

        TransferRequestHttp::parse(self.connection, self.socket, TransferHttpHeader{
            method, url_parameters, http_headers
        })
    }

    fn abort_http(self: Box<Self>, status: (u16, &'static str), body: Option<String>) -> AbortFuture {
        let mut socket = self.socket;
        let logger = self.connection.logger.clone();
        Box::pin(async move {
            if let Err(error) = send_http_response(&mut socket, status, body, vec![]).await {
                match error.kind() {
                    ErrorKind::BrokenPipe |
                    ErrorKind::UnexpectedEof |
                    ErrorKind::NotConnected => {
                        return;
                    },
                    _ => {
                        warn!(logger, "Failed to send/generate http error response: {}", error);
                    }
                }
            }
        })
    }
}

impl<S: AsyncWrite + AsyncRead + Unpin + Send + 'static> AcceptStep for AcceptStepHttp<S> {
    fn abort(self: Box<Self>, reason: &str) -> Option<Pin<Box<dyn Future<Output=()> + Send>>> {
        Some(self.abort_http((503, "Request Aborted"), Some(reason.to_string())))
    }

    fn advance(mut self: Box<Self>, cx: &mut Context<'_>) -> AcceptStepResult {
        let target_bytes = self.buffer_max_size;
        if let Poll::Ready(Err(error)) = self.socket.poll_buffered(cx, target_bytes) {
            return AcceptError::IoError(error).into();
        }

        let buffer = self.socket.filled_buffer();
        assert!(buffer.len() >= self.buffer_scan_index);

        if buffer.len() < 4 {
            return AcceptStepResult::Pending(self);
        }

        if let Some(end_index) = buffer.windows(4).rposition(|window| window == b"\r\n\r\n") {
            let http_buffer = buffer[0..end_index].to_vec();
            self.socket.consume_buffer(end_index + 4);

            return self.handle_http_data(http_buffer);
        } else if buffer.len() == self.buffer_max_size {
            let body = format!("header exceeds max size of {} bytes", self.buffer_max_size);
            return AcceptStepResult::NextStep(Box::new(AcceptStepFutureClose::new(
                self.abort_http((413, "Entity Too Large"), Some(body))
            )));
        }

        /* Will not wrap since the buffer is at least 4 bytes long */
        self.buffer_scan_index = buffer.len() - 4;

        AcceptStepResult::Pending(self)
    }
}

pub struct AcceptingConnectionHandle {
    connection_info: Arc<RequestClientInfo>,
    current_step: Option<Box<dyn AcceptStep>>,
}

impl AcceptingConnectionHandle {
    pub fn new(connection_info: Arc<RequestClientInfo>, socket: StatisticsStream<TcpStream>) -> Self {
        AcceptingConnectionHandle {
            connection_info: connection_info.clone(),
            current_step: Some(Box::new(AcceptStepProtocolIdentify::new(connection_info, BufferStreamReader::new(socket))))
        }
    }

    pub fn abort(&mut self, reason: &str) -> Option<AbortFuture> {
        if let Some(step) = self.current_step.take() {
            step.abort(reason)
        } else {
            None
        }
    }
}

impl Future for AcceptingConnectionHandle {
    type Output = std::result::Result<Box<dyn TransferRequest>, AcceptError>;

    fn poll(mut self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Self::Output> {
        while let Some(step) = self.current_step.take() {
            match step.advance(cx) {
                AcceptStepResult::Pending(result) => {
                    self.current_step = Some(result);
                    return Poll::Pending;
                },
                AcceptStepResult::NextStep(result) => {
                    self.current_step = Some(result);
                },
                AcceptStepResult::Errored(error) => return Poll::Ready(Err(error)),
                AcceptStepResult::Finished(result) => return Poll::Ready(Ok(result))
            }
        }

        Poll::Ready(Err(AcceptError::Aborted))
    }
}