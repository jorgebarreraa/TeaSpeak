#include "./AudioOutput.h"
#include "./AudioMerger.h"
#include "./AudioResampler.h"
#include "./AudioInterleaved.h"
#include "./AudioGain.h"
#include "./processing/AudioProcessor.h"
#include "../logger.h"
#include <cstring>
#include <algorithm>
#include <string>

using namespace std;
using namespace tc;
using namespace tc::audio;

AudioOutput::AudioOutput(size_t channels, size_t rate) : channel_count_{channels}, sample_rate_{rate} {
    assert(this->sample_rate_ % kChunkTimeMs == 0);
}

AudioOutput::~AudioOutput() {
	this->close_device();
	this->cleanup_buffers();
}

std::shared_ptr<AudioOutputSource> AudioOutput::create_source(ssize_t buf) {
	auto result = std::shared_ptr<AudioOutputSource>(new AudioOutputSource(this->channel_count_, this->sample_rate_, buf));
	{
        std::lock_guard source_lock{this->sources_mutex};
		this->sources_.push_back(result);
	}
	return result;
}

void AudioOutput::register_audio_processor(const std::shared_ptr<AudioProcessor> &processor) {
    std::lock_guard processor_lock{this->processors_mutex};
    this->audio_processors_.push_back(processor);
}

bool AudioOutput::unregister_audio_processor(const std::shared_ptr<AudioProcessor> &processor) {
    std::lock_guard processor_lock{this->processors_mutex};
    auto index = std::find(this->audio_processors_.begin(), this->audio_processors_.end(), processor);
    if(index == this->audio_processors_.end()) {
        return false;
    }

    this->audio_processors_.erase(index);
    return true;
}

void AudioOutput::cleanup_buffers() {
    free(this->chunk_buffer);
    free(this->source_merge_buffer);

	this->source_merge_buffer = nullptr;
	this->source_merge_buffer_length = 0;

	this->chunk_buffer = nullptr;
	this->chunk_buffer_length = 0;
}

void AudioOutput::ensure_chunk_buffer_space(size_t output_samples) {
    const auto own_chunk_size = (AudioOutput::kChunkTimeMs * this->sample_rate_ * this->channel_count_) / 1000;
    const auto min_chunk_byte_size = std::max(own_chunk_size, output_samples * this->current_output_channels) * sizeof(float);

    if(this->chunk_buffer_length < min_chunk_byte_size) {
        if(this->chunk_buffer) {
            ::free(this->chunk_buffer);
        }

        this->chunk_buffer = malloc(min_chunk_byte_size);
        this->chunk_buffer_length = min_chunk_byte_size;
    }
}

void AudioOutput::fill_buffer(void *output, size_t request_sample_count, size_t out_sample_rate, size_t out_channels) {
    assert(output);
    assert(this->playback_);
    if(!this->resampler_ || this->current_output_sample_rate != out_sample_rate) {
        log_info(category::audio, tr("Output sample rate changed from {} to {}"), this->resampler_ ? this->resampler_->output_rate() : 0, out_sample_rate);
        this->current_output_sample_rate = out_sample_rate;

        this->resampler_ = std::make_unique<AudioResampler>(this->sample_rate(), out_sample_rate, this->channel_count());
        if(!this->resampler_->valid()) {
            log_critical(category::audio, tr("Failed to allocate a new resampler. Audio output will be silent."));
        }
    }

    if(!this->resampler_->valid()) {
        return;
    }

    if(out_channels != this->current_output_channels) {
        log_info(category::audio, tr("Output channel count changed from {} to {}"), this->current_output_channels, out_channels);
        this->current_output_channels = out_channels;

        /*
         * Mark buffer as fully replayed and refill it with new data which fits the new channel count.
         */
        this->chunk_buffer_sample_length = 0;
        this->chunk_buffer_sample_offset = 0;
    }

    return this->fill_buffer_nochecks(output, request_sample_count, out_channels);
}

void AudioOutput::fill_buffer_nochecks(void *output, size_t request_sample_count, size_t out_channels) {
    auto remaining_samples{request_sample_count};
    auto remaining_buffer{output};

    if(this->chunk_buffer_sample_offset < this->chunk_buffer_sample_length) {
        /*
         * We can (partially) fill the output buffer with our current chunk.
         */

        const auto sample_count = std::min(this->chunk_buffer_sample_length - this->chunk_buffer_sample_offset, request_sample_count);
        memcpy(output, (float*) this->chunk_buffer + this->chunk_buffer_sample_offset * this->current_output_channels, sample_count * this->current_output_channels * sizeof(float));
        this->chunk_buffer_sample_offset += sample_count;

        if(sample_count == request_sample_count) {
            /* We've successfully willed the whole output buffer. */
            return;
        }

        remaining_samples = request_sample_count - sample_count;
        remaining_buffer = (float*) output + sample_count * this->current_output_channels;
    }

    this->fill_chunk_buffer();
    this->chunk_buffer_sample_offset = 0;
    return this->fill_buffer_nochecks(remaining_buffer, remaining_samples, out_channels);
}

