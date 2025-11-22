#include <misc/memtracker.h>
#include "CompressionHandler.h"
#define QLZ_COMPRESSION_LEVEL 1
#define QLZ_MEMORY_SAFE
#include "qlz/QuickLZ.h"
#include "buffers.h"

using namespace ts;
using namespace ts::connection;
using namespace std;

namespace ts::compression {
    class thread_buffer {
        public:
            void* get_buffer(size_t size) {
                if(size > 1024 * 1024 *5) /* we don't want to keep such big buffers in memory */
                    return malloc(size);

                if(this->buffer_length < size) {
                    free(this->buffer_ptr);

                    size = std::max(size, (size_t) 1024);
                    this->buffer_ptr = malloc(size);
                    this->buffer_length = size;
                }
                return buffer_ptr;
            }

            void free_buffer(void* ptr) {
                if(ptr == this->buffer_ptr) return;
                free(ptr);
            }

            ~thread_buffer() {
                free(this->buffer_ptr);
            }
        private:
            void* buffer_ptr{nullptr};
            size_t buffer_length{0};
    };

    class qlz_states {
        public:
            qlz_states() noexcept {
                this->state_compress = (qlz_state_compress*) malloc(sizeof(qlz_state_compress));
                this->state_decompress = (qlz_state_decompress*) malloc(sizeof(qlz_state_decompress));
            }

            ~qlz_states() {
                ::free(this->state_compress);
                ::free(this->state_decompress);
            }

            qlz_state_compress* state_compress{nullptr};
            qlz_state_decompress* state_decompress{nullptr};
        private:
    };

    thread_local thread_buffer qlz_buffer{};
    thread_local qlz_states qlz_states{};

    size_t qlz_decompressed_size(const void* payload, size_t payload_length) {
        if(payload_length < 9) return 0; /* payload too small */

        return qlz_size_decompressed((char*) payload) + 400;
    }

    bool qlz_decompress_payload(const void* payload, void* buffer, size_t* buffer_size) {
        if(!qlz_states.state_decompress) return false;
        assert(payload != buffer);

        size_t data_length = qlz_decompress((char*) payload, (char*) buffer, qlz_states.state_decompress);
        if(data_length <= 0)
            return false;

        /* test for overflow */
        if(data_length > *buffer_size) terminate();
        *buffer_size = data_length;
        return true;
    }

    size_t qlz_compressed_size(const void* payload, size_t payload_length) {
        assert(payload_length >= 9);
        //// "Always allocate size + 400 bytes for the destination buffer when compressing." <= http://www.quicklz.com/manual.html
        return max(min(payload_length * 2, (size_t) (payload_length + 400ULL)), (size_t) 24ULL); /* at least 12 bytes (QLZ header) */
    }

    bool qlz_compress_payload(const void* payload, size_t payload_length, void* buffer, size_t* buffer_length) {
        if(!qlz_states.state_compress) return false;

        assert(payload != buffer);
        assert(*buffer_length >= qlz_compressed_size(payload, payload_length));

        size_t compressed_length = qlz_compress(payload, (char*) buffer, payload_length, qlz_states.state_compress);
        if(compressed_length > *buffer_length) terminate();

        if(compressed_length <= 0)
            return false;
        *buffer_length = compressed_length;
        return true;
    }
}

bool CompressionHandler::compress(protocol::BasicPacket* packet, std::string &error) {
    auto packet_payload = packet->data();
    auto header_length = packet->length() - packet_payload.length();

    size_t max_compressed_payload_size = compression::qlz_compressed_size(packet_payload.data_ptr(), packet_payload.length());
    auto target_buffer = buffer::allocate_buffer(max_compressed_payload_size + header_length);

    size_t compressed_size{max_compressed_payload_size};
    if(!compression::qlz_compress_payload(packet_payload.data_ptr(), packet_payload.length(), &target_buffer[header_length], &compressed_size)) return false;

    memcpy(target_buffer.data_ptr(), packet->buffer().data_ptr(), header_length);
    packet->buffer(target_buffer.range(0, compressed_size + header_length));
    return true;
}

bool CompressionHandler::decompress(protocol::BasicPacket* packet, std::string &error) {
    auto expected_length = compression::qlz_decompressed_size(packet->data().data_ptr(), packet->data().length());
    if(expected_length > this->max_packet_size){ //Max 16MB. (97% Compression!)
        error = "Invalid packet size. (Calculated target length of " + to_string(expected_length) + ". Max length: " + to_string(this->max_packet_size) + ")";
        return false;
    } else if(expected_length == 0) {
        error = "Failed to calculate decompressed packet length";
        return false;
    }
    auto header_length = packet->header().length() + packet->mac().length();
    auto buffer = buffer::allocate_buffer(expected_length + header_length);

    size_t compressed_size{expected_length};
    if(!compression::qlz_decompress_payload(packet->data().data_ptr(), &buffer[header_length], &compressed_size)) return false;
    memcpy(buffer.data_ptr(), packet->buffer().data_ptr(), header_length);

    packet->buffer(buffer.range(0, compressed_size + header_length));
    return true;
}

bool CompressionHandler::progressPacketIn(protocol::BasicPacket* packet, std::string &error) {
    if(packet->isCompressed()) {
        if(!this->decompress(packet, error)) return false;
        packet->setCompressed(false);
    }
    return true;
}

bool CompressionHandler::progressPacketOut(protocol::BasicPacket* packet, std::string& error) {
    if(packet->has_flag(protocol::PacketFlag::Compressed) && !packet->isCompressed()) {
        if(!this->compress(packet, error)) return false;
        packet->setCompressed(true);
    }
    return true;
}

CompressionHandler::CompressionHandler() { }
CompressionHandler::~CompressionHandler() { }