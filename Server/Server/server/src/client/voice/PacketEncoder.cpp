//
// Created by WolverinDEV on 09/03/2020.
//
#include "PacketEncoder.h"

#include <log/LogUtils.h>
#include <protocol/buffers.h>
#include <protocol/CompressionHandler.h>
#include <protocol/CryptHandler.h>
#include <misc/endianness.h>

using namespace ts;
using namespace ts::server::server::udp;

PacketEncoder::PacketEncoder(ts::connection::CryptHandler *crypt_handler, protocol::PacketStatistics* pstats)
    : crypt_handler_{crypt_handler}, packet_statistics_{pstats} {

    this->acknowledge_manager_.callback_data = this;
    this->acknowledge_manager_.destroy_packet = [](void* packet) {
        reinterpret_cast<protocol::OutgoingServerPacket*>(packet)->unref();
    };
    this->acknowledge_manager_.callback_resend_failed = [](void* this_ptr, const auto& entry) {
        auto encoder = reinterpret_cast<PacketEncoder*>(this_ptr);
        encoder->callback_resend_failed(encoder->callback_data, entry);
    };
}

PacketEncoder::~PacketEncoder() {
    this->reset();
}

void PacketEncoder::reset() {
    this->acknowledge_manager_.reset();

    protocol::OutgoingServerPacket *write_head, *read_head;
    {
        std::lock_guard wlock{this->write_queue_mutex};
        write_head = std::exchange(this->encrypt_queue_head, nullptr);
        read_head = std::exchange(this->send_queue_head, nullptr);

        this->encrypt_queue_tail = &this->encrypt_queue_head;
        this->send_queue_tail = &this->send_queue_head;
    }

    while(write_head) {
        std::exchange(write_head, write_head->next)->unref();
    }

    while(read_head) {
        std::exchange(read_head, read_head->next)->unref();
    }
}

void PacketEncoder::send_packet(ts::protocol::OutgoingServerPacket *packet) {
    uint32_t full_id;
    {
        std::lock_guard id_lock{this->packet_id_mutex};
        full_id = this->packet_id_manager.generate_full_id(packet->packet_type());
    }
    packet->set_packet_id(full_id & 0xFFFFU);
    packet->generation = full_id >> 16U;

    auto category = stats::ConnectionStatistics::category::from_type(packet->packet_type());
    this->callback_connection_stats(this->callback_data, category, packet->packet_length() + 96); /* 96 for the UDP packet overhead */

    {
        std::lock_guard qlock{this->write_queue_mutex};
        *this->encrypt_queue_tail = packet;
        this->encrypt_queue_tail = &packet->next;
    }

    this->callback_request_write(this->callback_data);
}

void PacketEncoder::send_packet(protocol::PacketType type, const protocol::PacketFlags& flag, const void *payload, size_t payload_size) {
    auto packet = protocol::allocate_outgoing_server_packet(payload_size);

    packet->type_and_flags_ = (uint8_t) type | (uint8_t) flag;
    memcpy(packet->payload, payload, payload_size);

    this->send_packet(packet);
}

void PacketEncoder::send_packet_acknowledge(uint16_t pid, bool low) {
    char buffer[2];
    le2be16(pid, buffer);

    auto pflags = protocol::PacketFlag::Unencrypted | protocol::PacketFlag::NewProtocol;
    this->send_packet(low ? protocol::PacketType::ACK_LOW : protocol::PacketType::ACK, pflags, buffer, 2);
}


