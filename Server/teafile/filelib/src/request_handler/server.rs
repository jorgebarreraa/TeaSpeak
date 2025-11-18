use std::path::PathBuf;
use crate::events::{EventRegistry, EventEmitter};
use crate::request_handler::stats::{PendingQuotaSummary, RunningTransferSummary};
use std::sync::{Arc, Weak};
use std::collections::{BTreeMap, HashMap, VecDeque};
use parking_lot::{RwLock, Mutex};
use crate::files::VirtualFileSystem;
use tokio::io::ErrorKind;
use tokio::task::JoinHandle;
use crate::request_handler::{FileTransferInfo, TransferRequestError, TransferDirection};
use crate::request_handler::transfer::{RunningDownloadFileTransfer, RunningFileTransfer, FileTransferResult};
use crate::request::TransferRequest;
use futures::FutureExt;
use std::ops::DerefMut;
use slog::*;
use futures::future::AbortHandle;
use futures::task::Poll;
use std::borrow::Cow;

pub enum FileServerInstanceEvents {
    VirtualServerCreated(String),
    VirtualServerDeleted(String)
}

/// Info about a server instance
pub struct FileServerInstance {
    logger: slog::Logger,
    file_path: PathBuf,
    handler_unique_id: String,

    events: EventRegistry<FileServerInstanceEvents>,
    event_emitter: EventEmitter<FileServerInstanceEvents>,

    upload_quota: Arc<PendingQuotaSummary>,
    download_quota: Arc<PendingQuotaSummary>,
    transfer_count: Arc<RunningTransferSummary>,

    virtual_server: BTreeMap<String, Arc<RwLock<FileVirtualServer>>>,
}

impl FileServerInstance {
    pub fn new(logger: slog::Logger, unique_id: String, initial_quota: (u64, u64)) -> Self {
        let (events, event_emitter) = EventRegistry::new();
        FileServerInstance{
            logger,
            file_path: PathBuf::from(format!("server_instance_{}", unique_id)),
            handler_unique_id: unique_id.clone(),

            events,
            event_emitter,

            upload_quota: Arc::new(PendingQuotaSummary::new(None, (initial_quota.0, initial_quota.0))),
            download_quota: Arc::new(PendingQuotaSummary::new(None, (initial_quota.1, initial_quota.1))),
            transfer_count: Arc::new(RunningTransferSummary::new(None)),

            virtual_server: BTreeMap::new()
        }
    }

    pub fn events(&self) -> &EventRegistry<FileServerInstanceEvents> { &self.events }

    pub fn file_path(&self) -> Cow<'_, str> { self.file_path.to_string_lossy() }

    pub fn handler_unique_id(&self) -> &str {
        &self.handler_unique_id
    }

    pub fn virtual_server(&self) -> &BTreeMap<String, Arc<RwLock<FileVirtualServer>>> {
        &self.virtual_server
    }

    /// Create a new virtual server.
    pub fn create_virtual_server(&mut self, unique_id: String, initial_quota: (u64, u64)) -> Arc<RwLock<FileVirtualServer>> {
        if let Some(server) = self.virtual_server.get(&unique_id) {
            return server.clone();
        }

        let (events, event_emitter) = EventRegistry::new();
        let server_logger = self.logger.new(slog::o!("virtual-server" => unique_id.clone()));
        let server_fs_root = self.file_path.join(format!("virtual_server_{}", &unique_id));
        let server = FileVirtualServer{
            logger: server_logger.clone(),
            file_system: Arc::new(RwLock::new(VirtualFileSystem::new(server_logger, server_fs_root))),
            server_unique_id: unique_id.clone(),

            events,
            event_emitter,

            server_transfer_count: Arc::new(RunningTransferSummary::new(Some(self.transfer_count.clone()))),
            client_transfer_count: Default::default(),

            quota: Mutex::new(FileVirtualServerQuota::new(
                &self.upload_quota,
                &self.download_quota,
                initial_quota.0,
                initial_quota.1
            )),
            running_transfers: Default::default()
        };
        let server = Arc::new(RwLock::new(server));
        self.virtual_server.insert(unique_id.clone(), server.clone());
        self.event_emitter.fire_later(None, FileServerInstanceEvents::VirtualServerCreated(unique_id));

        server
    }

