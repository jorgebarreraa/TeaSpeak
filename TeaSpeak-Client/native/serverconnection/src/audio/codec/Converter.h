#pragma once

#include <string>
#include <memory>
#include <optional>

namespace tc::audio::codec {
    enum struct AudioCodec {
        Unknown,

        /* supported */
        OpusVoice,
        OpusMusic,

        /* Not yet supported */
        //Flac,

        /* Removed in summer 2020 */
        SpeexNarrow,
        SpeexWide,
        SpeexUltraWide,
        Celt,
    };

    class AudioEncoder;
    class AudioDecoder;

    [[nodiscard]] extern bool audio_encode_supported(const AudioCodec& /* codec */);
    [[nodiscard]] extern std::unique_ptr<AudioEncoder> create_audio_encoder(const AudioCodec& /* codec */);

    [[nodiscard]] extern bool audio_decode_supported(const AudioCodec& /* codec */);
    [[nodiscard]] extern std::unique_ptr<AudioDecoder> create_audio_decoder(const AudioCodec& /* codec */);

    [[nodiscard]] constexpr inline std::optional<uint8_t> audio_codec_to_protocol_id(const AudioCodec& codec) {
        switch(codec) {
            case AudioCodec::SpeexNarrow:
                return std::make_optional(0);

            case AudioCodec::SpeexWide:
                return std::make_optional(1);

            case AudioCodec::SpeexUltraWide:
                return std::make_optional(2);

            case AudioCodec::Celt:
                return std::make_optional(3);

            case AudioCodec::OpusVoice:
                return std::make_optional(4);

            case AudioCodec::OpusMusic:
                return std::make_optional(5);

            default:
                return std::nullopt;
        }
    }

    [[nodiscard]] constexpr inline std::optional<AudioCodec> audio_codec_from_protocol_id(uint8_t id) {
        switch(id) {
            case 0:
                return std::make_optional(AudioCodec::SpeexNarrow);

            case 1:
                return std::make_optional(AudioCodec::SpeexWide);

            case 2:
                return std::make_optional(AudioCodec::SpeexUltraWide);

            case 3:
                return std::make_optional(AudioCodec::Celt);

            case 4:
                return std::make_optional(AudioCodec::OpusVoice);

            case 5:
                return std::make_optional(AudioCodec::OpusMusic);

            default:
                return std::nullopt;
        }
    }

    [[nodiscard]] constexpr inline const char* audio_codec_name(const AudioCodec& codec) {
        switch(codec) {
            case AudioCodec::SpeexNarrow:
                return "speex narrow";

            case AudioCodec::SpeexWide:
                return "speex wide";

            case AudioCodec::SpeexUltraWide:
                return "speex ultra wide";

            case AudioCodec::Celt:
                return "celt";

            case AudioCodec::OpusVoice:
                return "opus voice";

            case AudioCodec::OpusMusic:
                return "opus music";

            case AudioCodec::Unknown:
                return "unknown";

            default:
                return "invalid";
        }
    }

    struct EncoderBufferInfo {
        size_t sample_count{0};
        bool head_sequence{false};
        bool flush_encoder{false};
    };

    /**
     *  Encoders should only be accessed by one thread at once
     */
    class AudioEncoder {
        public:
            explicit AudioEncoder() = default;
            virtual ~AudioEncoder() = default;

            [[nodiscard]] virtual bool valid() const = 0;
            [[nodiscard]] virtual bool initialize(std::string& /* error */) = 0;
            virtual void reset_sequence() = 0;

            /**
             * Get the codecs sample rate.
             */
            [[nodiscard]] virtual size_t sample_rate() const = 0;

            /**
             * Get the codecs audio channel count.
             */
            [[nodiscard]] virtual size_t channel_count() const = 0;


            /**
             * @returns the expected output length.
             *          If unknown a size near the MTU will be returned.
             */
            [[nodiscard]] virtual size_t expected_encoded_length(const float* /* samples */, size_t /* sample count */) const = 0;

            /**
             * Encode a chunk of audio data.
             * @return `true` on success else `false`
             */
            [[nodiscard]] virtual bool encode(std::string& /* error */, void* /* target buffer */, size_t& /* target length */, const EncoderBufferInfo& /* buffer info */, const float* /* samples */) = 0;
    };

    struct DecodePayloadInfo {
        /**
         * Use a value of zero to indicate packet loss
         */
        size_t byte_length{0};
        bool fec_decode{false};
    };

    class AudioDecoder {
        public:
            explicit AudioDecoder() = default;
            virtual ~AudioDecoder() = default;

            [[nodiscard]] virtual bool valid() const = 0;
            [[nodiscard]] virtual bool initialize(std::string& /* error */) = 0;
            virtual void reset_sequence() = 0;

            /**
             * Get the codecs sample rate (will be the output rate)
             */
            [[nodiscard]] virtual size_t sample_rate() const = 0;

            /**
             * Get the codecs audio channel count.
             */
            [[nodiscard]] virtual size_t channel_count() const = 0;

            /**
             * @returns the expected sample count
             */
            [[nodiscard]] virtual size_t expected_decoded_length(const void* /* payload */, size_t /* payload length */) const = 0;

            [[nodiscard]] virtual bool decode(std::string& /* error */, float* /* target buffer */, size_t& /* target sample count */, const DecodePayloadInfo& /* payload info */, const void* /* payload */) = 0;
    };
}