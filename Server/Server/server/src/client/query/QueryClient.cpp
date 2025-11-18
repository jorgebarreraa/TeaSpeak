#include <algorithm>
#include <src/server/QueryServer.h>
#include "QueryClient.h"
#include <netinet/tcp.h>
#include "src/InstanceHandler.h"
#include <pipes/errors.h>
#include <misc/std_unique_ptr.h>
#include <log/LogUtils.h>
#include "../../groups/GroupAssignmentManager.h"
#include "../../server/GlobalNetworkEvents.h"

using namespace std;
using namespace std::chrono;
using namespace ts;
using namespace ts::server;
using namespace ts::server::query;

#if defined(TCP_CORK) && !defined(TCP_NOPUSH)
    #define TCP_NOPUSH TCP_CORK
#endif

//#define DEBUG_TRAFFIC
NetworkBuffer* NetworkBuffer::allocate(size_t length) {
    auto result = (NetworkBuffer*) malloc(length + sizeof(NetworkBuffer));
    new (result) NetworkBuffer{};
    result->length = length;
    result->ref_count++;
    return result;
}

NetworkBuffer* NetworkBuffer::ref() {
    this->ref_count++;
    return this;
}

void NetworkBuffer::unref() {
    if(--this->ref_count == 0) {
        this->NetworkBuffer::~NetworkBuffer();
        ::free(this);
    }
}

QueryClient::QueryClient(QueryServer* handle, int sockfd) : ConnectedClient(handle->sql, nullptr), handle(handle), client_file_descriptor(sockfd) {
    memtrack::allocated<QueryClient>(this);
    int enabled = 1;
    int disabled = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &enabled, sizeof(enabled));
    if(setsockopt(sockfd, IPPROTO_TCP, TCP_NOPUSH, &disabled, sizeof disabled) < 0) {
        logError(this->getServerId(), "[Query] Could not disable nopush for {} ({}/{})", CLIENT_STR_LOG_PREFIX_(this), errno, strerror(errno));
    }

    if(setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &enabled, sizeof enabled) < 0) {
        logError(this->getServerId(), "[Query] Could not disable no delay for {} ({}/{})", CLIENT_STR_LOG_PREFIX_(this), errno, strerror(errno));
    }

    this->state = ConnectionState::CONNECTED;
    connectedTimestamp = system_clock::now();

    this->resetEventMask();
}

void QueryClient::initialize_weak_reference(const std::shared_ptr<ConnectedClient> &self) {
    ConnectedClient::initialize_weak_reference(self);

    this->command_queue = std::make_unique<ServerCommandQueue>(
            serverInstance->server_command_executor(),
            std::make_unique<QueryClientCommandHandler>(dynamic_pointer_cast<QueryClient>(self))
    );

    this->event_read = serverInstance->network_event_loop()->allocate_event(this->client_file_descriptor, EV_READ | EV_PERSIST, QueryClient::handle_event_read, this, nullptr);
    this->event_write = serverInstance->network_event_loop()->allocate_event(this->client_file_descriptor, EV_WRITE, QueryClient::handle_event_write, this, nullptr);
}

QueryClient::~QueryClient() {
    if(this->line_buffer) {
        ::free(this->line_buffer);
        this->line_buffer = nullptr;
    }

    this->ssl_handler.finalize();

    while(this->write_buffer_head) {
        auto buffer = std::exchange(this->write_buffer_head, this->write_buffer_head->next_buffer);
        buffer->unref();
    }
    this->write_buffer_tail = nullptr;

    memtrack::freed<QueryClient>(this);
}

void QueryClient::preInitialize() {
    this->properties()[property::CLIENT_TYPE] = ClientType::CLIENT_QUERY;
    this->properties()[property::CLIENT_TYPE_EXACT] = ClientType::CLIENT_QUERY;
    this->properties()[property::CLIENT_UNIQUE_IDENTIFIER] = "UnknownQuery";
    this->properties()[property::CLIENT_NICKNAME] = string() + "ServerQuery#" + this->getLoggingPeerIp() + "/" + to_string(this->getPeerPort());

    DatabaseHelper::assignDatabaseId(this->sql, this->getServerId(), this->ref());

    if(ts::config::query::sslMode == 0) {
        this->connectionType = ConnectionType::PLAIN;
        this->postInitialize();
    }
}