#define MAX_COMMAND_PACKET_PAYLOAD_LENGTH (487)
void PacketEncoder::send_command(const std::string_view &command, bool low, std::unique_ptr<std::function<void(bool)>> ack_listener) {
    std::optional<void*> temp_data_buffer{};

    const char* data_buffer{command.data()};
    size_t data_length{command.length()};

    uint8_t head_pflags{0};
    protocol::PacketType ptype{low ? protocol::PacketType::COMMAND_LOW : protocol::PacketType::COMMAND};
    protocol::OutgoingServerPacket *packets_head{nullptr};
    protocol::OutgoingServerPacket **packets_tail{&packets_head};

    /* only compress "long" commands */
    if(command.size() > 100) {
        size_t max_compressed_payload_size = compression::qlz_compressed_size(command.data(), command.length());
        auto compressed_buffer = ::malloc(max_compressed_payload_size);

        size_t compressed_size{max_compressed_payload_size};
        if(!compression::qlz_compress_payload(command.data(), command.length(), compressed_buffer, &compressed_size)) {
            logCritical(0, "Failed to compress command packet. Dropping packet");
            ::free(compressed_buffer);
            return;
        }

        /* we don't need to make the command longer than it is */
        if(compressed_size < command.length()) {
            data_length = compressed_size;
            data_buffer = (char*) compressed_buffer;
            temp_data_buffer = std::make_optional(compressed_buffer);
            head_pflags |= protocol::PacketFlag::Compressed;
        } else {
            ::free(compressed_buffer);
        }
    }

    uint8_t ptype_and_flags{(uint8_t) ((uint8_t) ptype | (uint8_t) protocol::PacketFlag::NewProtocol)};
    if(data_length > MAX_COMMAND_PACKET_PAYLOAD_LENGTH) {
        auto chunk_count = (size_t) ceil((float) data_length / (float) MAX_COMMAND_PACKET_PAYLOAD_LENGTH);
        auto chunk_size = (size_t) ceil((float) data_length / (float) chunk_count);

        while(true) {
            auto bytes = std::min(chunk_size, data_length);
            auto packet = protocol::allocate_outgoing_server_packet(bytes);
            packet->type_and_flags_ = ptype_and_flags;
            memcpy(packet->payload, data_buffer, bytes);

            *packets_tail = packet;
            packets_tail = &packet->next;
            assert(!packet->next);

            data_length -= bytes;
            if(data_length == 0) {
                packet->type_and_flags_ |= protocol::PacketFlag::Fragmented;
                break;
            }

            data_buffer += bytes;
        }
        packets_head->type_and_flags_ |= protocol::PacketFlag::Fragmented;
    } else {
        auto packet = protocol::allocate_outgoing_server_packet(data_length);
        packet->type_and_flags_ = ptype_and_flags;

        memcpy(packet->payload, data_buffer, data_length);

        packets_head = packet;
        packets_tail = &packet->next;
        assert(!packet->next);
    }

    {
        std::lock_guard id_lock{this->packet_id_mutex};

        uint32_t full_id;
        auto head = packets_head;
        while(head) {
            full_id = this->packet_id_manager.generate_full_id(ptype);

            head->set_packet_id(full_id & 0xFFFFU);
            head->generation = full_id >> 16U;

            /* loss stats (In order required so we're using the this->packet_id_mutex) */
            this->packet_statistics_->send_command(head->packet_type(), full_id);

            head = head->next;
        }
    }
    packets_head->type_and_flags_ |= head_pflags;

    /* general stats */
    {
        auto head = packets_head;
        while(head) {
            this->callback_connection_stats(this->callback_data, StatisticsCategory::COMMAND, head->packet_length() + 96); /* 96 for the UDP overhead */
            head = head->next;
        }
    }

    /* ack handler */
    {
        auto head = packets_head;
        while(head) {
            auto full_packet_id = (uint32_t) (head->generation << 16U) | head->packet_id();

            /* increase a reference for the ack handler */
            head->ref();

            /* Even thou the packet is yet unencrypted, it will be encrypted with the next write. The next write will be before the next resend because the next ptr must be null in order to resend a packet */
            if(&head->next == packets_tail) {
                this->acknowledge_manager_.process_packet(ptype, full_packet_id, head, std::move(ack_listener));
            } else {
                this->acknowledge_manager_.process_packet(ptype, full_packet_id, head, nullptr);
            }

            head = head->next;
        }
    }

    {
        std::lock_guard qlock{this->write_queue_mutex};
        *this->encrypt_queue_tail = packets_head;
        this->encrypt_queue_tail = packets_tail;
    }
    this->callback_request_write(this->callback_data);

    if(temp_data_buffer.has_value()) {
        ::free(*temp_data_buffer);
    }
}

void PacketEncoder::encrypt_pending_packets() {
    protocol::OutgoingServerPacket *packets_head, **packets_tail;
    {
        std::lock_guard wlock{this->write_queue_mutex};
        packets_head = std::exchange(this->encrypt_queue_head, nullptr);
        packets_tail = std::exchange(this->encrypt_queue_tail, &this->encrypt_queue_head);
    }

    auto current_packet{packets_head};
    while(current_packet) {
        this->encrypt_outgoing_packet(current_packet);
        current_packet = current_packet->next;
    }

    {
        std::lock_guard wlock{this->write_queue_mutex};
        *this->send_queue_tail = packets_head;
        this->send_queue_tail = packets_tail;
    }
}

