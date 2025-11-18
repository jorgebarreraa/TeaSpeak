//
// Created by WolverinDEV on 04/05/2020.
//

#include <event.h>
#include <cassert>
#include <netinet/tcp.h>
#include <log/LogUtils.h>
#include <misc/net.h>
#include <misc/base64.h>
#include <include/files/Config.h>
#include "./LocalFileProvider.h"
#include "./duration_utils.h"
#include "./HTTPUtils.h"
#include "./LocalFileTransfer.h"

#if defined(TCP_CORK) && !defined(TCP_NOPUSH)
    #define TCP_NOPUSH TCP_CORK
#endif

using namespace ts::server::file;
using namespace ts::server::file::transfer;

#define MAX_HTTP_HEADER_SIZE (4096)

inline void add_network_event(FileClient& transfer, event* ev, bool& ev_throttle_readd_flag, bool ignore_bandwidth) {
    timeval tv{0, 1}, *ptv{nullptr};
    {
        auto timeout = transfer.networking.disconnect_timeout;
        if(timeout.time_since_epoch().count() > 0) {
            auto now = std::chrono::system_clock::now();
            if(now < timeout) {
                auto duration = timeout - now;

                auto seconds = std::chrono::duration_cast<std::chrono::seconds>(timeout - now);
                duration -= seconds;

                auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(timeout - now);

                tv.tv_sec = seconds.count();
                tv.tv_usec = microseconds.count();
            }
            ptv = &tv;
        }
    }

    if(!ignore_bandwidth) {
        if(ev_throttle_readd_flag) return; /* we're already throttled */

        timeval ttv{};
        if(transfer.networking.throttle.should_throttle(ttv)) {
            if(transfer.networking.event_throttle)
                event_add(transfer.networking.event_throttle, &ttv);
            ev_throttle_readd_flag = true;
            return;
        }
    }

    event_add(ev, ptv);
}

void FileClient::add_network_read_event(bool ignore_bandwidth) {
    std::shared_lock slock{this->state_mutex};

    switch (this->state) {
        case STATE_FLUSHING:
        case STATE_DISCONNECTED:
            return;

        case STATE_AWAITING_KEY:
        case STATE_TRANSFERRING:
            break;

        default:
            assert(false);
            return;
    }
    if(this->state != STATE_AWAITING_KEY && this->state != STATE_TRANSFERRING)
        return;

    add_network_event(*this, this->networking.event_read, this->networking.add_event_read, ignore_bandwidth);
}

void FileClient::add_network_write_event(bool ignore_bandwidth) {
    std::shared_lock slock{this->state_mutex};
    this->add_network_write_event_nolock(ignore_bandwidth);
}

void FileClient::add_network_write_event_nolock(bool ignore_bandwidth) {
    switch (this->state) {
        case STATE_DISCONNECTED:
            return;

        case STATE_FLUSHING:
            /* flush our write buffer */
            break;

        case STATE_AWAITING_KEY:
            if(this->networking.protocol != FileClient::PROTOCOL_HTTPS) {
                assert(false);
                return;
            }
            break;

        case STATE_TRANSFERRING:
            break;

        default:
            assert(false);
            break;
    }

    add_network_event(*this, this->networking.event_write, this->networking.add_event_write, ignore_bandwidth);
}

bool FileClient::send_file_bytes(const void *snd_buffer, size_t size) {
    if(this->networking.protocol == FileClient::PROTOCOL_TS_V1) {
        return this->enqueue_network_buffer_bytes(snd_buffer, size);
    } else if(this->networking.protocol == FileClient::PROTOCOL_HTTPS) {
        this->networking.pipe_ssl.send(pipes::buffer_view{snd_buffer, size});
        return this->network_buffer.bytes > TRANSFER_MAX_CACHED_BYTES;
    } else {
        return false;
    }
}

bool FileClient::enqueue_network_buffer_bytes(const void *snd_buffer, size_t size) {
    auto tbuffer = allocate_buffer(size);
    tbuffer->length = size;
    tbuffer->offset = 0;
    memcpy(tbuffer->data, snd_buffer, size);

    size_t buffer_size;
    {
        std::lock_guard block{this->network_buffer.mutex};
        if(this->network_buffer.write_disconnected) {
            goto write_disconnected;
        }

        *this->network_buffer.buffer_tail = tbuffer;
        this->network_buffer.buffer_tail = &tbuffer->next;

        buffer_size = (this->network_buffer.bytes += size);
    }

    this->add_network_write_event(false);
    return buffer_size > TRANSFER_MAX_CACHED_BYTES;

    write_disconnected:
    deref_buffer(tbuffer);
    return false;
}

size_t FileClient::flush_network_buffer() {
    Buffer* current_head;
    size_t bytes;
    {
        std::lock_guard block{this->network_buffer.mutex};

        this->network_buffer.write_disconnected = true;
        bytes = std::exchange(this->network_buffer.bytes, 0);
        current_head = std::exchange(this->network_buffer.buffer_head, nullptr);
        this->network_buffer.buffer_tail = &this->network_buffer.buffer_head;
    }

    while(current_head) {
        auto next = current_head->next;
        deref_buffer(current_head);
        current_head = next;
    }

    return bytes;
}

NetworkingStartResult LocalFileTransfer::start_networking() {
    std::lock_guard nlock{this->network.mutex};
    assert(!this->network.active);

    this->network.active = true;
    this->network.event_base = event_base_new();
    if(!this->network.event_base) return NetworkingStartResult::OUT_OF_MEMORY;

    this->network.dispatch_thread = std::thread(&LocalFileTransfer::dispatch_loop_network, this);
    return NetworkingStartResult::SUCCESS;
}

