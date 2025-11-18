use std::net::SocketAddr;
use std::collections::{VecDeque, LinkedList};
use std::result::Result;
use std::str::FromStr;
use std::sync::{Arc, Weak};
use std::future::Future;
use std::pin::Pin;
use std::ops::{Deref, DerefMut};

use tokio::net::{TcpListener, TcpStream};
use tokio::io::ErrorKind;
use tokio::time::{Duration, Sleep};
use tokio::io::{  AsyncWrite, AsyncRead };
use tokio::macros::support::Poll;
use tokio::sync::mpsc;
use tokio::sync::mpsc::error::TrySendError;

use retain_mut::RetainMut;
use futures::future::AbortHandle;
use futures::{FutureExt, StreamExt, Stream};
use futures::task::{Waker, Context};

use slog::*;
use parking_lot::Mutex;

use self::accept::{AcceptingConnectionHandle, AcceptError};
use self::util::{set_keepalive, StreamStatistics, StatisticsStream};
use crate::config::ServerBinding;
use crate::request_handler::{FileTransferInfo, FT_KEY_SIZE};

pub(crate) mod accept;
pub mod util;
mod request;

#[derive(Copy, Clone, Debug)]
pub enum TransferExecuteError {
    InternalError,
    NotSupported
}

pub trait TransferRequest : Send {
    fn transfer_key(&self) -> &[u8; FT_KEY_SIZE];

    fn execute_download(&mut self, request_info: Arc<FileTransferInfo>) -> Result<Box<dyn NetworkTransferDownloadSink>, TransferExecuteError>;
    fn execute_upload(&mut self, request_info: Arc<FileTransferInfo>) -> Result<Box<dyn NetworkTransferUploadSink>, TransferExecuteError>;

    /// Close the request.
    /// If no upload/download has been executed the connection will be closed.
    // Note for later:
    // This might be the point to fall back into an accept state in order to reuse the connection.
    // This could be handy for HTTP connections.
    fn close_request(self: Box<Self>, reason: &str) -> Option<Pin<Box<dyn Future<Output=()> + Send>>>;
}

/// A network sink to transfer data to/from
pub trait NetworkTransferSink : Send + Unpin {
    fn network_statistics(&self) -> Arc<StreamStatistics>;

    /// Close the connection.
    /// If you want to flush, poll the returned future.
    /// If no future is returned, the connection has been closed.
    fn close_connection(self: Box<Self>, reason: Option<&str>) -> Option<Box<dyn Future<Output=()> + Send>>;
}

pub trait NetworkTransferDownloadSink: NetworkTransferSink + AsyncWrite { }
pub trait NetworkTransferUploadSink: NetworkTransferSink + AsyncRead { }

pub struct RequestClientInfo {
    logger: slog::Logger,
    binding: Arc<ActiveServerBinding>,
    peer_address: SocketAddr,
    network_statistics: Arc<StreamStatistics>
}

impl RequestClientInfo {
    pub fn logger(&self) -> &slog::Logger {
        &self.logger
    }

    pub fn peer_address(&self) -> &SocketAddr {
        &self.peer_address
    }

    pub fn network_statistics(&self) -> &Arc<StreamStatistics> {
        &self.network_statistics
    }
}

struct RequestClient {
    state: RequestClientSocket,
    info: Arc<RequestClientInfo>,
    abort_handle: Option<AbortHandle>,
    waker: Option<Waker>,
}

impl RequestClient {
    fn disconnect(&mut self, reason: &str) {
        match std::mem::replace(&mut self.state, RequestClientSocket::Disconnected) {
            RequestClientSocket::Accepting(mut handle) => {
                if let Some(future) = handle.abort(reason) {
                    self.state = RequestClientSocket::Disconnecting(future, Box::pin(tokio::time::sleep(Duration::from_secs(5))));
                }
            },
            RequestClientSocket::Disconnecting(future, instant) => {
                self.state = RequestClientSocket::Disconnecting(future, instant);
            },
            RequestClientSocket::Disconnected | RequestClientSocket::Transferring => {},
        }

        if let Some(waker) = &self.waker {
            waker.wake_by_ref();
        }
    }

