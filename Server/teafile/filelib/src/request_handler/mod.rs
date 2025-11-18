use crate::files::{FilePath, TargetInitializeError};
use std::collections::{BTreeMap, LinkedList};
use std::sync::{Arc};
use crate::request_handler::stats::{PendingQuota, RunningTransferSummary, RunningTransfer};
use parking_lot::{ RwLock, Mutex };
use std::path::PathBuf;
use tokio::io::ErrorKind;
use slog::*;
use std::time::Instant;
use crate::request::{TransferRequest, TransferExecuteError};
use crate::events::{EventRegistry, EventEmitter};
use std::ops::{DerefMut};
use std::fmt::{ Formatter, Display };
use futures::task::Context;
use tokio::macros::support::{Poll, Pin};
use tokio::time::{Interval, Duration};
use std::sync::atomic::{AtomicBool, Ordering};
use rand::Rng;
use rand::distributions::Alphanumeric;
use crate::request_handler::server::{FileServerInstance, FileVirtualServerEvents};

mod stats;
pub mod transfer;
pub mod server;

pub const FT_KEY_SIZE: usize = 32;

#[derive(Debug, Ord, PartialOrd, Eq, PartialEq, Copy, Clone)]
pub enum TransferDirection {
    Upload,
    Download
}

impl Display for TransferDirection {
    fn fmt(&self, f: &mut Formatter<'_>) -> std::fmt::Result {
        match self {
            TransferDirection::Upload => write!(f, "upload"),
            TransferDirection::Download => write!(f, "download"),
        }
    }
}

pub struct FileTransferInfo {
    handler_unique_id: String,
    server_unique_id: String,

    client_unique_id: String,
    /// Current client id (unused by the file transfer server).
    /// Only used by the TeaSpeak server to send notifies to the client.
    client_id: u64,

    /// The transfer key the client should send.
    /// Using base64 or any UTF-8 compatible binary format is required
    /// to allow all methods to work (e.g. HTTP(S)).
    transfer_key: [u8; FT_KEY_SIZE],

    /// The target absolute file path.
    absolute_file_path: PathBuf,
    file_path: FilePath,
    /// In upload mode the file size must be given.
    /// For download the file size must be queried before registering the transfer.
    file_size: u64,
    direction: TransferDirection,

    handled: AtomicBool,

    /// The user specific network bandwidth limit.
    /// If `None` no limit will be set.
    max_bandwidth: Option<u64>,

    /* TODO: Move this to somewhere else since `FileTransferInfo` could be hold forever by some third party. */
    /// The registered quota handle.
    /// Must be later cloned for the running transfer in order to
    /// increase the realized quota.
    registered_quota: Option<Arc<PendingQuota>>,
    registered_transfer: Option<RunningTransfer>,

    register_timestamp: Instant,

    client_transfer_id: u16,
    server_transfer_id: u16,
}

impl FileTransferInfo {
    pub fn handler_unique_id(&self) -> &str {
        &self.handler_unique_id
    }

    pub fn server_unique_id(&self) -> &str {
        &self.server_unique_id
    }

    pub fn client_unique_id(&self) -> &str {
        &self.client_unique_id
    }

    pub fn client_id(&self) -> u64 {
        self.client_id
    }

    pub fn transfer_key(&self) -> &[u8] {
        &self.transfer_key
    }

    pub fn absolute_file(&self) -> &PathBuf {
        &self.absolute_file_path
    }

    pub fn file_size(&self) -> u64 {
        self.file_size
    }

    pub fn direction(&self) -> TransferDirection {
        self.direction
    }

    pub fn client_transfer_id(&self) -> u16 {
        self.client_transfer_id
    }

    pub fn server_transfer_id(&self) -> u16 {
        self.server_transfer_id
    }

    /// Returns `true` if the transfer hasn't been handled yet and is still pending.
    pub fn is_pending(&self) -> bool {
        !self.handled.load(Ordering::Relaxed)
    }
}

