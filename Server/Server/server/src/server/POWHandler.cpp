#include "POWHandler.h"
#include "src/InstanceHandler.h"
#include "src/client/voice/VoiceClient.h"
#include <misc/endianness.h>
#include <log/LogUtils.h>

using namespace std;
using namespace std::chrono;
using namespace ts;
using namespace ts::server;

//#define POW_DEBUG
//#define POW_ERROR

POWHandler::POWHandler(ts::server::VoiceServer *server) : server(server) { }

void POWHandler::execute_tick() {
    auto now = system_clock::now();

    lock_guard lock(this->pending_clients_lock);
    this->pending_clients.erase(remove_if(this->pending_clients.begin(), this->pending_clients.end(), [&, now](const shared_ptr<Client>& client) {
        if(now - client->last_packet > std::chrono::seconds{5}) {
            #ifdef POW_ERROR
            if(client->state != LowHandshakeState::COMPLETED) { /* handshake succeeded */
                debugMessage(this->get_server_id(), "[POW] Dropping connection from {} (Timeout)", net::to_string(client->address));
            }
            #endif
            return true;
        }
        return false;
    }), this->pending_clients.end());
}

void POWHandler::delete_client(const std::shared_ptr<ts::server::POWHandler::Client> &client) {
    lock_guard lock(this->pending_clients_lock);
    auto it = find(this->pending_clients.begin(), this->pending_clients.end(), client);
    if(it != this->pending_clients.end())
        this->pending_clients.erase(it);
}

void POWHandler::handle_datagram(const std::shared_ptr<VoiceServerSocket>& socket, const sockaddr_storage &address,msghdr &info, const pipes::buffer_view &buffer) {
    if(buffer.length() < MAC_SIZE + CLIENT_HEADER_SIZE + 5) {
        return; /* too short packet! */
    }

    std::shared_ptr<Client> client;
    {
        lock_guard lock(this->pending_clients_lock);
        for(const auto& pending_client : this->pending_clients) {
            if(pending_client->socket == socket && memcmp(&address, &pending_client->address, sizeof(sockaddr_storage)) == 0) {
                client = pending_client;
                break;
            }
        }

        if(!client) {
            #ifdef POW_DEBUG
            debugMessage(this->get_server_id(), "[POW] Got a new connection from {}", net::to_string(address));
            #endif
            //We've got a new client
            client = make_shared<Client>();
            client->state = LowHandshakeState::UNSET;
            this->pending_clients.push_back(client);
        }
    }

    unique_lock lock(client->handle_lock, defer_lock_t{});
    if(!lock.try_lock_for(nanoseconds(15 * 1000))) {
        //Failed to acquire handle lock
        return;
    }

    if(client->state == LowHandshakeState::UNSET) { /* initialize client */
        client->socket = socket;
        client->client_version = be2le32(&buffer[MAC_SIZE + CLIENT_HEADER_SIZE]);
        memcpy(&client->address, &address, sizeof(client->address));
        udp::DatagramPacket::extract_info(info, client->address_info);

        client->state = LowHandshakeState::COOKIE_GET;
    }
    client->last_packet = system_clock::now();

    /* buffer is in client packet format, but we dont need to parse because we dont need the header */
    auto data = buffer.view(MAC_SIZE + CLIENT_HEADER_SIZE);
    //this->crypto.client_time = be2le32((char*) packet->data().data_ptr(), 0); /* client timestamp */
    auto packet_state = static_cast<LowHandshakeState>(data[4]);
    #ifdef POW_DEBUG
    debugMessage(this->get_server_id(), "[POW][{}] Received packet with state {}. length: {}", net::to_string(address), packet_state, data.length());
    #endif

    if(packet_state < client->state) {
        #ifdef POW_ERROR
        debugMessage(this->get_server_id(), "[POW][{}] Received packet with lower state then the current state. Jumping back. Current state: {}, New state: {}", net::to_string(address), client->state, packet_state);
        #endif
        client->state = packet_state;
    } else if(packet_state != client->state) {
        if(packet_state == LowHandshakeState::PUZZLE_GET && client->state == LowHandshakeState::COOKIE_GET) {
            if(!config::voice::enforce_coocie_handshake) {
                #ifdef POW_DEBUG
                debugMessage(this->get_server_id(), "[POW][{}] Client wants a puzzle, but we expected a cookie. Ignoring it and jump ahead to the puzzle.", net::to_string(address));
                #endif
                client->state = LowHandshakeState::PUZZLE_GET;
                client->server_control_data[0] = 0; /* disable cookie check */
                goto handle_packet;
            }
        }
        #ifdef POW_ERROR
        debugMessage(this->get_server_id(), "[POW][{}] Received packet an unexpected state. Expected: {}, Received: {}. Resetting client", net::to_string(address), client->state, packet_state);
        #endif
        this->reset_client(client);
        return;
    }

    handle_packet:
    if(packet_state == LowHandshakeState::COOKIE_GET)
        this->handle_cookie_get(client, data);
    else if(packet_state == LowHandshakeState::PUZZLE_GET)
        this->handle_puzzle_get(client, data);
    else if(packet_state == LowHandshakeState::PUZZLE_SOLVE)
        this->handle_puzzle_solve(client, data);
    else {
        #ifdef POW_ERROR
        debugMessage(this->get_server_id(), "[POW][{}] Got an invalid packet state of type {}. Resetting client.", net::to_string(address), packet_state);
        #endif
        this->reset_client(client);
        return;
    }
}