bool PacketEncoder::encrypt_outgoing_packet(ts::protocol::OutgoingServerPacket *packet) {
    if(packet->type_and_flags_ & protocol::PacketFlag::Unencrypted) {
        this->crypt_handler_->write_default_mac(packet->mac);
    } else {
        connection::CryptHandler::key_t crypt_key{};
        connection::CryptHandler::nonce_t crypt_nonce{};
        std::string error{};

        if(!this->crypt_handler_->encryption_initialized()) {
            crypt_key = connection::CryptHandler::kDefaultKey;
            crypt_nonce = connection::CryptHandler::kDefaultNonce;
        } else {
            if(!this->crypt_handler_->generate_key_nonce(false, (uint8_t) packet->packet_type(), packet->packet_id(), packet->generation, crypt_key, crypt_nonce)) {
                this->callback_crypt_error(this->callback_data, CryptError::KEY_GENERATION_FAILED, "");
                return false;
            }
        }

        auto crypt_result = this->crypt_handler_->encrypt(
                (char*) packet->packet_data() + protocol::ServerPacketParser::kHeaderOffset,
                protocol::ServerPacketParser::kHeaderLength,
                packet->payload, packet->payload_size,
                packet->mac,
                crypt_key, crypt_nonce,
                error
        );

        if(!crypt_result) {
            this->callback_crypt_error(this->callback_data, CryptError::KEY_GENERATION_FAILED, error);
            return false;
        }
    }

    return true;
}

bool PacketEncoder::pop_write_buffer(protocol::OutgoingServerPacket *&result) {
    bool need_encrypt{false}, more_packets;

    {
        std::lock_guard wlock{this->write_queue_mutex};
        if(this->send_queue_head) {
            result = this->send_queue_head;
            if(result->next) {
                assert(this->send_queue_tail != &result->next);
                this->send_queue_head = result->next;
            } else {
                assert(this->send_queue_tail == &result->next);
                this->send_queue_head = nullptr;
                this->send_queue_tail = &this->send_queue_head;
            }
        } else if(this->encrypt_queue_head) {
            result = this->encrypt_queue_head;
            if(result->next) {
                assert(this->encrypt_queue_tail != &result->next);
                this->encrypt_queue_head = result->next;
            } else {
                assert(this->encrypt_queue_tail == &result->next);
                this->encrypt_queue_head = nullptr;
                this->encrypt_queue_tail = &this->encrypt_queue_head;
            }

            need_encrypt = true;
        } else {
            result = nullptr;
            return false;
        }

        result->next = nullptr;
        more_packets = this->send_queue_head != nullptr || this->encrypt_queue_head != nullptr;
    }

    if(need_encrypt) {
        this->encrypt_outgoing_packet(result);
    }

    return more_packets;
}

void PacketEncoder::reenqueue_failed_buffer(protocol::OutgoingServerPacket *packet) {
    std::lock_guard wlock{this->write_queue_mutex};
    if(packet->next || &packet->next == this->send_queue_tail || &packet->next == this->encrypt_queue_tail) {
        /* packets seemed to gotten reenqueued already */
        return;
    }

    if(!this->send_queue_head) {
        this->send_queue_tail = &packet->next;
    }

    packet->next = this->send_queue_head;
    this->send_queue_head = packet;
}

void PacketEncoder::execute_resend(const std::chrono::system_clock::time_point &now, std::chrono::system_clock::time_point &next) {
    std::deque<std::shared_ptr<connection::AcknowledgeManager::Entry>> buffers{};
    std::string error{};

    this->acknowledge_manager_.execute_resend(now, next, buffers);

    if(!buffers.empty()) {
        size_t send_count{0};
        {
            std::lock_guard wlock{this->write_queue_mutex};
            for(auto& buffer : buffers) {
                auto packet = (protocol::OutgoingServerPacket*) buffer->packet_ptr;

                /* Test if the packet is still in the write/enqueue queue */
                if(packet->next) {
                    continue;
                }

                if(&packet->next == this->encrypt_queue_tail || &packet->next == this->send_queue_tail) {
                    continue;
                }

                packet->ref(); /* for the write queue again */
                *this->send_queue_tail = packet;
                this->send_queue_tail = &packet->next;

                send_count++;
                buffer->resend_count++;

                this->packet_statistics_->send_command((protocol::PacketType) buffer->packet_type, buffer->packet_full_id);
            }
        }

        this->callback_request_write(this->callback_data);
        this->callback_resend_stats(this->callback_data, buffers.size());
    }
}

bool PacketEncoder::wait_empty_write_and_prepare_queue(std::chrono::time_point<std::chrono::system_clock> until) {
    while(true) {
        {
            std::lock_guard wlock{this->write_queue_mutex};
            if(this->encrypt_queue_head)
                goto _wait;

            if(this->send_queue_head)
                goto _wait;
        }
        break;

        _wait:
        if(until.time_since_epoch().count() != 0 && std::chrono::system_clock::now() > until)
            return false;

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return true;
}