void QueryClient::postInitialize() {
    lock_guard command_lock(this->command_lock);
    this->connectTimestamp = system_clock::now();
    this->properties()[property::CLIENT_LASTCONNECTED] = duration_cast<seconds>(this->connectTimestamp.time_since_epoch()).count();

    if(ts::config::query::sslMode == 1 && this->connectionType != ConnectionType::SSL_ENCRYPTED) {
        command_result error{error::failed_connection_initialisation,  "Please use a SSL encryption!"};
        this->notifyError(error);
        error.release_data();
        this->disconnect("Please us a SSL encryption for more security.\nThe server denies also all other connections!");
        return;
    }

    send_message(config::query::motd);
    assert(this->handle);

    if(this->handle->ip_whitelist) {
        this->whitelisted = this->handle->ip_whitelist->contains(this->remote_address);
    } else {
        this->whitelisted = false;
    }

    if(!this->whitelisted && this->handle->ip_blacklist) {
        assert(this->handle->ip_blacklist);
        if(this->handle->ip_blacklist->contains(this->remote_address)) {
            Command cmd("error");
            auto err = findError("client_login_not_permitted");
            cmd["id"] = err.errorId;
            cmd["msg"] = err.message;
            cmd["extra_msg"] = "You're not permitted to use the query interface! (Your blacklisted)";
            this->sendCommand(cmd);
            this->disconnect("blacklisted");
            return;;
        }
    }
    debugMessage(LOG_QUERY, "Got new query client from {}. Whitelisted: {}", this->getLoggingPeerIp(), this->whitelisted);

    if(!this->whitelisted) {
        std::lock_guard connect_lock{this->handle->connected_clients_mutex};
        auto address = this->getPeerIp();
        if(this->handle->client_connect_bans.count(address) > 0) {
            auto ban = this->handle->client_connect_bans[address];
            Command cmd("error");
            auto err = findError("server_connect_banned");
            cmd["id"] = err.errorId;
            cmd["msg"] = err.message;
            cmd["extra_msg"] = "you may retry in " + to_string(duration_cast<seconds>(ban - system_clock::now()).count()) + " seconds";
            this->sendCommand(cmd);
            this->close_connection(std::chrono::system_clock::now() + std::chrono::seconds{1});
        }
    }

    this->task_update_needed_permissions.enqueue();
}

void QueryClient::send_message(const std::string_view& message) {
    if(this->state == ConnectionState::DISCONNECTED || !this->handle) {
        return;
    }

    if(this->connectionType == ConnectionType::PLAIN) {
        this->enqueue_write_buffer(message);
    } else if(this->connectionType == ConnectionType::SSL_ENCRYPTED) {
        this->ssl_handler.send(pipes::buffer_view{(void*) message.data(), message.length()});
    } else {
        logCritical(LOG_GENERAL, "Invalid query connection type to write to!");
    }
}


bool QueryClient::disconnect(const std::string &reason) {
    if(!reason.empty()) {
        ts::command_builder notify{"disconnect"};
        notify.put_unchecked(0, "reason", reason);
        this->sendCommand(notify, false);
    }

    return this->close_connection(system_clock::now() + seconds(1));
}

bool QueryClient::close_connection(const std::chrono::system_clock::time_point& flush_timeout_) {
    this->flush_timeout = flush_timeout_;

    bool should_flush = std::chrono::system_clock::now() < flush_timeout;
    {
        std::lock_guard network_lock{this->network_mutex};
        if(this->event_read) {
            event_del_noblock(this->event_read);
        }

        if(!should_flush && this->event_write) {
            event_del_noblock(this->event_write);
        }
    }

    if(should_flush) {
        this->handle->enqueue_query_disconnect(dynamic_pointer_cast<QueryClient>(this->ref()));
    } else {
        this->handle->enqueue_query_connection_close(dynamic_pointer_cast<QueryClient>(this->ref()));
    }

    return true;
}

