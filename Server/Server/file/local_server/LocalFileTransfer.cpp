//
// Created by WolverinDEV on 04/05/2020.
//

#include <cassert>
#include <event2/event.h>
#include <log/LogUtils.h>
#include <random>
#include "./LocalFileProvider.h"
#include "./LocalFileTransfer.h"
#include <experimental/filesystem>

namespace fs = std::experimental::filesystem;
using namespace ts::server::file;
using namespace ts::server::file::transfer;

Buffer* transfer::allocate_buffer(size_t size) {
    auto total_size = sizeof(Buffer) + size;
    auto buffer = (Buffer*) malloc(total_size);
    new (buffer) Buffer{};
    buffer->capacity = size;
    buffer->ref_count = 1;
    return buffer;
}

Buffer* transfer::ref_buffer(Buffer *buffer) {
    buffer->ref_count++;
    return buffer;
}

void transfer::deref_buffer(Buffer *buffer) {
    if(--buffer->ref_count == 0) {
        buffer->~Buffer();
        free(buffer);
    }
}

FileClient::~FileClient() {
    this->flush_network_buffer();
    this->flush_disk_buffer();

    assert(!this->disk_buffer.buffer_head);
    assert(!this->network_buffer.buffer_head);

    assert(!this->file.file_descriptor);
    assert(!this->file.currently_processing);
    assert(!this->file.next_client);

    assert(!this->networking.event_read);
    assert(!this->networking.event_write);

    assert(this->state == STATE_DISCONNECTED);
    memtrack::freed<FileClient>(this);
}

LocalFileTransfer::LocalFileTransfer(filesystem::LocalFileSystem *fs) : file_system_{fs} {}
LocalFileTransfer::~LocalFileTransfer() = default;

bool LocalFileTransfer::start() {
    (void) this->start_client_worker();

    {
        auto start_result = this->start_disk_io();
        switch (start_result) {
            case DiskIOStartResult::SUCCESS:
                break;
            case DiskIOStartResult::OUT_OF_MEMORY:
                logError(LOG_FT, "Failed to start disk worker (Out of memory)");
                goto error_exit_disk;
            default:
                logError(LOG_FT, "Failed to start disk worker ({})", (int) start_result);
                goto error_exit_disk;
        }
    }


    {
        auto start_result = this->start_networking();
        switch (start_result) {
            case NetworkingStartResult::SUCCESS:
                break;

            case NetworkingStartResult::OUT_OF_MEMORY:
                logError(LOG_FT, "Failed to start networking (Out of memory)");
                goto error_exit_network;

            default:
                logError(LOG_FT, "Failed to start networking ({})", (int) start_result);
                goto error_exit_network;
        }
    }

    return true;
    error_exit_network:
    this->shutdown_networking();

    error_exit_disk:
    this->shutdown_disk_io();
    this->shutdown_client_worker();
    return false;
}

void LocalFileTransfer::stop() {
    this->shutdown_networking();
    this->shutdown_disk_io();
    this->shutdown_client_worker();
}

std::shared_ptr<ExecuteResponse<TransferInitError, std::shared_ptr<Transfer>>>
        LocalFileTransfer::initialize_icon_transfer(Transfer::Direction direction, const std::shared_ptr<VirtualFileServer> &server, const TransferInfo &info) {
    return this->initialize_transfer(direction, server, 0, Transfer::TARGET_TYPE_ICON, info);
}

std::shared_ptr<ExecuteResponse<TransferInitError, std::shared_ptr<Transfer>>>
        LocalFileTransfer::initialize_avatar_transfer(Transfer::Direction direction, const std::shared_ptr<VirtualFileServer> &server, const TransferInfo &info) {
    return this->initialize_transfer(direction, server, 0, Transfer::TARGET_TYPE_AVATAR, info);
}

std::shared_ptr<ExecuteResponse<TransferInitError, std::shared_ptr<Transfer>>>
        LocalFileTransfer::initialize_channel_transfer(Transfer::Direction direction, const std::shared_ptr<VirtualFileServer> &server, ChannelId cid, const TransferInfo &info) {
    return this->initialize_transfer(direction, server, cid, Transfer::TARGET_TYPE_CHANNEL_FILE, info);
}