void POWHandler::send_data(const std::shared_ptr<ts::server::POWHandler::Client> &client, const pipes::buffer_view &buffer) {
    auto datagram = udp::DatagramPacket::create(client->address, client->address_info, buffer.length() + MAC_SIZE + SERVER_HEADER_SIZE, nullptr);
    if(!datagram) return; //Should never happen

    /* first 8 bytes mac */
    memcpy(&datagram->data[0], "TS3INIT1", 8);

    /* 2 bytes packet id (const 101) */
    le2be16(101, &datagram->data[8]);

    /* 1 byte flags and type */
    datagram->data[10] = (uint8_t) (0x08U | 0x80U);

    memcpy(&datagram->data[11], buffer.data_ptr(), buffer.length());
    client->socket->send_datagram(datagram);
}

void POWHandler::reset_client(const std::shared_ptr<ts::server::POWHandler::Client> &client) {
    uint8_t buffer[2] = {COMMAND_RESET, 0};
    this->send_data(client, pipes::buffer_view{buffer, 2});
    client->state = LowHandshakeState::COOKIE_GET;
}

inline void generate_random(uint8_t *destination, size_t length) {
    while(length-- > 0)
        *(destination++) = (uint8_t) rand();
}

void POWHandler::handle_cookie_get(const std::shared_ptr<ts::server::POWHandler::Client> &client, const pipes::buffer_view &buffer) {
    if(buffer.length() != 21) {
        #ifdef POW_ERROR
        debugMessage(this->get_server_id(), "[POW][{}][Cookie] Received an invalid packet with an invalid length. Expected {} bytes, but got {} bytes", net::to_string(client->address), 21, buffer.length());
        #endif
        return;
    }

    /* initialize data */
    if(client->server_control_data[0] == 0) {
        generate_random(client->server_control_data, 16);
        client->server_control_data[0] |= 1U;
    }

    /* parse values */
    memcpy(client->client_control_data, &buffer[9], 4);

    /* send response */
    {
        uint8_t response_buffer[21];
        response_buffer[0] = LowHandshakeState::COOKIE_SET;
        memcpy(&response_buffer[1], client->server_control_data, 16);
        *(uint32_t*) &response_buffer[17] = htonl(*(uint32_t*) &client->client_control_data);

        this->send_data(client, pipes::buffer_view{response_buffer, 21});
    }

    client->state = LowHandshakeState::PUZZLE_GET;
}

