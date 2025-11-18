//
// Created by WolverinDEV on 28/04/2020.
//

#include <netinet/in.h>
#include <log/LogUtils.h>
#include "LocalFileProvider.h"
#include "LocalFileSystem.h"
#include "LocalFileTransfer.h"

using namespace ts::server;
using LocalFileServer = file::LocalFileProvider;
using LocalVirtualFileServer = file::LocalVirtualFileServer;

std::shared_ptr<LocalFileServer> server_instance{};
bool file::initialize(std::string &error, const std::string& hostnames, uint16_t port) {
    server_instance = std::make_shared<LocalFileProvider>();

    if(!server_instance->initialize(error)) {
        server_instance = nullptr;
        return false;
    }

    bool any_bind{false};
    for(const auto& binding : net::resolve_bindings(hostnames, port)) {
        if(!std::get<2>(binding).empty()) {
            logError(LOG_FT, "Failed to resolve binding for {}: {}", std::get<0>(binding), std::get<2>(binding));
            continue;
        }

        auto result = dynamic_cast<transfer::LocalFileTransfer&>(server_instance->file_transfer()).add_network_binding({ std::get<0>(binding), std::get<1>(binding) });
        switch (result) {
            case transfer::NetworkingBindResult::SUCCESS:
                any_bind = true;
                break;

            case transfer::NetworkingBindResult::OUT_OF_MEMORY:
                logWarning(LOG_FT, "Failed to listen to address {}: Out of memory", std::get<0>(binding));
                continue;

            case transfer::NetworkingBindResult::FAILED_TO_LISTEN:
                logWarning(LOG_FT, "Failed to listen on {}: {}/{}", std::get<0>(binding), errno, strerror(errno));
                continue;

            case transfer::NetworkingBindResult::FAILED_TO_BIND:
                logWarning(LOG_FT, "Failed to bind on {}: {}/{}", std::get<0>(binding), errno, strerror(errno));
                continue;

            case transfer::NetworkingBindResult::BINDING_ALREADY_EXISTS:
                logWarning(LOG_FT, "Failed to bind on {}: binding already exists", std::get<0>(binding));
                continue;

            case transfer::NetworkingBindResult::NETWORKING_NOT_INITIALIZED:
                logWarning(LOG_FT, "Failed to bind on {}: networking not initialized", std::get<0>(binding));
                continue;

            case transfer::NetworkingBindResult::FAILED_TO_ALLOCATE_SOCKET:
                logWarning(LOG_FT, "Failed to allocate a socket for {}: {}/{}", std::get<0>(binding), errno, strerror(errno));
                continue;
        }
    }

    return any_bind;
}

void file::finalize() {
    auto server = std::exchange(server_instance, nullptr);
    if(!server) return;

    server->finalize();
}

std::shared_ptr<file::AbstractFileServer> file::server() {
    return server_instance;
}

LocalFileServer::LocalFileProvider() {
    this->file_system_ = new filesystem::LocalFileSystem();
    this->file_transfer_ = new transfer::LocalFileTransfer(this->file_system_);
}
LocalFileServer::~LocalFileProvider() {
    delete this->file_transfer_;
    delete this->file_system_;
};

bool LocalFileServer::initialize(std::string &error) {
    if(!this->file_system_->initialize(error, "files/"))
        return false;

    if(!this->file_transfer_->start()) {
        error = "transfer server startup failed";
        this->file_system_->finalize();
        return false;
    }
    return true;
}

void LocalFileServer::finalize() {
    this->file_transfer_->stop();
    this->file_system_->finalize();
}

file::filesystem::AbstractProvider &LocalFileServer::file_system() {
    return *this->file_system_;
}

file::transfer::AbstractProvider &LocalFileServer::file_transfer() {
    return *this->file_transfer_;
}

std::string file::LocalFileProvider::file_base_path() const {
    return this->file_system_->root_path();
}

std::shared_ptr<file::VirtualFileServer> LocalFileServer::register_server(ServerId server_id) {
    auto server = this->find_virtual_server(server_id);
    if(server) return server;

    server = std::make_shared<file::LocalVirtualFileServer>(server_id, std::to_string(server_id));
    {
        std::lock_guard slock{this->servers_mutex};
        this->servers_.push_back(server);
    }

    return server;
}

void LocalFileServer::unregister_server(ServerId server_id, bool delete_files) {
    auto server_unique_id = std::to_string(server_id);
    std::shared_ptr<VirtualFileServer> server{};

    {
        std::lock_guard slock{this->servers_mutex};
        auto it = std::find_if(this->servers_.begin(), this->servers_.end(), [&](const std::shared_ptr<VirtualFileServer>& server) {
            return server->unique_id() == server_unique_id;
        });

        if(it == this->servers_.end()) {
            return;
        }

        server = *it;
        this->servers_.erase(it);
    }

    using ErrorType = file::filesystem::ServerCommandErrorType;

    auto delete_result = this->file_system_->delete_server(server);
    if(!delete_result->wait_for(std::chrono::seconds{5})) {
        logError(LOG_INSTANCE, "Failed to wait for file directory deletion.");
    } else if(!delete_result->succeeded()) {
        switch (delete_result->error().error_type) {
            case ErrorType::FAILED_TO_DELETE_DIRECTORIES:
                logError(LOG_INSTANCE, "Failed to delete server {} file directories ({}).", server->server_id(), delete_result->error().error_message);
                break;

            case ErrorType::UNKNOWN:
            case ErrorType::FAILED_TO_CREATE_DIRECTORIES:
                logError(LOG_INSTANCE, "Failed to delete server {} file directory due to an unknown error: {}/{}",
                         server->server_id(), (int) delete_result->error().error_type, delete_result->error().error_message);
                break;
        }
    }
}

void LocalVirtualFileServer::max_networking_upload_bandwidth(int64_t value) {
    VirtualFileServer::max_networking_upload_bandwidth(value);
    this->upload_throttle.set_max_bandwidth(value);
}

void LocalVirtualFileServer::max_networking_download_bandwidth(int64_t value) {
    VirtualFileServer::max_networking_download_bandwidth(value);
    this->download_throttle.set_max_bandwidth(value);
}