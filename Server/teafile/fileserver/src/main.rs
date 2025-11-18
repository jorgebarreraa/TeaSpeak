#![feature(with_options)]
#![feature(new_uninit)]
#![feature(drain_filter)]
#![feature(label_break_value)]
#![feature(box_syntax)]
#![feature(btree_drain_filter)]
#![feature(hash_drain_filter)]
#![allow(dead_code)]

mod log;
mod terminal;

use std::time::Instant;
use tokio::time::Duration;

use slog::*;
use teaspeak_filelib::config::Config;
use crate::terminal::TerminalEvents;
use teaspeak_filelib::server::FileServer;

fn main() {
    let logger = log::create_file_logger();
    let runtime = tokio::runtime::Builder::new_multi_thread()
        .worker_threads(1)
        .enable_all()
        .build()
        .expect("failed to build runtime");

    {
        let _guard = runtime.enter();
        if let Err(error) = terminal::setup() {
            crit!(logger, "Failed to setup terminal: {}", error);
            return;
        }
    }

    let config = match std::fs::read("config_file_server.yml") {
        Ok(buffer) => buffer,
        Err(error) => {
            crit!(logger, "Failed to open config_file_server.yml in {}:", std::env::current_dir().unwrap().to_string_lossy());
            crit!(logger, "{}", error);
            return;
        }
    };

    let mut config: Config = match serde_yaml::from_slice(config.as_slice()) {
        Ok(config) => config,
        Err(error) => {
            crit!(logger, "Failed to parse config:");
            crit!(logger, "{}", error);
            return;
        }
    };
    info!(logger, "Config loaded successfully. Executing file server.");

    runtime.block_on(async_main(logger.clone(), &mut config));

    /* Just in case we've some blocking/stuck tasks */
    runtime.shutdown_timeout(Duration::from_secs(5));
    drop(logger);

    terminal::shutdown();
}

async fn async_main(logger: slog::Logger, config: &mut Config) {
    let mut server = FileServer::new(logger.clone());

    {
        let begin = Instant::now();
        if let Err(error) = server.start(config).await {
            crit!(logger, "Failed to start file server:");
            crit!(logger, "{}", error);
            return;
        }

        slog::info!(logger, "File server started within {}ms", begin.elapsed().as_millis());
    }

    loop {
        match terminal::next_event().await {
            TerminalEvents::Terminate => break,
            TerminalEvents::Command(command) => {
                let lcommand = command.to_lowercase();
                if lcommand == "exit" || lcommand == "end" {
                    break;
                }

                slog::info!(logger, "Received command: {}", command);
            }
        }
    }

    {
        info!(logger, "Stopping server");
        let begin = Instant::now();

        let shutdown_future = server.shutdown();

        if tokio::time::timeout(Duration::from_secs(5), shutdown_future).await.is_err() {
            warn!(logger, "Failed to shutdown server within 5 seconds (some clients may not be disconnected properly).");
        }

        info!(logger, "File server stopped within {}ms", begin.elapsed().as_millis());
    }
}