void POWHandler::handle_puzzle_get(const std::shared_ptr<ts::server::POWHandler::Client> &client, const pipes::buffer_view &buffer) {
    if(buffer.length() != 25) {
        #ifdef POW_ERROR
        debugMessage(this->get_server_id(), "[POW][{}][Puzzle] Received an invalid puzzle request with an invalid length. Expected {} bytes, but got {} bytes", net::to_string(client->address), 25, buffer.length());
        #endif
        return;
    }

    /* verify the server cookie */
    if(client->server_control_data[0] != 0) {
        if(memcmp(client->server_control_data, &buffer[5], 16) != 0) {
            #ifdef POW_ERROR
            debugMessage(this->get_server_id(), "[POW][{}][Puzzle] Received an invalid puzzle request. Returned server cookie dosnt match! Resetting client", net::to_string(client->address));
            #endif
            this->reset_client(client);
            return;
        }
    }

    if(!client->rsa_challenge)
        client->rsa_challenge = serverInstance->getVoiceServerManager()->rsaPuzzles()->next_puzzle();

    /* send response */
    {
        size_t response_length = 1 + 64 * 2 + 4 + 100;
        uint8_t response_buffer[response_length];

        /* first byte step */
        response_buffer[0] = LowHandshakeState::PUZZLE_SET;

        /* 64 bytes x */
        memcpy(&response_buffer[1], client->rsa_challenge->data_x, 64);

        /* 64 bytes n */
        memcpy(&response_buffer[1 + 64], client->rsa_challenge->data_n, 64);

        /* 2 bytes exponent (level) */
        le2be32(client->rsa_challenge->level, &response_buffer[1 + 64 + 64]);

        /* custom server data (empty) */
        memset(&response_buffer[1 + 64 * 2 + 4], 0, 100);

        this->send_data(client, pipes::buffer_view{response_buffer, response_length});
    }

    client->state = LowHandshakeState::PUZZLE_SOLVE;
}

void POWHandler::handle_puzzle_solve(const std::shared_ptr<ts::server::POWHandler::Client> &client, const pipes::buffer_view &buffer) {
    if(buffer.length() < 301) {
        #ifdef POW_ERROR
        debugMessage(this->get_server_id(), "[POW][{}][Puzzle] Received an invalid puzzle solution with an invalid length. Expected at least {} bytes, but got {} bytes", net::to_string(client->address), 301, buffer.length());
        #endif
        return;
    }

    if(!client->rsa_challenge) {
        #ifdef POW_ERROR
        debugMessage(this->get_server_id(), "[POW][{}][Puzzle] Received a puzzle solution for a puzzle which hasnt yet been decided! Resetting client", net::to_string(client->address));
        #endif
        this->reset_client(client);
        return;
    }

    /* validate data */
    {
        /* should we validate x,n and level as well? */
        if(memcmp(client->rsa_challenge->data_result, &buffer[4 + 1 + 2 * 64 + 04 + 100], 64) != 0) {
            #ifdef POW_ERROR
            debugMessage(this->get_server_id(), "[POW][{}][Puzzle] Received an invalid puzzle solution! Resetting client & puzzle", net::to_string(client->address));
            #endif
            constexpr static uint8_t empty_result[64]{0};
            if(memcmp(empty_result, &buffer[4 + 1 + 2 * 64 + 04 + 100], 64) == 0)
                client->rsa_challenge->fail_count++;
            client->rsa_challenge.reset(); /* get another RSA challenge */
            this->reset_client(client);
            return;
        }
    }

    auto command = buffer.view(301);
    #ifdef POW_DEBUG
    debugMessage(this->get_server_id(), "[POW][{}][Puzzle] Puzzle solved, received command {}", net::to_string(client->address), command.string());
    #endif

    auto voice_client = this->register_verified_client(client);
    if(voice_client) {
        auto rcommand = command::ReassembledCommand::allocate(command.length());
        memcpy(rcommand->command(), command.data_ptr(), rcommand->length());
        voice_client->connection->handlePacketCommand(rcommand);
        client->state = LowHandshakeState::COMPLETED;
    } else {
        #ifdef POW_ERROR
        debugMessage(this->get_server_id(), "[POW][{}][Puzzle] Failed to initialize client. Doing nothing, until a new packet.", net::to_string(client->address));
        #endif
    }
}

