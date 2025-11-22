#pragma once

#include "../misc/spin_mutex.h"
#include "./PacketLossCalculator.h"
#include "./Packet.h"

namespace ts::protocol {
    class PacketStatistics {
        public:
            struct PacketLossReport {
                uint32_t lost_voice{0};
                uint32_t lost_control{0};
                uint32_t lost_keep_alive{0};

                uint32_t received_voice{0};
                uint32_t received_control{0};
                uint32_t received_keep_alive{0};

                [[nodiscard]] inline float voice_loss() const {
                    const auto total_packets = this->received_voice + this->lost_voice;
                    if(total_packets == 0) return 0;
                    return this->lost_voice / (float) total_packets;
                }
                [[nodiscard]] inline float control_loss() const {
                    const auto total_packets = this->received_control + this->lost_control;
                    //if(total_packets == 0) return 0; /* not possible so remove this to speed it up */
                    return this->lost_control / (float) total_packets;
                }
                [[nodiscard]] inline float keep_alive_loss() const {
                    const auto total_packets = this->received_keep_alive + this->lost_keep_alive;
                    if(total_packets == 0) return 0;
                    return this->lost_keep_alive / (float) total_packets;
                }

                [[nodiscard]] inline float total_loss() const {
                    const auto total_lost = this->lost_voice + this->lost_control + this->lost_keep_alive;
                    const auto total_received = this->received_control + this->received_voice + this->received_keep_alive;
                    //if(total_received + total_lost == 0) return 0; /* not possible to speed this up */
                    return total_lost / (float) (total_lost + total_received);
                }
            };

            [[nodiscard]] PacketLossReport loss_report() const;
            [[nodiscard]] float current_packet_loss() const;

            void send_command(protocol::PacketType /* type */, uint32_t /* packet id */);
            void received_acknowledge(protocol::PacketType /* type */, uint32_t /* packet id */);

            void received_packet(protocol::PacketType /* type */, uint32_t /* packet id */);
            void tick();
            void reset();
            void reset_offsets();
        private:
            std::chrono::system_clock::time_point last_short{};

            spin_mutex data_mutex{};
            protocol::UnorderedPacketLossCalculator calculator_voice_whisper{};
            protocol::UnorderedPacketLossCalculator calculator_voice{};

            protocol::UnorderedPacketLossCalculator calculator_ack_low{};
            protocol::UnorderedPacketLossCalculator calculator_ack{};

            protocol::UnorderedPacketLossCalculator calculator_ping{};

            protocol::CommandPacketLossCalculator calculator_command{};
            protocol::CommandPacketLossCalculator calculator_command_low{};
    };
}