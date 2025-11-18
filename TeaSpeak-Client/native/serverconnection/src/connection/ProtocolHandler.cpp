#ifdef WIN32
#include <WinSock2.h>
#endif

#include "ProtocolHandler.h"
#include "Socket.h"
#include "../logger.h"
#include <misc/base64.h>
#include <misc/endianness.h>
#include <protocol/buffers.h>
#include <thread>
#include <iostream>

using namespace std;
using namespace std::chrono;
using namespace tc::connection;
using namespace ts::protocol;
using namespace ts;

ProtocolHandler::ProtocolHandler(ServerConnection* handle) : handle{handle}, packet_decoder{&this->crypt_handler, false} {
    this->packet_decoder.callback_argument = this;
    this->packet_decoder.callback_decoded_packet = ProtocolHandler::callback_packet_decoded;
    this->packet_decoder.callback_decoded_command = ProtocolHandler::callback_command_decoded;
    this->packet_decoder.callback_send_acknowledge = ProtocolHandler::callback_send_acknowledge;

    this->acknowledge_handler.callback_data = this;
    this->acknowledge_handler.callback_resend_failed = ProtocolHandler::callback_resend_failed;
    this->acknowledge_handler.destroy_packet = [](void* pkt_ptr) {
        auto packet = (OutgoingClientPacket*) pkt_ptr;
        packet->unref();
    };
}

ProtocolHandler::~ProtocolHandler() = default;


void ProtocolHandler::reset() {
    this->server_type = server_type::UNKNOWN;
    this->disconnect_id++; /* we've been resetted any pending disconnects are not from interest anymore */
    this->client_id = 0;
    this->acknowledge_handler.reset();
    this->connection_state = connection_state::INITIALIZING;

    { /* initialize pow handler */
        this->pow.state = pow_state::COOKIE_SET;
        this->pow.retry_count = 0;

        this->pow.last_buffer = pipes::buffer{};
        this->pow.last_resend = system_clock::time_point{};
        this->pow.last_response = system_clock::time_point{};

        this->pow.client_control_data[0] = 0; /* clear set flag, so the client generates a new pack */
    }

    {
        this->crypto.alpha[0] = 0;
        this->crypto.initiv_command = "";
        this->crypto.beta_length = 0;

        if(this->crypto.identity.has_value()) {
            ecc_free(&*this->crypto.identity);
            this->crypto.identity = std::nullopt;
        }
    }


    this->crypt_setupped = false;
    this->_packet_id_manager.reset();
    this->crypt_handler.reset();
    this->packet_decoder.reset();

    this->ping.ping_received_timestamp =  system_clock::time_point{};

    this->statistics_.control_bytes_received = 0;
    this->statistics_.control_bytes_send = 0;
    this->statistics_.voice_bytes_send = 0;
    this->statistics_.voice_bytes_received = 0;
}

bool ProtocolHandler::initialize_identity(const std::optional<std::string_view> &identity) {
    this->reset_identity();
    assert(!this->crypto.identity.has_value());

    this->crypto.identity.emplace();
    if(identity.has_value()) {
        auto result = ecc_import((u_char*) identity->data(), (unsigned long) identity->length(), &*this->crypto.identity);
        if(result == CRYPT_OK) {
            return true;
        } else {
            this->crypto.identity.reset();
            log_error(category::connection, tr("Failed to initialize crypto identity from given parameter: {}"), result);
            return false;
        }
    } else {
        prng_state rng_state{};
        memset(&rng_state, 0, sizeof(prng_state));
        int err;

        auto result = ecc_make_key_ex(&rng_state, find_prng("sprng"), &*this->crypto.identity, &ltc_ecc_sets[5]);
        if(result == CRYPT_OK) {
            return true;
        } else {
            this->crypto.identity.reset();
            log_error(category::connection, tr("Failed to generate ephemeral crypto identity: {}"), result);
            return false;
        }
    }
}

void ProtocolHandler::reset_identity() {
    this->crypto.identity.reset();
}

void ProtocolHandler::connect() {
    this->connection_state = connection_state::INIT_LOW;
    this->connect_timestamp = system_clock::now();
    this->pow_send_cookie_get();

    {
        auto command = this->generate_client_initiv();
        this->send_command(command, false, nullptr);
    }
}

