//
// Created by WolverinDEV on 10/03/2020.
//

#include "PacketDecoder.h"

#include "../misc/memtracker.h"
#include "./AcknowledgeManager.h"
#include "./CompressionHandler.h"
#include "./CryptHandler.h"

#ifdef FEATURE_LOGGING
#include <log/LogUtils.h>
#endif

using namespace ts;
using namespace ts::protocol;
using namespace ts::connection;

PacketDecoder::PacketDecoder(ts::connection::CryptHandler *crypt_handler, bool is_server)
        : is_server{is_server}, crypt_handler_{crypt_handler} {
    memtrack::allocated<PacketDecoder>(this);
}

PacketDecoder::~PacketDecoder() {
    memtrack::freed<PacketDecoder>(this);
    this->reset();
}

void PacketDecoder::reset() {
    {
        std::lock_guard buffer_lock(this->packet_buffer_lock);
        for(auto& buffer : this->_command_fragment_buffers) {
            buffer.reset();
        }
    }

    {
        std::lock_guard estimator_lock{this->incoming_generation_estimator_lock};
        for(auto& estimator : this->incoming_generation_estimators) {
            estimator.reset();
        }
    }
}

PacketProcessResult PacketDecoder::process_incoming_data(PacketParser &packet_parser, std::string& error) {
#ifdef FUZZING_TESTING_INCOMMING
    if(rand() % 100 < 20) {
        return PacketProcessResult::FUZZ_DROPPED;
    }

    #ifdef FIZZING_TESTING_DISABLE_HANDSHAKE
    if (this->client->state == ConnectionState::CONNECTED) {
    #endif
        if ((rand() % FUZZING_TESTING_DROP_MAX) < FUZZING_TESTING_DROP) {
            debugMessage(this->client->getServerId(), "{}[FUZZING] Dropping incoming packet of length {}", CLIENT_STR_LOG_PREFIX_(this->client), buffer.length());
            return;
        }
    #ifdef FIZZING_TESTING_DISABLE_HANDSHAKE
    }
    #endif
#endif
    assert(packet_parser.type() >= 0 && packet_parser.type() < this->incoming_generation_estimators.size());

    auto& generation_estimator = this->incoming_generation_estimators[packet_parser.type()];
    {
        std::lock_guard glock{this->incoming_generation_estimator_lock};
        packet_parser.set_estimated_generation(generation_estimator.visit_packet(packet_parser.packet_id()));
    }

    auto result = this->decrypt_incoming_packet(error, packet_parser);
    if(result != PacketProcessResult::SUCCESS) {
        return result;
    }

#ifdef LOG_INCOMPING_PACKET_FRAGMENTS
    debugMessage(lstream << CLIENT_LOG_PREFIX << "Recived packet. PacketId: " << packet->packetId() << " PacketType: " << packet->type().name() << " Flags: " << packet->flags() << " - " << packet->data() << endl);
#endif
    auto is_command = packet_parser.type() == protocol::COMMAND || packet_parser.type() == protocol::COMMAND_LOW;
    if(is_command) {
        auto& fragment_buffer = this->_command_fragment_buffers[command_fragment_buffer_index(packet_parser.type())];
        CommandFragment fragment_entry{
                packet_parser.packet_id(),
                packet_parser.estimated_generation(),

                packet_parser.flags(),
                (uint32_t) packet_parser.payload_length(),
                packet_parser.payload().own_buffer()
        };

        std::unique_lock queue_lock(fragment_buffer.buffer_lock);

        auto insert_result = fragment_buffer.insert_index2(packet_parser.full_packet_id(), std::move(fragment_entry));
        if(insert_result != 0) {
            queue_lock.unlock();

            error = "pid: " + std::to_string(packet_parser.packet_id()) + ", ";
            error += "bidx: " + std::to_string(fragment_buffer.current_index()) + ", ";
            error += "bcap: " + std::to_string(fragment_buffer.capacity());

            if(insert_result == -2) {
                return PacketProcessResult::DUPLICATED_PACKET;
            } else if(insert_result == -1) {
                this->callback_send_acknowledge(this->callback_argument, packet_parser.packet_id(), packet_parser.type() == protocol::COMMAND_LOW);
                return PacketProcessResult::BUFFER_UNDERFLOW;
            } else if(insert_result == 1) {
                return PacketProcessResult::BUFFER_OVERFLOW;
            }

            assert(false);
            return PacketProcessResult::UNKNOWN_ERROR;
        }

        this->callback_send_acknowledge(this->callback_argument, packet_parser.packet_id(), packet_parser.type() == protocol::COMMAND_LOW);

        ReassembledCommand* command{nullptr};
        CommandReassembleResult assemble_result;
        do {
            if(!queue_lock.owns_lock()) {
                queue_lock.lock();
            }

            assemble_result = this->try_reassemble_ordered_packet(fragment_buffer, queue_lock, command);

            if(assemble_result == CommandReassembleResult::SUCCESS || assemble_result == CommandReassembleResult::MORE_COMMANDS_PENDING) {
                this->callback_decoded_command(this->callback_argument, command);
            }

            if(command) {
                /* ownership hasn't transferred */
                ReassembledCommand::free(command);
                command = nullptr;
            }

            switch (assemble_result) {
                case CommandReassembleResult::NO_COMMANDS_PENDING:
                case CommandReassembleResult::SUCCESS:
                case CommandReassembleResult::MORE_COMMANDS_PENDING:
                    break;

                case CommandReassembleResult::SEQUENCE_LENGTH_TOO_LONG:
                    return PacketProcessResult::COMMAND_BUFFER_OVERFLOW;

                case CommandReassembleResult::COMMAND_TOO_LARGE:
                    return PacketProcessResult::COMMAND_TOO_LARGE;

                case CommandReassembleResult::COMMAND_DECOMPRESS_FAILED:
                    return PacketProcessResult::COMMAND_DECOMPRESS_FAILED;

                default:
                    assert(false);
                    break;
            }
        } while(assemble_result == CommandReassembleResult::MORE_COMMANDS_PENDING);
    } else {
        this->callback_decoded_packet(this->callback_argument, packet_parser);
    }

    return PacketProcessResult::SUCCESS;
}