void QueryClient::execute_final_disconnect() {
    assert(!this->server);

    {
        std::unique_lock network_lock{this->network_mutex};
        auto event_read_ = std::exchange(this->event_read, nullptr);
        auto event_write_ = std::exchange(this->event_write, nullptr);
        network_lock.unlock();

        if(event_read_) {
            event_del_block(event_read_);
            event_free(event_read_);
        }

        if(event_write_) {
            event_del_block(event_write_);
            event_free(event_write_);
        }
    }

    if(this->client_file_descriptor > 0) {
        if(shutdown(this->client_file_descriptor, SHUT_RDWR) < 0) {
            debugMessage(LOG_QUERY, "Could not shutdown query client socket! {} ({})", errno, strerror(errno));
        }

        if(close(this->client_file_descriptor) < 0) {
            debugMessage(LOG_QUERY, "Failed to close the query client socket! {} ({})", errno, strerror(errno));
        }

        this->client_file_descriptor = -1;
    }
}

void QueryClient::enqueue_write_buffer(const std::string_view &message) {
    auto buffer = NetworkBuffer::allocate(message.length());
    memcpy(buffer->data(), message.data(), message.length());

    {
        std::lock_guard buffer_lock{this->network_mutex};
        if(this->event_write) {
            *this->write_buffer_tail = buffer;
            this->write_buffer_tail = &buffer->next_buffer;

            event_add(this->event_write, nullptr);
            return;
        }
    }

    /* We don't have a network write event. Drop the buffer. */
    buffer->unref();
}

void QueryClient::handle_event_write(int fd, short, void *ptr_client) {
    auto client = (QueryClient*) ptr_client;

    NetworkBuffer* write_buffer{nullptr};
    {
        std::lock_guard buffer_lock{client->network_mutex};
        if(client->write_buffer_head) {
            write_buffer = client->write_buffer_head->ref();
        }
    }

    while(write_buffer) {
        auto length = send(fd, (const char*) write_buffer->data() + write_buffer->bytes_written, write_buffer->length - write_buffer->bytes_written, MSG_NOSIGNAL);
        if(length == -1) {
            write_buffer->unref();

            if (errno == EINTR || errno == EAGAIN) {
                std::lock_guard event_lock{client->network_mutex};
                if(client->event_write) {
                    event_add(client->event_write, nullptr);
                }
            } else {
                logError(LOG_QUERY, "{} Failed to send message ({}/{}). Closing connection.", client->getLoggingPrefix(), errno, strerror(errno));
                client->close_connection(std::chrono::system_clock::time_point{});

                {
                    std::unique_lock event_lock{client->network_mutex};
                    auto event_write_ = std::exchange(client->event_write, nullptr);
                    event_lock.unlock();

                    if(event_write_) {
                        event_del_noblock(event_write_);
                        event_free(event_write_);
                    }
                }

                /* the "this" ptr might be dangling now since we can't join the write event any more! */
            }

            return;
        }

        write_buffer->bytes_written += length;
        assert(write_buffer->bytes_written <= write_buffer->length);

        if(write_buffer->bytes_written == write_buffer->length) {
            /*
             * Even though we might free the buffer here we could still use the pointer for comparison.
             * If the buffer is still the head buffer it should not have been deallocated since
             * the queue itself holds a reference.
             */
            write_buffer->unref();

            /* Buffer must be freed, but we don't want do that while holding the lock */
            NetworkBuffer* cleanup_buffer{nullptr};

            /* Buffer finished, sending next one */
            {
                std::lock_guard buffer_lock{client->network_mutex};
                if(client->write_buffer_head == write_buffer) {
                    /* Buffer successfully send. Sending the next one. */
                    cleanup_buffer = client->write_buffer_head;

                    client->write_buffer_head = client->write_buffer_head->next_buffer;
                    if(client->write_buffer_head) {
                        /* we've a next buffer */
                        write_buffer = client->write_buffer_head->ref();
                    } else {
                        assert(client->write_buffer_tail == &write_buffer->next_buffer);
                        write_buffer = nullptr;
                        client->write_buffer_tail = &client->write_buffer_head;
                    }
                } else if(client->write_buffer_head) {
                    /* Our buffer got dropped (who knows why). Just send the next one. */
                    write_buffer = client->write_buffer_head->ref();
                } else {
                    /*
                     * Nothing more to write.
                     */
                    write_buffer = nullptr;
                }
            }

            if(cleanup_buffer) {
                cleanup_buffer->unref();
            }
        }
    }

    /* This state should only be reached when no more messages are pending to write */
    assert(!write_buffer);

    if(client->state == ConnectionState::DISCONNECTING) {
        client->handle->enqueue_query_connection_close(dynamic_pointer_cast<QueryClient>(client->ref()));
    }
}