std::shared_ptr<ExecuteResponse<TransferInitError, std::shared_ptr<Transfer>>> LocalFileTransfer::initialize_transfer(
        Transfer::Direction direction, const std::shared_ptr<VirtualFileServer> &server, ChannelId cid,
        Transfer::TargetType ttype,
        const TransferInfo &info) {
    auto response = this->create_execute_response<TransferInitError, std::shared_ptr<Transfer>>();

    std::lock_guard clock{this->transfer_create_mutex};
    if(info.max_concurrent_transfers > 0) {
        std::unique_lock tlock{this->transfers_mutex};
        {
            auto transfers = std::count_if(this->transfers_.begin(), this->transfers_.end(), [&](const std::shared_ptr<FileClient>& client) {
                return client->transfer && client->transfer->client_unique_id == info.client_unique_id && client->state < FileClient::STATE_FLUSHING;
            });
            transfers += std::count_if(this->pending_transfers.begin(), this->pending_transfers.end(), [&](const std::shared_ptr<Transfer>& transfer) {
                return transfer->client_unique_id == info.client_unique_id;
            });

            if(transfers >= info.max_concurrent_transfers) {
                response->emplace_fail(TransferInitError::CLIENT_TOO_MANY_TRANSFERS, std::to_string(transfers));
                return response;
            }
        }

        {
            auto server_transfers = this->pending_transfers.size();
            server_transfers += std::count_if(this->transfers_.begin(), this->transfers_.end(), [&](const std::shared_ptr<FileClient>& client) {
                return client->transfer;
            });
            if(server_transfers >= this->max_concurrent_transfers) {
                response->emplace_fail(TransferInitError::SERVER_TOO_MANY_TRANSFERS, std::to_string(server_transfers));
                return response;
            }
        }
    }

    auto transfer = std::make_shared<Transfer>();
    transfer->server_transfer_id = server->generate_transfer_id();
    transfer->server = server;
    transfer->channel_id = cid;
    transfer->target_type = ttype;
    transfer->direction = direction;

    transfer->client_id = 0; /* must be provided externally */
    transfer->client_transfer_id = 0; /* must be provided externally */

    transfer->server_addresses.reserve(this->network.bindings.size());
    for(auto& binding : this->network.bindings) {
        if(!binding->file_descriptor) continue;

        transfer->server_addresses.emplace_back(Transfer::Address{binding->hostname, net::port(binding->address)});
    }

    transfer->target_file_path = info.file_path;
    transfer->file_offset = info.file_offset;
    transfer->expected_file_size = info.expected_file_size;
    transfer->max_bandwidth = info.max_bandwidth;
    transfer->client_unique_id = info.client_unique_id;
    transfer->client_id = info.client_id;

    constexpr static std::string_view kTokenCharacters{"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"};
    transfer->transfer_key.resize(TRANSFER_KEY_LENGTH);
    for(auto& c : transfer->transfer_key) {
        c = kTokenCharacters[transfer_random_token_generator() % kTokenCharacters.length()];
    }
    transfer->transfer_key[0] = (char) 'r'; /* (114) */ /* a non valid SSL header type to indicate that we're using a file transfer key and not doing a SSL handshake */
    transfer->transfer_key[1] = (char) 'a'; /* ( 97) */
    transfer->transfer_key[2] = (char) 'w'; /* (119) */

    transfer->initialized_timestamp = std::chrono::system_clock::now();

    {
        std::string absolute_path{};
        switch (transfer->target_type) {
            case Transfer::TARGET_TYPE_AVATAR:
                absolute_path = this->file_system_->absolute_avatar_path(transfer->server, transfer->target_file_path);
                break;
            case Transfer::TARGET_TYPE_ICON:
                absolute_path = this->file_system_->absolute_icon_path(transfer->server, transfer->target_file_path);
                break;
            case Transfer::TARGET_TYPE_CHANNEL_FILE:
                absolute_path = this->file_system_->absolute_channel_path(transfer->server, transfer->channel_id, transfer->target_file_path);
                break;
            case Transfer::TARGET_TYPE_UNKNOWN:
            default:
                response->emplace_fail(TransferInitError::INVALID_FILE_TYPE, "");
                return response;
        }
        transfer->absolute_file_path = absolute_path;

        const auto root_path_length = this->file_system_->root_path().size();
        if(root_path_length < absolute_path.size())
            transfer->relative_file_path = absolute_path.substr(root_path_length);
        else
            transfer->relative_file_path = "error";
        transfer->file_name = fs::u8path(absolute_path).filename();
    }

    if(direction == Transfer::DIRECTION_DOWNLOAD) {
        auto path = fs::u8path(transfer->absolute_file_path);
        std::error_code error{};
        if(!fs::exists(path, error)) {
            response->emplace_fail(TransferInitError::FILE_DOES_NOT_EXISTS, "");
            return response;
        } else if(error) {
            logWarning(LOG_FT, "Failed to check for file at {}: {}. Assuming it does not exists.", transfer->absolute_file_path, error.value(), error.message());
            response->emplace_fail(TransferInitError::FILE_DOES_NOT_EXISTS, "");
            return response;
        }

        auto status = fs::status(path, error);
        if(error) {
            logWarning(LOG_FT, "Failed to status for file at {}: {}. Ignoring file transfer.", transfer->absolute_file_path, error.value(), error.message());
            response->emplace_fail(TransferInitError::IO_ERROR, "stat");
            return response;
        }

        if(status.type() != fs::file_type::regular) {
            response->emplace_fail(TransferInitError::FILE_IS_NOT_A_FILE, "");
            return response;
        }

        transfer->expected_file_size = fs::file_size(path, error);
        if(error) {
            logWarning(LOG_FT, "Failed to get file size for file at {}: {}. Ignoring file transfer.", transfer->absolute_file_path, error.value(), error.message());
            response->emplace_fail(TransferInitError::IO_ERROR, "file_size");
            return response;
        }

        if(info.download_client_quota_limit > 0 && info.download_client_quota_limit <= transfer->expected_file_size) {
            response->emplace_fail(TransferInitError::CLIENT_QUOTA_EXCEEDED, "");
            return response;
        }
        if(info.download_server_quota_limit > 0 && info.download_server_quota_limit <= transfer->expected_file_size) {
            response->emplace_fail(TransferInitError::SERVER_QUOTA_EXCEEDED, "");
            return response;
        }
    }

    {
        std::lock_guard tlock{this->transfers_mutex};
        this->pending_transfers.push_back(transfer);
    }

    switch (transfer->target_type) {
        case Transfer::TARGET_TYPE_AVATAR:
            logMessage(LOG_FT, "Initialized avatar transfer for avatar \"{}\" ({} bytes, transferring {} bytes).", transfer->target_file_path, transfer->expected_file_size, transfer->expected_file_size - transfer->file_offset);
            break;
        case Transfer::TARGET_TYPE_ICON:
            logMessage(LOG_FT, "Initialized icon transfer for icon \"{}\" ({} bytes, transferring {} bytes).",
                       transfer->target_file_path, transfer->expected_file_size, transfer->expected_file_size - transfer->file_offset);
            break;
        case Transfer::TARGET_TYPE_CHANNEL_FILE:
            logMessage(LOG_FT, "Initialized channel transfer for file \"{}/{}\" ({} bytes, transferring {} bytes).",
                       transfer->channel_id, transfer->target_file_path, transfer->expected_file_size, transfer->expected_file_size - transfer->file_offset);
            break;
        case Transfer::TARGET_TYPE_UNKNOWN:
        default:
            response->emplace_fail(TransferInitError::INVALID_FILE_TYPE, "");
            return response;
    }

    if(auto callback{this->callback_transfer_registered}; callback)
        callback(transfer);

    response->emplace_success(std::move(transfer));
    return response;
}

