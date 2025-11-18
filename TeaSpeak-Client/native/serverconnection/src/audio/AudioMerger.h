#pragma once

#include <cstring>

namespace tc::audio::merge {
    /*
     * The result buffer could be equal to one of the source buffers to prevent unnecessary allocations
     * Note: The sample order is irrelevant
     */
    extern bool merge_sources(void* /* result */, void* /* source a */, void* /* source b */, size_t /* channels */, size_t /* samples */);

    extern bool merge_n_sources(void* /* result */, void** /* sources */, size_t /* size_t sources count */, size_t /* channels */, size_t /* samples */);

    extern bool merge_channels_interleaved(void* /* result */, size_t /* result channels */, const void* /* source */, size_t /* source channels */, size_t /* samples */);
}