/* The +1 to identify the syscall */
constexpr static auto kReadBufferLength{1024 *  + 1};
void QueryClient::handle_event_read(int fd, short, void *ptr_client) {
    uint8_t buffer[kReadBufferLength];

    auto client = (QueryClient*) ptr_client;

    auto length = read(fd, buffer, kReadBufferLength);
    if(length <= 0) {
        /* error handling */
        if(length < 0 && errno == EAGAIN) {
            /* Nothing to read */
            return;
        }

        if(length == 0) {
            logMessage(LOG_QUERY, "{} Connection closed. Client disconnected.", client->getLoggingPrefix());
        } else {
            logMessage(LOG_QUERY, "{} Failed to received message ({}/{}). Closing connection.", client->getLoggingPrefix(), errno, strerror(errno));
        }

        client->close_connection(std::chrono::system_clock::time_point{});
        {
            std::unique_lock network_lock{client->network_mutex};
            auto event_read_ = std::exchange(client->event_read, nullptr);
            network_lock.unlock();

            if(event_read_) {
                event_del_noblock(event_read_);
                event_free(event_read_);
            }
        }

        /* the "this" ptr might be dangling now since we can't join the read event any more! */
        return;
    }

    client->handle_message_read(std::string_view{(const char *) buffer, (size_t) length});
}

inline bool is_ssl_handshake_header(const std::string_view& buffer) {
    if(buffer.length() < 0x05) return false; //Header too small!

    if(buffer[0] != 0x16) return false; //recordType=handshake

    if(buffer[1] < 1 || buffer[1] > 3) return false; //SSL version
    if(buffer[2] < 1 || buffer[2] > 3) return false; //TLS version

    return true;
}

void QueryClient::handle_message_read(const std::string_view &message) {
    if(this->state >= ConnectionState::DISCONNECTING) {
        /* We don't need to handle any messages. */
        return;
    }

    switch (this->connectionType) {
        case ConnectionType::PLAIN:
            this->handle_decoded_message(message);
            break;

        case ConnectionType::SSL_ENCRYPTED:
            this->ssl_handler.process_incoming_data(pipes::buffer_view{message.data(), message.length()});
            break;

        case ConnectionType::UNKNOWN: {
            if(config::query::sslMode != 0 && is_ssl_handshake_header(message)) {
                this->initializeSSL();

                /*
                 * - Content
                 * \x16
                 * -Version (1)
                 * \x03 \x00
                 * - length (2)
                 * \x00 \x04
                 *
                 * - Header
                 * \x00 -> hello request (3)
                 * \x05 -> length (4)
                 */

                this->ssl_handler.process_incoming_data(pipes::buffer_view{message.data(), message.length()});
            } else {
                this->connectionType = ConnectionType::PLAIN;
                this->postInitialize();
                this->handle_decoded_message(message);
            }
        }
    }
}

inline size_t line_buffer_size(size_t target_size) {
    return target_size;
}

