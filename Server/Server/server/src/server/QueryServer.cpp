//
// Created by wolverindev on 22.10.17.
//

#include "QueryServer.h"
#include <algorithm>
#include <netinet/tcp.h>
#include <src/VirtualServer.h>
#include <src/client/query/QueryClient.h>
#include <src/client/InternalClient.h>
#include <misc/rnd.h>
#include <src/InstanceHandler.h>
#include <ThreadPool/ThreadHelper.h>
#include <log/LogUtils.h>
#include "./GlobalNetworkEvents.h"

using namespace std;
using namespace std::chrono;
using namespace ts;
using namespace ts::server;

#if defined(TCP_CORK) && !defined(TCP_NOPUSH)
    #define TCP_NOPUSH TCP_CORK
#endif

QueryServer::QueryServer(sql::SqlManager* db) : sql(db) {
}

QueryServer::~QueryServer() {
    stop();
}

void QueryServer::unregisterConnection(const shared_ptr<QueryClient> &client) {
    {
        lock_guard lock(this->connected_clients_mutex);
        auto found = std::find(this->connected_clients.begin(), this->connected_clients.end(), client);
        if(found != this->connected_clients.end()) {
            this->connected_clients.erase(found);
        } else {
            logError(LOG_QUERY, "Attempted to unregister an invalid connection!");
        }
    }

    /* client->handle = nullptr; */
}

