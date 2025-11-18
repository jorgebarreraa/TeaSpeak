#pragma once

#include "../EventLoop.h"

namespace tc::audio {
    extern event::EventExecutor* encode_event_loop;
    extern event::EventExecutor* decode_event_loop;

    extern void init_event_loops();
    extern void shutdown_event_loops();
}