void QueryClient::handle_decoded_message(const std::string_view &message) {
    if(this->line_buffer_length + message.length() > this->line_buffer_capacity) {
        this->line_buffer_capacity = line_buffer_size(this->line_buffer_length + message.length());

        auto new_buffer = (char*) malloc(this->line_buffer_capacity);
        memcpy(new_buffer, this->line_buffer, this->line_buffer_length);
        free(this->line_buffer);
        this->line_buffer = new_buffer;
    }

    memcpy(this->line_buffer + this->line_buffer_length, message.data(), message.length());
    this->line_buffer_length += message.length();

    /*
     * Now we're analyzing the line buffer.
     * Note: Telnet commands will be executed as empty (idle commands)
     */
    size_t command_start_index{0}, command_end_index, command_start_next;
    for(; this->line_buffer_scan_offset < this->line_buffer_length; this->line_buffer_scan_offset++) {
        if(this->line_buffer[this->line_buffer_scan_offset] == '\n') {
            command_end_index = this->line_buffer_scan_offset;
            command_start_next = this->line_buffer_scan_offset + 1;
        } else if((uint8_t) this->line_buffer[this->line_buffer_scan_offset] == 255) {
            if(this->line_buffer_scan_offset + 3 > this->line_buffer_length) {
                /* We don't have enough space to fill the telnet command so we use that as the new scan offset */
                command_end_index = this->line_buffer_scan_offset;
                command_start_next = this->line_buffer_scan_offset;

                if(command_start_next == command_end_index) {
                    /* We've no prepended data so we're waiting for the tcp command. Loop finished. */
                    break;
                }
            } else {
                command_end_index = this->line_buffer_scan_offset;
                command_start_next = this->line_buffer_scan_offset + 3;

                logTrace(LOG_QUERY, "[{}:{}] Received telnet command, code = {}, option = {}",
                         this->getLoggingPeerIp(), this->getPeerPort(),
                         (uint8_t) this->line_buffer[this->line_buffer_scan_offset + 1],
                         (uint8_t) this->line_buffer[this->line_buffer_scan_offset + 2]
                );
            }
        } else {
            continue;
        }

        /* No need to check for the upper bounds since there will be \n or 255 before the end of the line */
        while(this->line_buffer[command_start_index] == '\r') {
            command_start_index++;
        }

        while(command_end_index > command_start_index + 1 && this->line_buffer[command_end_index - 1] == '\r') {
            command_end_index--;
        }

        std::string_view command_view{this->line_buffer + command_start_index, command_end_index - command_start_index};
        this->command_queue->enqueue_command_string(command_view);

        command_start_index = command_start_next;
        if(this->line_buffer_scan_offset + 1 < command_start_next) {
            this->line_buffer_scan_offset = command_start_next - 1;
        }
    }

    if(command_start_index > 0) {
        if(command_start_index == this->line_buffer_length) {
            this->line_buffer_length = 0;
            this->line_buffer_scan_offset = 0;
        } else {
            assert(this->line_buffer_length > command_start_index);
            assert(this->line_buffer_scan_offset > command_start_index);
            memcpy(this->line_buffer, this->line_buffer + command_start_index, this->line_buffer_length - command_start_index);
            this->line_buffer_length -= command_start_index;
            this->line_buffer_scan_offset -= command_start_index;
        }
    }

    if(this->line_buffer_length > ts::config::query::max_line_buffer) {
        this->line_buffer_length = 0; /* Buffer will be truncated later */
        logWarning(LOG_QUERY, "[{}] Client exceeded max query line buffer size. Disconnecting client.");
        this->disconnect("line buffer length exceeded");
    }

    /* Shrink if possible */
    if(this->line_buffer_capacity > 8 * 1024 && this->line_buffer_length < 8 * 1024) {
        this->line_buffer_capacity = 8 * 1024;
        auto new_buffer = (char*) malloc(this->line_buffer_capacity);
        memcpy(new_buffer, this->line_buffer, this->line_buffer_length);
        free(this->line_buffer);
        this->line_buffer = new_buffer;
    }
}