pub enum TransferHandlerEvents { }

pub struct TransferHandler {
    logger: slog::Logger,
    events: EventRegistry<TransferHandlerEvents>,
    event_emitter: EventEmitter<TransferHandlerEvents>,
    // Note: This variable should be hold while the whole transfer register process
    //       happens (to ensure quota and concurrent transfer limits)
    /* TODO: Cleanup for timeout */
    /// (server_transfer_id_index, pending_transfers)
    pending_transfers: Mutex<(u16, LinkedList<Arc<FileTransferInfo>>)>,
    server_instances: BTreeMap<String, Arc<RwLock<FileServerInstance>>>,

    transfer_tick: Mutex<Interval>,
}

#[derive(Copy, Clone, Debug)]
pub enum TransferRequestError {
    InvalidTransferKey,
    InstanceWentOffline,
    ServerWentOffline,
    NotImplemented,

    ExecuteError(TransferExecuteError),

    InternalIoError,

    FileInvalidPath,
    FileNotFound,
    FilePermissionsDenied,
    FileIoError,
    FileInUse
}

impl From<TargetInitializeError> for TransferRequestError {
    fn from(error: TargetInitializeError) -> Self {
        match error {
            TargetInitializeError::FileNotFound => Self::FileNotFound,
            TargetInitializeError::FileInUse => Self::FileInUse,
            TargetInitializeError::InvalidPath => Self::FileInvalidPath,
            TargetInitializeError::FileNotAccessible => Self::FilePermissionsDenied,
            TargetInitializeError::InternalIoError => Self::InternalIoError,
        }
    }
}

impl TransferHandler {
    pub fn new(logger: slog::Logger) -> Self {
        let (events, event_emitter) = EventRegistry::new();

        TransferHandler{
            logger,

            events,
            event_emitter,

            pending_transfers: Mutex::new((0, Default::default())),
            server_instances: BTreeMap::new(),

            transfer_tick: Mutex::new(tokio::time::interval(Duration::from_secs(1))),
        }
    }

    /// Register a new server instance.
    /// `initial_quota` contains (upload quota, download quota)
    pub fn create_server_instance(&mut self, unique_id: String, initial_quota: (u64, u64)) -> Arc<RwLock<FileServerInstance>> {
        if let Some(server) = self.server_instances.get(&unique_id) {
            return server.clone();
        }

        let instance = FileServerInstance::new(self.logger.new(o!("instance" => unique_id.clone())), unique_id.clone(), initial_quota);
        let instance = Arc::new(RwLock::new(instance));
        self.server_instances.insert(unique_id, instance.clone());
        instance
    }

    pub fn server_instances(&self) -> &BTreeMap<String, Arc<RwLock<FileServerInstance>>> {
        &self.server_instances
    }

    pub fn remove_server_instance(&mut self, unique_id: &str) {
        if let Some(instance) = self.server_instances.remove(unique_id) {
            instance.write().shutdown();
        }
    }

    pub fn create_transfer(&self) -> TransferBuilder {
        TransferBuilder::new(self)
    }

    pub fn handle_transfer_request(&self, request: &mut dyn TransferRequest) -> std::result::Result<(), TransferRequestError> {
        /* We could easily remove the pending transfer since it's still registered as quota and within the overall transfer count */
        let transfer = {
            let mut pending_transfers = self.pending_transfers.lock();
            let (_, pending_transfers) = pending_transfers.deref_mut();
            let result = pending_transfers.drain_filter(|entry| entry.transfer_key.eq(request.transfer_key())).next();
            result
        };

        let transfer = match transfer {
            Some(transfer) => transfer,
            None => return Err(TransferRequestError::InvalidTransferKey)
        };
        transfer.handled.store(true, Ordering::Relaxed);

        let server_virtual = {
            let server_instance = match self.server_instances.get(&transfer.handler_unique_id) {
                Some(instance) => instance.clone(),
                None => return Err(TransferRequestError::InstanceWentOffline)
            };
            let server_instance = server_instance.read();

            match server_instance.find_virtual_server(&transfer.server_unique_id) {
                Some(instance) => instance.clone(),
                None => return Err(TransferRequestError::ServerWentOffline)
            }
        };

        let mut server_virtual = server_virtual.write();
        server_virtual.handle_request(request, &transfer)
    }

