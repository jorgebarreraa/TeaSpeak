#pragma once

#include <cstdint>
#include <atomic>
#include <chrono>

namespace ts {
    namespace protocol {
        /**
         * Packet statistics tracking for voice connections
         */
        struct PacketStatistics {
            std::atomic<uint64_t> packets_sent{0};
            std::atomic<uint64_t> packets_received{0};
            std::atomic<uint64_t> bytes_sent{0};
            std::atomic<uint64_t> bytes_received{0};

            std::atomic<uint64_t> packets_lost{0};
            std::atomic<uint64_t> packets_out_of_order{0};

            std::chrono::system_clock::time_point last_packet_time;

            PacketStatistics() = default;
            ~PacketStatistics() = default;

            void reset() {
                packets_sent.store(0);
                packets_received.store(0);
                bytes_sent.store(0);
                bytes_received.store(0);
                packets_lost.store(0);
                packets_out_of_order.store(0);
                last_packet_time = std::chrono::system_clock::now();
            }

            void record_sent_packet(size_t size) {
                packets_sent.fetch_add(1);
                bytes_sent.fetch_add(size);
                last_packet_time = std::chrono::system_clock::now();
            }

            void record_received_packet(size_t size) {
                packets_received.fetch_add(1);
                bytes_received.fetch_add(size);
                last_packet_time = std::chrono::system_clock::now();
            }

            void record_lost_packet() {
                packets_lost.fetch_add(1);
            }

            void record_out_of_order_packet() {
                packets_out_of_order.fetch_add(1);
            }
        };
    }
}