void QueryClient::initializeSSL() {
    this->connectionType = ConnectionType::SSL_ENCRYPTED;

    this->ssl_handler.direct_process(pipes::PROCESS_DIRECTION_OUT, true);
    this->ssl_handler.direct_process(pipes::PROCESS_DIRECTION_IN, true);

    this->ssl_handler.callback_data([&](const pipes::buffer_view& buffer) {
        this->handle_decoded_message(std::string_view{buffer.data_ptr<char>(), buffer.length()});
    });
    this->ssl_handler.callback_write([&](const pipes::buffer_view& buffer) {
        this->enqueue_write_buffer(std::string_view{buffer.data_ptr<char>(), buffer.length()});
    });
    this->ssl_handler.callback_initialized = std::bind(&QueryClient::postInitialize, this);

    this->ssl_handler.callback_error([&](int code, const std::string& message) {
        if(code == PERROR_SSL_ACCEPT) {
            this->disconnect("invalid accept");
        } else if(code == PERROR_SSL_TIMEOUT)
            this->disconnect("invalid accept (timeout)");
        else
            logError(LOG_QUERY, "Got unknown ssl error ({} | {})", code, message);
    });

    {
        auto context = serverInstance->sslManager()->getQueryContext();

        auto options = make_shared<pipes::SSL::Options>();
        options->type = pipes::SSL::SERVER;
        options->context_method = TLS_method();
        options->default_keypair({context->privateKey, context->certificate});
        if(!this->ssl_handler.initialize(options)) {
            logError(LOG_QUERY, "[{}] Failed to setup ssl!", CLIENT_STR_LOG_PREFIX);
        }
    }
}

void QueryClient::sendCommand(const ts::Command &command, bool) {
    auto cmd = command.build();
    send_message(cmd + config::query::newlineCharacter);
    logTrace(LOG_QUERY, "Send command {}", cmd);
}

void QueryClient::sendCommand(const ts::command_builder &command, bool) {
    send_message(command.build() + config::query::newlineCharacter);
    logTrace(LOG_QUERY, "Send command {}", command.build());
}

void QueryClient::tick_server(const std::chrono::system_clock::time_point &time) {
    ConnectedClient::tick_server(time);
}

/* FIXME: TODO: Forbit this while beeing in finalDisconnect! */
void QueryClient::tick_query() {
    if(this->idleTimestamp.time_since_epoch().count() > 0 && system_clock::now() - this->idleTimestamp > minutes(5)){
        debugMessage(LOG_QUERY, "Dropping client " + this->getLoggingPeerIp() + "|" + this->getDisplayName() + ". (Timeout)");
        this->close_connection(system_clock::now() + seconds(1));
    }

    if(this->connectionType == ConnectionType::UNKNOWN && system_clock::now() - milliseconds(500) > connectedTimestamp) {
        this->connectionType = ConnectionType::PLAIN;
        this->postInitialize();
    }

    if(this->flush_timeout.time_since_epoch().count() > 0 && std::chrono::system_clock::now() > this->flush_timeout) {
        this->handle->enqueue_query_connection_close(dynamic_pointer_cast<QueryClient>(this->ref()));
    }
}

bool QueryClient::ignoresFlood() {
    return this->whitelisted || ConnectedClient::ignoresFlood();
}

void QueryClient::disconnect_from_virtual_server(const std::string& reason) {
    std::lock_guard command_lock{this->command_lock};

    auto old_server = std::exchange(this->server, nullptr);
    if(old_server) {
        {
            std::unique_lock tree_lock{old_server->channel_tree_mutex};
            if(this->currentChannel) {
                old_server->client_move(this->ref(), nullptr, nullptr, "", ViewReasonId::VREASON_USER_ACTION, false, tree_lock);
            }

            old_server->unregisterClient(this->ref(), reason, tree_lock);
        }

        this->loadDataForCurrentServer();
    }

    {
        std::lock_guard channel_lock{this->channel_tree_mutex};
        this->channel_tree->reset();
        this->currentChannel = nullptr;
    }
}