#pragma once

#include "Packet.h"

namespace ts {
    namespace compression {
        /* Attention: These methods does not validate the data! */
        size_t qlz_decompressed_size(const void* payload, size_t payload_length);
        bool qlz_decompress_payload(const void* payload, void* buffer, size_t* buffer_size); //Attention: payload & buffer must be differen!

        size_t qlz_compressed_size(const void* payload, size_t payload_length);
        bool qlz_compress_payload(const void* payload, size_t payload_length, void* buffer, size_t* buffer_length);
    }

    namespace connection {
        class CompressionHandler {
            public:
                CompressionHandler();
                ~CompressionHandler();

                bool progressPacketOut(protocol::BasicPacket*, std::string&);
                bool progressPacketIn(protocol::BasicPacket*, std::string&);

                size_t max_packet_size{16 * 1024};
            private:
                bool compress(protocol::BasicPacket*, std::string &error);
                bool decompress(protocol::BasicPacket*, std::string &error);
        };
    }
}