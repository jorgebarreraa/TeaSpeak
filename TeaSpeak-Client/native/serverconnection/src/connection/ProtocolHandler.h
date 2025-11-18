#pragma once

#include <chrono>
#include <cstdint>

#define NO_LOG
#ifdef WIN32
    #include <WinSock2.h> //Needs to be included; No clue why
#endif

#include <protocol/ringbuffer.h>
#include <protocol/Packet.h>
#include <protocol/CryptHandler.h>
#include <protocol/CompressionHandler.h>
#include <protocol/AcknowledgeManager.h>
#include <protocol/generation.h>
#include <protocol/PacketDecoder.h>
#include <query/Command.h>
#include "ServerConnection.h"

namespace tc::connection {
    class ServerConnection;

    namespace connection_state {
        enum value {
            INITIALIZING,
            INIT_LOW,
            INIT_HIGH,
            CONNECTING,
            CONNECTED,
            DISCONNECTING,
            DISCONNECTED
        };
    };
    namespace pow_state {
        enum value : uint8_t {
            COOKIE_GET,
            COOKIE_SET,
            PUZZLE_GET,
            PUZZLE_SET,
            PUZZLE_SOLVE,
            PUZZLE_RESET,
            COMPLETED,
            COMMAND_RESET = 127,
            UNSET = 0xFB
        };
    };

    struct ConnectionStatistics {
        size_t control_bytes_send{0};
        size_t control_bytes_received{0};

        size_t voice_bytes_send{0};
        size_t voice_bytes_received{0};
    };

    class ProtocolHandler {
            friend class ServerConnection;
        public:
            explicit ProtocolHandler(ServerConnection*);
            ~ProtocolHandler();

            void reset();
            void connect();
            void execute_tick();
            void execute_resend();

            const ConnectionStatistics& statistics();

            void progress_packet(const pipes::buffer_view& /* buffer */);

            void send_packet(ts::protocol::OutgoingClientPacket* /* packet */, bool /* skip id branding */); /* will claim ownership */
            void send_command(const std::string_view& /* build command command */, bool /* command low */, std::unique_ptr<std::function<void(bool)>> /* acknowledge listener */ = nullptr);
            void send_command(const ts::Command&, bool /* command low */, std::unique_ptr<std::function<void(bool)>> /* acknowledge listener */ = nullptr);

            void disconnect(const std::string& /* message */);
            void send_acknowledge(uint16_t /* packet id */, bool /* low */);

            /* Initialize a new crypto identity from the given string */
            bool initialize_identity(const std::optional<std::string_view>& identity);
            void reset_identity();

            inline std::chrono::microseconds current_ping() const { return this->ping.value; }

            connection_state::value connection_state = connection_state::INITIALIZING;
            server_type::value server_type = server_type::TEASPEAK;
        private:
            void do_close_connection(); /* only call from ServerConnection. Close all connections via ServerConnection! */

            static void callback_packet_decoded(void*, const ts::protocol::PacketParser&);
            static void callback_command_decoded(void*, ts::command::ReassembledCommand*&);
            static void callback_send_acknowledge(void*, uint16_t, bool);
            static void callback_resend_failed(void*, const std::shared_ptr<ts::connection::AcknowledgeManager::Entry>&);

            /* Ownership will be transfered */
            void handlePacketCommand(ts::command::ReassembledCommand* /* command */);
            void handlePacketAck(const ts::protocol::PacketParser&);
            void handlePacketVoice(const ts::protocol::PacketParser&);
            void handlePacketPing(const ts::protocol::PacketParser&);
            void handlePacketInit(const ts::protocol::PacketParser&);

            ServerConnection* handle;

            std::chrono::system_clock::time_point connect_timestamp;
            std::chrono::system_clock::time_point disconnect_timestamp;
            uint8_t disconnect_id = 0;

            struct {
                size_t retry_count{0};
                pow_state::value state;

                uint64_t client_ts3_build_timestamp = 173265950 /* TS3 */; /* needs to be lower than 173265950 for old stuff, else new protocol  */
                uint8_t client_control_data[4] = {0,0,0,0};
                uint8_t server_control_data[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
                uint8_t server_data[100];

                std::chrono::system_clock::time_point last_response;
                std::chrono::system_clock::time_point last_resend;
                pipes::buffer last_buffer;
            } pow;
            void pow_send_cookie_get();
            void send_init1_buffer();

            struct {
                uint8_t alpha[10];
                uint8_t beta[54];
                uint8_t beta_length; /* 10 or 54 */

                std::optional<ecc_key> identity{};

                std::string initiv_command;
            } crypto;
            std::string generate_client_initiv();

            uint16_t client_id{0};

            std::mutex packet_id_mutex{};
            ts::protocol::PacketIdManager _packet_id_manager;

            bool crypt_setupped{false};
            ts::connection::CryptHandler crypt_handler;
            ts::protocol::PacketDecoder packet_decoder;
            ts::connection::AcknowledgeManager acknowledge_handler;

            ConnectionStatistics statistics_{};

            struct {
                std::chrono::system_clock::time_point ping_send_timestamp{};
                std::chrono::system_clock::time_point ping_received_timestamp{};
                std::chrono::microseconds value{0};
                uint16_t ping_id{0};

                std::chrono::microseconds interval{2500};
            } ping;

            void handleCommandInitIVExpend(ts::Command&);
            void handleCommandInitIVExpend2(ts::Command&);
            void handleCommandInitServer(ts::Command&);

            void ping_send_request();
    };
}