    pub fn find_virtual_server(&self, unique_id: &str) -> Option<&Arc<RwLock<FileVirtualServer>>> {
        self.virtual_server.get(unique_id)
    }

    /// Delete a virtual server.
    /// If `delete_files` is set to `true` the function will return a handle which will full fill when all
    /// files have been deleted.
    pub fn delete_virtual_server(&mut self, unique_id: &str, delete_files: bool) -> Option<JoinHandle<()>> {
        let server = match self.virtual_server.remove(unique_id) {
            Some(server) => server,
            None => return None
        };

        let mut server = server.write();
        server.shutdown();

        self.event_emitter.fire_later(None, FileServerInstanceEvents::VirtualServerDeleted(unique_id.to_owned()));

        if !delete_files {
            return None;
        }

        let fs_root = server.file_system.read().root_path().clone();
        let logger = server.logger.clone();
        Some(tokio::task::spawn_blocking(move || {
            if let Err(error) = std::fs::remove_dir_all(&fs_root) {
                match error.kind() {
                    ErrorKind::NotFound => {},
                    error => {
                        warn!(logger, "Failed to delete server files at {}: {:?}", fs_root.to_string_lossy(), error);
                    }
                }
            }
        }))
    }

    pub fn shutdown(&mut self) {
        /* TODO: Shutdown all virtual servers, remove them and stop transfers */
    }
}

pub enum FileVirtualServerEvents {
    ClientQuotaChange{
        upload: HashMap<String, u64>,
        download: HashMap<String, u64>
    },

    FileTransferRegistered(Arc<FileTransferInfo>),
    FileTransferTimeout(Arc<FileTransferInfo>),
    FileTransferInitializeFailed(Arc<FileTransferInfo>, TransferRequestError),
}

pub struct FileVirtualServerQuota {
    server_upload_quota: Arc<PendingQuotaSummary>,
    server_download_quota: Arc<PendingQuotaSummary>,

    client_download_quota: HashMap<String, Arc<PendingQuotaSummary>>,
    client_upload_quota: HashMap<String, Arc<PendingQuotaSummary>>,

    last_client_download_quota: HashMap<String, u64>,
    last_client_upload_quota: HashMap<String, u64>,
}

impl FileVirtualServerQuota {
    pub fn new(
        parent_upload_quota: &Arc<PendingQuotaSummary>,
        parent_download_quota: &Arc<PendingQuotaSummary>,
        initial_upload_quota: u64,
        initial_download_quota: u64
    ) -> Self {
        FileVirtualServerQuota {
            server_upload_quota: Arc::new(PendingQuotaSummary::new(Some(parent_upload_quota.clone()), (initial_upload_quota, initial_upload_quota))),
            server_download_quota: Arc::new(PendingQuotaSummary::new(Some(parent_download_quota.clone()), (initial_download_quota, initial_download_quota))),

            client_download_quota: HashMap::new(),
            client_upload_quota: HashMap::new(),

            last_client_download_quota: Default::default(),
            last_client_upload_quota: Default::default(),
        }
    }

    pub fn client_quota(&mut self, client: &str, direction: TransferDirection) -> Arc<PendingQuotaSummary> {
        let quotas = match direction {
            TransferDirection::Upload => &mut self.client_upload_quota,
            TransferDirection::Download => &mut self.client_download_quota
        };

        if let Some(quota) = quotas.get(client) {
            quota.clone()
        } else {
            let quota = Arc::new(PendingQuotaSummary::new(Some(match direction {
                TransferDirection::Upload => self.server_upload_quota.clone(),
                TransferDirection::Download => self.server_download_quota.clone()
            }), (0, 0)));
            quotas.insert(client.to_owned(), quota.clone());
            quota
        }
    }

