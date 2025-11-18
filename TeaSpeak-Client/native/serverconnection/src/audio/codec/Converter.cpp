//
// Created by wolverindev on 28.04.19.
//

#include "Converter.h"

#include <memory>

#define HAVE_CODEC_OPUS

#ifdef HAVE_CODEC_OPUS
    #include "./OpusConverter.h"
#endif

using namespace tc::audio;
using namespace tc::audio::codec;

[[nodiscard]] constexpr inline bool audio_codec_supported(const AudioCodec &codec) {
    switch (codec) {

#ifdef HAVE_CODEC_OPUS
        case AudioCodec::OpusVoice:
        case AudioCodec::OpusMusic:
            return true;
#endif

#ifdef HAVE_CODEC_SPEEX
        case AudioCodec::Speex:
            return true;
#endif

#ifdef HAVE_CODEC_FLAC
        case AudioCodec::Flac:
            return true;
#endif

#ifdef HAVE_CODEC_CELT
        case AudioCodec::Celt:
            return true;
#endif

        default:
            return false;
    }
}

bool codec::audio_decode_supported(const AudioCodec &codec) {
    return audio_codec_supported(codec);
}

bool codec::audio_encode_supported(const AudioCodec &codec) {
    return audio_codec_supported(codec);
}

std::unique_ptr<AudioDecoder> codec::create_audio_decoder(const AudioCodec &codec) {
    switch (codec) {

#ifdef HAVE_CODEC_OPUS
        case AudioCodec::OpusVoice:
        case AudioCodec::OpusMusic:
            return std::make_unique<OpusAudioDecoder>(codec);
#endif

        default:
            return nullptr;
    }
}

std::unique_ptr<AudioEncoder> codec::create_audio_encoder(const AudioCodec &codec) {
    switch (codec) {

#ifdef HAVE_CODEC_OPUS
        case AudioCodec::OpusVoice:
        case AudioCodec::OpusMusic:
            return std::make_unique<OpusAudioEncoder>(codec);
#endif

        default:
            return nullptr;
    }
}