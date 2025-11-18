#pragma once

#include "Converter.h"
#include <opus/opus.h>
#include <mutex>

namespace tc::audio::codec {
    class OpusAudioEncoder : public AudioEncoder {
        public:
            explicit OpusAudioEncoder(AudioCodec /* target codec */);
            ~OpusAudioEncoder() override;

            bool valid() const override;

            bool initialize(std::string &string) override;

            void reset_sequence() override;

            size_t sample_rate() const override;

            size_t channel_count() const override;

            size_t expected_encoded_length(const float *pDouble, size_t size) const override;

            bool encode(std::string&, void *, size_t &, const EncoderBufferInfo &, const float *) override;

        private:
            AudioCodec target_codec_;
            OpusEncoder* encoder{nullptr};
    };

    class OpusAudioDecoder : public AudioDecoder {
        public:
            explicit OpusAudioDecoder(AudioCodec /* target codec */);
            ~OpusAudioDecoder() override;

            bool valid() const override;

            bool initialize(std::string &string) override;

            void reset_sequence() override;

            size_t sample_rate() const override;

            size_t channel_count() const override;

            size_t expected_decoded_length(const void *pVoid, size_t size) const override;

            bool decode(std::string &, float *, size_t &, const DecodePayloadInfo &, const void *) override;

        private:
            AudioCodec target_codec_;
            OpusDecoder* decoder{nullptr};
    };
}