    pub fn server_quota(&self, direction: TransferDirection) -> &Arc<PendingQuotaSummary> {
        match direction {
            TransferDirection::Upload => &self.server_upload_quota,
            TransferDirection::Download => &self.server_download_quota
        }
    }

    pub fn update_quota_deltas(&mut self, events: &mut EventEmitter<FileVirtualServerEvents>) {
        let mut changed_upload = HashMap::new();
        let mut changed_download = HashMap::new();

        Self::update_quota_delta(&mut self.client_upload_quota, &mut self.last_client_upload_quota, &mut changed_upload);
        Self::update_quota_delta(&mut self.client_download_quota, &mut self.last_client_download_quota, &mut changed_download);

        if changed_upload.is_empty() && changed_download.is_empty() {
            return;
        }

        events.fire_later(None, FileVirtualServerEvents::ClientQuotaChange {
            download: changed_download,
            upload: changed_upload
        });
    }

    fn update_quota_delta(
        current_values: &mut HashMap<String, Arc<PendingQuotaSummary>>,
        old_values: &mut HashMap<String, u64>,
        changed_values: &mut HashMap<String, u64>
    ) {
        let mut new_values = HashMap::<String, u64>::new();
        current_values.drain_filter(|key, value| {
            new_values.insert(key.clone(), value.realized_bytes());
            Arc::strong_count(value) == 1
        }).count();

        old_values.drain_filter(|key, value| {
            if let Some(new_value) = new_values.remove(key) {
                debug_assert!(new_value >= *value);
                let difference = new_value - *value;
                if difference > 0 {
                    *value = new_value;
                    changed_values.insert(key.clone(), difference);
                }
                false
            } else {
                /* Client has been removed in the previous run. No changes have been made, else he would have been added again. */
                true
            }
        }).count();

        for (key, value) in new_values {
            changed_values.insert(key.clone(), value);
            old_values.insert(key, value);
        }
    }
}

struct RunningFileTransferHandle {
    transfer: Box<dyn RunningFileTransfer>,
    abort_handle: Option<AbortHandle>,
}

pub struct FileVirtualServer {
    logger: slog::Logger,
    file_system: Arc<RwLock<VirtualFileSystem>>,
    server_unique_id: String,

    pub(crate) events: EventRegistry<FileVirtualServerEvents>,
    pub(crate) event_emitter: EventEmitter<FileVirtualServerEvents>,

    pub(crate) server_transfer_count: Arc<RunningTransferSummary>,
    pub(crate) client_transfer_count: Mutex<BTreeMap<String, Weak<RunningTransferSummary>>>,

    pub(crate) quota: Mutex<FileVirtualServerQuota>,

    running_transfers: Mutex<VecDeque<Arc<Mutex<RunningFileTransferHandle>>>>,
}

impl FileVirtualServer {
    pub fn server_unique_id(&self) -> &str {
        &self.server_unique_id
    }
    pub fn events(&self) -> &EventRegistry<FileVirtualServerEvents> {
        &self.events
    }
    pub fn logger(&self) -> &slog::Logger { &self.logger }

    pub fn file_system(&self) -> &Arc<RwLock<VirtualFileSystem>> {
        &self.file_system
    }

    /*
    pub fn running_transfers(&self) -> Vec<Arc<Mutex<dyn RunningFileTransfer>>> {
        Vec::from_iter(self.running_transfers.lock().iter().cloned())
    }
    */

    pub fn quota(&self) -> &Mutex<FileVirtualServerQuota> {
        &self.quota
    }

    pub fn server_transfer_count(&self) -> &Arc<RunningTransferSummary> {
        &self.server_transfer_count
    }