PacketProcessResult PacketDecoder::decrypt_incoming_packet(std::string& error, PacketParser &packet_parser) {
    /* decrypt the packet if needed */
    if(packet_parser.is_encrypted()) {
        CryptHandler::key_t crypt_key{};
        CryptHandler::nonce_t crypt_nonce{};

        bool use_default_key{!this->crypt_handler_->encryption_initialized()}, decrypt_result;

        decrypt_packet:
        if(use_default_key) {
            crypt_key = CryptHandler::kDefaultKey;
            crypt_nonce = CryptHandler::kDefaultNonce;
        } else {
            if(!this->crypt_handler_->generate_key_nonce(this->is_server, packet_parser.type(), packet_parser.packet_id(), packet_parser.estimated_generation(), crypt_key, crypt_nonce)) {
                return PacketProcessResult::DECRYPT_KEY_GEN_FAILED;
            }
        }

        auto mac = packet_parser.mac();
        auto header = packet_parser.header();
        auto payload = packet_parser.payload_ptr_mut();
        decrypt_result = this->crypt_handler_->decrypt(
                header.data_ptr(), header.length(),
                payload, packet_parser.payload_length(),
                mac.data_ptr(),
                crypt_key, crypt_nonce,
                error
        );

        if(!decrypt_result) {
            if(packet_parser.packet_id() < 10 && packet_parser.estimated_generation() == 0) {
                if(use_default_key) {
                    return PacketProcessResult::DECRYPT_FAILED;
                } else {
                    use_default_key = true;
                    goto decrypt_packet;
                }
            } else {
                return PacketProcessResult::DECRYPT_FAILED;
            }
        }
        packet_parser.set_decrypted();
    }

    return PacketProcessResult::SUCCESS;
}

bool PacketDecoder::verify_encryption_client_packet(const protocol::ClientPacketParser& packet_parser) {
    if(!packet_parser.is_encrypted()) {
        return false;
    }

    assert(packet_parser.type() >= 0 && packet_parser.type() < this->incoming_generation_estimators.size());
    return this->crypt_handler_->verify_encryption(packet_parser.buffer(), packet_parser.packet_id(), this->incoming_generation_estimators[packet_parser.type()].generation());
}

