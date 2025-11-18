#pragma once

#include <string_view>
#include <query/command3.h>
#include <Error.h>
#include <src/lincense/TeamSpeakLicense.h>
#include <tomcrypt.h>
#include <variant>

namespace ts::connection {
    class VoiceClientConnection;
}

namespace ts::server::server::udp {
    class CryptSetupHandler {
        public:
            enum struct CommandHandleResult {
                CONSUME_COMMAND,
                CLOSE_CONNECTION,
                PASS_THROUGH
            };
            using CommandResult = std::variant<ts::command_result, CommandHandleResult>;

            explicit CryptSetupHandler(connection::VoiceClientConnection*);

            [[nodiscard]] inline const auto& identity_key() const { return this->remote_key; }

            void set_client_protocol_time(uint32_t time) { this->client_protocol_time_ = time; }
            [[nodiscard]] inline auto client_protocol_time() const { return this->client_protocol_time_; }

            [[nodiscard]] inline auto last_handled_command() const { return this->last_command_; }

            /* Attention this method gets from the voice IO thread. It's not thread save! */
            [[nodiscard]] CommandHandleResult handle_command(const std::string_view& /* command */);
        private:
            connection::VoiceClientConnection* connection;

            std::chrono::system_clock::time_point last_command_{};

            std::mutex command_lock{};

            bool new_protocol{false};
            uint32_t client_protocol_time_{0};

            std::string seed_client{}; /* alpha */
            std::string seed_server{}; /* beta */

            std::shared_ptr<LicenseChainData> chain_data{};
            std::shared_ptr<ecc_key> remote_key{};

            CommandResult handleCommandClientInitIv(const ts::command_parser& /* command */);
            CommandResult handleCommandClientEk(const ts::command_parser& /* command */);

            CommandResult handleCommandClientInit(const ts::command_parser& /* command */);
    };
}