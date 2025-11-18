#pragma once

#include "./PacketEncoder.h"
#include "src/client/shared/ServerCommandExecutor.h"
#include "CryptSetupHandler.h"
#include "VoiceClient.h"
#include "../shared/WhisperHandler.h"
#include <chrono>
#include <condition_variable>
#include <deque>
#include <event.h>
#include <pipes/buffer.h>
#include <protocol/PingHandler.h>
#include <protocol/PacketDecoder.h>
#include <protocol/PacketStatistics.h>
#include <protocol/AcknowledgeManager.h>
#include <protocol/CompressionHandler.h>
#include <protocol/CryptHandler.h>
#include <protocol/buffers.h>
#include <protocol/generation.h>
#include <protocol/ringbuffer.h>
#include <utility>

//#define LOG_ACK_SYSTEM
#ifdef LOG_ACK_SYSTEM
    #define LOG_AUTO_ACK_REQUEST
    #define LOG_AUTO_ACK_RESPONSE
    #define LOG_PKT_RESEND
#endif

//#define PKT_LOG_PING
namespace ts {
    namespace server {
        class VoiceClient;
        class VoiceServer;
        class POWHandler;
        class VoiceServerSocket;
    }

    namespace connection {
        class VoiceClientConnection {
                friend class AcknowledgeManager;
                friend class server::VoiceServer;
                friend class server::VoiceClient;
                friend class server::POWHandler;

                using PacketDecoder = protocol::PacketDecoder;
                using PacketEncoder = server::server::udp::PacketEncoder;
                using PingHandler = server::server::udp::PingHandler;
                using CryptSetupHandler = server::server::udp::CryptSetupHandler;
                using ReassembledCommand = command::ReassembledCommand;

                using StatisticsCategory = stats::ConnectionStatistics::category;
            public:
                explicit VoiceClientConnection(server::VoiceClient*);
                virtual ~VoiceClientConnection();

                void send_packet(protocol::PacketType /* type */, const protocol::PacketFlags& /* flags */, const void* /* payload */, size_t /* payload length */);
                void send_packet(protocol::OutgoingServerPacket* /* packet */); /* method takes ownership of the packet */
                void send_command(const std::string_view& /* build command command */, bool /* command low */, std::unique_ptr<std::function<void(bool)>> /* acknowledge listener */);

                CryptHandler* getCryptHandler(){ return &crypt_handler; }

                std::shared_ptr<server::VoiceClient> getCurrentClient();

                bool wait_empty_write_and_prepare_queue(std::chrono::time_point<std::chrono::system_clock> until = std::chrono::time_point<std::chrono::system_clock>());

                void reset();
                void reset_remote_address();

                [[nodiscard]] std::string log_prefix();

                [[nodiscard]] inline auto virtual_server_id() const { return this->virtual_server_id_; }

                [[nodiscard]] inline const auto& remote_address_info() const { return this->remote_address_info_; }
                [[nodiscard]] inline const auto& socket() const { return this->socket_; }

                [[nodiscard]] inline auto& packet_statistics() { return this->packet_statistics_; }
                [[nodiscard]] inline auto& packet_decoder() { return this->packet_decoder_; }
                [[nodiscard]] inline auto& packet_encoder() { return this->packet_encoder_; }

                [[nodiscard]] inline auto& ping_handler() { return this->ping_handler_; }
                [[nodiscard]] inline auto& crypt_setup_handler() { return this->crypt_setup_handler_; }

                void handle_incoming_datagram(protocol::ClientPacketParser& /* packet */);
                bool verify_encryption(const protocol::ClientPacketParser& /* packet */);
            private:
                ServerId virtual_server_id_;
                server::VoiceClient* current_client;

                /* The remote address is stored within the client object.... FIXME! */
                std::shared_ptr<server::VoiceServerSocket> socket_{};
                server::udp::pktinfo_storage remote_address_info_{};

                CryptHandler crypt_handler; /* access to CryptHandler is thread save */
                protocol::PacketStatistics packet_statistics_{};

                PacketDecoder packet_decoder_;
                PacketEncoder packet_encoder_;

                CryptSetupHandler crypt_setup_handler_;
                PingHandler ping_handler_{};

                static void callback_packet_decoded(void*, const protocol::PacketParser&);
                static void callback_command_decoded(void*, ReassembledCommand*&);
                static void callback_send_acknowledge(void*, uint16_t, bool);
                static void callback_request_write(void*);
                static void callback_encode_crypt_error(void*, const PacketEncoder::CryptError&, const std::string&);
                static void callback_resend_failed(void*, const std::shared_ptr<AcknowledgeManager::Entry>&);
                static void callback_resend_statistics(void*, size_t);
                static void callback_outgoing_connection_statistics(void*, StatisticsCategory::value, size_t /* bytes */);
                static void callback_ping_send(void*, uint16_t&);
                static void callback_ping_send_recovery(void*);
                static void callback_ping_timeout(void*);

                /* Attention: All packet callbacks are called from the IO threads and are not thread save! */
                void handlePacketCommand(ReassembledCommand* /* command */); /* The ownership will be transferred */
                void handlePacketAck(const protocol::PacketParser&);
                void handlePacketAckLow(const protocol::PacketParser&);
                void handlePacketVoice(const protocol::PacketParser&);
                void handlePacketVoiceWhisper(const protocol::PacketParser&);
                void handlePacketPing(const protocol::PacketParser&);
                void handlePacketPong(const protocol::PacketParser&);
        };
    }
}