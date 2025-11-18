#pragma once

namespace tc::audio {
    /**
     * @return true if audio clipped
     */
    extern bool apply_gain(void* /* buffer */, size_t /* channel count */, size_t /* sample count */, float /* gain */);
}