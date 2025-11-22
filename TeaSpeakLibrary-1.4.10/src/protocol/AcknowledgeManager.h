#pragma once

#include <memory>
#include <chrono>
#include <functional>
#include <mutex>
#include "./Packet.h"
#include "./RtoCalculator.h"

namespace ts::connection {
    class AcknowledgeManager {
        public:
            struct Entry {
                uint32_t packet_full_id{0};
                uint8_t packet_type{0xFF};

                uint8_t resend_count{0};
                bool acknowledged : 1;
                uint8_t send_count : 7;

                std::chrono::system_clock::time_point first_send;
                std::chrono::system_clock::time_point next_resend;
                std::unique_ptr<std::function<void(bool)>> acknowledge_listener;

                void* packet_ptr;
            };

            typedef void(*callback_resend_failed_t)(void* /* user data */, const std::shared_ptr<Entry>& /* entry */);

            AcknowledgeManager();
            virtual ~AcknowledgeManager();

            [[nodiscard]] size_t awaiting_acknowledge();
            void reset();

            void process_packet(uint8_t /* packet type */, uint32_t /* full packet id */, void* /* packet ptr */, std::unique_ptr<std::function<void(bool)>> /* ack listener */);
            bool process_acknowledge(uint8_t /* packet type */, uint16_t /* packet id */, std::string& /* error */);

            void execute_resend(
                    const std::chrono::system_clock::time_point& /* now */,
                    std::chrono::system_clock::time_point& /* next resend */,
                    std::deque<std::shared_ptr<Entry>>& /* buffers to resend */
            );

            [[nodiscard]] inline const auto& rto_calculator() const { return this->rto_calculator_; }

            void(*destroy_packet)(void* /* packet */){nullptr};

            void* callback_data{nullptr};
            callback_resend_failed_t callback_resend_failed{[](auto, auto){}}; /* must be valid all the time */
        private:
            std::mutex entry_lock;
            std::deque<std::shared_ptr<Entry>> entries;
            protocol::RtoCalculator rto_calculator_{};
    };
}