constexpr static auto kTempChunkBufferSize{64 * 1024};
constexpr static auto kMaxChannelCount{32};
void AudioOutput::fill_chunk_buffer() {

    const auto chunk_local_sample_count = this->chunk_local_sample_count();
    assert(chunk_local_sample_count > 0);
    assert(this->current_output_channels <= kMaxChannelCount);

    std::vector<std::shared_ptr<AudioOutputSource>> sources{};
    sources.reserve(8);

    std::unique_lock sources_lock{this->sources_mutex};
    {
        sources.reserve(this->sources_.size());
        this->sources_.erase(std::remove_if(this->sources_.begin(), this->sources_.end(), [&](const std::weak_ptr<AudioOutputSource>& weak_source) {
            auto source = weak_source.lock();
            if(!source) {
                return true;
            }

            sources.push_back(std::move(source));
            return false;
        }), this->sources_.end());
    }

    {
        size_t actual_sources{0};
        auto sources_it = sources.begin();
        auto sources_end = sources.end();

        /* Initialize the buffer */
        while(sources_it != sources_end) {
            auto source = *sources_it;
            sources_it++;

            if(source->pop_samples(this->chunk_buffer, chunk_local_sample_count)) {
                /* Chunk buffer initialized */
                actual_sources++;
                break;
            }
        }

        if(!actual_sources) {
            /* We don't have any sources. Just provide silence */
            sources_lock.unlock();
            goto zero_chunk_exit;
        }

        /* Lets merge the rest */
        uint8_t temp_chunk_buffer[kTempChunkBufferSize];
        assert(kTempChunkBufferSize >= chunk_local_sample_count * this->channel_count_ * sizeof(float));

        while(sources_it != sources_end) {
            auto source = *sources_it;
            sources_it++;

            if(!source->pop_samples(temp_chunk_buffer, chunk_local_sample_count)) {
                continue;
            }

            actual_sources++;
            merge::merge_sources(this->chunk_buffer, this->chunk_buffer, temp_chunk_buffer, this->channel_count_, chunk_local_sample_count);
        }
    }
    sources_lock.unlock();

    if(this->volume_modifier == 0) {
        goto zero_chunk_exit;
    } else {
        audio::apply_gain(this->chunk_buffer, this->channel_count_, chunk_local_sample_count, this->volume_modifier);
    }

    /* Lets resample our chunk with our sample rate up/down to the device sample rate */
    if(this->resampler_) {
        this->chunk_buffer_sample_length = this->resampler_->estimated_output_size(chunk_local_sample_count);
        this->ensure_chunk_buffer_space(this->chunk_buffer_sample_length);

        if(!this->resampler_->process(this->chunk_buffer, this->chunk_buffer, chunk_local_sample_count, this->chunk_buffer_sample_length)) {
            log_error(category::audio, tr("Failed to resample audio output."));
            goto zero_chunk_exit;
        }

        if(!this->chunk_buffer_sample_length) {
            /* We need more input to create some resampled output */
            log_warn(category::audio, tr("Audio output resampling returned zero samples"));
            return;
        }
    } else {
        this->chunk_buffer_sample_length = chunk_local_sample_count;
    }

    /* Increase/decrease channel count */
    if(this->channel_count_ != this->current_output_channels) {
        if(!merge::merge_channels_interleaved(this->chunk_buffer, this->current_output_channels, this->chunk_buffer, this->channel_count_, this->chunk_buffer_sample_length)) {
            log_error(category::audio, tr("Failed to adjust channel count for audio output."));
            goto zero_chunk_exit;
        }
    }

    {
        std::unique_lock processor_lock{this->processors_mutex};
        auto processors = this->audio_processors_;
        processor_lock.unlock();

        if(!processors.empty()) {
            float temp_chunk_buffer[kTempChunkBufferSize / sizeof(float)];
            assert(kTempChunkBufferSize >= this->current_output_channels * this->chunk_buffer_sample_length * sizeof(float));

            audio::deinterleave(temp_chunk_buffer, (const float*) this->chunk_buffer, this->current_output_channels, this->chunk_buffer_sample_length);
            webrtc::StreamConfig stream_config{(int) this->current_output_sample_rate, this->current_output_channels};

            float* channel_ptr[kMaxChannelCount];
            for(size_t channel{0}; channel < this->current_output_channels; channel++) {
                channel_ptr[channel] = temp_chunk_buffer + (channel *  this->chunk_buffer_sample_length);
            }

            for(const std::shared_ptr<AudioProcessor>& processor : processors) {
                processor->analyze_reverse_stream(channel_ptr, stream_config);
            }
        }
    }

    return;
    zero_chunk_exit: {
        this->chunk_buffer_sample_length = (this->current_output_sample_rate * kChunkTimeMs) / 1000;
        this->ensure_chunk_buffer_space(this->chunk_buffer_sample_length);
        memset(this->chunk_buffer, 0, this->chunk_buffer_sample_length * this->current_output_channels * sizeof(float));
        return;
    };
}

void AudioOutput::set_device(const std::shared_ptr<AudioDevice> &new_device) {
    lock_guard lock(this->device_lock);
    if(this->device == new_device) {
        return;
    }

    this->close_device();
    this->device = new_device;
}

void AudioOutput::close_device() {
    lock_guard lock(this->device_lock);
    if(this->playback_) {
        this->playback_->remove_source(this);
        this->playback_->stop_if_possible();
        this->playback_.reset();
    }

    this->resampler_ = nullptr;
    this->device = nullptr;
}

bool AudioOutput::playback(std::string& error) {
	lock_guard lock(this->device_lock);
	if(!this->device) {
	    error = "invalid device handle";
        return false;
	}
    if(this->playback_) {
        return true;
    }

	this->playback_ = this->device->playback();
	if(!this->playback_) {
	    error = "failed to allocate memory";
        return false;
	}

    this->ensure_chunk_buffer_space(0);
	this->playback_->register_source(this);
    return this->playback_->start(error);
}