#include <log/LogUtils.h>
#include <misc/memtracker.h>
#include <protocol/Packet.h>
#include <ThreadPool/Timer.h>

#include "../../server/VoiceServer.h"
#include "./VoiceClientConnection.h"


//#define LOG_AUTO_ACK_AUTORESPONSE
//#define FUZZING_TESTING_INCOMMING
//#define FUZZING_TESTING_OUTGOING
//#define FIZZING_TESTING_DISABLE_HANDSHAKE
#define FUZZING_TESTING_DROP 8
#define FUZZING_TESTING_DROP_MAX 10

//#define CONNECTION_NO_STATISTICS

#define QLZ_COMPRESSION_LEVEL 1
#include "qlz/QuickLZ.h"

using namespace std;
using namespace std::chrono;
using namespace ts;
using namespace ts::connection;
using namespace ts::protocol;
using namespace ts::server;

VoiceClientConnection::VoiceClientConnection(VoiceClient* client)
    : current_client{client},
      crypt_handler{},
      packet_decoder_{&this->crypt_handler, true},
      packet_encoder_{&this->crypt_handler, &this->packet_statistics_},
      crypt_setup_handler_{this} {
    memtrack::allocated<VoiceClientConnection>(this);

    this->packet_decoder_.callback_argument = this;
    this->packet_decoder_.callback_decoded_packet = VoiceClientConnection::callback_packet_decoded;
    this->packet_decoder_.callback_decoded_command = VoiceClientConnection::callback_command_decoded;
    this->packet_decoder_.callback_send_acknowledge = VoiceClientConnection::callback_send_acknowledge;

    this->packet_encoder_.callback_data = this;
    this->packet_encoder_.callback_request_write = VoiceClientConnection::callback_request_write;
    this->packet_encoder_.callback_crypt_error = VoiceClientConnection::callback_encode_crypt_error;
    this->packet_encoder_.callback_resend_failed = VoiceClientConnection::callback_resend_failed;
    this->packet_encoder_.callback_resend_stats = VoiceClientConnection::callback_resend_statistics;
    this->packet_encoder_.callback_connection_stats = VoiceClientConnection::callback_outgoing_connection_statistics;

    this->ping_handler_.callback_argument = this;
    this->ping_handler_.callback_send_ping = VoiceClientConnection::callback_ping_send;
    this->ping_handler_.callback_send_recovery_command = VoiceClientConnection::callback_ping_send_recovery;
    this->ping_handler_.callback_time_outed = VoiceClientConnection::callback_ping_timeout;

    this->virtual_server_id_ = client->getServerId();
    debugMessage(client->getServer()->getServerId(), "Allocated new voice client connection at {}", (void*) this);
}

VoiceClientConnection::~VoiceClientConnection() {
    this->reset();
    this->current_client = nullptr;
    memtrack::freed<VoiceClientConnection>(this);
}

std::string VoiceClientConnection::log_prefix() {
    auto client = this->getCurrentClient();
    if(!client) return "[unknown / unknown]"; /* FIXME: Get the IP address here! */

    return CLIENT_STR_LOG_PREFIX_(client);
}

