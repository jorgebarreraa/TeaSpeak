#include <deque>
#include <log/LogUtils.h>
#include "WebServer.h"
#include "src/client/web/WebClient.h"
#include <netinet/tcp.h>
#include "src/InstanceHandler.h"
#include "./GlobalNetworkEvents.h"

using namespace std;
using namespace std::chrono;
using namespace ts;
using namespace ts::server;

#if defined(TCP_CORK) && !defined(TCP_NOPUSH)
    #define TCP_NOPUSH TCP_CORK
#endif

WebControlServer::WebControlServer(const std::shared_ptr<VirtualServer>& handle) : handle(handle) {}
WebControlServer::~WebControlServer() = default;

bool WebControlServer::start(const std::deque<std::shared_ptr<WebControlServer::Binding>>& target_bindings, std::string& error) {
    if(this->running()) {
        error = "server already running";
        return false;
    }
    this->_running = true;

    /* reserve backup file descriptor in case that the max file descriptors have been reached  */
    {
        this->server_reserve_fd = dup(1);
        if(this->server_reserve_fd < 0)
            logWarning(this->handle->getServerId(), "Failed to reserve a backup accept file descriptor. ({} | {})", errno, strerror(errno));
    }

    {
        for(auto& binding : target_bindings) {
            binding->file_descriptor = socket(binding->address.ss_family, SOCK_STREAM | SOCK_NONBLOCK, 0);
            if(binding->file_descriptor < 0) {
                logError(this->handle->getServerId(), "[Web] Failed to bind server to {}. (Failed to create socket: {} | {})", binding->as_string(), errno, strerror(errno));
                continue;
            }

            int enable = 1, disabled = 0;

            if (setsockopt(binding->file_descriptor, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
                logWarning(this->handle->getServerId(), "[Web] Failed to activate SO_REUSEADDR for binding {} ({} | {})", binding->as_string(), errno, strerror(errno));
            if(setsockopt(binding->file_descriptor, IPPROTO_TCP, TCP_NOPUSH, &disabled, sizeof disabled) < 0)
                logWarning(this->handle->getServerId(), "[Web] Failed to deactivate TCP_NOPUSH for binding {} ({} | {})", binding->as_string(), errno, strerror(errno));
            if(binding->address.ss_family == AF_INET6) {
                if(setsockopt(binding->file_descriptor, IPPROTO_IPV6, IPV6_V6ONLY, &enable, sizeof(int)) < 0)
                    logWarning(this->handle->getServerId(), "[Web] Failed to activate IPV6_V6ONLY for IPv6 binding {} ({} | {})", binding->as_string(), errno, strerror(errno));
            }
            if(fcntl(binding->file_descriptor, F_SETFD, FD_CLOEXEC) < 0)
                logWarning(this->handle->getServerId(), "[Web] Failed to set flag FD_CLOEXEC for binding {} ({} | {})", binding->as_string(), errno, strerror(errno));


            if (bind(binding->file_descriptor, (struct sockaddr *) &binding->address, sizeof(binding->address)) < 0) {
                logError(this->handle->getServerId(), "[Web] Failed to bind server to {}. (Failed to bind socket: {} | {})", binding->as_string(), errno, strerror(errno));
                close(binding->file_descriptor);
                continue;
            }

            if (listen(binding->file_descriptor, SOMAXCONN) < 0) {
                logError(this->handle->getServerId(), "[Web] Failed to bind server to {}. (Failed to listen: {} | {})", binding->as_string(), errno, strerror(errno));
                close(binding->file_descriptor);
                continue;
            }


            binding->event_accept = serverInstance->network_event_loop()->allocate_event(binding->file_descriptor, EV_READ | EV_PERSIST, WebControlServer::on_client_receive, this, nullptr);
            if(!binding->event_accept) {
                logError(this->handle->getServerId(), "[Web] Failed to allocate network event for binding {}.", binding->as_string());
                close(binding->file_descriptor);
                continue;
            }

            event_add(binding->event_accept, nullptr);
            this->bindings.push_back(binding);
        }

        if(this->bindings.empty()) {
            this->stop();
            error = "failed to bind to any address";
            return false;
        }
    }
    return true;
}

#define CLOSE_CONNECTION \
if(shutdown(file_descriptor, SHUT_RDWR) < 0) { \
    debugMessage(LOG_FT, "[{}] Failed to shutdown socket ({} | {}).", logging_address(remote_address), errno, strerror(errno)); \
} \
if(close(file_descriptor) < 0) { \
    debugMessage(LOG_FT, "[{}] Failed to close socket ({} | {}).", logging_address(remote_address), errno, strerror(errno)); \
}

inline std::string logging_address(const sockaddr_storage& address) {
    if(config::server::disable_ip_saving)
        return "X.X.X.X" + to_string(net::port(address));
    return net::to_string(address, true);
}

void WebControlServer::on_client_receive(int server_file_descriptor_, short, void *ptr_server) {
    auto server = (WebControlServer*) ptr_server;

    sockaddr_storage remote_address{};
    memset(&remote_address, 0, sizeof(remote_address));
    socklen_t address_length = sizeof(remote_address);

    int file_descriptor = accept(server_file_descriptor_, (struct sockaddr *) &remote_address, &address_length);
    if (file_descriptor < 0) {
        if(errno == EAGAIN) {
            return;
        }

        if(errno == EMFILE || errno == ENFILE) {
            if(errno == EMFILE) {
                logError(server->handle->getServerId(), "[Web] Server ran out file descriptors. Please increase the process file descriptor limit.");
            } else {
                logError(server->handle->getServerId(), "[Web] Server ran out file descriptors. Please increase the process and system-wide file descriptor limit.");
            }

            bool tmp_close_success = false;
            {
                lock_guard reserve_fd_lock(server->server_reserve_fd_lock);
                if(server->server_reserve_fd > 0) {
                    debugMessage(server->handle->getServerId(), "[Web] Trying to accept client with the reserved file descriptor to close the incoming connection.");
                    auto _ = [&]{
                        if(close(server->server_reserve_fd) < 0) {
                            debugMessage(server->handle->getServerId(), "[Web] Failed to close reserved file descriptor");
                            tmp_close_success = false;
                            return;
                        }
                        server->server_reserve_fd = 0;

                        errno = 0;
                        file_descriptor = accept(server_file_descriptor_, (struct sockaddr *) &remote_address, &address_length);
                        if(file_descriptor < 0) {
                            if(errno == EMFILE || errno == ENFILE)
                                debugMessage(server->handle->getServerId(), "[Web] [{}] Even with freeing the reserved descriptor accept failed. Attempting to reclaim reserved file descriptor", logging_address(remote_address));
                            else if(errno == EAGAIN);
                            else {
                                debugMessage(server->handle->getServerId(), "[Web] [{}] Failed to accept client with reserved file descriptor. ({} | {})", logging_address(remote_address), errno, strerror(errno));
                            }
                            server->server_reserve_fd = dup(1);
                            if(server->server_reserve_fd < 0)
                                debugMessage(server->handle->getServerId(), "[Web] [{}] Failed to reclaim reserved file descriptor. Future clients cant be accepted!", logging_address(remote_address));
                            else
                                tmp_close_success = true;
                            return;
                        }
                        debugMessage(server->handle->getServerId(), "[Web] [{}] Successfully accepted client via reserved descriptor (fd: {}). Disconnecting client.", logging_address(remote_address), file_descriptor);

                        CLOSE_CONNECTION
                        server->server_reserve_fd = dup(1);
                        if(server->server_reserve_fd < 0)
                            debugMessage(server->handle->getServerId(), "[Web] Failed to reclaim reserved file descriptor. Future clients cant be accepted!");
                        else
                            tmp_close_success = true;
                        logMessage(server->handle->getServerId(), "[Web] [{}] Dropping file transfer connection attempt because of too many open file descriptors.", logging_address(remote_address));
                    };
                    _();
                }
            }

            if(!tmp_close_success) {
                debugMessage(server->handle->getServerId(), "[Web] Sleeping two seconds because we're currently having no resources for this user. (Removing the accept event)");
                for(auto& binding : server->bindings)
                    event_del_noblock(binding->event_accept);
                server->accept_event_deleted = system_clock::now();
                return;
            }
            return;
        }
        logMessage(server->handle->getServerId(), "[Web] Got an error while accepting a new client. (errno: {}, message: {})", errno, strerror(errno));
        return;
    }


    auto client = std::make_shared<WebClient>(server, file_descriptor);
    memcpy(&client->remote_address, &remote_address, sizeof(remote_address));
    client->initialize_weak_reference(client);
    client->initialize();

    server->clientLock.lock();
    server->clients.push_back(client);
    server->clientLock.unlock();
    event_add(client->readEvent, nullptr);
    logMessage(server->handle->getServerId(), "[Web] Got new client from {}:{}", client->getLoggingPeerIp(), client->getPeerPort());
}

void WebControlServer::stop() {
    //TODO
    this->clientLock.lock();
    auto clList = this->clients;
    this->clientLock.unlock();

    for(const auto& e : clList) {
        e->close_connection(system_clock::now());
    }


    for(auto& binding : this->bindings) {
        if(binding->event_accept) {
            event_del_block(binding->event_accept);
            event_free(binding->event_accept);
            binding->event_accept = nullptr;
        }
        if(binding->file_descriptor > 0) {
            if(shutdown(binding->file_descriptor, SHUT_RDWR) < 0)
                logWarning(this->handle->getServerId(), "[Web] Failed to shutdown socket for binding {} ({} | {}).", binding->as_string(), errno, strerror(errno));
            if(close(binding->file_descriptor) < 0)
                logError(this->handle->getServerId(), "[Web] Failed to close socket for binding {} ({} | {}).", binding->as_string(), errno, strerror(errno));
            binding->file_descriptor = -1;
        }
    }
    this->bindings.clear();

    if(this->server_reserve_fd > 0) {
        if(close(this->server_reserve_fd) < 0)
            logError(this->handle->getServerId(), "[Web] Failed to close backup file descriptor ({} | {})", errno, strerror(errno));
    }
    this->server_reserve_fd = -1;
}

void WebControlServer::unregisterConnection(const std::shared_ptr<WebClient>& connection) {
    //TODO may checks?
    {
        std::lock_guard lock{this->clientLock};
        auto index = std::find(this->clients.begin(), this->clients.end(), connection);
        if(index == this->clients.end()) {
            return;
        }

        this->clients.erase(index);
    }
}

std::optional<std::string> WebControlServer::external_binding() {
    /* FIXME: TODO! */
    return std::nullopt;
}