    fn tick_client_statistics(&mut self) {
        let mut quota = self.quota.lock();
        quota.update_quota_deltas(&mut self.event_emitter);
    }


    pub(crate) fn handle_request(&mut self, request: &mut dyn TransferRequest, transfer: &Arc<FileTransferInfo>) -> std::result::Result<(), TransferRequestError> {
        match self.do_handle_request(request, transfer) {
            Ok(_) => Ok(()),
            Err(error) => {
                self.event_emitter.fire_later(None, FileVirtualServerEvents::FileTransferInitializeFailed(transfer.clone(), error.clone()));
                Err(error)
            }
        }
    }

     fn do_handle_request(&mut self, request: &mut dyn TransferRequest, transfer: &Arc<FileTransferInfo>) -> std::result::Result<(), TransferRequestError> {
        let transfer = match transfer.direction {
            TransferDirection::Upload => {
                self.handle_request_upload(request, transfer)
            },
            TransferDirection::Download => {
                self.handle_request_download(request, transfer)
            },
        }?;

        let transfer = Arc::new(Mutex::new(RunningFileTransferHandle{
            transfer,
            abort_handle: None
        }));

        let (processor, process_abort) = futures::future::abortable({
            let transfer = Arc::downgrade(&transfer);
            let logger = self.logger.clone();

            debug!(logger, "Starting transfer");
            let result = futures::future::poll_fn({
                let transfer = transfer.clone();
                move |cx| {
                    if let Some(transfer) = transfer.upgrade() {
                        transfer.lock().transfer.poll_process(cx)
                    } else {
                        Poll::Ready(FileTransferResult::Aborted)
                    }
                }
            });

            result.then(|result| async move {
                let _transfer = match transfer.upgrade(){
                    Some(transfer) => transfer,
                    None => return,
                };

                debug!(logger, "Transfer result: {:?}.", result);
                //let transfer = transfer.lock();
                //debug!(logger, "Transfer result: {:?}. Network send: {}", result, transfer.transferred_bytes());
            })
        });
        transfer.lock().abort_handle = Some(process_abort);
        tokio::spawn(processor);

        {
            let mut running_transfers = self.running_transfers.lock();
            running_transfers.push_back(transfer);
        }
        Ok(())
    }

    fn handle_request_upload(&mut self, _request: &mut dyn TransferRequest, transfer: &Arc<FileTransferInfo>) -> std::result::Result<Box<dyn RunningFileTransfer>, TransferRequestError> {
        let target = self.file_system.write().create_upload_target(&transfer.file_path, transfer.file_size);
        match target {
            Ok(mut target) => {
                /* TODO: Initialize the actual transfer */
                target.abort_upload();
            },
            Err(error) => return Err(error.into())
        }

        Err(TransferRequestError::NotImplemented)
    }

    fn handle_request_download(&mut self, request: &mut dyn TransferRequest, transfer_info: &Arc<FileTransferInfo>) -> std::result::Result<Box<dyn RunningFileTransfer>, TransferRequestError> {
        let target = match request.execute_download(transfer_info.clone()) {
            Ok(target) => target,
            Err(error) => return Err(TransferRequestError::ExecuteError(error))
        };

        let source = self.file_system.write().create_download_target(&transfer_info.file_path);
        let source = match source {
            Ok(source) => source,
            Err(error) => return Err(error.into())
        };

        let logger = self.logger.new(o!("transfer-id" => String::from_utf8_lossy(request.transfer_key()).to_string()));
        Ok(Box::new(RunningDownloadFileTransfer::new(logger, transfer_info.clone(), source, target)))
    }

    pub fn shutdown(&mut self) {
        let running_transfers = {
            let mut running_transfers = self.running_transfers.lock();
            std::mem::replace(running_transfers.deref_mut(), VecDeque::new())
        };

        for transfer in running_transfers {
            let mut transfer = transfer.lock();
            transfer.transfer.abort("server shutdown");
        };
    }
}