void VoiceClientConnection::handle_incoming_datagram(protocol::ClientPacketParser& packet_parser) {
#ifndef CONNECTION_NO_STATISTICS
    if(this->current_client) {
        auto stats = this->current_client->connectionStatistics;
        stats->logIncomingPacket(stats::ConnectionStatistics::category::from_type(packet_parser.type()), packet_parser.buffer().length() + 96); /* 96 for the UDP packet overhead */
    }
    this->packet_statistics().received_packet((protocol::PacketType) packet_parser.type(), packet_parser.full_packet_id());
#endif

    std::string error{};
    auto result = this->packet_decoder_.process_incoming_data(packet_parser, error);
    using PacketProcessResult = protocol::PacketProcessResult;
    switch (result) {
        case PacketProcessResult::SUCCESS:
        case PacketProcessResult::FUZZ_DROPPED: /* maybe some kind of log? */
        case PacketProcessResult::DECRYPT_FAILED: /* Silently drop this packet */
        case PacketProcessResult::DUPLICATED_PACKET:  /* no action needed, acknowledge should be send already  */
            break;

        case PacketProcessResult::DECRYPT_KEY_GEN_FAILED:
            /* no action needed, acknowledge should be send */
            logCritical(this->virtual_server_id_, "{} Failed to generate decrypt key. Dropping packet.", this->log_prefix());
            break;

        case PacketProcessResult::BUFFER_OVERFLOW:
        case PacketProcessResult::BUFFER_UNDERFLOW:
            debugMessage(this->virtual_server_id_, "{} Dropping command packet because command assembly buffer has an {}: {}",
                         this->log_prefix(),
                         result == PacketProcessResult::BUFFER_UNDERFLOW ? "underflow" : "overflow",
                         error
            );
            break;

        case PacketProcessResult::UNKNOWN_ERROR:
            logCritical(this->virtual_server_id_, "{} Having an unknown error while processing a incoming packet: {}",
                        this->log_prefix(),
                        error
            );
            goto disconnect_client;

        case PacketProcessResult::COMMAND_BUFFER_OVERFLOW:
            debugMessage(this->virtual_server_id_, "{} Having a command buffer overflow. This might cause the client to drop.", this->log_prefix());
            break;

        case PacketProcessResult::COMMAND_DECOMPRESS_FAILED:
            logWarning(this->virtual_server_id_, "{} Failed to decompress a command packet. Dropping command.", this->log_prefix());
            break;

        case PacketProcessResult::COMMAND_TOO_LARGE:
            logWarning(this->virtual_server_id_, "{} Received a too large command. Dropping client.", this->log_prefix());
            goto disconnect_client;

        case PacketProcessResult::COMMAND_SEQUENCE_LENGTH_TOO_LONG:
            logWarning(this->virtual_server_id_, "{} Received a too long command sequence. Dropping client.", this->log_prefix());
            goto disconnect_client;

        default:
            assert(false);
            break;
    }

    return;

    disconnect_client:;
    /* FIXME: Disconnect the client */
}

void VoiceClientConnection::callback_send_acknowledge(void *ptr_this, uint16_t packet_id, bool command_low) {
    reinterpret_cast<VoiceClientConnection*>(ptr_this)->packet_encoder_.send_packet_acknowledge(packet_id, command_low);
}

void VoiceClientConnection::callback_packet_decoded(void *ptr_this, const ts::protocol::PacketParser &packet) {
    auto connection = reinterpret_cast<VoiceClientConnection*>(ptr_this);
    switch (packet.type()) {
        case protocol::VOICE:
            connection->handlePacketVoice(packet);
            break;

        case protocol::VOICE_WHISPER:
            connection->handlePacketVoiceWhisper(packet);
            break;

        case protocol::ACK:
            connection->handlePacketAck(packet);
            break;

        case protocol::ACK_LOW:
            connection->handlePacketAckLow(packet);
            break;

        case protocol::PING:
            connection->handlePacketPing(packet);
            break;

        case protocol::PONG:
            connection->handlePacketPong(packet);
            break;

        case protocol::INIT1:
            /* We've received an init1 packet here. The connection should not send that kind of packets... */
            break;

        default:
            logError(connection->virtual_server_id_, "{} Received hand decoded packet, but we've no method to handle it. Dropping packet.", connection->log_prefix());
            assert(false);
            break;
    }
}

void VoiceClientConnection::callback_command_decoded(void *ptr_this, ReassembledCommand *&command) {
    auto connection = reinterpret_cast<VoiceClientConnection*>(ptr_this);

    /* we're exchanging the command so we're taking the ownership */
    connection->handlePacketCommand(std::exchange(command, nullptr));
}

bool VoiceClientConnection::verify_encryption(const protocol::ClientPacketParser& packet) {
    return this->packet_decoder_.verify_encryption_client_packet(packet);
}

std::shared_ptr<ts::server::VoiceClient> VoiceClientConnection::getCurrentClient() {
    if(!this->current_client) return nullptr;
    return std::dynamic_pointer_cast<server::VoiceClient>(this->current_client->ref());
}

bool VoiceClientConnection::wait_empty_write_and_prepare_queue(chrono::time_point<chrono::system_clock> until) {
    return this->packet_encoder_.wait_empty_write_and_prepare_queue(until);
}

void VoiceClientConnection::reset() {
    this->crypt_handler.reset();
    this->ping_handler_.reset();
    this->packet_decoder_.reset();
    this->packet_encoder_.reset();
}

void VoiceClientConnection::reset_remote_address() {
    memset(&this->current_client->remote_address, 0, sizeof(this->current_client->remote_address));
    memset(&this->remote_address_info_, 0, sizeof(this->remote_address_info_));
}