void ProtocolHandler::execute_tick() {
    auto now = system_clock::now();
    if(this->connection_state < connection_state::DISCONNECTED) {
        if(!this->pow.last_buffer.empty() && this->pow.last_resend < now - seconds(1)) {
            this->send_init1_buffer();
        }

        if(this->connection_state == connection_state::INIT_LOW || this->connection_state == connection_state::INIT_HIGH) {
            if(this->connect_timestamp < now - seconds(15) && false) {
                this->handle->call_connect_result.call(this->handle->errors.register_error("timeout (" + to_string(this->connection_state) + ")"), true);
                this->handle->close_connection();
                return;
            }
        }

        if(this->connection_state == connection_state::DISCONNECTING) {
            if(this->disconnect_timestamp < now - seconds(5)) { /* disconnect timeout */
                this->handle->close_connection();
                return;
            }
        }

        this->execute_resend();

        /* ping */
        if(this->connection_state == connection_state::CONNECTED) {
            if(this->ping.ping_send_timestamp + seconds(1) < now) {
                this->ping_send_request();
            }

            if(this->ping.ping_received_timestamp.time_since_epoch().count() > 0) {
                if(now - this->ping.ping_received_timestamp > seconds(30)) {
                    this->handle->execute_callback_disconnect.call(tr("ping timeout"), true);
                    this->handle->close_connection();
                    return;
                }
            } else
                this->ping.ping_received_timestamp = now;
        }
    }
}

void ProtocolHandler::send_init1_buffer() {
    this->pow.last_resend = std::chrono::system_clock::now();
    auto packet = protocol::allocate_outgoing_client_packet(this->pow.last_buffer.length());
    memcpy(packet->payload, this->pow.last_buffer.data_ptr(), this->pow.last_buffer.length());
    packet->type_and_flags_ = protocol::PacketType::INIT1 | protocol::PacketFlag::Unencrypted;
    *(uint16_t*) packet->packet_id_bytes_ = htons(101);
    this->send_packet(packet, true);
}

void ProtocolHandler::execute_resend() {
    if(this->connection_state >= connection_state::DISCONNECTED) {
        return;
    }

    std::deque<std::shared_ptr<ts::connection::AcknowledgeManager::Entry>> buffers;
    auto now = system_clock::now();
    system_clock::time_point next = now + seconds(5); /* in real we're doing it all 500ms */
    string error;

    this->acknowledge_handler.execute_resend(now, next, buffers);
    auto socket = this->handle->get_socket();
    if(!buffers.empty()) {
        log_trace(category::connection, tr("Resending {} packets"), buffers.size());
        if(socket) {
            for(const auto& buffer : buffers) {
                auto packet = (ts::protocol::OutgoingClientPacket*) buffer->packet_ptr;
                socket->send_message(pipes::buffer_view{packet->packet_data(), packet->packet_length()});

                /* only control packets are getting resend */
                this->statistics_.control_bytes_send += packet->packet_length();
            }
        }
    }

    this->handle->schedule_resend(next);
}

void ProtocolHandler::callback_resend_failed(void *ptr_this,
                                             const std::shared_ptr<ts::connection::AcknowledgeManager::Entry> &) {
    auto connection = reinterpret_cast<ProtocolHandler*>(ptr_this);
    log_error(category::connection, tr("Failed to receive acknowledge"));

    connection->handle->execute_callback_disconnect(tr("packet resend failed"));
    connection->handle->close_connection();
}

