//
// Created by WolverinDEV on 23/02/2020.
//

#include <csignal>
#include <netinet/tcp.h>
#include <event.h>
#include <ThreadPool/ThreadHelper.h>
#include <misc/endianness.h>
#include <shared/include/license/client.h>
#include <log/LogUtils.h>
#include "shared/include/license/client.h"
#include "crypt.h"

using namespace license::client;

LicenseServerClient::Buffer* LicenseServerClient::Buffer::allocate(size_t capacity) {
    static_assert(std::is_trivially_constructible<Buffer>::value);

    const auto allocated_bytes = sizeof(LicenseServerClient::Buffer) + capacity;
    auto result = malloc(allocated_bytes);
    if(!result) return nullptr;

    auto buffer = reinterpret_cast<LicenseServerClient::Buffer*>(result);
    buffer->capacity = capacity;
    buffer->fill = 0;
    buffer->offset = 0;
    buffer->data = (char*) result + sizeof(LicenseServerClient::Buffer);
    return buffer;
}

void LicenseServerClient::Buffer::free(Buffer *ptr) {
    static_assert(std::is_trivially_destructible<Buffer>::value);

    ::free(ptr);
}

LicenseServerClient::LicenseServerClient(const sockaddr_in &address, int pversion) : protocol_version{pversion} {
    memcpy(&this->network.address, &address, sizeof(address));
    TAILQ_INIT(&this->buffers.write);

    if(!this->buffers.read)
        this->buffers.read = Buffer::allocate(1024 * 8);
}

LicenseServerClient::~LicenseServerClient() {
    const auto is_event_loop = this->network.event_dispatch.get_id() == std::this_thread::get_id();
    {
        std::unique_lock slock{this->connection_lock};
        this->connection_state = ConnectionState::UNCONNECTED;
    }
    this->cleanup_network_resources(); /* force cleanup ignoring the previous state */
    if(is_event_loop) this->network.event_dispatch.detach();

    if(this->buffers.read)
        Buffer::free(this->buffers.read);
}

bool LicenseServerClient::start_connection(std::string &error) {
    bool event_dispatch_spawned{false};

    std::unique_lock slock{this->connection_lock};
    if(this->connection_state != ConnectionState::UNCONNECTED) {
        error = "invalid connection state";
        return false;
    }

    this->connection_state = ConnectionState::CONNECTING;
    this->communication.initialized = false;

    this->network.file_descriptor = socket(this->network.address.sin_family, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if(this->network.file_descriptor < 0) {
        error = "failed to allocate socket";
        goto error_cleanup;
    }

    signal(SIGPIPE, SIG_IGN);

    {
        auto connect_state = ::connect(this->network.file_descriptor, reinterpret_cast<const sockaddr *>(&this->network.address), sizeof(this->network.address));
        if(connect_state < 0 && errno != EINPROGRESS) {
            error = "connect() failed (" + std::string{strerror(errno)} + ")";
            goto error_cleanup;
        }
    }

    {
        int enabled{1}, disabled{0};
        if(setsockopt(this->network.file_descriptor, SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled)) < 0); //CERR("could not set reuse addr");
        if(setsockopt(this->network.file_descriptor, IPPROTO_TCP, TCP_CORK, &disabled, sizeof(disabled)) < 0); // CERR("could not set no push");

        if(fcntl(this->network.file_descriptor, F_SETFD, fcntl(this->network.file_descriptor, F_GETFL, 0)  | FD_CLOEXEC | O_NONBLOCK) < 0); // CERR("Failed to set FD_CLOEXEC and O_NONBLOCK (" + std::to_string(errno) + ")");
    }

    this->network.event_base = event_base_new();
    this->network.event_read = event_new(this->network.event_base, this->network.file_descriptor, EV_READ | EV_PERSIST, [](int, short e, void* _this) {
        auto client = reinterpret_cast<LicenseServerClient*>(_this);
        auto client_ref = client->weak_from_this().lock();
        if(!client_ref) {
            logCritical(0, "Network callback for expired client (E011)!");
            return;
        }
        client->callback_read(e);
        client_ref.reset();
    }, this);
    this->network.event_write = event_new(this->network.event_base, this->network.file_descriptor, EV_WRITE, [](int, short e, void* _this) {
        auto client = reinterpret_cast<LicenseServerClient*>(_this);
        auto client_ref = client->weak_from_this().lock();
        if(!client_ref) {
            logCritical(0, "Network callback for expired client (E012)!");
            return;
        }
        client->callback_write(e);
        client_ref.reset();
    }, this);

    this->network.event_dispatch = std::thread([&] {
        signal(SIGPIPE, SIG_IGN);

        event_add(this->network.event_read, nullptr);

        timeval connect_timeout{5, 0};
        event_add(this->network.event_write, &connect_timeout);

        auto event_base{this->network.event_base};
        event_base_loop(event_base, EVLOOP_NO_EXIT_ON_EMPTY);
        event_base_free(event_base);

        //this ptr might be dangling
    });

    return true;
    error_cleanup:
    this->cleanup_network_resources();
    this->connection_state = ConnectionState::UNCONNECTED;
    return false;
}