std::shared_ptr<ExecuteResponse<TransferActionError>> LocalFileTransfer::stop_transfer(const std::shared_ptr<VirtualFileServer>& server, transfer_id id, bool flush) {
    auto response = this->create_execute_response<TransferActionError>();

    std::shared_ptr<Transfer> transfer{};
    std::shared_ptr<FileClient> connected_transfer{};

    {
        std::lock_guard tlock{this->transfers_mutex};

        auto ct_it = std::find_if(this->transfers_.begin(), this->transfers_.end(), [&](const std::shared_ptr<FileClient>& t) {
            return t->transfer && t->transfer->server_transfer_id == id && t->transfer->server == server;
        });
        if(ct_it != this->transfers_.end())
            connected_transfer = *ct_it;
        else {
            auto t_it = std::find_if(this->pending_transfers.begin(), this->pending_transfers.end(), [&](const std::shared_ptr<Transfer>& t) {
                return t->server_transfer_id == id && t->server == server;
            });
            if(t_it != this->pending_transfers.end()) {
                transfer = *t_it;
                this->pending_transfers.erase(t_it);
            }
        }
    }

    if(!transfer) {
        if(connected_transfer)
            transfer = connected_transfer->transfer;
        else {
            response->emplace_fail(TransferActionError{TransferActionError::UNKNOWN_TRANSFER, ""});
            return response;
        }
    }

    if(connected_transfer) {
        this->invoke_aborted_callback(connected_transfer, { TransferError::USER_REQUEST, "" });
        logMessage(LOG_FT, "{} Stopping transfer due to an user request.", connected_transfer->log_prefix());

        std::unique_lock slock{connected_transfer->state_mutex};
        this->disconnect_client(connected_transfer, slock, flush);
    } else {
        this->invoke_aborted_callback(transfer, { TransferError::USER_REQUEST, "" });
        logMessage(LOG_FT, "Removing pending file transfer for id {}", id);
    }

    response->emplace_success();
    return response;
}

