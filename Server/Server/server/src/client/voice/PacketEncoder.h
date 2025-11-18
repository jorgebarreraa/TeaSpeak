#pragma once

#include <misc/spin_mutex.h>
#include <mutex>
#include <deque>
#include <protocol/Packet.h>
#include <protocol/AcknowledgeManager.h>
#include <protocol/PacketStatistics.h>
#include <src/ConnectionStatistics.h>

namespace ts::connection {
    class CryptHandler;
    class AcknowledgeManager;
}

namespace ts::server::server::udp {

    class PacketEncoder {
            using AcknowledgeEntry = connection::AcknowledgeManager::Entry;
            using StatisticsCategory = stats::ConnectionStatistics::category;
        public:
            enum struct CryptError {
                KEY_GENERATION_FAILED,
                ENCRYPT_FAILED /* contains some data */
            };

            typedef void(*callback_request_write_t)(void* /* user data */);
            typedef void(*callback_crypt_error_t)(void* /* user data */, const CryptError& /* error */, const std::string& /* details */);

            typedef void(*callback_resend_stats_t)(void* /* user data */, size_t /* resend packets */);
            typedef void(*callback_resend_failed_t)(void* /* user data */, const std::shared_ptr<AcknowledgeEntry>& /* entry */);

            typedef void(*callback_connection_stats_t)(void* /* user data */, StatisticsCategory::value, size_t /* bytes */);

            explicit PacketEncoder(connection::CryptHandler* /* crypt handler */, protocol::PacketStatistics* /* packet stats */);
            ~PacketEncoder();

            void reset();

            void send_packet(protocol::OutgoingServerPacket* /* packet */); /* will claim ownership */
            void send_packet(protocol::PacketType /* type */, const protocol::PacketFlags& /* flags */, const void* /* payload */, size_t /* payload length */);
            void send_command(const std::string_view& /* build command command */, bool /* command low */, std::unique_ptr<std::function<void(bool)>> /* acknowledge listener */);

            void send_packet_acknowledge(uint16_t /* packet id */, bool /* acknowledge low */);

            void execute_resend(const std::chrono::system_clock::time_point &now, std::chrono::system_clock::time_point &next);
            void encrypt_pending_packets();

            bool wait_empty_write_and_prepare_queue(std::chrono::time_point<std::chrono::system_clock> until = std::chrono::time_point<std::chrono::system_clock>());

            /**
             * Returns true if there is more data to write and false otherwise
             * @return
             */
            bool pop_write_buffer(protocol::OutgoingServerPacket*& /* packet */);
            void reenqueue_failed_buffer(protocol::OutgoingServerPacket* /* packet */);

            [[nodiscard]] inline auto& acknowledge_manager() { return this->acknowledge_manager_; }

            /* callbacks must be valid all the time! */
            void* callback_data{nullptr};

            callback_request_write_t callback_request_write{[](auto){}};
            callback_crypt_error_t callback_crypt_error{[](auto, auto, auto){}};

            callback_resend_stats_t callback_resend_stats{[](auto, auto){}};
            callback_resend_failed_t callback_resend_failed{[](auto, auto){}};

            callback_connection_stats_t callback_connection_stats{[](auto, auto, auto){}};
        private:
            connection::CryptHandler* crypt_handler_{nullptr};
            protocol::PacketStatistics* packet_statistics_{nullptr};
            connection::AcknowledgeManager acknowledge_manager_{};

            spin_mutex write_queue_mutex{};
            protocol::OutgoingServerPacket* send_queue_head{nullptr};
            protocol::OutgoingServerPacket** send_queue_tail{&send_queue_head};

            protocol::OutgoingServerPacket* encrypt_queue_head{nullptr};
            protocol::OutgoingServerPacket** encrypt_queue_tail{&encrypt_queue_head};

            protocol::PacketIdManager packet_id_manager;
            spin_mutex packet_id_mutex{};


            /* thread save function */
            bool encrypt_outgoing_packet(protocol::OutgoingServerPacket* /* packet */);
    };
}