bool QueryServer::start(const deque<shared_ptr<QueryServer::Binding>> &bindings_, std::string &error) {
    if(this->active) {
        error = "already started";
        return false;
    }
    this->active = true;

    /* load ip black/whitelist */
    {
        ip_blacklist = std::make_unique<IpListManager>("query_ip_blacklist.txt", std::deque<std::string>{"#A new line separated address blacklist", "#", "#For example if we dont want google:", "8.8.8.8"});
        ip_whitelist = std::make_unique<IpListManager>("query_ip_whitelist.txt", std::deque<std::string>{"#A new line separated address whitelist", "#Every ip have no flood and login attempt limit!", "127.0.0.1/8", "::1"});

        if(!this->ip_blacklist->reload(error)) {
            logError(LOG_QUERY, "Failed to load query blacklist: {}", error);
        }

        if(!this->ip_whitelist->reload(error)) {
            logError(LOG_QUERY, "Failed to load query whitelist: {}", error);
        }

        error.clear();
    }

    /* reserve backup file descriptor in case that the max file descriptors have been reached  */
    {
        this->server_reserve_fd = dup(1);
        if(this->server_reserve_fd < 0) {
            logWarning(LOG_QUERY, "Failed to reserve a backup accept file descriptor. ({} | {})", errno, strerror(errno));
        }
    }

    for(auto& binding : bindings_) {
        binding->file_descriptor = socket(binding->address.ss_family, (unsigned) SOCK_STREAM | (unsigned) SOCK_NONBLOCK, 0);
        if(binding->file_descriptor < 0) {
            logError(LOG_QUERY, "Failed to bind server to {}. (Failed to create socket: {} | {})", binding->as_string(), errno, strerror(errno));
            continue;
        }

        int enable = 1, disabled = 0;

        if (setsockopt(binding->file_descriptor, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
            logWarning(LOG_QUERY, "Failed to activate SO_REUSEADDR for binding {} ({} | {})", binding->as_string(), errno, strerror(errno));
        }

        if(setsockopt(binding->file_descriptor, IPPROTO_TCP, TCP_NOPUSH, &disabled, sizeof disabled) < 0) {
            logWarning(LOG_QUERY, "Failed to deactivate TCP_NOPUSH for binding {} ({} | {})", binding->as_string(), errno, strerror(errno));
        }

        if(binding->address.ss_family == AF_INET6) {
            if(setsockopt(binding->file_descriptor, IPPROTO_IPV6, IPV6_V6ONLY, &enable, sizeof(int)) < 0) {
                logWarning(LOG_QUERY, "Failed to activate IPV6_V6ONLY for IPv6 binding {} ({} | {})", binding->as_string(), errno, strerror(errno));
            }
        }

        if(fcntl(binding->file_descriptor, F_SETFD, FD_CLOEXEC) < 0) {
            logWarning(LOG_QUERY, "Failed to set flag FD_CLOEXEC for binding {} ({} | {})", binding->as_string(), errno, strerror(errno));
        }


        if (bind(binding->file_descriptor, (struct sockaddr *) &binding->address, sizeof(binding->address)) < 0) {
            logError(LOG_QUERY, "Failed to bind server to {}. (Failed to bind socket: {} | {})", binding->as_string(), errno, strerror(errno));
            close(binding->file_descriptor);
            continue;
        }

        if (listen(binding->file_descriptor, SOMAXCONN) < 0) {
            logError(LOG_QUERY, "Failed to bind server to {}. (Failed to listen: {} | {})", binding->as_string(), errno, strerror(errno));
            close(binding->file_descriptor);
            continue;
        }

        binding->event_accept = serverInstance->network_event_loop()->allocate_event(binding->file_descriptor, EV_READ | EV_PERSIST, [](int a, short b, void* c){ ((QueryServer *) c)->on_client_receive(a, b, c); }, this, nullptr);
        if(!binding->event_accept) {
            logError(LOG_QUERY, "Failed to allocate accept event for query binding", binding->as_string());
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

    this->tick_active = true;
    this->tick_thread = std::thread{[&]{ this->tick_executor(); }};
    threads::name(this->tick_thread, "query tick");
    return true;
}
void QueryServer::stop() {
    if(!this->active) {
        return;
    }

    this->active = false;

    /* 1. Shutdown all bindings so we don't get any new queries */
    for(auto& binding : this->bindings) {
        if(binding->event_accept) {
            event_del_block(binding->event_accept);
            event_free(binding->event_accept);
            binding->event_accept = nullptr;
        }

        if(binding->file_descriptor > 0) {
            /* Shutdown not needed since we're not connected. A shutdown would result in "Transport endpoint is not connected". */
            if(close(binding->file_descriptor) < 0) {
                logError(LOG_QUERY, "Failed to close socket for binding {} ({} | {}).", binding->as_string(), errno, strerror(errno));
            }

            binding->file_descriptor = -1;
        }
    }
    this->bindings.clear();

    /* 2. Disconnect all connected query clients */
    {
        ts::command_builder notify{"serverstop"};
        notify.put_unchecked(0, "stopped", "1");

        std::lock_guard client_lock{this->connected_clients_mutex};
        for(const auto &client : this->connected_clients) {
            client->sendCommand(notify, false);
            client->disconnect("server stopped");

            /*
             * Shortcircuiting the disconnect since we don't want the full "server leave" disconnect.
             * We only wan't to prevent the client form receiving any more notifications.
             */
            this->execute_query_disconnect(client, true);
        }
    }

    /* Await all clients to disconnect within 5 seconds. */
    {
        std::unique_lock client_lock{this->connected_clients_mutex};
        this->connected_client_disconnected_notify.wait_for(client_lock, std::chrono::seconds{5}, [&]{
            return this->connected_clients.empty();
        });
    }

    /* 3. Shutdown the query event loop (to finish of client disconnects as well) */
    {
        std::lock_guard tick_lock{this->tick_mutex};
        this->tick_active = false;
        this->tick_notify.notify_all();
    }
    threads::save_join(this->tick_thread, true);

    /*
     * 4. Force disconnect pending clients.
     */
    {
        std::unique_lock client_lock{this->connected_clients_mutex};
        auto connected_clients_ = std::move(this->connected_clients);
        client_lock.unlock();

        if(!connected_clients_.empty()) {
            logWarning(LOG_QUERY, "Failed to normally disconnect {} query clients. Closing connection.", connected_clients_.size());

            for(const auto& client : this->connected_clients) {
                this->execute_query_connection_close(client, false);
            }
        }
    }

    /* 6. Cleanup the servers reserve file descriptor */
    if(this->server_reserve_fd > 0) {
        if(close(this->server_reserve_fd) < 0) {
            logError(LOG_QUERY, "Failed to close backup file descriptor ({} | {})", errno, strerror(errno));
        }
    }
    this->server_reserve_fd = -1;
}

inline std::string logging_address(const sockaddr_storage& address) {
    if(config::server::disable_ip_saving)
        return "X.X.X.X" + to_string(net::port(address));
    return net::to_string(address, true);
}

inline void send_direct_disconnect(const sockaddr_storage& address, int file_descriptor, const char* message, size_t message_length) {
    auto enable_non_block = [&]{
        int flags = fcntl(file_descriptor, F_GETFL, 0);
        if (flags == -1) {
            debugMessage(LOG_QUERY, "[{}] Failed to set socket to nonblock. Flag query failed ({} | {})", logging_address(address), errno, strerror(errno));
            return;
        }

        flags &= ~O_NONBLOCK;
        if(fcntl(file_descriptor, F_SETFL, flags) == -1) {
            debugMessage(LOG_QUERY, "[{}] Failed to set socket to nonblock. Flag apply failed ({} | {})", logging_address(address), errno, strerror(errno));
            return;
        }
    };
    enable_non_block();

    {
        struct timeval timeout{};
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;
        if (setsockopt (file_descriptor, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
            debugMessage(LOG_QUERY, "[{}] Failed to set the send timeout on socket", logging_address(address));
        }
    }

    bool broken_pipe = false;
    auto send_ = [&](const char* data, size_t length) {
        if(broken_pipe) {
            return;
        }

        size_t written_bytes = 0;
        while(written_bytes < length) {
            auto result = send(file_descriptor, data + written_bytes, length - written_bytes, MSG_NOSIGNAL);
            if(result <= 0) {
                broken_pipe |= errno == EPIPE;
                debugMessage(LOG_QUERY, "[{}] Failed to send a message of length {}. Bytes written: {}, error: {} | {}", logging_address(address), length, written_bytes, errno, strerror(errno));
                return;
            } else {
                written_bytes += result;
            }
        }
    };

    /* we could ignore errors here */
    send_(config::query::motd.data(), config::query::motd.size());
    send_(message, message_length);

    /* "flush" with the last new line and then close */
    int flag = 1;
    if(setsockopt(file_descriptor, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int)) < 0) {
        debugMessage(LOG_QUERY, "[{}] Failed to enabled TCP no delay to flush the direct query disconnect socket ({} | {}).", logging_address(address), errno, strerror(errno));
    }
    send_(config::query::newlineCharacter.data(), config::query::newlineCharacter.size());

    if(shutdown(file_descriptor, SHUT_RDWR) < 0) {
        debugMessage(LOG_QUERY, "[{}] Failed to shutdown socket ({} | {}).", logging_address(address), errno, strerror(errno));
    }
    if(close(file_descriptor) < 0) {
        debugMessage(LOG_QUERY, "[{}] Failed to close socket ({} | {}).", logging_address(address), errno, strerror(errno));
    }
}

//dummyfdflood
//dummyfdflood clear

void QueryServer::on_client_receive(int server_file_descriptor, short, void *ptr_server) {
    auto query_server = (QueryServer*) ptr_server;

    sockaddr_storage remote_address{};
    memset(&remote_address, 0, sizeof(sockaddr_in));
    socklen_t address_length = sizeof(remote_address);

    int client_file_descriptor = accept(server_file_descriptor, (struct sockaddr *) &remote_address, &address_length);
    if (client_file_descriptor < 0) {
        if(errno == EAGAIN) {
            return;
        }

        if(errno == EMFILE || errno == ENFILE) {
            if(errno == EMFILE) {
                logError(LOG_QUERY, "Server ran out file descriptors. Please increase the process file descriptor limit or decrease the instance variable 'serverinstance_query_max_connections'");
            } else {
                logError(LOG_QUERY, "Server ran out file descriptors. Please increase the process and system-wide file descriptor limit or decrease the instance variable 'serverinstance_query_max_connections'");
            }

            bool tmp_close_success{false};
            {
                lock_guard reserve_fd_lock(query_server->server_reserve_fd_lock);
                if(query_server->server_reserve_fd > 0) {
                    debugMessage(LOG_QUERY, "Trying to accept client with the reserved file descriptor to send him a protocol limit reached exception.");
                    auto _ = [&]{
                        if(close(query_server->server_reserve_fd) < 0) {
                            debugMessage(LOG_QUERY, "Failed to close reserved file descriptor");
                            tmp_close_success = false;
                            return;
                        }
                        query_server->server_reserve_fd = 0;

                        errno = 0;
                        client_file_descriptor = accept(server_file_descriptor, (struct sockaddr *) &remote_address, &address_length);
                        if(client_file_descriptor < 0) {
                            if(errno == EMFILE || errno == ENFILE) {
                                debugMessage(LOG_QUERY, "[{}] Even with freeing the reserved descriptor accept failed. Attempting to reclaim reserved file descriptor", logging_address(remote_address));
                            } else if(errno == EAGAIN) {
                                /* Nothing to do */
                            } else {
                                debugMessage(LOG_QUERY, "[{}] Failed to accept client with reserved file descriptor. ({} | {})", logging_address(remote_address), errno, strerror(errno));
                            }
                            query_server->server_reserve_fd = dup(1);
                            if(query_server->server_reserve_fd < 0) {
                                debugMessage(LOG_QUERY, "[{}] Failed to reclaim reserved file descriptor. Future clients cant be accepted!", logging_address(remote_address));
                            } else {
                                tmp_close_success = true;
                            }
                            return;
                        }
                        debugMessage(LOG_QUERY, "[{}] Successfully accepted client via reserved descriptor (fd: {}). Initializing socket and sending MOTD and disconnect.", logging_address(remote_address), client_file_descriptor);

                        static auto resource_limit_error = R"(error id=57344 msg=query\sserver\sresource\slimit\sreached extra_msg=file\sdescriptor\slimit\sexceeded)";
                        send_direct_disconnect(remote_address, client_file_descriptor, resource_limit_error, strlen(resource_limit_error));

                        query_server->server_reserve_fd = dup(1);
                        if(query_server->server_reserve_fd < 0) {
                            debugMessage(LOG_QUERY, "Failed to reclaim reserved file descriptor. Future clients cant be accepted!");
                        } else {
                            tmp_close_success = true;
                        }
                        logMessage(LOG_QUERY, "[{}] Dropping new query connection attempt because of too many open file descriptors.", logging_address(remote_address));
                    };
                    _();
                }
            }

            if(!tmp_close_success) {
                debugMessage(LOG_QUERY, "Sleeping two seconds because we're currently having no resources for this user. (Removing the accept event)");
                for(auto& binding : query_server->bindings) {
                    event_del_noblock(binding->event_accept);
                }
                query_server->accept_event_deleted = system_clock::now();
                return;
            }
            return;
        }
        logMessage(LOG_QUERY, "Got an error while accepting a new client. (errno: {}, message: {})", errno, strerror(errno));
        return;
    }

    {
        unique_lock lock{query_server->connected_clients_mutex};
        auto max_connections = serverInstance->properties()[property::SERVERINSTANCE_QUERY_MAX_CONNECTIONS].as_unchecked<size_t>();
        if(max_connections > 0 && max_connections <= query_server->connected_clients.size()) {
            lock.unlock();
            logMessage(LOG_QUERY, "[{}] Dropping new query connection attempt because of too many connected query clients.", logging_address(remote_address));
            static auto query_server_full = R"(error id=4611 msg=max\sclients\sreached)";
            send_direct_disconnect(remote_address, client_file_descriptor, query_server_full, strlen(query_server_full));
            return;
        }

        auto max_ip_connections = serverInstance->properties()[property::SERVERINSTANCE_QUERY_MAX_CONNECTIONS_PER_IP].as_unchecked<size_t>();
        if(max_ip_connections > 0) {
            size_t connection_count = 0;
            for(auto& client : query_server->connected_clients) {
                if(net::address_equal(client->remote_address, remote_address))
                    connection_count++;
            }

            if(connection_count >= max_ip_connections) {
                lock.unlock();
                logMessage(LOG_QUERY, "[{}] Dropping new query connection attempt because of too many simultaneously connected session from this ip.", logging_address(remote_address));
                static auto query_server_full = R"(error id=4610 msg=too\smany\ssimultaneously\sconnected\ssessions)";//
                send_direct_disconnect(remote_address, client_file_descriptor, query_server_full, strlen(query_server_full));
                return;
            }
        }
    }

    auto client = std::make_shared<QueryClient>(query_server, client_file_descriptor);
    memcpy(&client->remote_address, &remote_address, sizeof(remote_address));
    client->initialize_weak_reference(client);

    {
        lock_guard lock(query_server->connected_clients_mutex);
        query_server->connected_clients.push_back(client);
    }

    client->preInitialize();
    if(client->event_read) {
        event_add(client->event_read, nullptr);
    }
    logMessage(LOG_QUERY, "Got new client from {}", client->getLoggingPeerIp() + ":" + to_string(client->getPeerPort()));
}

/* api */
inline deque<shared_ptr<QueryAccount>> query_accounts(sql::command& command) {
    deque<shared_ptr<QueryAccount>> result;
    command.query([&](int length, std::string* value, std::string* columns){
        auto entry = std::make_shared<PasswortedQueryAccount>();
        for(int index = 0; index < length; index++){
            try {
                if(columns[index] == "username")
                    entry->username = value[index];
                else if(columns[index] == "password")
                    entry->password = value[index];
                else if(columns[index] == "uniqueId")
                    entry->unique_id = value[index];
                else if(columns[index] == "server") {
                    entry->bound_server = value[index].empty() ? 0 : stoll(value[index]);
                }
            } catch (std::exception& ex) {
                logError(LOG_QUERY, "Failed to parse query account data for row {} ({})", columns[index], value[index]);
                return 0;
            }
        }

        result.push_back(entry);
        return 0;
    });

    return result;
}

std::shared_ptr<QueryAccount> QueryServer::create_query_account(const std::string &username, ts::ServerId server, const std::string &owner, const std::string &password) {
    LOG_SQL_CMD(sql::command(this->sql, "INSERT INTO `queries` (`username`, `password`, `uniqueId`, `server`) VALUES (:name, :password, :uid, :server)", variable{":name", username}, variable{":uid", owner}, variable{":password", password}, variable{":server", server}).execute());

    return this->find_query_account_by_name(username);
}

std::shared_ptr<PasswortedQueryAccount> QueryServer::load_password(const std::shared_ptr<QueryAccount> &account) {
    return dynamic_pointer_cast<PasswortedQueryAccount>(account);
}

bool QueryServer::delete_query_account(const std::shared_ptr<ts::server::QueryAccount> &account) {
    LOG_SQL_CMD(sql::command(this->sql, "DELETE FROM `queries` WHERE `username` = :username AND `server` = :server", variable{":username", account->username}, variable{":server", account->bound_server}).execute());

    return true;
}

std::deque<std::shared_ptr<QueryAccount>> QueryServer::list_query_accounts(OptionalServerId server_id) {
    auto command = (
            server_id != EmptyServerId ?
            sql::command(this->sql, "SELECT * FROM `queries` WHERE `server` = :server", variable{":server", server_id}) :
            sql::command(this->sql, "SELECT * FROM `queries`")
    );
    auto accounts = query_accounts(command);
    return accounts;
}

std::shared_ptr<QueryAccount> QueryServer::find_query_account_by_name(const std::string &name) {
    auto command = sql::command(this->sql, "SELECT * FROM `queries` WHERE `username` = :name", variable{":name", name});
    auto accounts = query_accounts(command);
    if(accounts.empty()) return nullptr;
    return accounts.back();
}

deque<shared_ptr<QueryAccount>> QueryServer::find_query_accounts_by_unique_id(const std::string &unique_id) {
    auto command = sql::command(this->sql, "SELECT * FROM `queries` WHERE `uniqueId` = :unique_id", variable{":name", unique_id});
    return query_accounts(command);
}

bool QueryServer::rename_query_account(const std::shared_ptr<ts::server::QueryAccount> &account, const std::string &new_name) {
    LOG_SQL_CMD(sql::command(this->sql, "UPDATE `queries` SET `username` = :new_name WHERE `username` = :old_name AND `server` = :server", variable{":new_name",new_name}, variable{":old_name", account->username}, variable{":server", account->bound_server}).execute());
    return true;
}

bool QueryServer::change_query_password(const std::shared_ptr<ts::server::QueryAccount> &account, const std::string &password) {
    LOG_SQL_CMD(sql::command(this->sql, "UPDATE `queries` SET `password` = :password WHERE `username` = :name AND `server` = :server", variable{":password", password}, variable{":name", account->username}, variable{":server", account->bound_server}).execute());
    return true;
}

void QueryServer::tick_clients() {
    decltype(this->connected_clients) connected_clients_;
    {
        lock_guard lock(this->connected_clients_mutex);
        connected_clients_ = this->connected_clients;
    }

    for(const auto& cl : connected_clients_) {
        cl->tick_query();
    }

    {
        std::lock_guard connect_lock{this->client_connect_mutex};

        std::vector<std::string> erase_bans{};
        erase_bans.reserve(32);

        for(auto& elm : this->client_connect_bans) {
            if(elm.second < system_clock::now()) {
                erase_bans.push_back(elm.first);
            }
        }

        for(const auto& ip : erase_bans) {
            this->client_connect_bans.erase(ip);
        }

        if(system_clock::now() - seconds(5) < client_connect_last_decrease) {
            this->client_connect_last_decrease = system_clock::now();

            std::vector<std::string> erase_attempts{};
            for(auto& elm : this->client_connect_count) {
                if(elm.second == 0) {
                    erase_attempts.push_back(elm.first);
                } else {
                    elm.second--;
                }
            }

            for(const auto& ip : erase_bans) {
                this->client_connect_count.erase(ip);
            }
        }
    }

    if(this->accept_event_deleted.time_since_epoch().count() != 0 && accept_event_deleted + seconds(5) < system_clock::now()) {
        debugMessage(LOG_QUERY, "Readding accept event and try again if we have enough resources again.");
        for(auto& binding : this->bindings) {
            event_add(binding->event_accept, nullptr);
        }
        accept_event_deleted = system_clock::time_point{};
    }
}

void QueryServer::tick_executor() {
    bool tick_clients;

    while(this->tick_active) {
        std::unique_lock tick_lock{this->tick_mutex};
        this->tick_notify.wait_until(tick_lock, this->tick_next_client_timestamp, [&]{
            return !this->tick_active || !this->tick_pending_disconnects.empty() || !this->tick_pending_connection_close.empty();
        });

        auto current_timestamp = std::chrono::system_clock::now();
        if(current_timestamp > this->tick_next_client_timestamp) {
            this->tick_next_client_timestamp = current_timestamp + std::chrono::milliseconds{500};
            tick_clients = true;
        } else {
            tick_clients = false;
        }

        auto pending_disconnects = std::move(this->tick_pending_disconnects);
        auto pending_closes = std::move(this->tick_pending_connection_close);

        if(!this->tick_active) {
            if(pending_disconnects.empty() && pending_closes.empty()) {
                /* We're done with our work */
                break;
            }
        }
        tick_lock.unlock();

        if(tick_clients) {
            this->tick_clients();
        }

        for(const auto& pending_disconnect : pending_disconnects) {
            auto client = pending_disconnect.lock();
            if(!client) {
                continue;
            }

            this->execute_query_disconnect(client, false);
        }

        for(const auto& pending_close : pending_closes) {
            auto client = pending_close.lock();
            if(!client) {
                continue;
            }

            this->execute_query_connection_close(client, true);
        }
    }
}

void QueryServer::enqueue_query_disconnect(const std::shared_ptr<QueryClient> &client) {
    std::lock_guard lock{this->tick_mutex};
    if(!this->tick_active) {
        logCritical(LOG_GENERAL, "Tried to close a query connection without an active query event loop.");
        return;
    }

    this->tick_pending_disconnects.push_back(client);
    this->tick_notify.notify_one();
}

void QueryServer::enqueue_query_connection_close(const std::shared_ptr<QueryClient> &client) {
    std::lock_guard lock{this->tick_mutex};
    if(!this->tick_active) {
        logCritical(LOG_GENERAL, "Tried to close a query connection without an active query event loop.");
        return;
    }

    this->tick_pending_connection_close.push_back(client);
    this->tick_notify.notify_one();
}

void QueryServer::execute_query_disconnect(const std::shared_ptr<QueryClient> &client, bool shutdown_disconnect) {
    {
        std::lock_guard state_lock{client->state_lock};
        if(client->state >= ConnectionState::DISCONNECTING) {
            /* client will already be disconnected */
            return;
        }

        client->state = ConnectionState::DISCONNECTING;
    }

    if(!shutdown_disconnect) {
        client->disconnect_from_virtual_server("");
    }

    {
        std::lock_guard network_lock{client->network_mutex};
        if(client->event_write) {
            event_add(client->event_write, nullptr);
        }
    }
}

void QueryServer::execute_query_connection_close(const std::shared_ptr<QueryClient> &client, bool warn_unknown_client) {
    {
        std::lock_guard state_lock{client->state_lock};
        if(client->state == ConnectionState::DISCONNECTED) {
            /* client has already been disconnected */
            return;
        }

        client->state = ConnectionState::DISCONNECTED;
    }

    client->disconnect_from_virtual_server("");
    client->execute_final_disconnect();

    {
        std::lock_guard client_lock{this->connected_clients_mutex};
        auto index = std::find(this->connected_clients.begin(), this->connected_clients.end(), client);
        if(index == this->connected_clients.end()) {
            if(warn_unknown_client) {
                logWarning(LOG_QUERY, "Closed the connection of an unknown/unregistered query.");
            }
            return;
        }

        this->connected_clients.erase(index);
        this->connected_client_disconnected_notify.notify_all();
    }
}