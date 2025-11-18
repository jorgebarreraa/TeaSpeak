//
// Created by WolverinDEV on 04/05/2020.
//

#include <cassert>
#include <event2/event.h>
#include <log/LogUtils.h>
#include "./LocalFileProvider.h"
#include "./LocalFileTransfer.h"

using namespace ts::server::file;
using namespace ts::server::file::transfer;

ClientWorkerStartResult LocalFileTransfer::start_client_worker() {
    assert(!this->disconnect.active);
    this->disconnect.active = true;

    this->disconnect.dispatch_thread = std::thread(&LocalFileTransfer::dispatch_loop_client_worker, this);
    return ClientWorkerStartResult::SUCCESS;
}

void LocalFileTransfer::shutdown_client_worker() {
    if(!this->disconnect.active) return;
    this->disconnect.active = false;

    this->disconnect.notify_cv.notify_all();
    if(this->disconnect.dispatch_thread.joinable())
        this->disconnect.dispatch_thread.join();

    {
        std::unique_lock tlock{this->transfers_mutex};
        if(!this->transfers_.empty())
            logWarning(LOG_FT, "Shutting down disconnect worker even thou we still have some active clients. This could cause memory leaks.");
    }
}

void LocalFileTransfer::disconnect_client(const std::shared_ptr<FileClient> &client, std::unique_lock<std::shared_mutex>& state_lock, bool flush) {
    assert(state_lock.owns_lock());

    if(client->state == FileClient::STATE_DISCONNECTED || (client->state == FileClient::STATE_FLUSHING && flush)) {
        return; /* shall NOT happen */
    }

#define del_ev_noblock(event) if(event) event_del_noblock(event)

    client->state = flush ? FileClient::STATE_FLUSHING : FileClient::STATE_DISCONNECTED;
    client->timings.disconnecting = std::chrono::system_clock::now();
    if(flush) {
        const auto network_flush_time = client->networking.throttle.expected_writing_time(client->network_buffer.bytes) + std::chrono::seconds{10};

        del_ev_noblock(client->networking.event_read);

        client->networking.disconnect_timeout = std::chrono::system_clock::now() + network_flush_time;
        debugMessage(LOG_FT, "{} Disconnecting client. Flushing pending bytes (max {} seconds)", client->log_prefix(), std::chrono::floor<std::chrono::seconds>(network_flush_time).count());

        client->add_network_write_event_nolock(false);
        this->enqueue_disk_io(client);
    } else {
        del_ev_noblock(client->networking.event_read);
        del_ev_noblock(client->networking.event_write);
        del_ev_noblock(client->networking.event_throttle);

        this->disconnect.notify_cv.notify_one();
    }

#undef del_ev_noblock
}

void LocalFileTransfer::test_disconnecting_state(const std::shared_ptr<FileClient> &client) {
    if(client->state != FileClient::STATE_FLUSHING)
        return;

    if(!client->buffers_flushed())
        return;

    debugMessage(LOG_FT, "{} Disk and network buffers are flushed. Closing connection.", client->log_prefix());
    std::unique_lock s_lock{client->state_mutex};
    this->disconnect_client(client, s_lock, false);
}

