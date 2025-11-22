#pragma once

#include <utility>
#include <cassert>
#include <cstddef>
#include <bitset>

namespace ts::protocol {
    template <size_t N>
    struct bit_set;

    template <>
    struct bit_set<32> {
            constexpr static auto native_memory_bytes = sizeof(uint32_t);
            constexpr static auto native_memory_bits = native_memory_bytes * 8;
        public:
            bit_set() : memory{0} {}
            explicit bit_set(uint32_t mem) : memory{mem} {}

            [[nodiscard]] inline auto max_bits() const { return native_memory_bits; }

            inline auto shift_in_bounds(size_t offset) {
                assert(offset <= this->native_memory_bits);

                auto result = bit_set<native_memory_bits>{this->memory >> (native_memory_bits - offset)};
                this->memory <<= offset;
                return result;
            }

            inline void set_unchecked(size_t offset) {
                this->memory |= 1U << offset;
            }

            [[nodiscard]] inline auto count() const {
                /* std::bitset will get optimized away */
                return std::bitset<32>(this->memory).count();
            }

            inline auto clear() { return bit_set<native_memory_bits>{std::exchange(this->memory, 0)}; }
        private:
            uint32_t memory;
    };

    /* will we ever have more than 2^32 packets? Hell no. That would be over (2^32 / (50 * 60 * 60 * 24) := 994.2 days continuous speaking */
    class UnorderedPacketLossCalculator {
        public:
            UnorderedPacketLossCalculator() = default;

            void packet_received(uint32_t /* packet id */);
            void short_stats();
            void reset();
            void reset_offsets();

            [[nodiscard]] inline auto last_packet_id() const { return this->packet_history_offset; }
            [[nodiscard]] inline bool valid_data() const { return this->packet_history_offset >= 32; }

            [[nodiscard]] inline uint32_t received_packets() const { return this->received_packets_; }
            [[nodiscard]] inline uint32_t lost_packets() const { return this->lost_packets_; }

            [[nodiscard]] inline uint32_t received_packets_total() const { return this->received_packets_total_; }
            [[nodiscard]] inline uint32_t lost_packets_total() const { return this->lost_packets_total_; }

            [[nodiscard]] inline uint32_t unconfirmed_received_packets() const { return (uint32_t) this->packet_history.count(); };
            [[nodiscard]] inline uint32_t unconfirmed_lost_packets() const { return (uint32_t) (this->packet_history.max_bits() - this->packet_history.count()); };

        private:
            uint32_t received_packets_{0}, received_packets_total_{0}, lost_packets_{0}, lost_packets_total_{0};

            uint32_t packet_history_offset{0}, last_history_offset{0};
            bit_set<32> packet_history{};
    };

    class CommandPacketLossCalculator {
            constexpr static auto packet_ack_counts_length{32};
        public:
            CommandPacketLossCalculator() = default;

            void packet_send(uint32_t /* packet id */);
            void ack_received(uint32_t /* packet id */); //Attention: This is a full ID!

            void short_stats();
            void reset();
            void reset_offsets();

            [[nodiscard]] inline bool valid_data() const { return true;  }

            [[nodiscard]] inline uint32_t received_packets() const { return this->received_packets_; }
            [[nodiscard]] inline uint32_t lost_packets() const { return this->lost_packets_; }

            [[nodiscard]] inline uint32_t received_packets_total() const { return this->received_packets_total_; }
            [[nodiscard]] inline uint32_t lost_packets_total() const { return this->lost_packets_total_; }

            [[nodiscard]] inline uint32_t unconfirmed_received_packets() const { return 0; };
            [[nodiscard]] inline uint32_t unconfirmed_lost_packets() const {
                uint32_t result{0};
                for(auto& e : this->packet_ack_counts)
                    result += e;
                return result;
            };
        private:
            uint32_t received_packets_{0}, received_packets_total_{0}, lost_packets_{0}, lost_packets_total_{0};

            uint32_t packet_history_offset{0}, packets_send_unshorten{0};
            uint8_t packet_ack_counts[packet_ack_counts_length]{0};
    };
}