void VoiceClientConnection::send_packet(protocol::PacketType type, const protocol::PacketFlags& flag, const void *payload, size_t payload_size) {
    this->packet_encoder_.send_packet(type, flag, payload, payload_size);
}

void VoiceClientConnection::send_packet(protocol::OutgoingServerPacket* packet) {
    this->packet_encoder_.send_packet(packet);
}

void VoiceClientConnection::send_command(const std::string_view &cmd, bool b, std::unique_ptr<std::function<void(bool)>> cb) {
    this->packet_encoder_.send_command(cmd, b, std::move(cb));
}

void VoiceClientConnection::callback_encode_crypt_error(void *ptr_this,
                                                        const PacketEncoder::CryptError &error,
                                                        const std::string &detail) {
    auto connection = reinterpret_cast<VoiceClientConnection*>(ptr_this);
    switch (error) {
        case PacketEncoder::CryptError::ENCRYPT_FAILED:
            logError(connection->virtual_server_id_, "{} Failed to encrypt packet. Error: {}", connection->log_prefix(), detail);
            break;

        case PacketEncoder::CryptError::KEY_GENERATION_FAILED:
            logError(connection->virtual_server_id_, "{} Failed to generate crypt key/nonce for sending a packet. This should never happen! Dropping packet.", connection->log_prefix());
            break;

        default:
            assert(false);
            return;
    }
}

void VoiceClientConnection::callback_request_write(void *ptr_this) {
    auto connection = reinterpret_cast<VoiceClientConnection*>(ptr_this);
    connection->socket_->enqueue_client_write(connection->current_client->ref_self_voice);
}

void VoiceClientConnection::callback_resend_failed(void *ptr_this, const shared_ptr<AcknowledgeManager::Entry> &entry) {
    auto connection = reinterpret_cast<VoiceClientConnection*>(ptr_this);

    debugMessage(connection->virtual_server_id_, "{} Failed to execute packet resend of packet {}. Dropping connection.", connection->log_prefix(), entry->packet_full_id);
    auto client = connection->getCurrentClient();
    assert(client); /* TIXME! */

    if(client->state == ConnectionState::CONNECTED) {
        client->disconnect(ViewReasonId::VREASON_TIMEOUT, config::messages::timeout::packet_resend_failed, nullptr, true);
    }

    {
        /*
         * The connection timeout out.
         * We don't expect any disconnect packages to be acknowledges so we don't need to
         * await for such.
         */
        std::lock_guard lock{client->flush_mutex};
        client->disconnect_acknowledged = std::make_optional(true);
    }
    client->close_connection(std::chrono::system_clock::time_point{});
}

void VoiceClientConnection::callback_resend_statistics(void *ptr_this, size_t send_count) {
    auto connection = reinterpret_cast<VoiceClientConnection*>(ptr_this);

    logTrace(connection->virtual_server_id_, "{} Resending {} packets.", connection->log_prefix(), send_count);
}

void VoiceClientConnection::callback_outgoing_connection_statistics(void *ptr_this,
                                                                    ts::stats::ConnectionStatistics::category::value category,
                                                                    size_t send_count) {
    auto connection = reinterpret_cast<VoiceClientConnection*>(ptr_this);
    auto client = connection->getCurrentClient();
    if(!client) return;

    auto statistics = client->connectionStatistics;
    if(!statistics) return;

    statistics->logOutgoingPacket(category, send_count);
}

void VoiceClientConnection::callback_ping_send(void *ptr_this, uint16_t &id) {
    auto connection = reinterpret_cast<VoiceClientConnection*>(ptr_this);

    auto packet = protocol::allocate_outgoing_server_packet(0);
    packet->ref();

    packet->type_and_flags_ = (uint8_t) PacketType::PING | (uint8_t) PacketFlag::Unencrypted;
    connection->packet_encoder_.send_packet(packet);
    id = packet->packet_id();

    packet->unref();
}

void VoiceClientConnection::callback_ping_send_recovery(void *ptr_this) {
    auto connection = reinterpret_cast<VoiceClientConnection*>(ptr_this);

    connection->send_command("notifyconnectioninforequest invokerids=0", false, nullptr);
}

void VoiceClientConnection::callback_ping_timeout(void *ptr_this) {
    (void) ptr_this;
    /* doing nothing a packet resend failed will cause the client to disconnect */
}