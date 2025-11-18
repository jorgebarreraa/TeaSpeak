#pragma once

#include <nan.h>

namespace tc::audio {
    extern NAN_METHOD(available_devices);
    extern NAN_METHOD(await_initialized_js);
    extern NAN_METHOD(initialized_js);

    namespace player {
        extern NAN_MODULE_INIT(init_js);

        extern NAN_METHOD(current_playback_device);
        extern NAN_METHOD(set_playback_device);

        extern NAN_METHOD(create_stream);

        extern NAN_METHOD(get_master_volume);
        extern NAN_METHOD(set_master_volume);
    }
}