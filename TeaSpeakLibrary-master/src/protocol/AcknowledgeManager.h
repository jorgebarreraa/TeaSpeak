#pragma once

#include <memory>
#include <protocol/Packet.h>

#define DEBUG_ACKNOWLEDGE
namespace ts::connection {
    class VoiceClientConnection;
    class AcknowledgeManager {
        public:
            struct Entry {
                uint16_t packet_id{0};
                uint16_t generation_id{0};

                uint8_t packet_type{0xFF};
                uint8_t resend_count{0};
                bool acknowledged : 1;
                uint8_t send_count : 7;


                pipes::buffer buffer;
                std::chrono::system_clock::time_point first_send;
                std::chrono::system_clock::time_point next_resend;
                std::unique_ptr<threads::Future<bool>> acknowledge_listener;
            };

            AcknowledgeManager();
            virtual ~AcknowledgeManager();

            size_t awaiting_acknowledge();
            void reset();

            void process_packet(ts::protocol::BasicPacket& /* packet */);
            bool process_acknowledge(uint8_t packet_type, uint16_t /* packet id */, std::string& /* error */);

            ssize_t execute_resend(
                    const std::chrono::system_clock::time_point& /* now */,
                    std::chrono::system_clock::time_point& /* next resend */,
                    std::deque<std::shared_ptr<Entry>>& /* buffers to resend */,
                    std::string& /* error */
            );

            [[nodiscard]] inline auto current_rto() const { return this->rto; }
            [[nodiscard]] inline auto current_srtt() const { return this->srtt; }
            [[nodiscard]] inline auto current_rttvar() const { return this->rttvar; }
        private:
            std::mutex entry_lock;
            std::deque<std::shared_ptr<Entry>> entries;

            float rto{1000};
            float srtt{-1};
            float rttvar{};

            constexpr static auto alpha{.125f};
            constexpr static auto beta{.25f};

            void update_rto(size_t /* response time */);
    };
}