#include "AudioSender.h"
#include "VoiceConnection.h"
#include "../ServerConnection.h"
#include "../../audio/AudioEventLoop.h"
#include "../../audio/AudioMerger.h"
#include "../../audio/AudioReframer.h"

using namespace std;
using namespace tc;
using namespace tc::audio;
using namespace tc::audio::codec;
using namespace tc::connection;

VoiceSender::VoiceSender(tc::connection::VoiceConnection *handle) : handle{handle} {}

VoiceSender::~VoiceSender() {
    /* Note: We can't be within the event loop since if we were we would have a shared reference*/
	audio::encode_event_loop->cancel(dynamic_pointer_cast<event::EventEntry>(this->_ref.lock()));

    {
        lock_guard buffer_lock{this->raw_audio_buffer_mutex};
        while(this->raw_audio_buffers_head) {
            auto buffer = std::exchange(this->raw_audio_buffers_head, this->raw_audio_buffers_head->next);
            buffer->~AudioFrame();
            ::free(this->raw_audio_buffers_head);
        }
        this->raw_audio_buffers_tail = &this->raw_audio_buffers_head;
    }
}

void VoiceSender::set_voice_send_enabled(bool flag) {
	this->voice_send_enabled = flag;
}

void VoiceSender::send_data(const float *data, size_t samples, size_t rate, size_t channels) {
	if(!this->voice_send_enabled) {
		log_warn(category::voice_connection, tr("Dropping raw audio frame because voice sending has been disabled!"));
		return;
	}

	/* aligned for float values */
	const auto aligned_frame_size{((sizeof(AudioFrame) + 3) / sizeof(float)) * sizeof(float)};

	auto frame = (AudioFrame*) malloc(aligned_frame_size + samples * channels * sizeof(float));
	new (frame) AudioFrame{};

    frame->sample_count = samples;
	frame->sample_rate = rate;
	frame->channels = channels;

	frame->buffer = (float*) frame + aligned_frame_size / sizeof(float);
    memcpy(frame->buffer, data, samples * channels * sizeof(float));

	frame->timestamp = chrono::system_clock::now();

	{
		lock_guard buffer_lock(this->raw_audio_buffer_mutex);
        *this->raw_audio_buffers_tail = frame;
        this->raw_audio_buffers_tail = &frame->next;
	}

	audio::encode_event_loop->schedule(dynamic_pointer_cast<event::EventEntry>(this->_ref.lock()));
}

void VoiceSender::send_stop() {
    auto frame = (AudioFrame*) malloc(sizeof(AudioFrame));
    new (frame) AudioFrame{};
	frame->timestamp = chrono::system_clock::now();

	{
		lock_guard buffer_lock{this->raw_audio_buffer_mutex};
		*this->raw_audio_buffers_tail = frame;
		this->raw_audio_buffers_tail = &frame->next;
	}

	audio::encode_event_loop->schedule(dynamic_pointer_cast<event::EventEntry>(this->_ref.lock()));
}

void VoiceSender::finalize() {
    auto execute_lock = this->execute_lock(true);
	this->handle = nullptr;
}

void VoiceSender::event_execute(const std::chrono::system_clock::time_point &point) {
	static auto max_time = chrono::milliseconds(10);

	bool reschedule = false;
	auto now = chrono::system_clock::now();
	while(true) {
		std::unique_lock buffer_lock{this->raw_audio_buffer_mutex};
		if(!this->raw_audio_buffers_head) {
		    break;
		}

		auto next_buffer = std::exchange(this->raw_audio_buffers_head, this->raw_audio_buffers_head->next);
		if(!this->raw_audio_buffers_head) {
            assert(this->raw_audio_buffers_tail == &next_buffer->next);
            this->raw_audio_buffers_tail = &this->raw_audio_buffers_head;
		}
		buffer_lock.unlock();

        //TODO: Drop too old buffers!

		if(this->handle) {
            this->encode_raw_frame(next_buffer);
		}

        next_buffer->~AudioFrame();
        ::free(next_buffer);
        if(chrono::system_clock::now() - now > max_time) {
            reschedule = true;
            break;
        }
	}

	if(reschedule) {
		log_warn(category::voice_connection, tr("Audio data decode will take longer than {} us. Enqueueing for later"), chrono::duration_cast<chrono::microseconds>(max_time).count());
		audio::decode_event_loop->schedule(dynamic_pointer_cast<event::EventEntry>(this->_ref.lock()));
	}
}

