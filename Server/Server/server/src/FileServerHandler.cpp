//
// Created by WolverinDEV on 12/05/2020.
//

#include <files/FileServer.h>
#include <files/Config.h>

#include "./client/ConnectedClient.h"
#include "FileServerHandler.h"

using namespace ts::server;
using namespace ts::server::file;

FileServerHandler::FileServerHandler(ts::server::InstanceHandler *instance) : instance_{instance} {}

bool FileServerHandler::initialize(std::string &error) {
    if(!file::initialize(error,
                         serverInstance->properties()[property::SERVERINSTANCE_FILETRANSFER_HOST].value(),
                         serverInstance->properties()[property::SERVERINSTANCE_FILETRANSFER_PORT].as_or<uint16_t>(30303))) {
        return false;
    }


#if 1
    file::config::ssl_option_supplier = [&]{
        return this->instance_->sslManager()->web_ssl_options();
    };
#endif

    auto server = file::server();
    assert(server);

    auto& transfer = server->file_transfer();
    transfer.callback_transfer_registered = std::bind(&FileServerHandler::callback_transfer_registered, this, std::placeholders::_1);
    transfer.callback_transfer_started = std::bind(&FileServerHandler::callback_transfer_started, this, std::placeholders::_1);
    transfer.callback_transfer_finished = std::bind(&FileServerHandler::callback_transfer_finished, this, std::placeholders::_1);

    transfer.callback_transfer_aborted = std::bind(&FileServerHandler::callback_transfer_aborted, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
    transfer.callback_transfer_statistics = std::bind(&FileServerHandler::callback_transfer_statistics, this, std::placeholders::_1, std::placeholders::_2);
    return true;
}

void FileServerHandler::finalize() {
    file::finalize();
}

void FileServerHandler::callback_transfer_registered(const std::shared_ptr<transfer::Transfer> &transfer) {
    auto server = this->instance_->getVoiceServerManager()->findServerById(transfer->server->server_id());
    if(!server) return; /* well that's bad */

    const auto bytes = transfer->expected_file_size - transfer->file_offset;
    if(transfer->direction == transfer::Transfer::DIRECTION_UPLOAD) {
        server->properties()[property::VIRTUALSERVER_TOTAL_BYTES_UPLOADED].increment_by<int64_t>(bytes);
        server->properties()[property::VIRTUALSERVER_MONTH_BYTES_UPLOADED].increment_by<int64_t>(bytes);
    } else {
        server->properties()[property::VIRTUALSERVER_TOTAL_BYTES_DOWNLOADED].increment_by<int64_t>(bytes);
        server->properties()[property::VIRTUALSERVER_MONTH_BYTES_DOWNLOADED].increment_by<int64_t>(bytes);
    }

    auto client = server->find_client_by_id(transfer->client_id);
    if(client && client->getUid() == transfer->client_unique_id) {
        if(transfer->direction == transfer::Transfer::DIRECTION_UPLOAD) {
            client->properties()[property::CLIENT_TOTAL_BYTES_UPLOADED].increment_by<int64_t>(bytes);
            client->properties()[property::CLIENT_MONTH_BYTES_UPLOADED].increment_by<int64_t>(bytes);
        } else {
            client->properties()[property::CLIENT_MONTH_BYTES_DOWNLOADED].increment_by<int64_t>(bytes);
            client->properties()[property::CLIENT_MONTH_BYTES_DOWNLOADED].increment_by<int64_t>(bytes);
        }
    }
}

void FileServerHandler::callback_transfer_aborted(const std::shared_ptr<transfer::Transfer> &transfer,
                                                  const transfer::TransferStatistics &statistics,
                                                  const ts::server::file::transfer::TransferError &error) {
    auto server = this->instance_->getVoiceServerManager()->findServerById(transfer->server->server_id());
    if(!server) return; /* well that's bad */

    if(statistics.file_total_size < statistics.file_current_offset)
        return;

    const int64_t bytes_left = statistics.file_total_size - statistics.file_current_offset;
    if(transfer->direction == transfer::Transfer::DIRECTION_UPLOAD) {
        server->properties()[property::VIRTUALSERVER_TOTAL_BYTES_UPLOADED].increment_by<int64_t>(-bytes_left);
        server->properties()[property::VIRTUALSERVER_MONTH_BYTES_UPLOADED].increment_by<int64_t>(-bytes_left);
    } else {
        server->properties()[property::VIRTUALSERVER_TOTAL_BYTES_DOWNLOADED].increment_by<int64_t>(-bytes_left);
        server->properties()[property::VIRTUALSERVER_MONTH_BYTES_DOWNLOADED].increment_by<int64_t>(-bytes_left);
    }

    auto client = server->find_client_by_id(transfer->client_id);
    if(client && client->getUid() == transfer->client_unique_id) {
        if(transfer->direction == transfer::Transfer::DIRECTION_UPLOAD) {
            client->properties()[property::CLIENT_TOTAL_BYTES_UPLOADED].increment_by<int64_t>(-bytes_left);
            client->properties()[property::CLIENT_MONTH_BYTES_UPLOADED].increment_by<int64_t>(-bytes_left);
        } else {
            client->properties()[property::CLIENT_MONTH_BYTES_DOWNLOADED].increment_by<int64_t>(-bytes_left);
            client->properties()[property::CLIENT_MONTH_BYTES_DOWNLOADED].increment_by<int64_t>(-bytes_left);
        }

        ts::command_builder notify{"notifystatusfiletransfer"};

        notify.put_unchecked(0, "clientftfid", transfer->client_transfer_id);
        notify.put(0, "size", 0);

        ts::command_result status{};
        using ErrorType = ts::server::file::transfer::TransferError::Type;
        switch (error.error_type) {
            case ErrorType::TRANSFER_TIMEOUT:
                status.reset(ts::command_result{error::file_transfer_connection_timeout});
                break;

            case ErrorType::DISK_IO_ERROR:
            case ErrorType::DISK_TIMEOUT:
            case ErrorType::DISK_INITIALIZE_ERROR:
                status.reset(ts::command_result{error::file_io_error});
                break;

            case ErrorType::UNKNOWN:
            case ErrorType::NETWORK_IO_ERROR:
                status.reset(ts::command_result{error::file_connection_lost});
                break;

            case ErrorType::UNEXPECTED_CLIENT_DISCONNECT:
            case ErrorType::UNEXPECTED_DISK_EOF:
                status.reset(ts::command_result{error::file_transfer_interrupted});

            case ErrorType::USER_REQUEST:
                status.reset(ts::command_result{error::file_transfer_canceled});
                break;
        }
        client->writeCommandResult(notify, status, "status");
        client->sendCommand(notify);
    }
}

void FileServerHandler::callback_transfer_statistics(const std::shared_ptr<transfer::Transfer> &transfer,
                                                     const ts::server::file::transfer::TransferStatistics &statistics) {
    auto server = this->instance_->getVoiceServerManager()->findServerById(transfer->server->server_id());
    if(!server) return; /* well that's bad */

    auto client = server->find_client_by_id(transfer->client_id);
    if(!client || client->getUid() != transfer->client_unique_id) {
        /* client not online anymore, but we could still log this as server traffic */
        if(transfer->direction == transfer::Transfer::DIRECTION_UPLOAD) {
            server->getServerStatistics()->logFileTransferIn(statistics.delta_file_bytes_transferred);
        } else {
            server->getServerStatistics()->logFileTransferOut(statistics.delta_file_bytes_transferred);
        }
        return;
    }

    if(transfer->direction == transfer::Transfer::DIRECTION_UPLOAD) {
        client->getConnectionStatistics()->logFileTransferIn(statistics.delta_file_bytes_transferred);
    } else {
        client->getConnectionStatistics()->logFileTransferOut(statistics.delta_file_bytes_transferred);
    }

    if(client->getType() == ClientType::CLIENT_TEAMSPEAK) {
        return; /* TS3 does not know this notify */
    }

    ts::command_builder notify{"notifyfiletransferprogress"};
    notify.put_unchecked(0, "clientftfid", transfer->client_transfer_id);

    notify.put_unchecked(0, "file_bytes_transferred", statistics.file_bytes_transferred);
    notify.put_unchecked(0, "network_bytes_send", statistics.network_bytes_send);
    notify.put_unchecked(0, "network_bytes_received", statistics.network_bytes_received);

    notify.put_unchecked(0, "file_start_offset", statistics.file_start_offset);
    notify.put_unchecked(0, "file_current_offset", statistics.file_current_offset);
    notify.put_unchecked(0, "file_total_size", statistics.file_total_size);

    notify.put_unchecked(0, "network_current_speed", statistics.current_speed);
    notify.put_unchecked(0, "network_average_speed", statistics.average_speed);

    client->sendCommand(notify);
}

void FileServerHandler::callback_transfer_started(const std::shared_ptr<transfer::Transfer> &transfer) {
    auto server = this->instance_->getVoiceServerManager()->findServerById(transfer->server->server_id());
    if(!server) return; /* well that's bad */

    auto client = server->find_client_by_id(transfer->client_id);
    if(!client || client->getUid() != transfer->client_unique_id) {
        return;
    }


    ts::command_builder notify{"notifyfiletransferstarted"};
    notify.put_unchecked(0, "clientftfid", transfer->client_transfer_id);
    client->sendCommand(notify);
}

void FileServerHandler::callback_transfer_finished(const std::shared_ptr<transfer::Transfer> &transfer) {
    auto server = this->instance_->getVoiceServerManager()->findServerById(transfer->server->server_id());
    if(!server) {
        return; /* well that's bad */
    }

    auto client = server->find_client_by_id(transfer->client_id);
    if(!client || client->getUid() != transfer->client_unique_id) {
        return;
    }


    if(client->getType() == ClientType::CLIENT_TEAMSPEAK) {
        return;
    }

    ts::command_builder notify{"notifystatusfiletransfer"};

    notify.put_unchecked(0, "clientftfid", transfer->client_transfer_id);
    notify.put(0, "size", transfer->expected_file_size); /* not sure where TeamSpeak counts from */
    notify.put_unchecked(0, "status", (int) error::file_transfer_complete);
    notify.put_unchecked(0, "msg", findError(error::file_transfer_complete).message);

    /* TODO: Some stats? */

    client->sendCommand(notify);
}