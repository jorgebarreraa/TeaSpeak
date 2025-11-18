#pragma once

#include <Properties.h>
#include <protocol/Packet.h>
#include <chrono>
#include "Definitions.h"

namespace ts {
    namespace server {
        class VirtualServer;
    }

    namespace stats {
        template <typename value_t>
        struct BandwidthEntry {
            std::array<value_t, 3> connection_packets_sent{};
            std::array<value_t, 3> connection_bytes_sent{};
            std::array<value_t, 3> connection_packets_received{};
            std::array<value_t, 3> connection_bytes_received{};

            value_t file_bytes_sent{0};
            value_t file_bytes_received{0};

            template <typename other_type>
            inline BandwidthEntry& operator=(const BandwidthEntry<other_type>& other) {
                for(size_t index{0}; index < this->connection_packets_sent.size(); index++)
                    this->connection_packets_sent[index] = other.connection_packets_sent[index];
                for(size_t index{0}; index < this->connection_bytes_sent.size(); index++)
                    this->connection_bytes_sent[index] = other.connection_bytes_sent[index];
                for(size_t index{0}; index < this->connection_packets_received.size(); index++)
                    this->connection_packets_received[index] = other.connection_packets_received[index];
                for(size_t index{0}; index < this->connection_bytes_received.size(); index++)
                    this->connection_bytes_received[index] = other.connection_bytes_received[index];

                this->file_bytes_sent = other.file_bytes_sent;
                this->file_bytes_received = other.file_bytes_received;
                return *this;
            }

            template <typename target_t>
            inline BandwidthEntry<target_t> mul(double factor) const {
                BandwidthEntry<target_t> result{};
                result = *this;
                for(auto& val : result.connection_packets_sent) val *= factor;
                for(auto& val : result.connection_bytes_sent) val *= factor;
                for(auto& val : result.connection_packets_received) val *= factor;
                for(auto& val : result.connection_bytes_received) val *= factor;

                result.file_bytes_sent *= factor;
                result.file_bytes_received *= factor;
                return result;
            }

            template <typename other_type>
            inline BandwidthEntry& operator+=(const BandwidthEntry<other_type>& other) {
                for(size_t index{0}; index < this->connection_packets_sent.size(); index++)
                    this->connection_packets_sent[index] += other.connection_packets_sent[index];
                for(size_t index{0}; index < this->connection_bytes_sent.size(); index++)
                    this->connection_bytes_sent[index] += other.connection_bytes_sent[index];
                for(size_t index{0}; index < this->connection_packets_received.size(); index++)
                    this->connection_packets_received[index] += other.connection_packets_received[index];
                for(size_t index{0}; index < this->connection_bytes_received.size(); index++)
                    this->connection_bytes_received[index] += other.connection_bytes_received[index];

                this->file_bytes_sent += other.file_bytes_sent;
                this->file_bytes_received += other.file_bytes_received;
                return *this;
            }

            template <typename other_type>
            inline BandwidthEntry operator+(const BandwidthEntry<other_type>& other) {
                return BandwidthEntry{*this} += other;
            }

            template <typename atomic_t>
            inline void atomic_exchange(BandwidthEntry<std::atomic<atomic_t>>& source) {
                for(size_t index{0}; index < this->connection_packets_sent.size(); index++)
                    this->connection_packets_sent[index] = source.connection_packets_sent[index].exchange(0);
                for(size_t index{0}; index < this->connection_bytes_sent.size(); index++)
                    this->connection_bytes_sent[index] = source.connection_bytes_sent[index].exchange(0);
                for(size_t index{0}; index < this->connection_packets_received.size(); index++)
                    this->connection_packets_received[index] = source.connection_packets_received[index].exchange(0);
                for(size_t index{0}; index < this->connection_bytes_received.size(); index++)
                    this->connection_bytes_received[index] = source.connection_bytes_received[index].exchange(0);

                this->file_bytes_sent = source.file_bytes_sent.exchange(0);
                this->file_bytes_received = source.file_bytes_received.exchange(0);
            }
        };

        struct FileTransferStatistics {
            uint64_t bytes_received{0};
            uint64_t bytes_sent{0};
        };

        class ConnectionStatistics {
            public:
                struct category {
                    /* Only three categories. Map unknown to category 0 */
                    enum value {
                        COMMAND,
                        KEEP_ALIVE,
                        VOICE,
                        UNKNOWN = COMMAND
                    };

                    constexpr static std::array<category::value, 16> lookup_table{
                            VOICE, /* Voice */
                            VOICE, /* VoiceWhisper */
                            COMMAND, /* Command */
                            COMMAND, /* CommandLow */
                            KEEP_ALIVE, /* Ping */
                            KEEP_ALIVE, /* Pong */
                            COMMAND, /* Ack */
                            COMMAND, /* AckLow */
                            COMMAND, /* */

                            UNKNOWN,
                            UNKNOWN,
                            UNKNOWN,
                            UNKNOWN,
                            UNKNOWN,
                            UNKNOWN
                    };

                    /* much faster than a switch */
                    inline static category::value from_type(uint8_t type){
                        return lookup_table[type & 0xFU];
                    }
                };
                explicit ConnectionStatistics(std::shared_ptr<ConnectionStatistics>  /* root */);
                ~ConnectionStatistics();

                void logIncomingPacket(const category::value& /* category */, size_t /* length */);
                void logOutgoingPacket(const category::value& /* category */, size_t /* length */);
                void logFileTransferIn(uint32_t);
                void logFileTransferOut(uint32_t);

                void tick();

                [[nodiscard]] inline const BandwidthEntry<uint64_t>& total_stats() const { return this->total_statistics; }
                [[nodiscard]] inline BandwidthEntry<uint32_t> second_stats() const { return this->statistics_second; }
                [[nodiscard]] BandwidthEntry<uint32_t> minute_stats() const;

                FileTransferStatistics file_stats();
                std::pair<uint64_t, uint64_t> mark_file_bytes();
            private:
                std::shared_ptr<ConnectionStatistics> handle;

                BandwidthEntry<uint64_t> total_statistics{};

                BandwidthEntry<std::atomic<uint64_t>> statistics_second_current{};
                BandwidthEntry<uint32_t> statistics_second{}; /* will be updated every second by the stats from the "current_second" */
                std::array<BandwidthEntry<uint32_t>, 60> statistics_minute{};
                uint32_t statistics_minute_offset{0}; /* pointing to the upcoming minute */
                std::chrono::system_clock::time_point last_second_tick{};

                std::atomic<uint64_t> file_bytes_sent{0};
                std::atomic<uint64_t> file_bytes_received{0};

                uint64_t mark_file_bytes_sent{0};
                uint64_t mark_file_bytes_received{0};
        };
    }
}