void LocalFileTransfer::dispatch_loop_client_worker(void *ptr_transfer) {
    auto provider = reinterpret_cast<LocalFileTransfer*>(ptr_transfer);

    while(provider->disconnect.active) {
        {
            std::unique_lock dlock{provider->disconnect.mutex};
            provider->disconnect.notify_cv.wait_for(dlock, std::chrono::milliseconds {500}); /* report all 500ms the statistics */
        }
        /* run the disconnect worker at least once before exiting */

        /* transfer statistics */
        {
            std::unique_lock tlock{provider->transfers_mutex};
            auto transfers = provider->transfers_;
            tlock.unlock();
            for(const auto& transfer : transfers) {
                switch(transfer->state) {
                    case FileClient::STATE_TRANSFERRING:
                        break;
                    case FileClient::STATE_FLUSHING:
                        if(!transfer->transfer)
                            continue;

                        if(transfer->transfer->direction != Transfer::DIRECTION_DOWNLOAD)
                            continue;

                        if(transfer->buffers_flushed())
                            continue;

                        break; /* we're still transferring (sending data) */
                    case FileClient::STATE_AWAITING_KEY:
                    case FileClient::STATE_DISCONNECTED:
                    default:
                        continue;
                }

                provider->report_transfer_statistics(transfer);
            }
        }

        {
            std::deque<std::shared_ptr<Transfer>> timeouted_transfers{};

            {
                std::unique_lock tlock{provider->transfers_mutex};

                auto now = std::chrono::system_clock::now();
                std::copy_if(provider->pending_transfers.begin(), provider->pending_transfers.end(), std::back_inserter(timeouted_transfers), [&](const std::shared_ptr<Transfer>& t) {
                    return t->initialized_timestamp + std::chrono::seconds{10} < now;
                });
                provider->pending_transfers.erase(std::remove_if(provider->pending_transfers.begin(), provider->pending_transfers.end(), [&](const auto& t) {
                    return std::find(timeouted_transfers.begin(), timeouted_transfers.end(), t) != timeouted_transfers.end();
                }), provider->pending_transfers.end());
            }

            for(const auto& pt : timeouted_transfers)
                provider->invoke_aborted_callback(pt, { TransferError::TRANSFER_TIMEOUT, "" });

            if(!timeouted_transfers.empty())
                logMessage(LOG_FT, "Removed {} pending transfers because no request has been made for them.", timeouted_transfers.size());
        }


        {
            std::deque<std::shared_ptr<FileClient>> disconnected_clients{};
            {
                std::unique_lock tlock{provider->transfers_mutex};

                auto now = std::chrono::system_clock::now();
                std::copy_if(provider->transfers_.begin(), provider->transfers_.end(), std::back_inserter(disconnected_clients), [&](const std::shared_ptr<FileClient>& t) {
                    std::shared_lock slock{t->state_mutex};
                    if(t->state == FileClient::STATE_DISCONNECTED) {
                        return true;
                    } else if(t->state == FileClient::STATE_AWAITING_KEY) {
                        return t->timings.connected + std::chrono::seconds{10} < now;
                    } else if(t->state == FileClient::STATE_TRANSFERRING) {
                        assert(t->transfer);
                        if(t->transfer->direction == Transfer::DIRECTION_UPLOAD) {
                            return t->timings.last_read + std::chrono::seconds{5} < now;
                        } else if(t->transfer->direction == Transfer::DIRECTION_DOWNLOAD) {
                            return t->timings.last_write + std::chrono::seconds{5} < now;
                        }
                    } else if(t->state == FileClient::STATE_FLUSHING) {
                        if(t->networking.disconnect_timeout.time_since_epoch().count() > 0)
                            return t->networking.disconnect_timeout + std::chrono::seconds{5} < now;
                        return t->timings.disconnecting + std::chrono::seconds{30} < now;
                    }
                    return false;
                });
                provider->transfers_.erase(std::remove_if(provider->transfers_.begin(), provider->transfers_.end(), [&](const auto& t) {
                    return std::find(disconnected_clients.begin(), disconnected_clients.end(), t) != disconnected_clients.end();
                }), provider->transfers_.end());
            }

            for(auto& client : disconnected_clients) {
                switch(client->state) {
                    case FileClient::STATE_AWAITING_KEY:
                        logMessage(LOG_FT, "{} Received no key. Dropping client.", client->log_prefix());
                        break;
                    case FileClient::STATE_TRANSFERRING:
                        logMessage(LOG_FT, "{} Networking timeout. Dropping client", client->log_prefix());
                        provider->invoke_aborted_callback(client, { TransferError::TRANSFER_TIMEOUT, "" });
                        break;
                    case FileClient::STATE_FLUSHING:
                        if(!client->buffers_flushed())
                            logMessage(LOG_FT, "{} Failed to flush connection. Dropping client", client->log_prefix());
                        else
                            ; /* we just awaited a client disconnect */
                        break;
                    case FileClient::STATE_DISCONNECTED:
                    default:
                        break;
                }
                {
                    std::unique_lock slock{client->state_mutex};
                    client->state = FileClient::STATE_DISCONNECTED;
                    /*
                     * First of all disconnect the client from the network so no actions could be triggered by that way.
                     * Secondly finalize all network components, so no data is pending anywhere
                     * Thirdly drop the client's disk worker (if it's an upload the data should be written already, else we don't care anyways)
                     */
                    provider->finalize_networking(client, slock);
                    provider->finalize_client_ssl(client);
                    provider->finalize_file_io(client, slock);
                }

                debugMessage(LOG_FT, "{} Destroying transfer.", client->log_prefix());
            }
        }
    }
}

void LocalFileTransfer::report_transfer_statistics(const std::shared_ptr<FileClient> &client) {
    auto callback{this->callback_transfer_statistics};
    if(!callback) return;

    callback(client->transfer, this->generate_transfer_statistics_report(client));
}

TransferStatistics LocalFileTransfer::generate_transfer_statistics_report(const std::shared_ptr<FileClient> &client) {
    TransferStatistics stats{};

    stats.network_bytes_send = client->statistics.network_send.total_bytes;
    stats.network_bytes_received = client->statistics.network_received.total_bytes;
    stats.file_bytes_transferred = client->statistics.file_transferred.total_bytes;

    stats.delta_network_bytes_received = client->statistics.network_received.take_delta();
    stats.delta_network_bytes_send = client->statistics.network_received.take_delta();

    stats.delta_file_bytes_transferred = client->statistics.file_transferred.take_delta();

    stats.file_start_offset = client->transfer->file_offset;
    stats.file_current_offset = client->statistics.file_transferred.total_bytes + client->transfer->file_offset;
    stats.file_total_size = client->transfer->expected_file_size;

    stats.average_speed = client->statistics.file_transferred.average_bandwidth();
    stats.current_speed = client->statistics.file_transferred.current_bandwidth();

    return stats;
}

void LocalFileTransfer::invoke_aborted_callback(const std::shared_ptr<FileClient> &client,
                                                const ts::server::file::transfer::TransferError &error) {
    auto callback{this->callback_transfer_aborted};
    if(!callback || !client->transfer) return;

    callback(client->transfer, this->generate_transfer_statistics_report(client), error);
}

void LocalFileTransfer::invoke_aborted_callback(const std::shared_ptr<Transfer> &transfer,
                                                const ts::server::file::transfer::TransferError &error) {
    auto callback{this->callback_transfer_aborted};
    if(!callback) return;

    TransferStatistics stats{};

    stats.network_bytes_send = 0;
    stats.network_bytes_received = 0;
    stats.file_bytes_transferred = 0;

    stats.delta_network_bytes_received = 0;
    stats.delta_network_bytes_send = 0;

    stats.delta_file_bytes_transferred = 0;

    stats.file_start_offset = transfer->file_offset;
    stats.file_current_offset = transfer->file_offset;
    stats.file_total_size = transfer->expected_file_size;

    callback(transfer, stats, error);
}