void LicenseServerClient::close_connection() {
    std::unique_lock slock{this->connection_lock};
    if(this->connection_state == ConnectionState::UNCONNECTED) return;
    this->connection_state = ConnectionState::UNCONNECTED;

    this->cleanup_network_resources();
}

void LicenseServerClient::cleanup_network_resources() {
    const auto is_event_loop = this->network.event_dispatch.get_id() == std::this_thread::get_id();

    if(this->network.event_read) {
        if(is_event_loop) event_del_noblock(this->network.event_read);
        else event_del_block(this->network.event_read);
        event_free(this->network.event_read);
        this->network.event_read = nullptr;
    }

    if(this->network.event_write) {
        if(is_event_loop) event_del_noblock(this->network.event_write);
        else event_del_block(this->network.event_write);
        event_free(this->network.event_write);
        this->network.event_write = nullptr;
    }

    if(this->network.event_base) {
        event_base_loopexit(this->network.event_base, nullptr);
        if(!is_event_loop)
            threads::save_join(this->network.event_dispatch, false);
        this->network.event_base = nullptr; /* event base has been saved by the event dispatcher and will be freed there */
    }

    if(this->network.file_descriptor) {
        ::close(this->network.file_descriptor);
        this->network.file_descriptor = 0;
    }

    {
        std::lock_guard block{this->buffers.lock};
        auto buffer = TAILQ_FIRST(&this->buffers.write);
        while(buffer) {
            auto next = TAILQ_NEXT(buffer, tail);
            Buffer::free(buffer);
            buffer = next;
        }
        TAILQ_INIT(&this->buffers.write);
        this->buffers.notify_empty.notify_all();
    }
}

void LicenseServerClient::callback_read(short events) {
    constexpr static auto buffer_size{1024};

    ssize_t read_bytes{0};
    char buffer[buffer_size];

    read_bytes = recv(this->network.file_descriptor, buffer, buffer_size, MSG_DONTWAIT);
    if(read_bytes <= 0) {
        if(errno == EAGAIN) return;
        std::unique_lock slock{this->connection_lock};

        std::string disconnect_reason{};
        bool disconnect_expected{false};
        switch (this->connection_state) {
            case ConnectionState::CONNECTING:
                disconnect_reason = "connect error (" + std::string{strerror(errno)} + ")";
                disconnect_expected = false;
                break;
            case ConnectionState::INITIALIZING:
            case ConnectionState::CONNECTED:
                disconnect_reason = "read error (" + std::string{strerror(errno)} + ")";
                disconnect_expected = false;
                break;
            case ConnectionState::DISCONNECTING:
                disconnect_expected = true;
                break;
            case ConnectionState::UNCONNECTED:
                return; /* we're obsolete */
        }

        if(auto callback{this->callback_disconnected}; callback) {
            slock.unlock();
            callback(disconnect_expected, disconnect_reason);
            slock.lock();
        }

        if(this->connection_state != ConnectionState::UNCONNECTED) {
            this->cleanup_network_resources();
            this->connection_state = ConnectionState::UNCONNECTED;
        }
        return;
    }

    this->handle_data(buffer, (size_t) read_bytes);
}