    fn handle_transfer_timeout(&self, transfer: Arc<FileTransferInfo>) {
        transfer.handled.store(true, Ordering::Relaxed);

        let server_virtual = {
            let server_instance = match self.server_instances.get(&transfer.handler_unique_id) {
                Some(instance) => instance.clone(),
                None => return,
            };
            let server_instance = server_instance.read();

            match server_instance.find_virtual_server(&transfer.server_unique_id) {
                Some(instance) => instance.clone(),
                None => return,
            }
        };

        let mut server_virtual = server_virtual.write();
        debug!(server_virtual.logger(), "Transfer {} timed out.", transfer.server_transfer_id);
        server_virtual.event_emitter.fire_later(None, FileVirtualServerEvents::FileTransferTimeout(transfer));
    }

    pub fn poll_pending_transfers(self: Pin<&Self>, cx: &mut Context<'_>) -> Poll<()> {
        while let Poll::Ready(_) = self.transfer_tick.lock().poll_tick(cx) {
            let removed_transfers = {
                let mut transfers = self.pending_transfers.lock();
                let (_, transfers) = transfers.deref_mut();

                transfers
                    .drain_filter(|transfer| transfer.register_timestamp.elapsed().as_secs() >= 10)
                    .collect::<Vec<_>>()
            };

            for transfer in removed_transfers {
                self.handle_transfer_timeout(transfer);
            }
        }

        Poll::Pending
    }
}

pub struct TransferBuilder<'a> {
    transfer_handler: &'a TransferHandler,
    transfer_info: FileTransferInfo,

    file: Option<FilePath>,

    /// Override an existing target file.
    /// This is only from importance on file upload.
    override_exiting: bool,

    /// The user specific file quota limit.
    /// Since we have no overview of the total user quota limit,
    /// we can only limit the quota of the pending transfers.
    max_pending_client_quota: Option<u64>,
    max_pending_server_quota: Option<u64>,

    /// Max concurrent transfers which could be run by the client.
    max_concurrent_client_transfers: Option<usize>,
    /// Max concurrent transfers which could be run by the server.
    max_concurrent_server_transfers: Option<usize>,
}

#[derive(Debug, Copy, Clone, Eq, PartialEq)]
pub enum TransferRegisterError {
    /// The file path hasn't been set via the builder
    MissingFilePath,
    /// The server info hasn't been set
    MissingServerInfo,
    /// The client info hasn't been set
    MissingClientInfo,

    /// The target server could not be found
    InvalidServerInstance,
    InvalidVirtualServer,
    /// The target file path could not be found or is invalid
    InvalidFilePath,

    TargetFileDoesNotExists,
    TargetFileIsNotAFile,
    TargetFileQueryFailed,

    QuotaClientExceeded,
    QuotaVirtualServerExceeded,

    ConcurrentLimitClientReached,
    ConcurrentLimitVirtualServerReached,
    ConcurrentLimitServerReached,
}