void ProtocolHandler::progress_packet(const pipes::buffer_view &buffer) {
    if(this->connection_state >= connection_state::DISCONNECTED) {
        log_warn(category::connection, tr("Dropping received packet. We're already disconnected."));
        return;
    }

    protocol::ServerPacketParser packet_parser{buffer};
    if(!packet_parser.valid()) {
        log_error(category::connection, tr("Received a packet which is too small. ({})"), buffer.length());
        return;
    }

    switch (packet_parser.type()) {
        case protocol::PacketType::VOICE:
        case protocol::PacketType::VOICE_WHISPER:
            this->statistics_.voice_bytes_received += buffer.length();
            break;

        case protocol::PacketType::COMMAND:
        case protocol::PacketType::COMMAND_LOW:
            this->statistics_.control_bytes_received += buffer.length();
            break;

        case protocol::PacketType::INIT1:
            this->handlePacketInit(packet_parser);
            return;
    }

    std::string error{};
    auto decode_result = this->packet_decoder.process_incoming_data(packet_parser, error);
    using PacketProcessResult = protocol::PacketProcessResult;
    switch (decode_result) {
        case PacketProcessResult::SUCCESS:
        case PacketProcessResult::FUZZ_DROPPED: /* maybe some kind of log? */
        case PacketProcessResult::DUPLICATED_PACKET:  /* no action needed, acknowledge should be send already  */
            break;
        case PacketProcessResult::DECRYPT_FAILED: /* Silently drop this packet */
            log_error(category::connection, tr("Failed to decrypt inbound packet ({}). Dropping a packet."), error);
            break;
        case PacketProcessResult::DECRYPT_KEY_GEN_FAILED:
            /* no action needed, acknowledge should be send */
            log_error(category::connection, tr("Failed to generate decrypt key. Dropping a packet."), buffer.length());
            break;

        case PacketProcessResult::BUFFER_OVERFLOW:
        case PacketProcessResult::BUFFER_UNDERFLOW:
            log_error(category::connection, tr("Dropping command packet because command assembly buffer has an {}: {}"),
                      decode_result == PacketProcessResult::BUFFER_UNDERFLOW ? "underflow" : "overflow",
                      error);
            break;

        case PacketProcessResult::UNKNOWN_ERROR:
            log_error(category::connection, tr("Having an unknown error while processing a incoming packet: {}"),
                      error);
            goto disconnect_client;

        case PacketProcessResult::COMMAND_BUFFER_OVERFLOW:
            log_error(category::connection, tr("Having a command buffer overflow. This might cause us to drop."),
                      error);
            break;

        case PacketProcessResult::COMMAND_DECOMPRESS_FAILED:
            log_error(category::connection, tr("Failed to decompress a command packet. Dropping command."),
                      error);
            break;

        case PacketProcessResult::COMMAND_TOO_LARGE:
            log_error(category::connection, tr("Received a too large command. Dropping command."),
                      error);
            break;

        case PacketProcessResult::COMMAND_SEQUENCE_LENGTH_TOO_LONG:
            log_error(category::connection, tr("Received a too long command sequence. Dropping command."), error);
            break;

        default:
            assert(false);
            break;
    }
    return;

    disconnect_client:;
    /* TODO! */
}

void ProtocolHandler::callback_packet_decoded(void *ptr_this, const ts::protocol::PacketParser &packet) {
    auto connection = reinterpret_cast<ProtocolHandler*>(ptr_this);

    if(connection->connection_state >= connection_state::DISCONNECTED) {
        log_warn(category::connection, tr("Don't handle received packets because we're already disconnected."));
        return;
    }

    switch (packet.type()) {
        case protocol::VOICE:
        case protocol::VOICE_WHISPER:
            connection->handlePacketVoice(packet);
            break;

        case protocol::ACK:
        case protocol::ACK_LOW:
            connection->handlePacketAck(packet);
            break;

        case protocol::PING:
        case protocol::PONG:
            connection->handlePacketPing(packet);
            break;

        case protocol::INIT1:
            /* We've received an init1 packet here. The connection should not send that kind of packets... */
            break;

        default:
            log_error(category::connection, tr("Received hand decoded packet, but we've no method to handle it. Dropping packet."));
            assert(false);
            break;
    }
}

void ProtocolHandler::callback_command_decoded(void *ptr_this, ts::command::ReassembledCommand *&command) {
    auto connection = reinterpret_cast<ProtocolHandler*>(ptr_this);
    if(connection->connection_state >= connection_state::DISCONNECTED) {
        log_warn(category::connection, tr("Don't handle received command because we're already disconnected."));
        return;
    }

    connection->handlePacketCommand(std::exchange(command, nullptr));
}

void ProtocolHandler::callback_send_acknowledge(void *ptr_this, uint16_t packet_id, bool low) {
    auto connection = reinterpret_cast<ProtocolHandler*>(ptr_this);
    connection->send_acknowledge(packet_id, low);
}