void LicenseServerClient::callback_write(short events) {
    bool add_write_event{this->connection_state == ConnectionState::DISCONNECTING};
    if(events & EV_TIMEOUT) {
        std::unique_lock slock{this->connection_lock};
        if(this->connection_state == ConnectionState::CONNECTING || this->connection_state == ConnectionState::INITIALIZING) {
            /* connect timeout */
            if(auto callback{this->callback_disconnected}; callback) {
                slock.unlock();
                callback(false, "connect timeout");
                slock.lock();
            }

            if(this->connection_state != ConnectionState::UNCONNECTED) {
                this->cleanup_network_resources();
                this->connection_state = ConnectionState::UNCONNECTED;
            }
        } else if(this->connection_state == ConnectionState::DISCONNECTING) {
            /* disconnect timeout */
            this->cleanup_network_resources();
            this->connection_state = ConnectionState::UNCONNECTED;
        }
        return;
    }

    if(events & EV_WRITE) {
        if(this->connection_state == ConnectionState::CONNECTING) {
            this->callback_socket_connected();
            if(this->connection_state == ConnectionState::UNCONNECTED) /* state may change in the callback */
                return;
        }

        ssize_t written_bytes{0};

        std::unique_lock block{this->buffers.lock};
        auto buffer = TAILQ_FIRST(&this->buffers.write);
        if(!buffer) {
            this->buffers.notify_empty.notify_all();
            return;
        }
        block.unlock();
        written_bytes = send(this->network.file_descriptor, (char*) buffer->data + buffer->offset, buffer->fill - buffer->offset, MSG_DONTWAIT);

        if(written_bytes <= 0) {
            if(errno == EAGAIN) goto readd_event;
            std::unique_lock slock{this->connection_lock};

            std::string disconnect_reason{};
            bool disconnect_expected{false};
            switch (this->connection_state) {
                case ConnectionState::CONNECTING:
                case ConnectionState::INITIALIZING:
                case ConnectionState::CONNECTED:
                    disconnect_reason = "write error (" + std::string{strerror(errno)} + ")";
                    disconnect_expected = false;
                    break;
                case ConnectionState::DISCONNECTING:
                    disconnect_expected = true;
                    break;
                case ConnectionState::UNCONNECTED:
                    return; /* we're obsolete */
            }
            if(auto callback{this->callback_disconnected}; callback) {
                slock.unlock();
                callback(disconnect_expected, disconnect_reason);
                slock.lock();
            }

            if(this->connection_state != ConnectionState::UNCONNECTED) {
                this->cleanup_network_resources();
                this->connection_state = ConnectionState::UNCONNECTED;
            }
            return;
        }

        buffer->offset += (size_t) written_bytes;
        if(buffer->offset >= buffer->fill) {
            assert(buffer->offset == buffer->fill);
            block.lock();
            TAILQ_REMOVE(&this->buffers.write, buffer, tail);
            if(!TAILQ_FIRST(&this->buffers.write)) {
                this->buffers.notify_empty.notify_all();
            } else {
                add_write_event = true;
            }
            block.unlock();
            Buffer::free(buffer);
        }
    }

    if(this->network.event_write && add_write_event) {
        readd_event:
        auto timeout = this->disconnect_timeout;
        if(timeout.time_since_epoch().count() == 0)
            event_add(this->network.event_write, nullptr);
        else {
            auto now = std::chrono::system_clock::now();
            struct timeval t{0, 1};
            if(now > timeout) {
                this->callback_write(EV_TIMEOUT);
                return;
            } else {
                auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(timeout - now);
                auto seconds = std::chrono::duration_cast<std::chrono::seconds>(microseconds);
                microseconds -= seconds;

                t.tv_usec = microseconds.count();
                t.tv_sec = seconds.count();
            }
            event_add(this->network.event_write, &t);
        }
    }
}

