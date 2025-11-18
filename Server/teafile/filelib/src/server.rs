use crate::config::Config;
use crate::request::RequestServer;
use std::sync::Arc;
use parking_lot::{Mutex, RwLock};
use tokio::macros::support::{Poll, Pin};
use futures::StreamExt;
use slog::{ error };
use tokio::time::Duration;
use crate::request_handler::{TransferHandler, TransferDirection};
use std::ops::{Deref, DerefMut};
use crate::files::FilePath;
use crate::request_handler::server::FileVirtualServerEvents;

pub struct FileServer {
    logger: slog::Logger,

    started: bool,
    request_server: Arc<Mutex<RequestServer>>,
    transfer_handler: Arc<RwLock<TransferHandler>>,
}

impl FileServer {
    pub fn new(logger: slog::Logger) -> Self {
        let result = FileServer{
            logger: logger.clone(),
            started: false,

            request_server: Arc::new(Mutex::new(RequestServer::new(logger.clone()))),
            transfer_handler: Arc::new(RwLock::new(TransferHandler::new(logger)))
        };

        result
    }



    pub async fn start(&mut self, config: &mut Config) -> Result<(), String> {
        if self.started {
            return Err("server already running".to_string());
        }

        if let Err(error) = self.do_start(config).await {
            slog::debug!(self.logger, "Failed to start server. Stopping server and returning error.");
            if tokio::time::timeout(Duration::from_secs(30), self.do_shutdown()).await.is_err() {
                slog::crit!(self.logger, "Failed to stop server after failed start within 30 seconds.");
                slog::crit!(self.logger, "Start failed because of: {}", error);
                panic!("Server stop failed within 30 seconds after failed start");
                /* it's better to panic! here since the FileServer is now in an undefined state */
            }

            return Err(error);
        }

        self.started = true;
        Ok(())
    }

    async fn do_start(&mut self, config: &mut Config) -> Result<(), String> {
        {
            let mut request_server = self.request_server.lock();

            let mut bound_hosts = 0;
            for binding in config.bindings.iter_mut() {
                if binding.certificate.is_none() {
                    binding.certificate = Some(config.fallback_certificates.certificate.clone());
                }

                if binding.private_key.is_none() {
                    binding.private_key = Some(config.fallback_certificates.private_key.clone());
                }

                if let Err(error) = request_server.add_binding(binding).await {
                    error!(self.logger, "Failed to bind server on {}: {}", binding.address, error);
                    continue;
                }

                bound_hosts += 1;
            }

            if bound_hosts == 0 {
                return Err("Failed to bind to any address".to_string());
            }
        }

        {
            let server = self.request_server.clone();
            let handler = Arc::downgrade(&self.transfer_handler);

            tokio::spawn({
                let server = Arc::downgrade(&server);
                let logger = self.logger.clone();
                futures::future::poll_fn(move |cx| {
                    if let Some(server) = server.upgrade() {
                        let mut server = server.lock();
                        match server.poll_next_unpin(cx) {
                            Poll::Pending => Poll::Pending,
                            Poll::Ready(None) => Poll::Ready(()),
                            Poll::Ready(Some(mut request)) => {
                                drop(server);
                                let promise = {
                                    if let Some(handler) = handler.upgrade() {
                                        match handler.read().handle_transfer_request(request.deref_mut()) {
                                            Ok(_) => {
                                                /*
                                                 * Just in case the request hasn't been executed (should never happen)
                                                 * we properly close the connection
                                                 */
                                                request.close_request("request succeeded")
                                            },
                                            Err(error) => {
                                                slog::info!(logger, "Failed to full fill request for transfer key {}: {:?}", String::from_utf8_lossy(request.transfer_key()), error);
                                                request.close_request(&format!("{:?}", error))
                                            }
                                        }
                                    } else {
                                        request.close_request("handler went away")
                                    }
                                };

                                if let Some(promise) = promise {
                                    /* TODO: Take into account for shutdowns */
                                    tokio::spawn(promise);
                                }

                                cx.waker().wake_by_ref();
                                Poll::Pending
                            }
                        }
                    } else {
                        Poll::Ready(())
                    }
                })
            });
        }

        {
            let handler = self.transfer_handler.clone();
            tokio::spawn(futures::future::poll_fn(move |cx| {
                let handler = handler.read();
                Pin::new(handler.deref()).poll_pending_transfers(cx)
            }));
        }

        {
            let mut handler = self.transfer_handler.write();
            let (handler_id, server_id) = {
                let server = handler.create_server_instance("testing_instance".to_owned(), (0, 0));
                let mut server = server.write();
                let virtual_server = server.create_virtual_server("testing_virtual_server".to_owned(), (0, 0));
                let virtual_server = virtual_server.read();

                virtual_server.events().register_listener(Arc::new(|event| {
                    match event {
                        &FileVirtualServerEvents::FileTransferRegistered(_) => println!("Transfer registered"),
                        &FileVirtualServerEvents::FileTransferTimeout(_) => println!("Transfer timeout"),
                        &FileVirtualServerEvents::FileTransferInitializeFailed(_, _) => println!("Transfer failed"),
                        _ => {}
                    }

                    true
                }));

                let virtual_server_id = virtual_server.server_unique_id().to_owned();
                (server.handler_unique_id().to_owned(), virtual_server_id)
            };

            let transfer = handler.create_transfer()
                .server(handler_id, server_id)
                .client(1, "test_client".to_owned(), 2)
                .file(FilePath::Icon("icon_test".to_owned()), TransferDirection::Download, 0)
                .register()
                .expect("failed to create dummy transfer");
            slog::info!(self.logger, "Transfer key: {}", String::from_utf8_lossy(transfer.transfer_key()));
        }

        Ok(())
    }


    pub async fn shutdown(&mut self) {
        if !self.started {
            return;
        }

        self.started = false;
        self.do_shutdown().await;
    }

    async fn do_shutdown(&mut self) {
        let mut shutdown_futures = futures::stream::FuturesUnordered::new();
        shutdown_futures.push(self.request_server.lock().shutdown());

        while let Some(_) = shutdown_futures.next().await {}
    }

    pub fn request_server(&self) -> &Arc<Mutex<RequestServer>> {
        &self.request_server
    }

    pub fn transfer_handler(&self) -> &Arc<RwLock<TransferHandler>> {
        &self.transfer_handler
    }
}