impl<'a> TransferBuilder<'a> {
    fn new(transfer_handler: &'a TransferHandler) -> Self {
        let mut transfer_key = unsafe { std::mem::MaybeUninit::<[u8; FT_KEY_SIZE]>::uninit().assume_init() };

        {
            transfer_key[0] = 'r' as u8;
            transfer_key[1] = 'a' as u8;
            transfer_key[2] = 'w' as u8;
            let mut random = rand::thread_rng().sample_iter(&Alphanumeric);
            for index in 3..transfer_key.len() {
                transfer_key[index] = random.next().expect("failed to generate random character")
            }
        }
        transfer_key.copy_from_slice(b"rawITmVI1QB4tPZPQrkb7ggAS2CxMckU");

        TransferBuilder{
            transfer_handler,
            transfer_info: FileTransferInfo{
                handler_unique_id: String::new(),
                server_unique_id: String::new(),

                client_unique_id: String::new(),
                client_id: 0,

                /* spaces */
                transfer_key,

                absolute_file_path: PathBuf::new(),
                file_path: FilePath::Icon("".to_string()),
                file_size: 0,
                direction: TransferDirection::Upload,

                max_bandwidth: None,

                registered_quota: None,
                registered_transfer: None,

                register_timestamp: Instant::now(),

                server_transfer_id: 0,
                client_transfer_id: 0,

                handled: AtomicBool::new(false)
            },

            max_pending_server_quota: None,
            max_pending_client_quota: None,

            max_concurrent_server_transfers: None,
            max_concurrent_client_transfers: None,

            override_exiting: false,
            file: None
        }
    }

    pub fn server(mut self, handler_unique_id: String, server_unique_id: String) -> Self {
        self.transfer_info.handler_unique_id = handler_unique_id;
        self.transfer_info.server_unique_id = server_unique_id;
        self
    }

    pub fn client(mut self, client_id: u64, client_unique_id: String, client_transfer_id: u16) -> Self {
        self.transfer_info.client_id = client_id;
        self.transfer_info.client_unique_id = client_unique_id;
        self.transfer_info.client_transfer_id = client_transfer_id;
        self
    }

    pub fn file(mut self, file_path: FilePath, direction: TransferDirection, file_size: u64) -> Self {
        self.file = Some(file_path);
        self.transfer_info.direction = direction;
        self.transfer_info.file_size = file_size;
        self
    }

    pub fn limit_bandwidth(mut self, bandwidth: Option<u64>) -> Self {
        self.transfer_info.max_bandwidth = bandwidth;
        self
    }

    pub fn limit_pending_quota(mut self, server: Option<u64>, client: Option<u64>) -> Self {
        self.max_pending_client_quota = client;
        self.max_pending_server_quota = server;
        self
    }

    pub fn limit_concurrent_transfers(mut self, server: Option<usize>, client: Option<usize>) -> Self {
        self.max_concurrent_client_transfers = client;
        self.max_concurrent_server_transfers = server;
        self
    }