shared_ptr<VoiceClient> POWHandler::register_verified_client(const std::shared_ptr <ts::server::POWHandler::Client> &client) {
    std::shared_ptr<VoiceClient> voice_client;
    {
        lock_guard lock(this->server->connectionLock);
        for(const auto& connection : this->server->activeConnections) {
            if(memcmp(&connection->remote_address, &client->address, sizeof(client->address)) != 0) {
                continue;
            }

            switch(connection->connectionState()) {
                case ConnectionState::DISCONNECTING:
                case ConnectionState::DISCONNECTED:
                    /* Connection already disconnecting/disconnected. Don't use this connection. */
                    continue;

                case ConnectionState::INIT_LOW:
                case ConnectionState::INIT_HIGH:
                    /* It seems like a clientinitiv command resend. Process it. */
                    break;

                case ConnectionState::CONNECTED: {
                    auto timestamp_now = std::chrono::system_clock::now();
                    auto last_client_alive_signal = std::max(connection->connection->ping_handler().last_ping_response(), connection->connection->ping_handler().last_command_acknowledged());
                    if(timestamp_now - last_client_alive_signal < std::chrono::seconds(5)) {
                        logMessage(connection->connection->virtual_server_id(), "{} Client initialized session reconnect, but last alive signal is not older then 5 seconds ({}). Ignoring attempt.",
                                   connection->connection->log_prefix(),
                                   duration_cast<std::chrono::milliseconds>(timestamp_now - last_client_alive_signal).count()
                        );

                        /* FIXME: Somehow send an error? */
                        return nullptr;
                    } else if(!config::voice::allow_session_reinitialize) {
                        logMessage(connection->connection->virtual_server_id(), "{} Client initialized session reconnect and last ping response is older then 5 seconds ({}). Dropping attempt because its not allowed due to config settings",
                                   connection->connection->log_prefix(),
                                   duration_cast<std::chrono::milliseconds>(timestamp_now - last_client_alive_signal).count()
                        );

                        /* FIXME: Somehow send an error? */
                        return nullptr;
                    }

                    logMessage(connection->connection->virtual_server_id(), "{} Client initialized reconnect and last ping response is older then 5 seconds ({}). Allowing attempt and dropping old connection.",
                               connection->connection->log_prefix(),
                               duration_cast<std::chrono::milliseconds>(timestamp_now - last_client_alive_signal).count()
                    );

                    connection->close_connection(std::chrono::system_clock::time_point{});
                    {
                        std::lock_guard flush_lock{connection->flush_mutex};
                        connection->disconnect_acknowledged = std::make_optional(true);
                    }

                    continue;
                }

                case ConnectionState::UNKNWON:
                default:
                    assert(false);
                    continue;
            }

            voice_client = connection;
            break;
        }
    }

    if(!voice_client) {
        voice_client = std::make_shared<VoiceClient>(this->server->get_server()->getVoiceServer(), &client->address);
        voice_client->initialize_weak_reference(voice_client);
        voice_client->initialize();

        voice_client->connection->socket_ = client->socket;
        voice_client->state = ConnectionState::INIT_LOW;
        memcpy(&voice_client->connection->remote_address_info_, &client->address_info, sizeof(client->address_info));

        {
            std::lock_guard lock{this->server->connectionLock};
            this->server->activeConnections.push_back(voice_client);
        }

        debugMessage(this->get_server_id(), "Having new voice client. Remote address: {}:{}", voice_client->getLoggingPeerIp(), voice_client->getPeerPort());
    }

    voice_client->getConnection()->crypt_setup_handler().set_client_protocol_time(client->client_version);
    //voice_client->last_packet_handshake = system_clock::now();
    return voice_client;
}