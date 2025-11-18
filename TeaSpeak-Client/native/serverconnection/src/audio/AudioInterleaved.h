#pragma once

namespace tc::audio {
    extern void deinterleave(
            float* /* dest */,
            const float * /* source */,
            size_t /* channel count */,
            size_t /* sample count */
    );

    extern void interleave(
            float* /* dest */,
            const float * /* source */,
            size_t /* channel count */,
            size_t /* sample count */
    );

    extern void interleave_vec(
            float* /* dest */,
            const float * const * /* sources */,
            size_t /* channel count */,
            size_t /* sample count */
    );
}