    /* Only for shutdowns. */
    fn disconnect_into_future(&mut self, reason: &str) -> Option<Pin<Box<dyn Future<Output=()> + Send>>>{
        self.disconnect(reason);

        match std::mem::replace(&mut self.state, RequestClientSocket::Disconnected) {
            RequestClientSocket::Disconnecting(future, _) => Some(future),
            _ => None
        }
    }
}

impl Drop for RequestClient {
    fn drop(&mut self) {
        trace!(self.info.logger, "Accepting connection deallocated at {:?}.", self as *const _);
    }
}

enum RequestClientSocket {
    Accepting(AcceptingConnectionHandle),
    Transferring,
    Disconnecting(Pin<Box<dyn Future<Output=()> + Send>>, Pin<Box<Sleep>>),
    Disconnected,
}

pub struct ActiveServerBinding {
    info: ServerBinding,
    address: SocketAddr,
    shutdown_handle: Mutex<Option<AbortHandle>>
}

impl ActiveServerBinding {
    pub fn info(&self) -> &ServerBinding {
        &self.info
    }
}

struct RequestServerClients {
    logger: slog::Logger,
    ref_self: Weak<Mutex<RequestServerClients>>,
    network_statistics: Arc<StreamStatistics>,
    client_connections: LinkedList<Arc<Mutex<RequestClient>>>,
    request_upstream: mpsc::Sender<Box<dyn TransferRequest>>
}

impl RequestServerClients {
    fn handle_new_client(&mut self, mut socket: TcpStream, address: SocketAddr, binding: Arc<ActiveServerBinding>) {
        let _ = socket.set_nodelay(true);
        if let Err(error) = set_keepalive(&mut socket, Some(Duration::from_secs(120)), Some(Duration::from_secs(5)), Some(9)) {
            warn!(self.logger, "Failed to enable the tcp keep alive for {}. Dropping connection: {}", &address, error);
            return;
        }

        let socket = StatisticsStream::new_with_statistics(
            socket,
            Arc::new(StreamStatistics::new_parent(&self.network_statistics))
        );

        let socket_statistics = socket.statistics().clone();
        let client_info = Arc::new(RequestClientInfo {
            logger: self.logger.new(o!("client" => address.to_string())),
            peer_address: address,
            binding: binding.clone(),
            network_statistics: socket_statistics.clone()
        });

        let client = RequestClient{
            state: RequestClientSocket::Accepting(AcceptingConnectionHandle::new(client_info.clone(), socket)),
            info: client_info.clone(),
            abort_handle: None,
            waker: None
        };

        let client = Arc::new(Mutex::new(client));
        /* Adding 8 due to the Mutex, just to look good */
        trace!(client_info.logger, "Client allocated at {:?}", (client.deref() as *const _ as *const u8).wrapping_add(8));
        debug!(client_info.logger, "Client received from network binding {}", &binding.address);

        let (future, abort) = futures::future::abortable(
            Self::execute_client(self.ref_self.upgrade().expect("missing self reference"), client.clone())
        );
        client.lock().abort_handle = Some(abort);

        self.client_connections.push_back(client);
        tokio::spawn(future);
    }