    pub fn register(mut self) -> std::result::Result<Arc<FileTransferInfo>, TransferRegisterError> {
        let file_path = match self.file {
            Some(file) => file,
            None => return Err(TransferRegisterError::MissingFilePath),
        };
        self.transfer_info.file_path = file_path.clone();

        let server_instance = match self.transfer_handler.server_instances.get(&self.transfer_info.handler_unique_id) {
            Some(server) => server.clone(),
            None => return Err(TransferRegisterError::InvalidServerInstance),
        };
        let server_instance = server_instance.read();

        let virtual_server = match server_instance.find_virtual_server(&self.transfer_info.server_unique_id) {
            Some(server) => server.clone(),
            None => return Err(TransferRegisterError::InvalidVirtualServer),
        };
        let virtual_server = virtual_server.read();

        let virtual_file_system = virtual_server.file_system().read();
        let absolute_path = match virtual_file_system.generate_absolut_path(&file_path) {
            Some(path) => path,
            None => return Err(TransferRegisterError::InvalidFilePath),
        };

        if self.transfer_info.direction == TransferDirection::Download {
            let file_info = match std::fs::metadata(&absolute_path) {
                Ok(info) => info,
                Err(error) => {
                    return Err(match error.kind() {
                        ErrorKind::NotFound |
                        ErrorKind::PermissionDenied => TransferRegisterError::TargetFileDoesNotExists,
                        error => {
                            warn!(self.transfer_handler.logger, "Failed to query file info for {}: {:?}", absolute_path.to_string_lossy(), error);
                            TransferRegisterError::TargetFileQueryFailed
                        }
                    });
                },
            };

            if !file_info.is_file() {
                return Err(TransferRegisterError::TargetFileIsNotAFile);
            }

            self.transfer_info.file_size = file_info.len();
        }

        /* From now on everything will be blocking and in sync */
        let mut pending_transfers = self.transfer_handler.pending_transfers.lock();
        let (server_transfer_id_index, pending_transfers) = pending_transfers.deref_mut();

        {
            let client_transfer_count ={
                let mut client_transfers = virtual_server.client_transfer_count.lock();
                if let Some(client_transfer) = client_transfers.get_mut(&self.transfer_info.client_unique_id) {
                    if let Some(client_transfer) = client_transfer.upgrade() {
                        client_transfer
                    } else {
                        let summary = Arc::new(RunningTransferSummary::new(Some(virtual_server.server_transfer_count.clone())));
                        *client_transfer = Arc::downgrade(&summary);
                        summary
                    }
                } else {
                    let summary = Arc::new(RunningTransferSummary::new(Some(virtual_server.server_transfer_count.clone())));
                    client_transfers.insert(self.transfer_info.client_unique_id.clone(), Arc::downgrade(&summary));
                    summary
                }
            };

            /* Register the new transfer. If we exceed the max concurrent transfers we'll unregister it again by dropping this reference */
            self.transfer_info.registered_transfer = Some(RunningTransfer::new(client_transfer_count.clone()));

            if let Some(max_transfers) = &self.max_concurrent_client_transfers {
                if client_transfer_count.running_transfers() > *max_transfers {
                    return Err(TransferRegisterError::ConcurrentLimitClientReached);
                }
            }

            if let Some(max_transfers) = &self.max_concurrent_server_transfers {
                if virtual_server.server_transfer_count.running_transfers() > *max_transfers {
                    return Err(TransferRegisterError::ConcurrentLimitVirtualServerReached);
                }
            }
        }

        {
            let mut quota = virtual_server.quota.lock();
            let client_quota = quota.client_quota(&self.transfer_info.client_unique_id, self.transfer_info.direction);

            /* Register the file quota. If we exceed the quota the registered bytes will be automatically dropped */
            self.transfer_info.registered_quota = Some(Arc::new(PendingQuota::new(self.transfer_info.file_size, client_quota.clone())));

            if let Some(max_quota) = &self.max_pending_client_quota {
                let (target, current) = client_quota.info();

                if target - current > *max_quota {
                    return Err(TransferRegisterError::QuotaClientExceeded);
                }
            }

            if let Some(max_quota) = &self.max_pending_server_quota {
                let server_quota = quota.server_quota(self.transfer_info.direction);
                if server_quota.info().0 > *max_quota {
                    return Err(TransferRegisterError::QuotaVirtualServerExceeded);
                }
            }
        }

        if *server_transfer_id_index == 0 {
            *server_transfer_id_index = *server_transfer_id_index + 1;
        }
        self.transfer_info.server_transfer_id = *server_transfer_id_index;
        *server_transfer_id_index = server_transfer_id_index.wrapping_add(1);

        self.transfer_info.register_timestamp = Instant::now();
        let transfer_info = Arc::new(self.transfer_info);
        pending_transfers.push_back(transfer_info.clone());

        debug!(self.transfer_handler.logger, "Registered new file transfer for {} ({}). Transfer id: {}", absolute_path.to_string_lossy(), transfer_info.direction, transfer_info.server_transfer_id);
        virtual_server.event_emitter.clone().fire_later(None, FileVirtualServerEvents::FileTransferRegistered(transfer_info.clone()));
        Ok(transfer_info)
    }
}