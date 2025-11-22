#pragma once

#include "./Packet.h"

namespace ts::compression {
    /* Attention: These methods does not validate the data! */
    size_t qlz_decompressed_size(const void* payload, size_t payload_length);
    bool qlz_decompress_payload(const void* payload, void* buffer, size_t* buffer_size); //Attention: payload & buffer must be different!

    size_t qlz_compressed_size(const void* payload, size_t payload_length);
    bool qlz_compress_payload(const void* payload, size_t payload_length, void* buffer, size_t* buffer_length);
}