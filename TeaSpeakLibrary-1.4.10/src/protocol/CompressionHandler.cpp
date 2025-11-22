#include <misc/memtracker.h>
#include "CompressionHandler.h"
#define QLZ_COMPRESSION_LEVEL 1
#define QLZ_MEMORY_SAFE
#include "qlz/QuickLZ.h"

using namespace ts;
using namespace std;

namespace ts::compression {
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
    };

    thread_local qlz_states qlz_states{};

    size_t qlz_decompressed_size(const void* payload, size_t payload_length) {
        if(payload_length < 9) {
            return 0; /* payload too small */
        }

        return qlz_size_decompressed((char*) payload) + 400;
    }

    bool qlz_decompress_payload(const void* payload, void* buffer, size_t* buffer_size) {
        if(!qlz_states.state_decompress) {
            return false;
        }
        assert(payload != buffer);

        size_t data_length = qlz_decompress((char*) payload, (char*) buffer, qlz_states.state_decompress);
        if(data_length <= 0) {
            return false;
        }

        /* test for overflow */
        if(data_length > *buffer_size) terminate();
        *buffer_size = data_length;
        return true;
    }

    size_t qlz_compressed_size(const void* payload, size_t payload_length) {
        (void) payload;
        assert(payload_length >= 9);
        return payload_length + 400; // http://www.quicklz.com/manual.html
    }

    bool qlz_compress_payload(const void* payload, size_t payload_length, void* buffer, size_t* buffer_length) {
        if(!qlz_states.state_compress) {
            return false;
        }

        assert(payload != buffer);
        assert(*buffer_length >= qlz_compressed_size(payload, payload_length));

        size_t compressed_length = qlz_compress(payload, (char*) buffer, payload_length, qlz_states.state_compress);
        if(compressed_length > *buffer_length) {
            terminate();
        }

        if(compressed_length <= 0) {
            return false;
        }

        *buffer_length = compressed_length;
        return true;
    }
}