void LicenseServerClient::handle_data(void *recv_buffer, size_t length) {
    auto& buffer = this->buffers.read;
    assert(buffer);

    if(buffer->capacity - buffer->offset - buffer->fill < length) {
        if(buffer->capacity - buffer->fill > length) {
            memcpy(buffer->data, (char*) buffer->data + buffer->offset, buffer->fill);
            buffer->offset = 0;
        } else {
            auto new_buffer = Buffer::allocate(buffer->fill + length);
            memcpy(new_buffer->data, (char*) buffer->data + buffer->offset, buffer->fill);
            new_buffer->fill = buffer->fill;
            Buffer::free(buffer);
            buffer = new_buffer;
        }
    }
    auto buffer_ptr = (char*) buffer->data;
    auto& buffer_offset = buffer->offset;
    auto& buffer_length = buffer->fill;

    memcpy((char*) buffer_ptr + buffer_offset + buffer_length, recv_buffer, length);
    buffer_length += length;

    while(true) {
        if(buffer_length < sizeof(protocol::packet_header)) return;

        auto header = reinterpret_cast<protocol::packet_header*>(buffer_ptr + buffer_offset);
        if(header->length > 1024 * 8) {
            if(auto callback{this->callback_disconnected}; callback)
                callback(false, "received a too large message");
            this->disconnect("received too large message", std::chrono::system_clock::time_point{});
            return;
        }

        if(buffer_length < header->length + sizeof(protocol::packet_header)) return;

        this->handle_raw_packet(header->packetId, buffer_ptr + buffer_offset + sizeof(protocol::packet_header), header->length);
        if(this->connection_state == ConnectionState::UNCONNECTED) return; /* state may change while we're handing the packet */

        buffer_offset += header->length + sizeof(protocol::packet_header);
        buffer_length -= header->length + sizeof(protocol::packet_header);
    }
}

void LicenseServerClient::send_message(protocol::PacketType type, const void *payload, size_t size) {
    const auto packet_size = size + sizeof(protocol::packet_header);
    auto buffer = Buffer::allocate(packet_size);
    buffer->fill = packet_size;

    auto header = (protocol::packet_header*) buffer->data;
    header->length = packet_size;
    header->packetId = type;
    memcpy((char*) buffer->data + sizeof(protocol::packet_header), payload, size);
    if(this->communication.initialized)
        xorBuffer((char*) buffer->data + sizeof(protocol::packet_header), size, this->communication.crypt_key.data(), this->communication.crypt_key.length());

    std::lock_guard clock{this->connection_lock};
    if(this->connection_state == ConnectionState::UNCONNECTED || !this->network.event_write) {
        Buffer::free(buffer);
        return;
    }
    {
        std::lock_guard block{this->buffers.lock};
        TAILQ_INSERT_TAIL(&this->buffers.write, buffer, tail);
    }
    event_add(this->network.event_write, nullptr);
}

void LicenseServerClient::disconnect(const std::string &message, std::chrono::system_clock::time_point timeout) {
    auto now = std::chrono::system_clock::now();
    if(now > timeout)
        timeout = now + std::chrono::seconds{timeout.time_since_epoch().count() ? 1 : 0};

    std::unique_lock clock{this->connection_lock};
    if(this->connection_state == ConnectionState::DISCONNECTING) {
        this->disconnect_timeout = std::min(this->disconnect_timeout, timeout);
        if(this->network.event_write)
            event_add(this->network.event_write, nullptr); /* let the write update the timeout */
        return;
    }
    this->disconnect_timeout = timeout;

    if(this->connection_state != ConnectionState::INITIALIZING && this->connection_state != ConnectionState::CONNECTED) {
        clock.unlock();
        this->close_connection();
        return;
    }

    this->connection_state = ConnectionState::DISCONNECTING;
    if(this->network.event_read)
        event_del_noblock(this->network.event_read);
    clock.unlock();

    this->send_message(protocol::PACKET_DISCONNECT, message.data(), message.length());
}

