#include "WebClient.h"
#include "protocol/RawCommand.h"
#include <log/LogUtils.h>
#include <src/server/VoiceServer.h>
#include <src/InstanceHandler.h>

using namespace std;
using namespace std::chrono;
using namespace ts;
using namespace ts::server;
using namespace ts::protocol;

void WebClient::handleMessageWrite(int fd, short, void *ptr_client) {
    auto client = dynamic_pointer_cast<WebClient>(((WebClient*) ptr_client)->ref());
    assert(client);

    unique_lock buffer_lock(client->queue_mutex);
    if(client->queue_write.empty()) return;

    auto buffer = client->queue_write[0];
    client->queue_write.pop_front();

    auto written = send(fd, buffer.data_ptr(), buffer.length(), MSG_NOSIGNAL | MSG_DONTWAIT);
    if(written == -1) {
        buffer_lock.unlock();

        if (errno == EINTR || errno == EAGAIN) {
            lock_guard event_lock(client->event_mutex);
            if(client->writeEvent)
                event_add(client->writeEvent, nullptr);
            return;
        } else {
            //new ServerConnection(globalClient).startConnection({ host: "localhost", port: 9987}, new HandshakeHandler(profiles.default_profile(), "test"))

            {
                std::lock_guard event_lock{client->event_mutex};
                if(client->writeEvent) {
                    event_del_noblock(client->writeEvent);
                    event_free(client->writeEvent);
                    client->writeEvent = nullptr;
                }
            }
            debugMessage(client->getServerId(), "[{}] Failed to write message (length {}, errno {}, message {}) Disconnecting client.", client->getLoggingPrefix(), written, errno, strerror(errno));
        }

        client->close_connection(system_clock::now()); /* close connection in a new thread */
        return;
    }

    if(written < buffer.length()) {
        buffer = buffer.range((size_t) written); /* get the overhead */
        client->queue_write.push_front(buffer);
    }

    if(client->queue_write.empty())
        return;

    /* reschedule new write */
    buffer_lock.unlock();
    lock_guard event_lock(client->event_mutex);
    if(client->writeEvent)
        event_add(client->writeEvent, nullptr);
}

/* The +2 to identify the syscall */
constexpr static auto kReadBufferSize{1024 * 4 + 2};
void WebClient::handleMessageRead(int fd, short, void *ptr_client) {
    auto client = dynamic_pointer_cast<WebClient>(((WebClient*) ptr_client)->ref());
    assert(client);

    uint8_t buffer[kReadBufferSize];
    auto length = recv(fd, buffer, kReadBufferSize, MSG_NOSIGNAL | MSG_DONTWAIT);
    if(length <= 0) {
        /* error handling "slow path" */
        if(length < 0 && errno == EAGAIN) {
            /* We've currently no data queued */
            return;
        }

        debugMessage(client->getServerId(), "[{}] Failed to read message (length {}, errno {}, message: {}). Closing connection.", client->getLoggingPrefix(), length, errno, strerror(errno));

        {
            lock_guard lock{client->event_mutex};
            if(client->readEvent) {
                event_del_noblock(client->readEvent);
            }
        }
        client->close_connection(system_clock::now()); /* direct close, but from another thread */
        return;
    }

    auto command = command::ReassembledCommand::allocate((size_t) length);
    memcpy(command->command(), buffer, (size_t) length);

    client->command_queue->enqueue_command_execution(command);
}

void WebClient::enqueue_raw_packet(const pipes::buffer_view &msg) {
    auto buffer = msg.own_buffer(); /* TODO: Use buffer::allocate_buffer(...) */
    {
        lock_guard queue_lock(this->queue_mutex);
        this->queue_write.push_back(buffer);
    }
    {
        lock_guard lock(this->event_mutex);
        if(this->writeEvent) {
            event_add(this->writeEvent, nullptr);
        }
    }

    this->connectionStatistics->logOutgoingPacket(stats::ConnectionStatistics::category::COMMAND, buffer.length());
}

inline bool is_ssl_handshake_header(const std::string_view& buffer) {
    if(buffer.length() < 0x05) return false; //Header too small!

    if(buffer[0] != 0x16) return false; //recordType=handshake

    if(buffer[1] < 1 || buffer[1] > 3) return false; //SSL version
    if(buffer[2] < 1 || buffer[2] > 3) return false; //TLS version

    return true;
}

bool WebClient::process_next_message(const std::string_view &buffer) {
    lock_guard execute_lock(this->execute_mutex);
    if(this->state != ConnectionState::INIT_HIGH && this->state != ConnectionState::INIT_LOW && this->state != ConnectionState::CONNECTED) {
        return false;
    }

    this->connectionStatistics->logIncomingPacket(stats::ConnectionStatistics::category::COMMAND, buffer.length());
    if(!this->ssl_detected) {
        this->ssl_detected = true;
        this->ssl_encrypted = is_ssl_handshake_header(buffer);
        if(this->ssl_encrypted) {
            logMessage(this->getServerId(), "[{}] Using encrypted basic connection.", CLIENT_STR_LOG_PREFIX_(this));
        } else {
            logMessage(this->getServerId(), "[{}] Using unencrypted basic connection.", CLIENT_STR_LOG_PREFIX_(this));
        }
    }
    if(this->ssl_encrypted) {
        this->ssl_handler.process_incoming_data(pipes::buffer_view{buffer.data(), buffer.length()});
    } else {
        this->ws_handler.process_incoming_data(pipes::buffer_view{buffer.data(), buffer.length()});
    }
    return true;
}