constexpr static auto kTempBufferMaxSampleCount{1024 * 8};
void VoiceSender::encode_raw_frame(const AudioFrame* frame) {
    if(frame->sample_rate == 0) {
        /* Audio sequence end */
        this->audio_sequence_no = 0;

        auto codec_protocol_id = audio::codec::audio_codec_to_protocol_id(this->current_codec);
        if(codec_protocol_id.has_value()) {
            this->flush_current_codec();

            if(this->codec_encoder) {
                this->codec_encoder->reset_sequence();
            }

            auto server = this->handle->handle();
            server->send_voice_data(nullptr, 0, *codec_protocol_id, false);
        }
        return;
    }

    if(this->current_codec != this->target_codec_) {
        auto codec_protocol_id = audio::codec::audio_codec_to_protocol_id(this->target_codec_);
        if(!codec_protocol_id.has_value()) {
            /* we can't send it so no need to initialize it */
            return;
        }

        this->flush_current_codec();

        this->audio_sequence_no = 0;
        this->codec_resampler = nullptr;
        this->codec_reframer = nullptr;
        this->codec_encoder = nullptr;
        this->current_codec = this->target_codec_;

        if(!audio::codec::audio_encode_supported(this->current_codec)) {
            log_warn(category::voice_connection, tr("Audio sender set to codec where encoding is not supported. Do not send any audio data."));
            return;
        }

        this->codec_encoder = audio::codec::create_audio_encoder(this->current_codec);
        if(!this->codec_encoder) {
            log_error(category::voice_connection, tr("Failed to allocate new audio encoder for codec {}"), (uint32_t) this->target_codec_);
            return;
        }

        std::string error{};
        if(!this->codec_encoder->initialize(error)) {
            log_error(category::voice_connection, tr("Failed to initialize auto encoder (codec {}) {}"), (uint32_t) this->target_codec_, error);
            this->codec_encoder = nullptr;
            return;
        }
    }

    if(!this->codec_encoder) {
        /* Codec failed to initialize */
        return;
    }

    const auto codec_channel_count = this->codec_encoder->channel_count();
    const auto codec_sample_rate = this->codec_encoder->sample_rate();

    float temp_buffer[kTempBufferMaxSampleCount];
    size_t current_sample_count{frame->sample_count};
    float* current_sample_buffer;

    if(frame->channels != codec_channel_count) {
        assert(kTempBufferMaxSampleCount >= frame->sample_count * codec_channel_count);
        if(!audio::merge::merge_channels_interleaved(temp_buffer, codec_channel_count, frame->buffer, frame->channels, frame->sample_count)) {
            log_warn(category::voice_connection, tr("Failed to merge channels to output stream channel count! Dropping local voice packet"));
            return;
        }

        current_sample_buffer = temp_buffer;
    } else {
        current_sample_buffer = frame->buffer;
    }

    if(frame->sample_rate != codec_sample_rate) {
        if(!this->codec_resampler || this->codec_resampler->input_rate() != frame->sample_rate) {
            this->codec_resampler = std::make_unique<audio::AudioResampler>(frame->sample_rate, codec_sample_rate, codec_channel_count);
        }

        size_t resampled_sample_count{this->codec_resampler->estimated_output_size(frame->sample_count)};
        assert(kTempBufferMaxSampleCount >= resampled_sample_count * codec_channel_count);
        if(!this->codec_resampler->process(temp_buffer, current_sample_buffer, frame->sample_count, resampled_sample_count)) {
            log_error(category::voice_connection, tr("Failed to resample buffer. Dropping audio frame"));
            return;
        }

        current_sample_buffer = temp_buffer;
        current_sample_count = resampled_sample_count;
    }

    if(!this->codec_reframer) {
        this->codec_reframer = std::make_unique<audio::AudioReframer>(codec_channel_count, (size_t) (0.02 * codec_sample_rate));
        this->codec_reframer->on_frame = [&](const float* sample_buffer) {
            assert(this->codec_reframer);
            this->handle_network_frame(sample_buffer, this->codec_reframer->target_size(), false);
        };
        this->codec_reframer->on_flush = [&](const float* sample_buffer, size_t sample_count) {
            this->handle_network_frame(sample_buffer, sample_count, true);
        };
    }

    this->codec_reframer->process(current_sample_buffer, current_sample_count);
}

constexpr static auto kMaxPacketSize{1500};
void VoiceSender::handle_network_frame(const float *sample_buffer, size_t sample_count, bool is_flush) {
    assert(this->codec_encoder);
    auto codec_protocol_id = audio::codec::audio_codec_to_protocol_id(this->current_codec);
    if(!codec_protocol_id.has_value()) {
        return;
    }

    //log_trace(category::voice_connection, tr("Encoding audio chunk of {}/{} aka {}ms with codec {}"),
    // sample_count, this->codec_encoder->sample_rate(), sample_count * 1000 / this->codec_encoder->sample_rate(), *this->current_codec_);

    char packet_buffer[kMaxPacketSize];
    size_t packet_size{kMaxPacketSize};

    EncoderBufferInfo buffer_info{};
    buffer_info.flush_encoder = is_flush;
    buffer_info.sample_count = sample_count;
    buffer_info.head_sequence = this->audio_sequence_no++ < 5;

    std::string error{};
    if(!this->codec_encoder->encode(error, packet_buffer, packet_size, buffer_info, sample_buffer)) {
        log_error(category::voice_connection, tr("Failed to encode voice: {}"), error);
        return;
    }

    if(!packet_size) {
        /* No audio packet created */
        return;
    }

    auto server = this->handle->handle();
    server->send_voice_data(packet_buffer, packet_size, *codec_protocol_id, buffer_info.head_sequence);
}

void VoiceSender::flush_current_codec() {
    if(!this->codec_reframer) {
        return;
    }

    this->codec_reframer->flush();
}