inline void apply_transfer_info(const std::shared_ptr<Transfer>& transfer, ActiveFileTransfer& info) {
    info.server_transfer_id = transfer->server_transfer_id;
    info.client_transfer_id = transfer->client_transfer_id;
    info.direction = transfer->direction;
    info.client_id = transfer->client_id;
    info.client_unique_id = transfer->client_unique_id;

    info.file_path = transfer->relative_file_path;
    info.file_name = transfer->file_name;

    info.expected_size = transfer->expected_file_size;
}

std::shared_ptr<ExecuteResponse<TransferListError, std::vector<ActiveFileTransfer>>> LocalFileTransfer::list_transfer() {
    std::vector<ActiveFileTransfer> transfer_infos{};
    auto response = this->create_execute_response<TransferListError, std::vector<ActiveFileTransfer>>();

    std::unique_lock tlock{this->transfers_mutex};
    auto awaiting_transfers = this->pending_transfers;
    auto running_transfers = this->transfers_;
    tlock.unlock();

    transfer_infos.reserve(awaiting_transfers.size() + running_transfers.size());
    for(const auto& transfer : awaiting_transfers) {
        ActiveFileTransfer info{};
        apply_transfer_info(transfer, info);
        info.size_done = transfer->file_offset;

        info.status = ActiveFileTransfer::NOT_STARTED;
        info.runtime = std::chrono::milliseconds{0};

        info.average_speed = 0;
        info.current_speed = 0;
        transfer_infos.push_back(info);
    }

    for(const auto& client : running_transfers) {
        auto transfer = client->transfer;
        if(!transfer) continue;

        ActiveFileTransfer info{};
        apply_transfer_info(transfer, info);
        info.size_done = transfer->file_offset + client->statistics.file_transferred.total_bytes;

        info.status = ActiveFileTransfer::RUNNING;
        info.runtime = std::chrono::floor<std::chrono::milliseconds>(std::chrono::system_clock::now() - client->timings.key_received);

        info.average_speed = client->statistics.file_transferred.average_bandwidth();
        info.current_speed = client->statistics.file_transferred.current_bandwidth();
        transfer_infos.push_back(info);
    }

    response->emplace_success(std::move(transfer_infos));
    return response;
}