#pragma once

#include <misc/spin_mutex.h>
#include <mutex>
#include <deque>
#include <protocol/Packet.h>
#include <protocol/generation.h>
#include <protocol/ringbuffer.h>
#include "./RawCommand.h"

namespace ts::connection {
    class CryptHandler;
}

namespace ts::protocol {
    enum struct PacketProcessResult {
        SUCCESS,
        UNKNOWN_ERROR,

        FUZZ_DROPPED,

        DUPLICATED_PACKET, /* error message contains debug properties */
        BUFFER_OVERFLOW, /* error message contains debug properties */
        BUFFER_UNDERFLOW, /* error message contains debug properties */

        COMMAND_BUFFER_OVERFLOW, /* can cause a total connection drop */
        COMMAND_SEQUENCE_LENGTH_TOO_LONG, /* unrecoverable error */
        COMMAND_TOO_LARGE,
        COMMAND_DECOMPRESS_FAILED,

        DECRYPT_KEY_GEN_FAILED,
        DECRYPT_FAILED, /* has custom message */
    };

    enum struct CommandReassembleResult {
        SUCCESS,

        MORE_COMMANDS_PENDING, /* equal with success */
        NO_COMMANDS_PENDING,

        COMMAND_TOO_LARGE, /* this is a fatal error to the connection */
        COMMAND_DECOMPRESS_FAILED,

        SEQUENCE_LENGTH_TOO_LONG /* unrecoverable error */
    };

    /* TODO: Implement for the client the command overflow recovery option! */
    class PacketDecoder {
            using CommandFragment = command::CommandFragment;
            using ReassembledCommand = command::ReassembledCommand;

            typedef protocol::FullPacketRingBuffer<CommandFragment, 32, CommandFragment> command_fragment_buffer_t;
            typedef std::array<command_fragment_buffer_t, 2> command_packet_reassembler;
        public:
            /* direct function calls are better optimized out */
            typedef void(*callback_decoded_packet_t)(void* /* cb argument */, const protocol::PacketParser&);
            typedef void(*callback_decoded_command_t)(void* /* cb argument */, ReassembledCommand*& /* command */); /* must move the command, else it gets freed */
            typedef void(*callback_send_acknowledge_t)(void* /* cb argument */, uint16_t /* packet id */, bool /* is command low */);

            explicit PacketDecoder(connection::CryptHandler* /* crypt handler */, bool /* is server */);
            ~PacketDecoder();

            void reset();

            bool verify_encryption_client_packet(const protocol::ClientPacketParser& /* packet */);

            /* true if commands might be pending */
            PacketProcessResult process_incoming_data(protocol::PacketParser &/* packet */, std::string& /* error detail */);
            void register_initiv_packet();

            void* callback_argument{nullptr};
            callback_decoded_packet_t callback_decoded_packet{[](auto, auto&){}}; /* needs to be valid all the time! */
            callback_decoded_command_t callback_decoded_command{[](auto, auto&){}}; /* needs to be valid all the time! */
            callback_send_acknowledge_t callback_send_acknowledge{[](auto, auto, auto){}}; /* needs to be valid all the time! */
        private:
            bool is_server;
            connection::CryptHandler* crypt_handler_{nullptr};

            spin_mutex incoming_generation_estimator_lock{};
            std::array<protocol::GenerationEstimator, 9> incoming_generation_estimators{}; /* implementation is thread save */

            std::recursive_mutex packet_buffer_lock;
            command_packet_reassembler _command_fragment_buffers;

            static inline uint8_t command_fragment_buffer_index(uint8_t packet_index) {
                return packet_index & 0x1U; /* use 0 for command and 1 for command low */
            }

            PacketProcessResult decrypt_incoming_packet(std::string &error /* error */, protocol::PacketParser &packet_parser/* packet */);
            CommandReassembleResult try_reassemble_ordered_packet(command_fragment_buffer_t& /* buffer */, std::unique_lock<std::mutex>& /* buffer lock */, ReassembledCommand*& /* command */);
    };
}
