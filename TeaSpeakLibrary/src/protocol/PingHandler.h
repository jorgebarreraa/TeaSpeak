#pragma once

#include <cstdint>
#include <chrono>

namespace ts::server::server::udp {
    class PingHandler {
        public:
            typedef void(*callback_time_outed_t)(void* /* cb data */);
            typedef void(*callback_send_ping_t)(void* /* cb data */, uint16_t& /* ping id */);
            typedef void(*callback_send_recovery_command_t)(void* /* cb data */);

            void reset();

            void tick(const std::chrono::system_clock::time_point&);
            void received_pong(uint16_t /* ping id */);
            void received_command_acknowledged();

            [[nodiscard]] inline std::chrono::milliseconds current_ping() const { return this->current_ping_; }
            [[nodiscard]] inline std::chrono::system_clock::time_point last_ping_response() const { return this->last_response_; }
            [[nodiscard]] inline std::chrono::system_clock::time_point last_command_acknowledged() const { return this->last_command_acknowledge_; }

            void* callback_argument{nullptr};
            callback_send_ping_t callback_send_ping{nullptr};
            callback_send_recovery_command_t callback_send_recovery_command{nullptr};
            callback_time_outed_t callback_time_outed{nullptr};
        private:
            constexpr static std::chrono::milliseconds kPingRequestInterval{1000};
            constexpr static std::chrono::milliseconds kPingTimeout{15 * 1000};

            constexpr static std::chrono::milliseconds kRecoveryRequestInterval{1000};
            constexpr static std::chrono::milliseconds kRecoveryTimeout{15 * 1000};

            std::chrono::milliseconds current_ping_{0};

            uint16_t last_ping_id{0};
            std::chrono::system_clock::time_point last_response_{};
            std::chrono::system_clock::time_point last_request_{};

            std::chrono::system_clock::time_point last_command_acknowledge_{};
            std::chrono::system_clock::time_point last_recovery_command_send{};

            void send_ping_request();
            void send_recovery_request();
    };
}