void ProtocolHandler::send_packet(ts::protocol::OutgoingClientPacket *packet, bool skip_id_branding) {
    if(!skip_id_branding) {
        uint32_t full_id;
        {
            std::lock_guard lock{this->packet_id_mutex};
            full_id = this->_packet_id_manager.generate_full_id(packet->packet_type());
        }
        packet->set_packet_id(full_id & 0xFFFFU);
        packet->generation = full_id >> 16U;
    }

    *(uint16_t*) packet->client_id_bytes = htons(this->client_id);
    packet->next = nullptr;

    auto socket = this->handle->get_socket();
    if(!socket) {
        packet->unref();
        return;
    }

    /* Since we assume that the packets gets written instantly we're setting the next ptr to null */
    if(packet->type_and_flags_ & PacketFlag::Unencrypted) {
        this->crypt_handler.write_default_mac(packet->mac);
    } else {
        ts::connection::CryptHandler::key_t crypt_key{};
        ts::connection::CryptHandler::nonce_t crypt_nonce{};
        if(!this->crypt_setupped) {
            crypt_key = ts::connection::CryptHandler::kDefaultKey;
            crypt_nonce = ts::connection::CryptHandler::kDefaultNonce;
        } else {
            if(!this->crypt_handler.generate_key_nonce(true, packet->packet_type(), packet->packet_id(), packet->generation, crypt_key, crypt_nonce)) {
                log_error(category::connection, tr("Failed to generate crypt key/nonce. Dropping packet"));
                packet->unref();
                return;
            }
        }

        std::string error{};
        auto crypt_result = this->crypt_handler.encrypt(
                (char*) packet->packet_data() + protocol::ClientPacketParser::kHeaderOffset,
                protocol::ClientPacketParser::kHeaderLength,
                packet->payload, packet->payload_size,
                packet->mac,
                crypt_key, crypt_nonce, error);

        if(!crypt_result){
            log_error(category::connection, tr("Failed to encrypt packet: {}"), error);
            packet->unref();
            return;
        }
    }

    switch(packet->packet_type()) {
        case PacketType::COMMAND:
        case PacketType::COMMAND_LOW:
            this->statistics_.control_bytes_send += packet->packet_length();
            break;

        case PacketType::VOICE:
        case PacketType::VOICE_WHISPER:
            this->statistics_.voice_bytes_send += packet->packet_length();
            break;

        default:
            break;
    }

    /* TODO: Don't copy the packet for the socket. Instead just enqueue it. */
    socket->send_message(pipes::buffer_view{packet->packet_data(), packet->packet_length()});
    packet->unref();
}

