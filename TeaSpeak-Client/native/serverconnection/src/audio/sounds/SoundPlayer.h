#pragma once

#include <functional>
#include <string_view>

#ifdef NODEJS_API
#include <nan.h>
#endif

namespace tc::audio::sounds {
    typedef uintptr_t sound_playback_id;

    enum struct PlaybackResult {
        SUCCEEDED,
        CANCELED,
        SOUND_NOT_INITIALIZED,
        FILE_OPEN_ERROR,
        PLAYBACK_ERROR /* has the extra error set */
    };

    typedef std::function<void(PlaybackResult, const std::string&)> callback_status_t;
    struct PlaybackSettings {
        std::string file{};
        float volume{1.f};
        /* ATTENTION: This callback may be called within the audio loop! */
        callback_status_t callback{};
    };

    extern sound_playback_id playback_sound(const PlaybackSettings& /* settings */);
    extern void cancel_playback(const sound_playback_id&);

#ifdef NODEJS_API
    extern NAN_METHOD(playback_sound_js);
    extern NAN_METHOD(cancel_playback_js);
#endif
}