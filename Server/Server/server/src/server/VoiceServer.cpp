#include "POWHandler.h"
#include <thread>
#include <algorithm>
#include <unistd.h>
#include <fcntl.h>
#include "../client/voice/VoiceClient.h"
#include <log/LogUtils.h>
#include <misc/endianness.h>
#include "src/VirtualServerManager.h"
#include "../InstanceHandler.h"
#include "./GlobalNetworkEvents.h"

using namespace std;
using namespace std::chrono;
using namespace ts::server;
using namespace ts::buffer;
using namespace ts;

VoiceServer::VoiceServer(const std::shared_ptr<VirtualServer>& server) {
    this->server = server;
    this->pow_handler = make_unique<POWHandler>(this);
}

VoiceServer::~VoiceServer() { }

bool VoiceServer::start(const std::deque<sockaddr_storage>& address_list, std::string& error) {
    if(this->running) {
        return false;
    }
    this->running = true;

    size_t active_sockets{0};
    for(const auto& address : address_list) {
        auto socket = std::make_shared<VoiceServerSocket>(this, address);
        this->sockets.push_back(socket);

        std::string socket_error{};
        if(socket->activate(socket_error)) {
            active_sockets++;
        } else {
            logError(this->server->getServerId(), "Failed to bind UDP socket {}: {}", net::to_string(socket->address()), socket_error);
        }
    }

    if(!active_sockets) {
        error = "failed to bind to any host";
        goto error_exit;
    }

    {
        auto task_scheduled = serverInstance->general_task_executor()->schedule_repeating(
                this->handshake_tick_task,
                "voice server tick " + std::to_string(this->get_server()->getServerId()),
                std::chrono::seconds{1},
                [&](const auto&) {
                    this->tickHandshakingClients();
                }
        );

        if(!task_scheduled) {
            error = "failed to schedule voice server tick task";
            goto error_exit;
        }
    }
    return true;

    error_exit:

    this->running = false;

    for(const auto& socket : this->sockets) {
        socket->deactivate();
    }
    this->sockets.clear();

    if(this->handshake_tick_task > 0) {
        auto task = serverInstance->general_task_executor()->cancel_task_joinable(this->handshake_tick_task);
        this->handshake_tick_task = 0;
        task.wait();
    }
    return false;
}

void VoiceServer::tickHandshakingClients() {
    this->pow_handler->execute_tick();

    decltype(this->activeConnections) connections;
    {
        lock_guard lock(this->connectionLock);
        connections = this->activeConnections;
    }

    for(const auto& client : connections) {
        if(client->state == ConnectionState::INIT_HIGH || client->state == ConnectionState::INIT_LOW) {
            client->tick_server(system_clock::now());
        }
    }
}

void VoiceServer::execute_resend(const std::chrono::system_clock::time_point &now, std::chrono::system_clock::time_point &next) {
    decltype(this->activeConnections) connections;
    {
        lock_guard lock(this->connectionLock);
        connections = this->activeConnections;
    }
    string error;
    for(const auto& client : connections) {
        auto connection = client->getConnection();
        sassert(connection); /* its not possible that a client hasn't a connection! */
        connection->packet_encoder().execute_resend(now, next);
    }
}

bool VoiceServer::stop(const std::chrono::milliseconds& flushTimeout) {
    if(!this->running) {
        return false;
    }
    this->running = false;

    this->connectionLock.lock();
    auto list = this->activeConnections;
    this->connectionLock.unlock();
    for(const auto &e : list) {
        e->close_connection(system_clock::now() + seconds(1));
    }

    auto beg = system_clock::now();
    while(!this->activeConnections.empty() && flushTimeout.count() != 0 && system_clock::now() - beg < flushTimeout) {
        threads::self::sleep_for(milliseconds(10));
    }

    for(const auto& connection : this->activeConnections) {
        connection->voice_server = nullptr;
    }
    this->activeConnections.clear();

    auto tick_task_future = serverInstance->general_task_executor()->cancel_task_joinable(this->handshake_tick_task);
    if(tick_task_future.wait_for(std::chrono::seconds{5}) != std::future_status::ready) {
        logCritical(this->get_server()->getServerId(), "Failed to shutdown tick executor");
    }

    for(const auto& bind : this->sockets) {
        bind->deactivate();
    }
    this->sockets.clear();
    return true;
}

std::shared_ptr<VoiceClient> VoiceServer::findClient(ts::ClientId client) {
    lock_guard lock(this->connectionLock);

    for(const auto& elm : this->activeConnections) {
        if(elm->getClientId() == client)
            return elm;
    }
    return nullptr;
}

std::shared_ptr<VoiceClient> VoiceServer::findClient(sockaddr_in *addr, bool) {
    lock_guard lock(this->connectionLock);

    /* Use a reverse iterator so we're getting the "last"/"newest" connection instance */
    for(auto it{this->activeConnections.rbegin()}; it != this->activeConnections.rend(); it++) {
        auto& elm = *it;
        if(elm->isAddressV4()) {
            if(elm->getAddressV4()->sin_addr.s_addr == addr->sin_addr.s_addr) {
                if(elm->getAddressV4()->sin_port == addr->sin_port) {
                    return elm;
                }
            }
        }
    }
    return nullptr;
}

std::shared_ptr<VoiceClient> VoiceServer::findClient(sockaddr_in6 *addr, bool) {
    lock_guard lock(this->connectionLock);

    /* Use a reverse iterator so we're getting the "last"/"newest" connection instance */
    for(auto it{this->activeConnections.rbegin()}; it != this->activeConnections.rend(); it++) {
        auto& elm = *it;
        if(elm->isAddressV6()) {
            if(memcmp(elm->getAddressV6()->sin6_addr.__in6_u.__u6_addr8, addr->sin6_addr.__in6_u.__u6_addr8, 16) == 0) {
                if(elm->getAddressV6()->sin6_port == addr->sin6_port) {
                    return elm;
                }
            }
        }
    }

    return nullptr;
}

bool VoiceServer::unregisterConnection(std::shared_ptr<VoiceClient> connection) {
    lock_guard lock(this->connectionLock);

    auto found = std::find(this->activeConnections.begin(), this->activeConnections.end(), connection);
    if(found != activeConnections.end())
        this->activeConnections.erase(found);
    else logError(LOG_GENERAL, "unregisterConnection(...) -> could not find client");
    return true;
}

void VoiceServer::handleClientAddressChange(const std::shared_ptr<VoiceClient> &client,
                                            const sockaddr_storage &remote_address,
                                            const udp::pktinfo_storage &remote_address_info) {
    auto old_address = net::to_string(client->get_remote_address());
    auto new_address = net::to_string(remote_address);

    auto command = "dummy_ipchange old_ip=" + old_address + " new_ip=" + new_address;
    client->server_command_queue()->enqueue_command_string(command);
    memcpy(&client->remote_address, &remote_address, sizeof(remote_address));
    memcpy(&client->connection->remote_address_info_, &remote_address_info, sizeof(remote_address_info));
}