bool LicenseServerClient::await_disconnect() {
    {
        std::lock_guard clock{this->connection_lock};
        if(this->connection_state != ConnectionState::DISCONNECTING)
            return this->connection_state == ConnectionState::UNCONNECTED;
    }
    /* state might change here, but when we're disconnected the write buffer will be empty */
    std::unique_lock block{this->buffers.lock};
    while(TAILQ_FIRST(&this->buffers.write))
        this->buffers.notify_empty.wait(block);

    return std::chrono::system_clock::now() <= this->disconnect_timeout;
}

void LicenseServerClient::callback_socket_connected() {
    {
        std::lock_guard clock{this->connection_lock};
        if(this->connection_state != ConnectionState::CONNECTING) return;
        this->connection_state = ConnectionState::INITIALIZING;
    }

    uint8_t handshakeBuffer[4];
    handshakeBuffer[0] = 0xC0;
    handshakeBuffer[1] = 0xFF;
    handshakeBuffer[2] = 0xEE;
    handshakeBuffer[3] = this->protocol_version;

    this->send_message(protocol::PACKET_CLIENT_HANDSHAKE, handshakeBuffer, 4);
}

void LicenseServerClient::handle_raw_packet(license::protocol::PacketType type, void * buffer, size_t length) {
    /* decrypt packet */
    if(this->communication.initialized)
        xorBuffer((char*) buffer, length, this->communication.crypt_key.data(), this->communication.crypt_key.length());

    if(type == protocol::PACKET_DISCONNECT) {
        if(auto callback{this->callback_disconnected}; callback)
            callback(false, std::string{(const char*) buffer, length});
        this->close_connection();
        return;
    }

    if(!this->communication.initialized) {
        if(type != protocol::PACKET_SERVER_HANDSHAKE) {
            if(auto callback{this->callback_disconnected}; callback)
                callback(false, "expected handshake packet");
            this->disconnect("expected handshake packet", std::chrono::system_clock::time_point{});
            return;
        }

        this->handle_handshake_packet(buffer, length);
        this->communication.initialized = true;
        return;
    }

    if(auto callback{this->callback_message}; callback)
        callback(type, buffer, length);
    else
        ; //TODO: Print error?
}

void LicenseServerClient::handle_handshake_packet(void *buffer, size_t length) {
    const auto data_ptr = (const char*) buffer;

    std::string error{};
    if(this->connection_state != ConnectionState::INITIALIZING) {
        error = "invalid protocol state";
        goto handle_error;
    }

    if(length < 5) {
        error = "invalid packet size";
        goto handle_error;
    }

    if((uint8_t) data_ptr[0] != 0xAF || (uint8_t) data_ptr[1] != 0xFE) {
        error = "invalid handshake signature";
        goto handle_error;
    }
    if((uint8_t) data_ptr[2] != this->protocol_version) {
        error = "Invalid license protocol version. Please update TeaSpeak!";
        goto handle_error;
    }

    {
        auto key_length = be2le16(data_ptr, 3);
        if(length < key_length + 5) {
            error = "invalid packet size";
            goto handle_error;
        }
        this->communication.crypt_key = std::string(data_ptr + 5, key_length);
        this->communication.initialized = true;
    }

    if(auto callback{this->callback_connected}; callback)
        callback();
    return;

    handle_error:
    if(auto callback{this->callback_disconnected}; callback)
        callback(false, error);
    this->disconnect(error, std::chrono::system_clock::time_point{});
}