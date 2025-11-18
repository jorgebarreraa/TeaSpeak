#pragma once

#include <cstdint>
#include <memory>

namespace tc::audio {
    /* Every sample is a float (4byte) */
    struct SampleBuffer {
        static std::shared_ptr<SampleBuffer> allocate(uint8_t /* channels */, uint16_t /* samples */);

        uint16_t sample_size;
        uint16_t sample_index;

        char sample_data[
                /* windows does not allow zero sized arrays */
#ifndef WIN32
                0
#else
                1
#endif
        ];
    };
}