#define MAX_COMMAND_PACKET_PAYLOAD_LENGTH (487)
void ProtocolHandler::send_command(const std::string_view &command, bool low, std::unique_ptr<std::function<void(bool)>> ack_listener) {
    bool own_data_buffer{false};
    void* own_data_buffer_ptr; /* immutable! */

    const char* data_buffer{command.data()};
    size_t data_length{command.length()};

    uint8_t head_pflags{0};
    protocol::PacketType ptype{low ? protocol::PacketType::COMMAND_LOW : protocol::PacketType::COMMAND};
    protocol::OutgoingClientPacket *packets_head{nullptr};
    protocol::OutgoingClientPacket **packets_tail{&packets_head};

    /* only compress "long" commands */
    if(command.size() > 100) {
        size_t max_compressed_payload_size = compression::qlz_compressed_size(command.data(), command.length());
        auto compressed_buffer = ::malloc(max_compressed_payload_size);

        size_t compressed_size{max_compressed_payload_size};
        if(!compression::qlz_compress_payload(command.data(), command.length(), compressed_buffer, &compressed_size)) {
            //logCritical(0, "Failed to compress command packet. Dropping packet");
            /* TODO: Log! */
            ::free(compressed_buffer);
            return;
        }

        /* we don't need to make the command longer than it is */
        if(compressed_size < command.length()) {
            own_data_buffer = true;
            data_buffer = (char*) compressed_buffer;
            own_data_buffer_ptr = compressed_buffer;
            data_length = compressed_size;
            head_pflags |= protocol::PacketFlag::Compressed;
        } else {
            ::free(compressed_buffer);
        }
    }

    uint8_t ptype_and_flags{(uint8_t) ((uint8_t) ptype | (uint8_t) protocol::PacketFlag::NewProtocol)};
    if(data_length > MAX_COMMAND_PACKET_PAYLOAD_LENGTH) {
        auto chunk_count = (size_t) ceil((float) data_length / (float) MAX_COMMAND_PACKET_PAYLOAD_LENGTH);
        auto chunk_size = (size_t) ceil((float) data_length / (float) chunk_count);

        while(true) {
#ifdef WIN32
            auto bytes = min(chunk_size, data_length);
#else
            auto bytes = std::min(chunk_size, data_length);
#endif
            auto packet = protocol::allocate_outgoing_client_packet(bytes);
            packet->type_and_flags_ = ptype_and_flags;
            memcpy(packet->payload, data_buffer, bytes);

            *packets_tail = packet;
            packets_tail = &packet->next;

            data_length -= bytes;
            if(data_length == 0) {
                packet->type_and_flags_ |= protocol::PacketFlag::Fragmented;
                break;
            }
            data_buffer += bytes;
        }
        packets_head->type_and_flags_ |= protocol::PacketFlag::Fragmented;
    } else {
        auto packet = protocol::allocate_outgoing_client_packet(data_length);
        packet->type_and_flags_ = ptype_and_flags;

        memcpy(packet->payload, data_buffer, data_length);

        packets_head = packet;
        packets_tail = &packet->next;
    }

    {
        std::lock_guard id_lock{this->packet_id_mutex};

        uint32_t full_id;
        auto head = packets_head;
        while(head) {
            full_id = this->_packet_id_manager.generate_full_id(ptype);

            head->set_packet_id(full_id & 0xFFFFU);
            head->generation = full_id >> 16U;

            head = head->next;
        }
    }
    packets_head->type_and_flags_ |= head_pflags;

    /* ack handler */
    {
        auto head = packets_head;
        while(head) {
            auto full_packet_id = (uint32_t) (head->generation << 16U) | head->packet_id();

            /* increase a reference for the ack handler */
            head->ref();

            /* Even thou the packet is yet unencrypted, it will be encrypted with the next write. The next write will be before the next resend because the next ptr must be null in order to resend a packet */
            if(&head->next == packets_tail) {
                this->acknowledge_handler.process_packet(ptype, full_packet_id, head, std::move(ack_listener));
            } else {
                this->acknowledge_handler.process_packet(ptype, full_packet_id, head, nullptr);
            }

            head = head->next;
        }
    }

    auto head = packets_head;
    while(head) {
        auto packet = head;
        head = head->next;
        this->send_packet(packet, true);
    }

    if(own_data_buffer) {
        ::free(own_data_buffer_ptr);
    }
}

void ProtocolHandler::send_command(const ts::Command &cmd, bool low, std::unique_ptr<std::function<void(bool)>> ack_callback) {
    auto data = cmd.build();
    this->send_command(data, low, std::move(ack_callback));
}

void ProtocolHandler::send_acknowledge(uint16_t packet_id, bool low) {
    auto packet = protocol::allocate_outgoing_client_packet(2);

    packet->type_and_flags_ = (uint8_t) (low ? protocol::PacketType::ACK_LOW : protocol::PacketType::ACK) |
                              (uint8_t) (protocol::PacketFlag::NewProtocol);

    //if(!this->crypt_setupped) {
    //    packet->type_and_flags_ |= protocol::PacketFlag::Unencrypted;
    //}

    le2be16(packet_id, packet->payload);
    this->send_packet(packet, false);
}

void ProtocolHandler::do_close_connection() {
    this->connection_state = connection_state::DISCONNECTED;
}

void ProtocolHandler::disconnect(const std::string &reason) {
    if(this->connection_state >= connection_state::DISCONNECTING) {
        return;
    }

    this->connection_state = connection_state::DISCONNECTING;
    this->disconnect_timestamp = system_clock::now();

    auto did = ++this->disconnect_id;
    Command cmd("clientdisconnect");
    cmd["reasonmsg"] = reason;
    this->send_command(cmd, false, std::make_unique<std::function<void(bool)>>([&, did](bool success) {
        /* if !success then we'll have prop already triggered the timeout and this here is obsolete */
        if(success && this->connection_state == connection_state::DISCONNECTING && this->disconnect_id == did) {
            this->handle->close_connection();
        }
    }));
}

const ConnectionStatistics& ProtocolHandler::statistics() {
    return this->statistics_;
}