    fn execute_client(server: Arc<Mutex<RequestServerClients>>, client: Arc<Mutex<RequestClient>>) -> Pin<Box<dyn Future<Output=()> + Send>> {
        Box::pin(futures::future::poll_fn({
            let server = server.clone();
            let client = client.clone();
            move |cx| {
                let mut locked_client = client.lock();
                locked_client.waker = Some(cx.waker().clone());

                match &mut locked_client.state {
                    RequestClientSocket::Accepting(accept_handle) => {
                        let accept_result = match accept_handle.poll_unpin(cx) {
                            Poll::Pending => return Poll::Pending,
                            Poll::Ready(result) => result
                        };

                        if let Some(request) = Self::handle_client_request(locked_client.deref_mut(), accept_result) {
                            locked_client.state = RequestClientSocket::Transferring;
                            drop(locked_client);

                            let enqueue_result = server.lock().request_upstream.try_send(request);
                            return match enqueue_result {
                                Ok(_) => {
                                    /* We're done with the client */
                                    Poll::Ready(())
                                },
                                Err(request) => {
                                    let mut locked_client = client.lock();
                                    let request = match request {
                                        TrySendError::Closed(request) => {
                                            warn!(locked_client.info.logger, "Failed to enqueue request (Queue closed)");
                                            request
                                        },
                                        TrySendError::Full(request) => {
                                            warn!(locked_client.info.logger, "Failed to enqueue request (Queue full)");
                                            request
                                        },
                                    };

                                    if let Some(close_future) = request.close_request("failed to enqueue request") {
                                        locked_client.state = RequestClientSocket::Disconnecting(close_future, Box::pin(tokio::time::sleep(Duration::from_secs(5))));
                                    } else {
                                        locked_client.state = RequestClientSocket::Disconnected;
                                    }

                                    cx.waker().wake_by_ref();
                                    Poll::Pending
                                }
                            }
                        } else {
                            locked_client.disconnect("failed to accept connection");
                        }
                        Poll::Pending
                    },
                    RequestClientSocket::Disconnecting(future, timeout) => {
                        if matches!(future.as_mut().poll(cx), Poll::Ready(_)) {
                            return Poll::Ready(());
                        }

                        if matches!(timeout.as_mut().poll_unpin(cx), Poll::Ready(_)) {
                            debug!(locked_client.info.logger, "Failed to successfully disconnect the client. Force closing connection.");
                            return Poll::Ready(());
                        }

                        return Poll::Pending;
                    },
                    RequestClientSocket::Disconnected => Poll::Ready(()),
                    RequestClientSocket::Transferring => Poll::Ready(())
                }
            }
        }).then(|_| async move {
            {
                let mut clients = server.lock();
                clients.client_connections.drain_filter(|entry| {
                    (*entry).deref() as *const _ == client.deref() as *const _
                }).count();
            }

            let old_state = std::mem::replace(&mut client.lock().state, RequestClientSocket::Disconnected);
            if matches!(&old_state, &RequestClientSocket::Transferring) {
                debug!(client.lock().info.logger, "Accepting connection passed to request handler.");
            } else {
                debug!(client.lock().info.logger, "Accepting connection disconnected.");
            }
        }))
    }

    fn handle_client_request(client: &mut RequestClient, result: Result<Box<dyn TransferRequest>, AcceptError>) -> Option<Box<dyn TransferRequest>> {
        if let Err(error) = result {
            match error {
                AcceptError::Denied => {
                    trace!(client.info.logger, "Client accept denied. Closing connection.");
                },
                AcceptError::Aborted => {
                    trace!(client.info.logger, "Client accept aborted. Closing connection.");
                },
                AcceptError::SslCertificateRejected => {
                    warn!(client.info.logger, "Client signalled ssl certificate rejection. Connection closed.");
                },
                AcceptError::IoError(error) => {
                    match error.kind() {
                        ErrorKind::UnexpectedEof => {
                            trace!(client.info.logger, "Remote closed the connection. Removing client.");
                        },
                        _ => {
                            warn!(client.info.logger, "Received io error: {}. Closing connection.", error);
                        }
                    }
                },
                AcceptError::CustomError(error) => {
                    warn!(client.info.logger, "Received custom error: {}. Closing connection.", error);
                },
                AcceptError::SslError(error) => {
                    warn!(client.info.logger, "Received ssl error: {}. Closing connection.", error);
                }
            }

            return None;
        }

        Some(result.expect("expected a result"))
    }
}

