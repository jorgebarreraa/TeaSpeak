#include "OpusConverter.h"
#include "../../logger.h"

using namespace std;
using namespace tc::audio::codec;

/* The opus encoder */
OpusAudioEncoder::OpusAudioEncoder(AudioCodec target_codec) : target_codec_{target_codec} {}
OpusAudioEncoder::~OpusAudioEncoder() noexcept {
    if(this->encoder) {
        opus_encoder_destroy(this->encoder);
        this->encoder = nullptr;
    }
};

bool OpusAudioEncoder::valid() const {
    return this->encoder != nullptr;
}

bool OpusAudioEncoder::initialize(string &error) {
    int application_type;
    switch(this->target_codec_) {
        case AudioCodec::OpusVoice:
            application_type = OPUS_APPLICATION_VOIP;
            break;

        case AudioCodec::OpusMusic:
            application_type = OPUS_APPLICATION_AUDIO;
            break;

        default:
            error = "target codec isn't opus";
            return false;
    }

    int error_id{0};
    this->encoder = opus_encoder_create((opus_int32) this->sample_rate(), (int) this->channel_count(), application_type, &error_id);
    if(!this->encoder || error_id) {
        error = "failed to create encoder (" + to_string(error_id) + ")";
        goto cleanup_error;
    }

    error_id = opus_encoder_ctl(this->encoder, OPUS_SET_BITRATE(64000));
    if(error_id) {
        error = "failed to set bitrate (" + to_string(error_id) + ")";
        goto cleanup_error;
    }

    error_id = opus_encoder_ctl(this->encoder, OPUS_SET_INBAND_FEC(1));
    if(error_id) {
        error = "failed to enable fec (" + to_string(error_id) + ")";
        goto cleanup_error;
    }

    error_id = opus_encoder_ctl(this->encoder, OPUS_SET_PACKET_LOSS_PERC(15));
    if(error_id) {
        error = "failed to assume a 15% packet loss (" + to_string(error_id) + ")";
        goto cleanup_error;
    }

    return true;

    cleanup_error:
    if(this->encoder) {
        opus_encoder_destroy(this->encoder);
        this->encoder = nullptr;
    }
    return false;
}

void OpusAudioEncoder::reset_sequence() {
    auto result = opus_encoder_ctl(this->encoder, OPUS_RESET_STATE);
    if(result != OPUS_OK) {
        log_warn(category::audio, tr("Failed to reset opus encoder. Opus result: {}"), result);
    }
}

size_t OpusAudioEncoder::sample_rate() const {
    return 48000;
}

size_t OpusAudioEncoder::channel_count() const {
    if(this->target_codec_ == AudioCodec::OpusMusic) {
        return 2;
    } else {
        return 1;
    }
}

size_t OpusAudioEncoder::expected_encoded_length(const float *, size_t) const {
    return 1500;
}

bool OpusAudioEncoder::encode(std::string& error, void *target_buffer, size_t &target_size, const EncoderBufferInfo &info, const float *source_buffer) {
    if(info.sample_count == 0) {
        /* flush request but we've no internal buffers */
        target_size = 0;
        return true;
    }

    /* TODO: Use some calculated variables here provided via EncoderBufferInfo */
    opus_encoder_ctl(encoder, OPUS_SET_PACKET_LOSS_PERC(info.head_sequence ? 100 : 15));
    auto result = opus_encode_float(this->encoder, source_buffer, (int) info.sample_count, (uint8_t*) target_buffer, (opus_int32) target_size);
    if(result < OPUS_OK) {
        error = to_string(result) + "|" + opus_strerror(result);
        return false;
    }

    target_size = (size_t) result;
    return true;
}

/* The Opus decoder */
OpusAudioDecoder::OpusAudioDecoder(AudioCodec target_codec) : target_codec_{target_codec} {}
OpusAudioDecoder::~OpusAudioDecoder() noexcept {
    if(this->decoder) {
        opus_decoder_destroy(this->decoder);
        this->decoder = nullptr;
    }
};

bool OpusAudioDecoder::valid() const {
    return this->decoder != nullptr;
}

bool OpusAudioDecoder::initialize(string &error) {
    int error_id{0};
    this->decoder = opus_decoder_create((opus_int32) this->sample_rate(), (int) this->channel_count(), &error_id);
    if(!this->decoder || error_id) {
        error = "failed to create decoder (" + to_string(error_id) + ")";
        return false;
    }

    return true;
}

void OpusAudioDecoder::reset_sequence() {
    auto result = opus_decoder_ctl(this->decoder, OPUS_RESET_STATE);
    if(result != OPUS_OK) {
        log_warn(category::audio, tr("Failed to reset opus decoder. Opus result: {}"), result);
    }
}

size_t OpusAudioDecoder::sample_rate() const {
    return 48000;
}

size_t OpusAudioDecoder::channel_count() const {
    if(this->target_codec_ == AudioCodec::OpusMusic) {
        return 2;
    } else {
        return 1;
    }
}

size_t OpusAudioDecoder::expected_decoded_length(const void *payload, size_t payload_size) const {
    auto result = opus_decoder_get_nb_samples(this->decoder, (uint8_t*) payload, payload_size);
    if(result <= 0) {
        return 0;
    }
    return (size_t) result;
}

bool OpusAudioDecoder::decode(string &error, float *sample_buffer, size_t &sample_count, const DecodePayloadInfo &info, const void *payload) {
    auto result = opus_decode_float(this->decoder,
                                    info.byte_length == 0 ? nullptr : (uint8_t*) payload, (opus_int32) info.byte_length,
                                    (float*) sample_buffer, (int) sample_count,
                                    info.fec_decode ? 1 : 0
    );

    if(result < 0) {
        error = to_string(result) + "|" + opus_strerror(result);
        return false;
    }

    sample_count = (size_t) result;
    return true;
}