void PacketDecoder::register_initiv_packet() {
    auto& fragment_buffer = this->_command_fragment_buffers[command_fragment_buffer_index(protocol::COMMAND)];
    std::unique_lock buffer_lock(fragment_buffer.buffer_lock);
    fragment_buffer.set_full_index_to(1); /* the first packet (0) is already the clientinitiv packet */
}

CommandReassembleResult PacketDecoder::try_reassemble_ordered_packet(
        command_fragment_buffer_t &buffer,
        std::unique_lock<std::mutex> &buffer_lock,
        ReassembledCommand *&assembled_command) {
    assert(buffer_lock.owns_lock());

    if(!buffer.front_set()) {
        return CommandReassembleResult::NO_COMMANDS_PENDING;
    }

    uint8_t packet_flags;

    std::unique_ptr<ReassembledCommand, void(*)(ReassembledCommand*)> rcommand{nullptr, ReassembledCommand::free};

    /* lets find out if we've to reassemble the packet */
    auto& first_buffer = buffer.slot_value(0);
    if(first_buffer.packet_flags & PacketFlag::Fragmented) {
        uint16_t sequence_length{1};
        size_t total_payload_length{first_buffer.payload_length};
        do {
            if(sequence_length >= buffer.capacity()) {
                return CommandReassembleResult::SEQUENCE_LENGTH_TOO_LONG;
            }

            if(!buffer.slot_set(sequence_length)) {
                return CommandReassembleResult::NO_COMMANDS_PENDING; /* we need more packets */
            }

            auto& packet = buffer.slot_value(sequence_length++);
            total_payload_length += packet.payload_length;
            if(packet.packet_flags & PacketFlag::Fragmented) {
                /* yep we find the end */
                break;
            }
        } while(true);

        /* ok we have all fragments lets reassemble */
        /*
         * Packet sequence could never be so long. If it is so then the data_length() returned an invalid value.
         * We're checking it here because we dont want to make a huge allocation
         */
        assert(total_payload_length < 512 * 1024 * 1024);

        rcommand.reset(ReassembledCommand::allocate(total_payload_length));
        char* packet_buffer_ptr = rcommand->command();
        size_t packet_count{0};

        packet_flags = buffer.slot_value(0).packet_flags;
        while(packet_count < sequence_length) {
            auto fragment = buffer.pop_front();
            memcpy(packet_buffer_ptr, fragment.payload.data_ptr(), fragment.payload_length);

            packet_buffer_ptr += fragment.payload_length;
            packet_count++;
        }

        /* We don't have log functions for our TeaClient */
#if !defined(_NDEBUG) && defined(FEATURE_LOGGING)
        if((packet_buffer_ptr - 1) != &rcommand->command()[rcommand->length() - 1]) {
            logCritical(0,
                        "Buffer over/underflow: packet_buffer_ptr != &packet_buffer[packet_buffer.length() - 1]; packet_buffer_ptr := {}; packet_buffer.end() := {}",
                        (void*) packet_buffer_ptr,
                        (void*) &rcommand->command()[rcommand->length() - 1]
            );
        }
#endif
    } else {
        auto packet = buffer.pop_front();
        packet_flags = packet.packet_flags;

        rcommand.reset(ReassembledCommand::allocate(packet.payload_length));
        memcpy(rcommand->command(), packet.payload.data_ptr(), packet.payload_length);
    }

    auto more_commands_pending = buffer.front_set(); /* set the more flag if we have more to process */
    buffer_lock.unlock();

    if(packet_flags & PacketFlag::Compressed) {
        std::string error{};

        auto compressed_command = std::move(rcommand);
        auto decompressed_size = compression::qlz_decompressed_size(compressed_command->command(), compressed_command->length());
        if(decompressed_size > 64 * 1024 * 1024) {
            return CommandReassembleResult::COMMAND_TOO_LARGE;
        }

        rcommand.reset(ReassembledCommand::allocate(decompressed_size));
        if(!compression::qlz_decompress_payload(compressed_command->command(), rcommand->command(), &decompressed_size)) {
            return CommandReassembleResult::COMMAND_DECOMPRESS_FAILED;
        }

        rcommand->set_length(decompressed_size);
    }

    assembled_command = rcommand.release();
    return more_commands_pending ? CommandReassembleResult::MORE_COMMANDS_PENDING : CommandReassembleResult::SUCCESS;
}