NetworkingBindResult LocalFileTransfer::add_network_binding(const NetworkBinding &binding) {
    std::lock_guard nlock{this->network.mutex};
    if(!this->network.active)
        return NetworkingBindResult::NETWORKING_NOT_INITIALIZED;

    for(const auto& abinding : this->network.bindings) {
        if(net::address_equal(abinding->address, binding.address) && net::port(abinding->address) == net::port(binding.address))
            return NetworkingBindResult::BINDING_ALREADY_EXISTS;
    }

    NetworkingBindResult result{NetworkingBindResult::SUCCESS};
    auto abinding = std::make_shared<ActiveNetworkBinding>();
    abinding->handle = this;
    abinding->hostname = binding.hostname;
    memcpy(&abinding->address, &binding.address, sizeof(binding.address));

    abinding->file_descriptor = socket(abinding->address.ss_family, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if(!abinding->file_descriptor) {
        //logWarning(LOG_FT, "Failed to allocate socket for {}: {}/{}", abinding->hostname, errno, strerror(errno));
        return NetworkingBindResult::FAILED_TO_ALLOCATE_SOCKET;
    }


    int enable = 1, disabled = 0;

    if (setsockopt(abinding->file_descriptor, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
        logWarning(LOG_FT, "Failed to activate SO_REUSEADDR for binding {} ({}/{})", abinding->hostname, errno, strerror(errno));

    if(setsockopt(abinding->file_descriptor, IPPROTO_TCP, TCP_NOPUSH, &disabled, sizeof disabled) < 0)
        logWarning(LOG_FT, "Failed to deactivate TCP_NOPUSH for binding {} ({}/{})", abinding->hostname, errno, strerror(errno));

    if(abinding->address.ss_family == AF_INET6) {
        if(setsockopt(abinding->file_descriptor, IPPROTO_IPV6, IPV6_V6ONLY, &enable, sizeof(int)) < 0)
            logWarning(LOG_FT, "Failed to activate IPV6_V6ONLY for IPv6 binding {} ({}/{})", abinding->hostname, errno, strerror(errno));
    }
    if(fcntl(abinding->file_descriptor, F_SETFD, FD_CLOEXEC) < 0)
        logWarning(LOG_FT, "Failed to set flag FD_CLOEXEC for binding {} ({}/{})", abinding->hostname, errno, strerror(errno));


    if (bind(abinding->file_descriptor, (struct sockaddr *) &abinding->address, sizeof(abinding->address)) < 0) {
        //logError(LOG_FT, "Failed to bind server to {}. (Failed to bind socket: {}/{})", binding->hostname, errno, strerror(errno));
        result = NetworkingBindResult::FAILED_TO_BIND;
        goto reset_binding;
    }

    if (listen(abinding->file_descriptor, 8) < 0) {
        //logError(LOG_FT, "Failed to bind server to {}. (Failed to listen: {}/{})", binding->hostname, errno, strerror(errno));
        result = NetworkingBindResult::FAILED_TO_LISTEN;
        goto reset_binding;
    }

    abinding->accept_event = event_new(this->network.event_base, abinding->file_descriptor, (unsigned) EV_READ | (unsigned) EV_PERSIST, &LocalFileTransfer::callback_transfer_network_accept, &*abinding);
    if(!abinding->accept_event) {
        result = NetworkingBindResult::OUT_OF_MEMORY;
        goto reset_binding;
    }

    event_add(abinding->accept_event, nullptr);
    logMessage(LOG_FT, "Started to listen on {}:{}", abinding->hostname, net::port(abinding->address));
    this->network.bindings.push_back(std::move(abinding));

    return NetworkingBindResult::SUCCESS;

    reset_binding:
    if(abinding->accept_event) {
        event_free(abinding->accept_event);
        abinding->accept_event = nullptr;
    }

    if(abinding->file_descriptor > 0)
        ::close(abinding->file_descriptor);
    abinding->file_descriptor = 0;
    abinding->handle = nullptr;
    return result;
}

std::vector<NetworkBinding> LocalFileTransfer::active_network_bindings() {
    std::lock_guard nlock{this->network.mutex};
    std::vector<NetworkBinding> result{};
    result.reserve(this->network.bindings.size());

    for(const auto& binding : this->network.bindings) {
        auto& rbinding = result.emplace_back();
        rbinding.hostname = binding->hostname;
        memcpy(&rbinding.address, &binding->address, sizeof(rbinding.address));
    }

    return result;
}

NetworkingUnbindResult LocalFileTransfer::remove_network_binding(const NetworkBinding &binding) {
    std::lock_guard nlock{this->network.mutex};
    std::shared_ptr<ActiveNetworkBinding> abinding{};
    for(auto it = this->network.bindings.begin(); it != this->network.bindings.end(); it++) {
        abinding = *it;

        if(net::address_equal(abinding->address, binding.address) && net::port(abinding->address) == net::port(binding.address)) {
            this->network.bindings.erase(it);
            break;
        }
        abinding = nullptr;
    }

    if(!abinding)
        return NetworkingUnbindResult::UNKNOWN_BINDING;

    if(abinding->accept_event) {
        event_del_block(abinding->accept_event);
        event_free(abinding->accept_event);
        abinding->accept_event = nullptr;
    }

    if(abinding->file_descriptor > 0)
        ::close(abinding->file_descriptor);
    abinding->file_descriptor = 0;
    abinding->handle = nullptr;

    return NetworkingUnbindResult::SUCCESS;
}

void LocalFileTransfer::shutdown_networking() {
    event_base* ev_base;
    std::thread dispatch_thread{};
    {
        std::lock_guard nlock{this->network.mutex};
        if(!this->network.active) return;
        this->network.active = false;

        for(auto& binding : this->network.bindings) {
            if(binding->accept_event) {
                event_del_block(binding->accept_event);
                event_free(binding->accept_event);
                binding->accept_event = nullptr;
            }

            if(binding->file_descriptor > 0)
                ::close(binding->file_descriptor);
            binding->file_descriptor = 0;
            binding->handle = nullptr;
        }
        this->network.bindings.clear();

        ev_base = std::exchange(this->network.event_base, nullptr);
        std::swap(this->network.dispatch_thread, dispatch_thread);
    }

    {
        std::unique_lock tlock{this->transfers_mutex};
        auto transfers = this->transfers_;
        tlock.unlock();
        for(const auto& transfer : transfers) {
            std::unique_lock slock{transfer->state_mutex};
            this->disconnect_client(transfer, slock, false);
        }
    }

    if(ev_base)
        event_base_loopbreak(ev_base);

    if(dispatch_thread.joinable())
        dispatch_thread.join();

    if(ev_base)
        event_base_free(ev_base);
}

void LocalFileTransfer::dispatch_loop_network(void *provider_ptr) {
    auto provider = reinterpret_cast<LocalFileTransfer*>(provider_ptr);

    while(provider->network.active) {
        assert(provider->network.event_base);
        event_base_loop(provider->network.event_base, EVLOOP_NO_EXIT_ON_EMPTY);
    }
}

NetworkInitializeResult LocalFileTransfer::initialize_networking(const std::shared_ptr<FileClient> &client, int file_descriptor) {
    client->networking.file_descriptor = file_descriptor;

    client->networking.event_read = event_new(this->network.event_base, file_descriptor, EV_READ, &LocalFileTransfer::callback_transfer_network_read, &*client);
    client->networking.event_write = event_new(this->network.event_base, file_descriptor, EV_WRITE, &LocalFileTransfer::callback_transfer_network_write, &*client);
    client->networking.event_throttle = evtimer_new(this->network.event_base, &LocalFileTransfer::callback_transfer_network_throttle, &*client);

    if(!client->networking.event_read || !client->networking.event_write || !client->networking.event_throttle)
        goto oom_exit;

    client->add_network_read_event(true);

    client->timings.connected = std::chrono::system_clock::now();
    client->timings.last_write = client->timings.connected;
    client->timings.last_read = client->timings.connected;

    return NetworkInitializeResult::SUCCESS;

    oom_exit:
    if(auto event{std::exchange(client->networking.event_read, nullptr)}; event)
        event_free(event);
    if(auto event{std::exchange(client->networking.event_write, nullptr)}; event)
        event_free(event);
    if(auto event{std::exchange(client->networking.event_throttle, nullptr)}; event)
        event_free(event);

    return NetworkInitializeResult::OUT_OF_MEMORY;
}

void LocalFileTransfer::finalize_networking(const std::shared_ptr<FileClient> &client, std::unique_lock<std::shared_mutex>& state_lock) {
    assert(state_lock.owns_lock());

    auto ev_read = std::exchange(client->networking.event_read, nullptr);
    auto ev_write = std::exchange(client->networking.event_write, nullptr);
    auto ev_throttle = std::exchange(client->networking.event_throttle, nullptr);

    state_lock.unlock();
    if (ev_read) {
        event_del_block(ev_read);
        event_free(ev_read);
    }
    if (ev_write) {
        event_del_block(ev_write);
        event_free(ev_write);
    }
    if (ev_throttle) {
        event_del_block(ev_throttle);
        event_free(ev_throttle);
    }
    state_lock.lock();

    if (client->networking.file_descriptor > 0) {
        ::shutdown(client->networking.file_descriptor, SHUT_RDWR);
        ::close(client->networking.file_descriptor);
    }
    client->networking.file_descriptor = 0;
}

#if 0
void dp_log(void* ptr, pipes::Logger::LogLevel level, const std::string& name, const std::string& message, ...) {
    auto max_length = 1024 * 8;
    char buffer[max_length];

    va_list args;
    va_start(args, message);
    max_length = vsnprintf(buffer, max_length, message.c_str(), args);
    va_end(args);

    debugMessage(LOG_GENERAL, "[{}][{}] {}", level, name, std::string{buffer});
}
#endif


bool LocalFileTransfer::initialize_client_ssl(const std::shared_ptr<FileClient> &client) {
    std::string error;

    auto& ssl_pipe = client->networking.pipe_ssl;

    std::shared_ptr<pipes::SSL::Options> options{};
    auto ssl_option_supplier = config::ssl_option_supplier;
    if(!ssl_option_supplier || !(options = ssl_option_supplier())) {
        logError(0, "{} Failed to initialize client SSL pipe because we've no SSL options.", client->log_prefix());

        client->flush_network_buffer(); /* invalidate all network write operations */
        std::unique_lock slock{client->state_mutex};
        client->handle->disconnect_client(client, slock, true);
        return false;
    }

    if(!ssl_pipe.initialize(options, error)) {
        logWarning(0, "{} Failed to initialize client SSL pipe ({}). Disconnecting client.", client->log_prefix(), error);

        client->flush_network_buffer(); /* invalidate all network write operations */
        std::unique_lock slock{client->state_mutex};
        client->handle->disconnect_client(client, slock, true);
        return false;
    }

#if 0
    auto logger = std::make_shared<pipes::Logger>();
    logger->callback_log = dp_log;
    ssl_pipe.logger(logger);
#endif

    ssl_pipe.direct_process(pipes::PROCESS_DIRECTION_IN, true);
    ssl_pipe.direct_process(pipes::PROCESS_DIRECTION_OUT, true);
    ssl_pipe.callback_initialized = [client] {
        logTrace(LOG_FT, "{} SSL layer has been initialized", client->log_prefix());
    };

    ssl_pipe.callback_data([&, client](const pipes::buffer_view& message) {
        client->handle->handle_transfer_read(client, message.data_ptr<char>(), message.length());
    });

    ssl_pipe.callback_error([client](int error, const std::string & error_detail) {
        logMessage(LOG_FT, "{} Received SSL error ({}/{}). Closing connection.", client->log_prefix(), error, error_detail);

        std::unique_lock slock{client->state_mutex};
        client->handle->disconnect_client(client, slock, false);
    });

    ssl_pipe.callback_write([client](const pipes::buffer_view& buffer) {
        client->enqueue_network_buffer_bytes(buffer.data_ptr(), buffer.length());
        client->add_network_write_event(false);
    });

    return true;
}

void LocalFileTransfer::finalize_client_ssl(const std::shared_ptr<FileClient> &client) {
    auto& ssl_pipe = client->networking.pipe_ssl;

    ssl_pipe.callback_initialized = []{};
    ssl_pipe.callback_write([](const pipes::buffer_view&){});
    ssl_pipe.callback_error([](auto, const auto&){});
    ssl_pipe.callback_data([](const auto&){});
}

void LocalFileTransfer::callback_transfer_network_accept(int fd, short, void *ptr_binding) {
    auto binding = reinterpret_cast<ActiveNetworkBinding*>(ptr_binding);
    auto transfer = binding->handle;

    sockaddr_storage address{};
    socklen_t address_length{sizeof(address)};
    auto client_fd = ::accept4(fd, reinterpret_cast<sockaddr*>(&address), &address_length, 0); //SOCK_NONBLOCK
    if(client_fd <= 0) {
        /* TODO: Reserve one file descriptor in case of out of file descriptors (see current implementation) */
        logError(LOG_FT, "Failed to accept new client: {}/{}", errno, strerror(errno));
        return;
    }

    int enabled = 1;
    if(setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &enabled, sizeof enabled) < 0)
        logError(LOG_FT, "Cant enable TCP no delay for socket {}. Error: {}/{}", client_fd, errno, strerror(errno));

    auto client = std::make_shared<FileClient>(transfer);
    memcpy(&client->networking.address, &address, sizeof(sockaddr_storage));

    logMessage(LOG_FT, "{} Connection received.", client->log_prefix());
    auto ninit = transfer->initialize_networking(client, client_fd);
    switch(ninit) {
        case NetworkInitializeResult::SUCCESS: {
            std::lock_guard tlock{transfer->transfers_mutex};
            transfer->transfers_.push_back(std::move(client));
            return;
        }
        case NetworkInitializeResult::OUT_OF_MEMORY:
            client->state = FileClient::STATE_DISCONNECTED; /* required else the deallocate assert will fail */
            logError(LOG_FT, "{} Failed to initialize transfer client because we ran out of memory. Closing connection.",  client->log_prefix());
            ::close(client_fd);
            return;

        default:
            client->state = FileClient::STATE_DISCONNECTED; /* required else the deallocate assert will fail */
            logError(LOG_FT, "{} Failed to initialize transfer client. Closing connection.", client->log_prefix());
            ::close(client_fd);
            return;
    }
}

void LocalFileTransfer::callback_transfer_network_throttle(int, short, void *ptr_transfer) {
    auto transfer = reinterpret_cast<FileClient*>(ptr_transfer);

    if(std::exchange(transfer->networking.add_event_write, false))
        transfer->add_network_write_event(true);

    if(std::exchange(transfer->networking.add_event_read, false))
        transfer->add_network_read_event(true);
}

void LocalFileTransfer::callback_transfer_network_read(int fd, short events, void *ptr_transfer) {
    auto transfer = reinterpret_cast<FileClient*>(ptr_transfer);
    std::shared_ptr<FileClient> client{};
    try {
        client = transfer->shared_from_this();
    } catch (std::bad_weak_ptr& ex) {
        logCritical(LOG_FT, "Network read worker encountered a bad weak ptr to a client. This indicated something went horribly wrong! Please submit this on https://forum.teaspeak.de !!!");
        return;
    }

    if((unsigned) events & (unsigned) EV_TIMEOUT) {
        /* should never happen, receive timeouts are done via the client tick */
    }

    if((unsigned) events & (unsigned) EV_READ) {
        constexpr size_t buffer_size{4096};
        char buffer[buffer_size];

        while(true) {
            const auto max_read_buffer = transfer->networking.throttle.bytes_left();
            if(!max_read_buffer) break; /* network throttle */

            errno = 0;
            auto read = ::recv(fd, buffer, std::min(buffer_size, std::min(max_read_buffer, transfer->networking.max_read_buffer_size)), MSG_NOSIGNAL | MSG_DONTWAIT);
            //logTrace(0, "Read {}, max {} | {}", read, std::min(buffer_size, std::min(max_read_buffer, transfer->networking.max_read_buffer_size)), max_read_buffer);
            if(read <= 0) {
                if(read == 0) {
                    std::unique_lock slock{transfer->state_mutex};
                    auto original_state = transfer->state;
                    transfer->handle->disconnect_client(client, slock, true);
                    slock.unlock();

                    switch(original_state) {
                        case FileClient::STATE_AWAITING_KEY:
                            logMessage(LOG_FT, "{} Disconnected without sending any key or initializing a transfer.", transfer->log_prefix());
                            break;
                        case FileClient::STATE_TRANSFERRING: {
                            assert(transfer->transfer);

                            /* since we're counting the bytes directly on read, a disconnect would mean that not all data has been transferred */
                            if(transfer->transfer->direction == Transfer::DIRECTION_UPLOAD) {
                                logMessage(LOG_FT, "{} Disconnected while receiving file. Received {} out of {} bytes",
                                           transfer->log_prefix(), transfer->statistics.file_transferred.total_bytes, transfer->transfer->expected_file_size - transfer->transfer->file_offset);
                            } else if(transfer->transfer->direction == Transfer::DIRECTION_DOWNLOAD) {
                                logMessage(LOG_FT, "{} Disconnected while sending file. Send {} out of {} bytes",
                                           transfer->log_prefix(), transfer->statistics.file_transferred.total_bytes, transfer->transfer->expected_file_size - transfer->transfer->file_offset);
                            }

                            transfer->handle->invoke_aborted_callback(client, { TransferError::UNEXPECTED_CLIENT_DISCONNECT, "" });
                            break;
                        }
                        case FileClient::STATE_FLUSHING:
                            logMessage(LOG_FT, "{} Remote client closed connection. Finalizing disconnect.", transfer->log_prefix());
                            break;

                        case FileClient::STATE_DISCONNECTED:
                        default:
                            break;
                    }
                } else {
                    if(errno == EAGAIN) {
                        transfer->add_network_read_event(false);
                        return;
                    }

                    std::unique_lock slock{transfer->state_mutex};
                    auto original_state = transfer->state;
                    transfer->handle->disconnect_client(client, slock, true);
                    slock.unlock();

                    switch(original_state) {
                        case FileClient::STATE_AWAITING_KEY:
                            logMessage(LOG_FT, "{} Received a read error for an unauthenticated client: {}/{}. Closing connection.", transfer->log_prefix(), errno, strerror(errno));
                            break;
                        case FileClient::STATE_TRANSFERRING:
                            assert(transfer->transfer);

                            /* since we're counting the bytes directly on read, a disconnect would mean that not all data has been transferred */
                            if(transfer->transfer->direction == Transfer::DIRECTION_UPLOAD) {
                                logMessage(LOG_FT, "{} Received read error while receiving file. Received {} out of {} bytes: {}/{}",
                                           transfer->log_prefix(), transfer->statistics.file_transferred.total_bytes, transfer->transfer->expected_file_size - transfer->transfer->file_offset, errno, strerror(errno));
                            } else if(transfer->transfer->direction == Transfer::DIRECTION_DOWNLOAD) {
                                logMessage(LOG_FT, "{} Received read error while sending file. Send {} out of {} bytes: {}/{}",
                                           transfer->log_prefix(), transfer->statistics.file_transferred.total_bytes, transfer->transfer->expected_file_size - transfer->transfer->file_offset, errno, strerror(errno));
                            }


                            transfer->handle->invoke_aborted_callback(client, { TransferError::NETWORK_IO_ERROR, strerror(errno) });
                            break;
                        case FileClient::STATE_FLUSHING:
                        case FileClient::STATE_DISCONNECTED:
                        default:
                            break;
                    }
                    return;
                }
                return;
            } else {
                transfer->timings.last_read = std::chrono::system_clock::now();
                transfer->statistics.network_received.increase_bytes(read);
                bool throttle_read = transfer->networking.throttle.increase_bytes(read);

                if(transfer->state != FileClient::STATE_AWAITING_KEY && !(transfer->state == FileClient::STATE_TRANSFERRING && transfer->transfer->direction == Transfer::DIRECTION_UPLOAD)) {
                    debugMessage(LOG_FT, "{} Received {} bytes without any need. Dropping them.", transfer->log_prefix(), read);
                    return;
                }

                size_t bytes_buffered{0};
                if(transfer->state == FileClient::STATE_AWAITING_KEY) {
                    bytes_buffered = transfer->handle->handle_transfer_read_raw(client, buffer, read);
                } else if(transfer->state == FileClient::STATE_TRANSFERRING) {
                    if(transfer->transfer->direction == Transfer::DIRECTION_UPLOAD) {
                        bytes_buffered = transfer->handle->handle_transfer_read_raw(client, buffer, read);
                    } else {
                        debugMessage(LOG_FT, "{} Received {} bytes without any need. Dropping them.", transfer->log_prefix(), read);
                    }
                }

                if(transfer->state == FileClient::STATE_FLUSHING || transfer->state == FileClient::STATE_DISCONNECTED)
                    break;

                if(bytes_buffered > TRANSFER_MAX_CACHED_BYTES) {
                    transfer->network_buffer.buffering_stopped = true;
                    debugMessage(LOG_FT, "{} Stopping network read, temp buffer full.", transfer->log_prefix());
                    return; /* no read event readd, buffer filled */
                }

                if(throttle_read)
                    break;
            }
        }

        transfer->add_network_read_event(false); /* read event is not persistent */
    }
}

void LocalFileTransfer::callback_transfer_network_write(int fd, short events, void *ptr_transfer) {
    auto transfer = reinterpret_cast<FileClient*>(ptr_transfer);
    std::shared_ptr<FileClient> client{};
    try {
        client = transfer->shared_from_this();
    } catch (std::bad_weak_ptr& ex) {
        logCritical(LOG_FT, "Network write worker encountered a bad weak ptr to a client. This indicated something went horribly wrong! Please submit this on https://forum.teaspeak.de !!!");
        return;
    }

    if((unsigned) events & (unsigned) EV_TIMEOUT) {
        if(transfer->state == FileClient::STATE_FLUSHING) {
            {
                std::unique_lock nb_lock{transfer->network_buffer.mutex};
                if(transfer->network_buffer.bytes > 0) {
                    nb_lock.unlock();
                    transfer->flush_network_buffer();
                    debugMessage(LOG_FT, "{} Failed to flush networking buffer in given timeout. Marking it as flushed.", transfer->log_prefix());
                }
            }


            if(!std::exchange(transfer->finished_signal_send, true)) {
                if(transfer->transfer) {
                    transfer->handle->invoke_aborted_callback(client, { TransferError::NETWORK_IO_ERROR, "failed to flush outgoing buffer" });
                }
            }

            transfer->handle->test_disconnecting_state(client);
            return;
        }
    }

    if((unsigned) events & (unsigned) EV_WRITE) {
        Buffer* buffer{nullptr};
        size_t buffer_left_size{0};

        while(true) {
            {
                std::lock_guard block{transfer->network_buffer.mutex};

                if(!transfer->network_buffer.buffer_head) {
                    buffer_left_size = 0;
                    assert(transfer->network_buffer.bytes == 0);
                    break;
                }

                buffer = ref_buffer(transfer->network_buffer.buffer_head);
                buffer_left_size = transfer->network_buffer.bytes;
            }

            const auto max_write_bytes = transfer->networking.throttle.bytes_left();
            if(!max_write_bytes) break; /* network throttle */

            assert(buffer->offset < buffer->length);
            auto written = ::send(fd, buffer->data + buffer->offset, std::min((size_t) (buffer->length - buffer->offset), max_write_bytes), MSG_DONTWAIT | MSG_NOSIGNAL);
            if(written <= 0) {
                deref_buffer(buffer);
                if(errno == EAGAIN) {
                    transfer->add_network_write_event(false);
                    break;
                }

                if(transfer->state == FileClient::STATE_TRANSFERRING) {
                    assert(transfer->transfer);

                    if(written == 0) {
                        /* EOF, how the hell is this event possible?! (Read should already catch it) */
                        logError(LOG_FT, "{} Client disconnected unexpectedly on write. Send {} bytes out of {}.",
                                 transfer->log_prefix(), transfer->statistics.file_transferred.total_bytes, transfer->transfer ? transfer->transfer->expected_file_size - transfer->transfer->file_offset : -1);

                        transfer->handle->invoke_aborted_callback(client, { TransferError::UNEXPECTED_CLIENT_DISCONNECT, "" });
                    } else {
                        logError(LOG_FT, "{} Received network write error. Send {} bytes out of {}. Closing transfer.",
                                 transfer->log_prefix(), transfer->statistics.file_transferred.total_bytes, transfer->transfer ? transfer->transfer->expected_file_size - transfer->transfer->file_offset : -1);

                        transfer->handle->invoke_aborted_callback(client, { TransferError::NETWORK_IO_ERROR, strerror(errno) });
                    }
                } else if(transfer->state == FileClient::STATE_FLUSHING && transfer->transfer) {
                    {
                        std::lock_guard block{transfer->network_buffer.mutex};
                        if(transfer->network_buffer.bytes == 0)
                            goto disconnect_client;
                    }

                    transfer->flush_network_buffer();
                    if(written == 0) {
                        logError(LOG_FT, "{} Received unexpected client disconnect while flushing the network buffer. Transfer failed.", transfer->log_prefix());
                        transfer->handle->invoke_aborted_callback(client, { TransferError::UNEXPECTED_CLIENT_DISCONNECT, "" });
                    } else {
                        logError(LOG_FT, "{} Received network write error while flushing the network buffer. Closing transfer.",
                                 transfer->log_prefix());

                        transfer->handle->invoke_aborted_callback(client, { TransferError::NETWORK_IO_ERROR, strerror(errno) });
                    }
                }

                disconnect_client:
                /* invalidate all network write operations, but still flush the disk IO buffer */
                if(size_t bytes_dropped{transfer->flush_network_buffer()}; bytes_dropped > 0) {
                    if(transfer->state != FileClient::STATE_TRANSFERRING) {
                        logWarning(LOG_FT, "{} Dropped {} bytes due to a write error ({}/{})",
                                transfer->log_prefix(), bytes_dropped, errno, strerror(errno));
                    }
                }

                std::unique_lock slock{transfer->state_mutex};
                /* no need to flush anything here, write will only be invoked on a client download */
                transfer->handle->disconnect_client(client, slock, false);
                return;
            } else {
                buffer->offset += written;
                assert(buffer->offset <= buffer->length);
                if(buffer->length == buffer->offset) {
                    {
                        std::lock_guard block{transfer->network_buffer.mutex};
                        if(transfer->network_buffer.buffer_head == buffer) {
                            transfer->network_buffer.buffer_head = buffer->next;
                            if(!buffer->next) {
                                transfer->network_buffer.buffer_tail = &transfer->network_buffer.buffer_head;
                            }

                            assert(transfer->network_buffer.bytes >= written);
                            transfer->network_buffer.bytes -= written;
                            buffer_left_size = transfer->network_buffer.bytes;

                            /* Will not trigger a memory free since we're still holding onto one reference */
                            deref_buffer(buffer);
                        } else {
                            /* the buffer got remove */
                        }
                    }
                } else {
                    std::lock_guard block{transfer->network_buffer.mutex};
                    if(transfer->network_buffer.buffer_head == buffer) {
                        assert(transfer->network_buffer.bytes >= written);
                        transfer->network_buffer.bytes -= written;
                        buffer_left_size = transfer->network_buffer.bytes;
                    } else {
                        /* the buffer got removed */
                    }
                }

                transfer->timings.last_write = std::chrono::system_clock::now();
                transfer->statistics.network_send.increase_bytes(written);

                deref_buffer(buffer);

                if(transfer->networking.throttle.increase_bytes(written)) {
                    break; /* we've to slow down */
                }
            }
        }

        if(buffer_left_size > 0) {
            transfer->add_network_write_event(false);
        } else if(transfer->state == FileClient::STATE_FLUSHING) {
            transfer->handle->test_disconnecting_state(client);

            if(!std::exchange(transfer->finished_signal_send, true)) {
                if(transfer->transfer && transfer->statistics.file_transferred.total_bytes + transfer->transfer->file_offset == transfer->transfer->expected_file_size) {
                    logMessage(LOG_FT, "{} Finished file transfer within {}. Closing connection.", transfer->log_prefix(), duration_to_string(std::chrono::system_clock::now() - transfer->timings.key_received));
                    transfer->handle->report_transfer_statistics(client);
                    if(auto callback{transfer->handle->callback_transfer_finished}; callback)
                        callback(transfer->transfer);
                }
            }
            return;
        }
        transfer->handle->enqueue_disk_io(client);
    }
}

inline std::string transfer_key_to_string(char key[TRANSFER_KEY_LENGTH]) {
    std::string result{};
    result.resize(TRANSFER_KEY_LENGTH);

    for(size_t index{0}; index < TRANSFER_KEY_LENGTH; index++)
        result[index] = isprint(key[index]) ? key[index] : '.';

    return result;
}

size_t LocalFileTransfer::handle_transfer_read_raw(const std::shared_ptr<FileClient> &client, const char *buffer, size_t length) {
    if(client->networking.protocol == FileClient::PROTOCOL_TS_V1) {
        return this->handle_transfer_read(client, buffer, length);
    } else if(client->networking.protocol == FileClient::PROTOCOL_HTTPS) {
        client->networking.pipe_ssl.process_incoming_data(pipes::buffer_view{buffer, length});
        return client->network_buffer.bytes;
    } else if(client->networking.protocol != FileClient::PROTOCOL_UNKNOWN) {
        assert(false);
        logWarning(LOG_FT, "{} Read bytes with unknown protocol. Closing connection.", client->log_prefix());

        std::unique_lock slock{client->state_mutex};
        client->handle->disconnect_client(client, slock, true);
        return (size_t) -1;
    }

    if(client->state != FileClient::STATE_AWAITING_KEY) {
        logWarning(LOG_FT, "{} Read bytes with unknown protocol but having not awaiting key state. Closing connection.", client->log_prefix());

        std::unique_lock slock{client->state_mutex};
        client->handle->disconnect_client(client, slock, true);
        return (size_t) -1;
    }

    /* lets read the key bytes (16) and then decide */
    if(client->transfer_key.provided_bytes < TRANSFER_KEY_LENGTH) {
        const auto bytes_write = std::min(TRANSFER_KEY_LENGTH - client->transfer_key.provided_bytes, length);
        memcpy(client->transfer_key.key + client->transfer_key.provided_bytes, buffer, bytes_write);
        length -= bytes_write;
        buffer += bytes_write;
        client->transfer_key.provided_bytes += bytes_write;
    }

    if(client->transfer_key.provided_bytes < TRANSFER_KEY_LENGTH) {
        return 0; /* we need more data */
    }

    if(pipes::SSL::is_ssl((uint8_t*) client->transfer_key.key, client->transfer_key.provided_bytes)) {
        client->networking.protocol = FileClient::PROTOCOL_HTTPS;
        client->networking.http_header_buffer.reset(allocate_buffer(MAX_HTTP_HEADER_SIZE)); /* max 8k header */
        client->networking.max_read_buffer_size = (size_t) MAX_HTTP_HEADER_SIZE; /* HTTP-Header are sometimes a bit bigger. Dont cap max bandwidth here. */
        debugMessage(LOG_FT, "{} Using protocol HTTPS for file transfer.", client->log_prefix());

        char first_bytes[TRANSFER_KEY_LENGTH];
        memcpy(first_bytes, client->transfer_key.key, TRANSFER_KEY_LENGTH);
        client->transfer_key.provided_bytes = 0;

        if(!this->initialize_client_ssl(client)) {
            return (size_t) -1;
        }

        client->networking.pipe_ssl.process_incoming_data(pipes::buffer_view{first_bytes, TRANSFER_KEY_LENGTH});
        if(length > 0) {
            client->networking.pipe_ssl.process_incoming_data(pipes::buffer_view{buffer, length});
        }
        return client->network_buffer.bytes;
    } else {
        client->networking.protocol = FileClient::PROTOCOL_TS_V1;
        debugMessage(LOG_FT, "{} Using protocol RAWv1 for file transfer.", client->log_prefix());

        std::string error_detail{};
        auto key_result = this->handle_transfer_key_provided(client, error_detail);
        switch(key_result) {
            case TransferKeyApplyResult::SUCCESS:
                if(client->transfer->direction == Transfer::DIRECTION_DOWNLOAD) {
                    logMessage(LOG_FT, "{} Successfully initialized file download for file {}.", client->log_prefix(), client->transfer->absolute_file_path);
                    this->enqueue_disk_io(client); /* we've to take initiative */
                } else {
                    logMessage(LOG_FT, "{} Successfully initialized file upload to file {}.", client->log_prefix(), client->transfer->absolute_file_path);
                }

                return length ? this->handle_transfer_read(client, buffer, length) : 0;

            case TransferKeyApplyResult::UNKNOWN_KEY:
                logMessage(LOG_FT, "{} Disconnecting client because we don't recognise his key ({}).", client->log_prefix(), transfer_key_to_string(client->transfer_key.key));
                break;

            case TransferKeyApplyResult::FILE_ERROR:
                assert(client->transfer);
                this->invoke_aborted_callback(client, { TransferError::DISK_INITIALIZE_ERROR, error_detail });

                logMessage(LOG_FT, "{} Disconnecting client because we failed to open the target file.", client->log_prefix());
                break;

            case TransferKeyApplyResult::INTERNAL_ERROR:
            default:
                this->invoke_aborted_callback(client, { TransferError::UNKNOWN, error_detail });
                logMessage(LOG_FT, "{} Disconnecting client because of an unknown key initialize error ({}).", client->log_prefix(), (int) key_result);
                break;
        }


        std::unique_lock slock{client->state_mutex};
        client->handle->disconnect_client(client, slock, true);
        return (size_t) -1;
    }

    return 0;
}

size_t LocalFileTransfer::handle_transfer_read(const std::shared_ptr<FileClient> &client, const char *buffer, size_t length) {
    if(client->state == FileClient::STATE_AWAITING_KEY) {
        if(client->networking.protocol == FileClient::PROTOCOL_HTTPS) {
            assert(client->networking.http_header_buffer);
            auto header_buffer = &*client->networking.http_header_buffer;

            http::HttpResponse response{};
            size_t overhead_length{0};
            char* overhead_data_ptr{nullptr};

            if(header_buffer->offset + length > header_buffer->capacity) {
                logMessage(LOG_FT, "{} Closing connection due to an too long HTTP(S) header (over {} bytes)", client->log_prefix(), header_buffer->capacity);
                response.code = http::code::code(413, "Entity Too Large");
                response.setHeader("x-error-message", { "header exceeds max size of " + std::to_string(header_buffer->capacity) });
                goto send_response_exit;
            }

            {
                http::HttpRequest request{};

                const auto old_offset = header_buffer->offset;
                memcpy(header_buffer->data + header_buffer->offset, buffer, length);
                header_buffer->offset += length;

                constexpr static std::string_view header_end_token{"\r\n\r\n"};
                auto header_view = std::string_view{header_buffer->data, header_buffer->offset};
                auto header_end = header_view.find(header_end_token, old_offset > 3 ? old_offset - 3 : 0);
                if(header_end == std::string::npos) return 0;

                debugMessage(LOG_FT, "{} Received clients HTTP header.", client->log_prefix());
                if(!http::parse_request(std::string{header_view.data(), header_end}, request)) {
                    logError(LOG_FT, "{} Failed to parse HTTP request. Disconnecting client.", client->log_prefix());

                    response.code = http::code::code(400, "Bad Request");
                    response.setHeader("x-error-message", { "failed to parse http request" });
                    goto send_response_exit;
                }

                if(auto header = request.findHeader("Sec-Fetch-Mode"); request.method == "OPTIONS") {
                    logMessage(0, "{} Received options request (probably due to cors). Sending allow response and disconnecting client.", client->log_prefix());
                    response.code = http::code::_200;
                    goto send_response_exit;
                }

#if 0
                std::map<std::string, std::string> query{};
                if(!http::parse_url_parameters(request.url, query)) {
                    logMessage(0, "{} Received request but missing URL parameters ({})", client->log_prefix(), request.url);
                    response.code = http::code::code(400, "Bad Request");
                    goto send_response_exit;
                }
#endif

                std::string transfer_key{};
                if(request.parameters.count("transfer-key") == 0 || (transfer_key = request.parameters.at("transfer-key")).empty()) {
                    logMessage(0, "{} Missing transfer key parameter. Disconnecting client.", client->log_prefix());
                    response.code = http::code::code(400, "Bad Request");
                    response.setHeader("x-error-message", { "missing transfer key" });
                    goto send_response_exit;
                }

                if(transfer_key.length() != TRANSFER_KEY_LENGTH) {
                    logMessage(0, "{} Received too short/long transfer key. Expected {} but received {}. Disconnecting client.", client->log_prefix(), TRANSFER_KEY_LENGTH, transfer_key.length());
                    response.code = http::code::code(400, "Bad Request");
                    response.setHeader("x-error-message", { "key too short/long" });
                    goto send_response_exit;
                }
                client->transfer_key.provided_bytes = TRANSFER_KEY_LENGTH;
                memcpy(client->transfer_key.key, transfer_key.data(), TRANSFER_KEY_LENGTH);

                client->file.query_media_bytes = true;

                std::string error_detail{};
                auto key_result = this->handle_transfer_key_provided(client, error_detail);
                switch(key_result) {
                    case TransferKeyApplyResult::SUCCESS:
                        if(client->transfer->direction == Transfer::DIRECTION_DOWNLOAD) {
                            logMessage(LOG_FT, "{} Successfully initialized file download for file {}.", client->log_prefix(), client->transfer->absolute_file_path);
                        } else {
                            logMessage(LOG_FT, "{} Successfully upload file download to file {}.", client->log_prefix(), client->transfer->absolute_file_path);
                        }
                        break;

                    case TransferKeyApplyResult::FILE_ERROR:
                        assert(client->transfer);

                        this->invoke_aborted_callback(client, { TransferError::DISK_INITIALIZE_ERROR, error_detail });
                        logMessage(LOG_FT, "{} Disconnecting client because we failed to open the target file.", client->log_prefix());
                        response.code = http::code::code(500, "Internal Server Error");
                        response.setHeader("x-error-message", { error_detail });
                        goto send_response_exit;

                    case TransferKeyApplyResult::UNKNOWN_KEY:
                        logMessage(LOG_FT, "{} Disconnecting client because we don't recognise his key ({}).", client->log_prefix(), transfer_key_to_string(client->transfer_key.key));
                        response.code = http::code::code(406, "Not Acceptable");
                        response.setHeader("x-error-message", { "unknown key" });
                        goto send_response_exit;

                    case TransferKeyApplyResult::INTERNAL_ERROR:
                    default:
                        this->invoke_aborted_callback(client, { TransferError::UNKNOWN, error_detail });
                        logMessage(LOG_FT, "{} Disconnecting client because of an unknown key initialize error ({}).", client->log_prefix(), (int) key_result);
                        response.code = http::code::code(500, "Internal Server Error");
                        response.setHeader("x-error-message", { error_detail.empty() ? "failed to initialize transfer" : error_detail });
                        goto send_response_exit;
                }

                response.code = http::code::_200;
                if(client->transfer->direction == Transfer::DIRECTION_DOWNLOAD) {
                    const auto download_name = request.findHeader("download-name");
                    response.setHeader("Content-Length", { std::to_string(client->transfer->expected_file_size - client->transfer->file_offset) });

                    response.setHeader("Content-type", {"application/octet-stream; "});
                    response.setHeader("Content-Transfer-Encoding", {"binary"});

                    response.setHeader("Content-Disposition", {
                        "attachment; filename=\"" + http::encode_url(request.parameters.count("download-name") > 0 ? request.parameters.at("download-name") : client->transfer->file_name) + "\""
                    });

                    response.setHeader("X-media-bytes", { base64::encode((char*) client->file.media_bytes, client->file.media_bytes_length) });
                    client->networking.http_state = FileClient::HTTP_STATE_DOWNLOADING;
                    goto send_response_exit;
                } else {
                    /* we're sending a HTTP response later */
                    client->networking.http_state = FileClient::HTTP_STATE_AWAITING_BOUNDARY;
                    goto initialize_exit;
                }

                overhead_length = header_buffer->offset - header_end - header_end_token.length();
                overhead_data_ptr = header_buffer->data + header_end + header_end_token.length();
            }

            send_response_exit:
            this->send_http_response(client, response);
            if(response.code->code != 200 || !client->transfer) {
                std::unique_lock slock{client->state_mutex};
                client->handle->disconnect_client(client, slock, true);
                return (size_t) -1;
            }

            initialize_exit:
            if(client->transfer->direction == Transfer::DIRECTION_DOWNLOAD)
                this->enqueue_disk_io(client); /* we've to take initiative */

            header_buffer->offset = 0;
            return overhead_length == 0 ? 0 : this->handle_transfer_read(client, overhead_data_ptr, overhead_length);
        } else {
            logError(LOG_FT, "{} Protocol variable contains invalid protocol for awaiting key state. Disconnecting client.", client->log_prefix());

            std::unique_lock slock{client->state_mutex};
            client->handle->disconnect_client(client, slock, true);
            return (size_t) -1;
        }
    } else if(client->state == FileClient::STATE_TRANSFERRING) {
        assert(client->transfer);
        if(client->transfer->direction != Transfer::DIRECTION_UPLOAD) {
            debugMessage(LOG_FT, "{} Read {} bytes from client even though we're only sending a file. Ignoring it.", client->log_prefix(), length);
            return 0;
        }

        if(client->networking.protocol == FileClient::PROTOCOL_HTTPS) {
            std::string error_message{};
            const auto upload_result = client->handle->handle_transfer_upload_http(client, buffer, length);
            switch(upload_result) {
                case TransferUploadHTTPResult::FINISH: {
                    assert(!client->finished_signal_send); /* we should be faster than the networking flush */
                    client->finished_signal_send = true;
                    logMessage(LOG_FT, "{} File upload has been completed in {}. Disconnecting client.", client->log_prefix(), duration_to_string(std::chrono::system_clock::now() - client->timings.key_received));

                    this->report_transfer_statistics(client);
                    if(auto callback{this->callback_transfer_finished}; callback)
                        callback(client->transfer);

                    std::unique_lock slock{client->state_mutex};
                    client->handle->disconnect_client(client, slock, true);
                    return client->network_buffer.bytes; /* a bit unexact but the best we could get away with it */
                }

                case TransferUploadHTTPResult::MORE_DATA_TO_RECEIVE:
                    return client->network_buffer.bytes; /* a bit unexact but the best we could get away with it */

                case TransferUploadHTTPResult::MISSING_CONTENT_TYPE:
                    logMessage(LOG_FT, "{} Missing boundary content type. Disconnecting client.", client->log_prefix());
                    error_message = "invalid boundary content type";
                    break;

                case TransferUploadHTTPResult::INVALID_CONTENT_TYPE:
                    logMessage(LOG_FT, "{} Invalid boundary content type. Disconnecting client.", client->log_prefix());
                    error_message = "missing boundary content type";
                    break;

                case TransferUploadHTTPResult::BOUNDARY_MISSING:
                    logMessage(LOG_FT, "{} Missing boundary token. Disconnecting client.", client->log_prefix());
                    error_message = "missing boundary token";
                    break;

                case TransferUploadHTTPResult::BOUNDARY_INVALID:
                    logMessage(LOG_FT, "{} Invalid boundary. Disconnecting client.", client->log_prefix());
                    error_message = "invalid boundary";
                    break;

                case TransferUploadHTTPResult::BOUNDARY_TOKEN_INVALID:
                    logMessage(LOG_FT, "{} Invalid boundary token. Disconnecting client.", client->log_prefix());
                    error_message = "invalid boundary token";
                    break;
            }

            http::HttpResponse response{};

            response.code = http::code::code(510, "Not Extended");
            response.setHeader("x-error-message", { error_message });
            client->handle->send_http_response(client, response);

            std::unique_lock slock{client->state_mutex};
            client->handle->disconnect_client(client, slock, true);

            return (size_t) -1;
        } else if(client->networking.protocol == FileClient::PROTOCOL_TS_V1) {
            size_t written_bytes{0};
            auto result = this->handle_transfer_upload_raw(client, buffer, length, &written_bytes);

            switch (result) {
                case TransferUploadRawResult::FINISH_OVERFLOW:
                case TransferUploadRawResult::FINISH: {
                    assert(!client->finished_signal_send); /* we should be faster than the networking flush */
                    client->finished_signal_send = true;
                    if(result == TransferUploadRawResult::FINISH_OVERFLOW)
                        logMessage(LOG_FT, "{} Client send {} too many bytes (Transfer length was {}). Dropping them, flushing the disk IO and closing the transfer.", client->log_prefix(), length - written_bytes, duration_to_string(std::chrono::system_clock::now() - client->timings.key_received));
                    else
                        logMessage(LOG_FT, "{} File upload has been completed in {}. Flushing disk IO and closing the connection.", client->log_prefix(), duration_to_string(std::chrono::system_clock::now() - client->timings.key_received));
                    this->report_transfer_statistics(client);
                    if(auto callback{this->callback_transfer_finished}; callback)
                        callback(client->transfer);

                    std::unique_lock slock{client->state_mutex};
                    client->handle->disconnect_client(client, slock, true);
                    return (size_t) -1;
                }

                case TransferUploadRawResult::MORE_DATA_TO_RECEIVE:
                    return client->network_buffer.bytes; /* a bit unexact but the best we could get away with it */
            }
        } else {
            logWarning(LOG_FT, "{} Read message for client with unknown protocol. Dropping {} bytes.", client->log_prefix(), length);
            return 0;
        }
    } else {
        logWarning(LOG_FT, "{} Read message at invalid client state ({}). Dropping message.", client->log_prefix(), client->state);
    }
    return 0;
}

TransferKeyApplyResult LocalFileTransfer::handle_transfer_key_provided(const std::shared_ptr<FileClient> &client, std::string& error_detail) {
    {
        std::lock_guard tlock{this->transfers_mutex};
        for(auto it = this->pending_transfers.begin(); it != this->pending_transfers.end(); it++) {
            if(memcmp((*it)->transfer_key.data(), client->transfer_key.key, std::min((size_t) TRANSFER_KEY_LENGTH, (*it)->transfer_key.length())) == 0) {
                client->transfer = *it;
                this->pending_transfers.erase(it);
                break;
            }
        }
    }

    if(!client->transfer) {
        return TransferKeyApplyResult::UNKNOWN_KEY;
    }

    if(client->transfer->direction == Transfer::DIRECTION_UPLOAD) {
        auto server = dynamic_pointer_cast<LocalVirtualFileServer>(client->transfer->server);
        assert(server);
        client->networking.throttle.right = &server->upload_throttle;
    } else if(client->transfer->direction == Transfer::DIRECTION_DOWNLOAD) {
        auto server = dynamic_pointer_cast<LocalVirtualFileServer>(client->transfer->server);
        assert(server);
        client->networking.throttle.right = &server->download_throttle;
    }

    if(client->transfer->max_bandwidth > 0) {
        debugMessage(LOG_FT, "{} Limit network bandwidth especially for the client to {} bytes/second", client->log_prefix(), client->transfer->max_bandwidth);
        client->networking.client_throttle.set_max_bandwidth(client->transfer->max_bandwidth);
    }
    client->networking.max_read_buffer_size = (size_t) -1; /* limit max bandwidth via throttle */

    auto io_init_result = this->initialize_file_io(client);
    if(io_init_result != FileInitializeResult::SUCCESS) {
        logMessage(LOG_FT, "{} Failed to initialize file {}: {}/{}. Disconnecting client.",
                client->log_prefix(), client->transfer->direction == Transfer::DIRECTION_UPLOAD ? "writer" : "reader", (int) io_init_result, kFileInitializeResultMessages[(int) io_init_result]);
        error_detail = std::to_string((int) io_init_result) + "/" + std::string{kFileInitializeResultMessages[(int) io_init_result]};
        return TransferKeyApplyResult::FILE_ERROR;
    }

    {
        std::unique_lock slock{client->state_mutex};
        if(client->state != FileClient::STATE_AWAITING_KEY) {
            return TransferKeyApplyResult::SUCCESS; /* something disconnected the client */
        }

        client->state = FileClient::STATE_TRANSFERRING;
    }

    if(auto callback{this->callback_transfer_started}; callback) {
        callback(client->transfer);
    }

    client->timings.key_received = std::chrono::system_clock::now();
    return TransferKeyApplyResult::SUCCESS;
}

TransferUploadRawResult LocalFileTransfer::handle_transfer_upload_raw(const std::shared_ptr<FileClient> &client, const char *buffer, size_t length, size_t* bytesWritten) {
    auto write_length = length;
    auto write_offset = client->statistics.file_transferred.total_bytes + client->transfer->file_offset;
    TransferUploadRawResult result{TransferUploadRawResult::MORE_DATA_TO_RECEIVE};

    if(write_offset + write_length > client->transfer->expected_file_size) {
        result = TransferUploadRawResult::FINISH_OVERFLOW;
        write_length = client->transfer->expected_file_size - write_offset;
    } else if(write_offset + write_length == client->transfer->expected_file_size) {
        result = TransferUploadRawResult::FINISH;
    }

    client->statistics.file_transferred.increase_bytes(write_length);
    client->enqueue_disk_buffer_bytes(buffer, write_length);
    this->enqueue_disk_io(client);
    if(bytesWritten) {
        *bytesWritten = write_length;
    }

    return result;
}

//Example boundary:
//------WebKitFormBoundaryaWP8XAzMBnMOJznv\r\nContent-Disposition: form-data; name="file"; filename="blob"\r\nContent-Type: application/octet-stream\r\n\r\n
TransferUploadHTTPResult LocalFileTransfer::handle_transfer_upload_http(const std::shared_ptr<FileClient> &client,
                                                                        const char *buffer, size_t length) {
    constexpr static std::string_view boundary_end_token{"\r\n\r\n"};
    constexpr static std::string_view boundary_token_end_token{"\r\n"};

    if(client->networking.http_state == FileClient::HTTP_STATE_AWAITING_BOUNDARY) {
        assert(client->networking.http_header_buffer);

        /* Notice: The buffer ptr might be some data within our header buffer! But since its somewhere in the back its okey */
        auto boundary_buffer = &*client->networking.http_header_buffer;
        if(boundary_buffer->offset + length > boundary_buffer->capacity)
            return TransferUploadHTTPResult::BOUNDARY_MISSING;

        const auto old_offset = boundary_buffer->offset;
        memcpy(boundary_buffer->data + boundary_buffer->offset, buffer, length);
        boundary_buffer->offset += length;

        auto buffer_view = std::string_view{boundary_buffer->data, boundary_buffer->offset};
        auto boundary_end = buffer_view.find(boundary_end_token, old_offset > 3 ? old_offset - 3 : 0);
        if(boundary_end == std::string::npos)
            return TransferUploadHTTPResult::MORE_DATA_TO_RECEIVE;

        auto boundary_token_end = buffer_view.find(boundary_token_end_token);
        if(boundary_token_end + boundary_token_end_token.size() >= boundary_end)
            return TransferUploadHTTPResult::BOUNDARY_TOKEN_INVALID;

        const auto boundary_token = buffer_view.substr(0, boundary_token_end);
        client->networking.http_boundary = boundary_token;
        logTrace(LOG_FT, "{} Received clients HTTP file boundary ({}).", client->log_prefix(), boundary_token);

        const auto boundary_header_offset = boundary_token_end + boundary_token_end_token.size();
        const auto boundary_payload = buffer_view.substr(boundary_header_offset, boundary_end - boundary_header_offset);

        http::HttpRequest boundary{};
        if(!http::parse_request(std::string{boundary_payload}, boundary))
            return TransferUploadHTTPResult::BOUNDARY_INVALID;

        const auto content_type = boundary.findHeader("Content-Type");
        if(!content_type || content_type.values.empty())
            return TransferUploadHTTPResult::MISSING_CONTENT_TYPE;
        /* A bit relaxed here
        else if(content_type.values[0] != "application/octet-stream")
            return TransferUploadHTTPResult::INVALID_CONTENT_TYPE;
        */

        const auto overhead_length = boundary_buffer->offset - boundary_end - boundary_end_token.length();
        const auto overhead_data_ptr = boundary_buffer->data + boundary_end + boundary_end_token.length();

        client->networking.http_state = FileClient::HTTP_STATE_UPLOADING;
        boundary_buffer->offset = 0;
        return overhead_length == 0 ? TransferUploadHTTPResult::MORE_DATA_TO_RECEIVE : this->handle_transfer_upload_http(client, overhead_data_ptr, overhead_length);
    } else if(client->networking.http_state == FileClient::HTTP_STATE_UPLOADING) {
        size_t bytes_written{0};
        auto result = this->handle_transfer_upload_raw(client, buffer, length, &bytes_written);
        switch(result) {
            case TransferUploadRawResult::MORE_DATA_TO_RECEIVE:
                return TransferUploadHTTPResult::MORE_DATA_TO_RECEIVE;

            case TransferUploadRawResult::FINISH_OVERFLOW:
            case TransferUploadRawResult::FINISH:
                debugMessage(LOG_FT, "{} File upload has been completed in {}. Awaiting file end boundary.", client->log_prefix(), duration_to_string(std::chrono::system_clock::now() - client->timings.key_received));
                client->networking.http_state = FileClient::HTTP_STATE_AWAITING_BOUNDARY_END;

                if(length != bytes_written)
                    return this->handle_transfer_upload_http(client, buffer + bytes_written, length - bytes_written);
                return TransferUploadHTTPResult::MORE_DATA_TO_RECEIVE;

            default:
                assert(false);
                return TransferUploadHTTPResult::MORE_DATA_TO_RECEIVE;
        }
    } else if(client->networking.http_state == FileClient::HTTP_STATE_AWAITING_BOUNDARY_END) {
        assert(client->networking.http_header_buffer);

        /* Notice: The buffer ptr might be some data within our header buffer! But since its somewhere in the back its okey */
        auto boundary_buffer = &*client->networking.http_header_buffer;
        if(boundary_buffer->offset + length > boundary_buffer->capacity)
            return TransferUploadHTTPResult::BOUNDARY_MISSING;

        memcpy(boundary_buffer->data + boundary_buffer->offset, buffer, length);
        boundary_buffer->offset += length;

        const auto expected_boundary_size = 2 + client->networking.http_boundary.size() + 4;
        if(boundary_buffer->offset < expected_boundary_size)
            return TransferUploadHTTPResult::MORE_DATA_TO_RECEIVE;

        if(memcmp(boundary_buffer->data, "\r\n", 2) != 0) {
            debugMessage(LOG_FT, "{} File boundary seems invalid/miss matching. Expected \\r\\n at {}.", client->log_prefix(), 0);
            goto callback_exit;
        }

        if(memcmp(boundary_buffer->data + 2 + client->networking.http_boundary.size(), "--\r\n", 4) != 0) {
            debugMessage(LOG_FT, "{} File boundary seems invalid/miss matching. Expected --\\r\\n at {}.", client->log_prefix(), 2 + client->networking.http_boundary.size());
            goto callback_exit;
        }

        if(memcmp(boundary_buffer->data + 2, client->networking.http_boundary.data(), client->networking.http_boundary.size()) != 0) {
            debugMessage(LOG_FT, "{} File upload has been completed but end boundary does not match ({} != {})! Ignoring miss match and finishing transfer", client->log_prefix(),
                std::string_view{boundary_buffer->data + 2, client->networking.http_boundary.size()},
                client->networking.http_boundary
            );

            goto callback_exit;
        }

        if(boundary_buffer->offset > expected_boundary_size) {
            debugMessage(LOG_FT, "{} File boundary has been received, but received {} more unexpected bytes. Ignoring these and finishing the upload.",
                    client->log_prefix(), boundary_buffer->offset - expected_boundary_size);
        } else {
            debugMessage(LOG_FT, "{} File boundary has been received.", client->log_prefix());
        }

        callback_exit:
        /* send a proper HTTP response */
        { 
            http::HttpResponse response{};
            response.code = http::code::_200;
            this->send_http_response(client, response);
        };
        return TransferUploadHTTPResult::FINISH;
    } else {
        logWarning(0, "{} Received HTTP(S) data, for an invalid HTTP state ({}).", client->log_prefix(), (int) client->networking.http_state);
        return TransferUploadHTTPResult::MORE_DATA_TO_RECEIVE;
    }
}

inline void apply_cors_and_connection_headers(http::HttpResponse &response) {
    response.setHeader("Connection", {"close"}); /* close the connection instance, we dont want multiple requests */
    response.setHeader("Access-Control-Allow-Methods", {"GET, POST"});
    response.setHeader("Access-Control-Allow-Origin", {"*"});

    auto requestHeaders = response.findHeader("Access-Control-Request-Headers").values;
    if(requestHeaders.empty()) {
        requestHeaders.emplace_back("*");
    }
    response.setHeader("Access-Control-Allow-Headers", requestHeaders); //access-control-allow-headers
    response.setHeader("Access-Control-Max-Age", {"86400"});
}

void LocalFileTransfer::send_http_response(const std::shared_ptr<FileClient> &client, http::HttpResponse &response) {
    apply_cors_and_connection_headers(response);
    response.setHeader("Access-Control-Expose-Headers", {"*, x-error-message, Content-Length, X-media-bytes, Content-Disposition"});

    const auto payload = response.build();
    client->send_file_bytes(payload.data(), payload.length());
}