pub struct RequestServer {
    logger: slog::Logger,

    active_bindings: VecDeque<Arc<ActiveServerBinding>>,
    connections: Arc<Mutex<RequestServerClients>>,

    request_downstream: mpsc::Receiver<Box<dyn TransferRequest>>
}

impl RequestServer {
    pub fn new(logger: slog::Logger) -> Self {
        let (request_upstream, request_downstream) = tokio::sync::mpsc::channel(32);

        let client_internal = Arc::new(Mutex::new(RequestServerClients {
            logger: logger.clone(),
            ref_self: Default::default(),
            network_statistics: Arc::new(StreamStatistics::new()),
            client_connections: LinkedList::new(),
            request_upstream,
        }));
        client_internal.lock().ref_self = Arc::downgrade(&client_internal);

        RequestServer {
            logger,
            active_bindings: VecDeque::with_capacity(4),
            connections: client_internal,
            request_downstream
        }
    }

    pub async fn add_binding(&mut self, binding: &ServerBinding) -> std::io::Result<()> {
        let address = match SocketAddr::from_str(&binding.address) {
            Ok(address) => address,
            Err(_) => return Err(std::io::Error::new(ErrorKind::InvalidData, "failed to parse address"))
        };

        let binding = Arc::new(ActiveServerBinding {
            info: binding.clone(),
            address,
            shutdown_handle: Mutex::new(None)
        });

        let listener = TcpListener::bind(&binding.address).await?;

        let logger = self.logger.clone();
        let binding_ = binding.clone();
        let connections = self.connections.clone();
        let (task, abort) = futures::future::abortable(async move {
            loop {
                match listener.accept().await {
                    Ok((socket, address)) => {
                        connections.lock().handle_new_client(socket, address, binding_.clone());
                    },
                    Err(error) => {
                        warn!(logger, "Failed to accept new client on {}: {}", &binding_.address, error);
                    }
                }
            }
        });
        tokio::spawn(task);

        *binding.shutdown_handle.lock() = Some(abort);
        self.active_bindings.push_back(binding);

        return Ok(());
    }

    pub fn active_bindings(&self) -> Vec<SocketAddr> {
        self.active_bindings.iter().map(|binding| binding.address.clone()).collect::<Vec<_>>()
    }

    pub fn shutdown_binding(&mut self, address: &SocketAddr) {
        self.active_bindings.retain_mut(|binding| {
            if binding.address == *address {
                let mut abort = binding.shutdown_handle.lock();
                if let Some(abort) = abort.take() {
                    abort.abort();
                }

                false
            } else {
                true
            }
        });
    }

    /// Shut down the whole file server.
    /// Note: This should be called being within the tokio runtime!
    pub fn shutdown(&mut self) -> Pin<Box<dyn Future<Output=()> + Send>> {
        while let Some(binding) = self.active_bindings.front() {
            let address = binding.address.clone();
            self.shutdown_binding(&address);
        }

        let mut internals = self.connections.lock();
        let mut shutdown_futures = futures::stream::FuturesUnordered::new();

        for entry in internals.client_connections.iter() {
            let mut entry = entry.lock();
            if let Some(abort) = &entry.abort_handle {
                abort.abort();
            }

            if let Some(future) = entry.disconnect_into_future("server shutdown") {
                shutdown_futures.push(future);
            }
        }

        internals.client_connections.clear();

        Box::pin(async move {
            while let Some(_) = shutdown_futures.next().await {}
        })
    }
}

impl Stream for RequestServer {
    type Item = Box<dyn TransferRequest>;

    fn poll_next(mut self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Option<Self::Item>> {
        self.request_downstream.poll_recv(cx)
    }
}

impl Drop for RequestServer {
    fn drop(&mut self) {
        let promise = self.shutdown();

        /* Just in case we need to disconnect somebody */
        if let Ok(handle) = tokio::runtime::Handle::try_current() {
            handle.spawn(promise);
        }
    }
}