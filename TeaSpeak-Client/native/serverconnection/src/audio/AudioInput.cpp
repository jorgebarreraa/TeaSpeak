#include <cstring>
#include <memory>
#include <string>
#include "./AudioInput.h"
#include "./AudioReframer.h"
#include "./AudioResampler.h"
#include "./AudioMerger.h"
#include "./AudioGain.h"
#include "./AudioInterleaved.h"
#include "./AudioOutput.h"
#include "./AudioLevelMeter.h"
#include "./processing/AudioProcessor.h"
#include "./AudioEventLoop.h"
#include "../logger.h"

using namespace std;
using namespace tc;
using namespace tc::audio;

extern tc::audio::AudioOutput* global_audio_output;
AudioInput::AudioInput(size_t channels, size_t sample_rate) :
    channel_count_{channels},
    sample_rate_{sample_rate},
    input_buffer{(sample_rate * channels * sizeof(float) * kInputBufferCapacityMs) / 1000}
{
    this->event_loop_entry = std::make_shared<EventLoopCallback>(this);

    {
        this->initialize_hook_handle = std::make_shared<AudioInitializeHook>();
        this->initialize_hook_handle->input = this;

        std::weak_ptr weak_handle{this->initialize_hook_handle};
        audio::initialize([weak_handle] {
            auto handle = weak_handle.lock();
            if(!handle) {
                return;
            }

            std::lock_guard lock{handle->mutex};
            if(!handle->input) {
                return;
            }

            auto processor = std::make_shared<AudioProcessor>();
            if(!processor->initialize()) {
                log_error(category::audio, tr("Failed to initialize audio processor."));
                return;
            }

            global_audio_output->register_audio_processor(processor);
            handle->input->audio_processor_ = processor;
        });
    }
}

AudioInput::~AudioInput() {
    {
        std::lock_guard lock{this->initialize_hook_handle->mutex};
        this->initialize_hook_handle->input = nullptr;
    }

    {
        audio::encode_event_loop->cancel(this->event_loop_entry);
        this->event_loop_entry->execute_lock(true);
    }

    if(this->audio_processor_) {
        assert(global_audio_output);
        global_audio_output->unregister_audio_processor(this->audio_processor_);
        this->audio_processor_ = nullptr;
    }

	this->close_device();
}

void AudioInput::set_device(const std::shared_ptr<AudioDevice> &device) {
    std::lock_guard lock{this->input_source_lock};
    if(device == this->input_device) {
        return;
    }

    this->close_device();
    this->input_device = device;
}

void AudioInput::close_device() {
    std::lock_guard lock{this->input_source_lock};
    if(this->input_recorder) {
        this->input_recorder->remove_consumer(this);
        this->input_recorder->stop_if_possible();
        this->input_recorder.reset();
    }
    this->resampler_ = nullptr;
    this->input_device = nullptr;
}

bool AudioInput::record(std::string& error) {
    std::lock_guard lock{this->input_source_lock};
    if(!this->input_device) {
        error = "no device";
        return false;
    }

    if(this->input_recorder) {
        return true;
    }

    this->input_recorder = this->input_device->record();
    if(!this->input_recorder) {
        error = "failed to get recorder";
        return false;
    }

    this->input_recorder->register_consumer(this);
    if(!this->input_recorder->start(error)) {
        this->input_recorder->remove_consumer(this);
        this->input_recorder.reset();
        return false;
    }
    return true;
}

bool AudioInput::recording() {
    return this->input_recorder != nullptr;
}

void AudioInput::stop() {
    if(!this->input_recorder) return;

    this->input_recorder->remove_consumer(this);
    this->input_recorder->stop_if_possible();
    this->input_recorder.reset();

    this->resampler_ = nullptr;
}

std::vector<std::shared_ptr<AudioInputConsumer>> AudioInput::consumers() {
    std::vector<std::shared_ptr<AudioInputConsumer>> result{};
    result.reserve(10);

    std::lock_guard consumer_lock{this->consumers_mutex};
    result.reserve(this->consumers_.size());

    this->consumers_.erase(std::remove_if(this->consumers_.begin(), this->consumers_.end(), [&](const std::weak_ptr<AudioInputConsumer>& weak_consumer) {
        auto consumer = weak_consumer.lock();
        if(!consumer) {
            return true;
        }

        result.push_back(consumer);
        return false;
    }), this->consumers_.end());

    return result;
}

void AudioInput::register_consumer(const std::shared_ptr<AudioInputConsumer>& consumer) {
    std::lock_guard lock{this->consumers_mutex};
    this->consumers_.push_back(consumer);
}

void AudioInput::remove_consumer(const std::shared_ptr<AudioInputConsumer> &target_consumer) {
    std::lock_guard consumer_lock{this->consumers_mutex};

    this->consumers_.erase(std::remove_if(this->consumers_.begin(), this->consumers_.end(), [&](const std::weak_ptr<AudioInputConsumer>& weak_consumer) {
        auto consumer = weak_consumer.lock();
        if(!consumer) {
            return true;
        }

        return consumer == target_consumer;
    }), this->consumers_.end());
}

std::shared_ptr<AudioInputAudioLevelMeter> AudioInput::create_level_meter(bool preprocess) {
    auto level_meter = std::make_shared<AudioInputAudioLevelMeter>();

    {
        std::lock_guard lock{this->consumers_mutex};
        auto& level_meters = preprocess ? this->level_meter_preprocess : this->level_meter_postprocess;
        level_meters.push_back(level_meter);
    }

    return level_meter;
}

void AudioInput::allocate_input_buffer_samples(size_t samples) {
    const auto expected_byte_size = samples * this->channel_count_ * sizeof(float);
    if(expected_byte_size > this->input_buffer.capacity()) {
        log_critical(category::audio, tr("Resampled audio input data would be larger than our input buffer capacity."));
        return;
    }

    if(this->input_buffer.free_count() < expected_byte_size) {
        log_warn(category::audio, tr("Audio input buffer overflow."));

        const auto free_samples = this->input_buffer.free_count() / this->channel_count_ / sizeof(float);
        assert(samples >= free_samples);

        const auto missing_samples = samples - free_samples;
        this->input_buffer.advance_read_ptr(missing_samples * this->channel_count_ * sizeof(float));
    }
}

void AudioInput::consume(const void *input, size_t sample_count, size_t sample_rate, size_t channels) {
    constexpr static auto kTempBufferMaxSampleCount{1024 * 8};
    float temp_buffer[kTempBufferMaxSampleCount];

    /* TODO: Short circuit for silence here */
    if(!this->resampler_ || this->current_input_sample_rate != sample_rate) {
        log_info(category::audio, tr("Input sample rate changed from {} to {}"), this->resampler_ ? this->resampler_->output_rate() : 0, sample_rate);
        this->current_input_sample_rate = sample_rate;

        this->resampler_ = std::make_unique<AudioResampler>(this->sample_rate(), sample_rate, this->channel_count());
        if(!this->resampler_->valid()) {
            log_critical(category::audio, tr("Failed to allocate a new resampler. Audio input will be silent."));
        }
    }

    if(!this->resampler_->valid()) {
        return;
    }

    if(channels != this->channel_count_) {
        if(channels < 1 || channels > 2) {
            log_critical(category::audio, tr("AudioInput received audio data with an unsupported channel count of {}."), channels);
            return;
        }

        if(sample_count * this->channel_count_ > kTempBufferMaxSampleCount) {
            log_critical(category::audio, tr("Received audio chunk bigger than our temp stack buffer. Received {} samples but can hold only {}."), sample_count, kTempBufferMaxSampleCount / this->channel_count_);
            return;
        }

        audio::merge::merge_channels_interleaved(temp_buffer, this->channel_count_, input, channels, sample_count);
        input = temp_buffer;
    }

    const auto expected_size = this->resampler_->estimated_output_size(sample_count);
    this->allocate_input_buffer_samples(expected_size);

    size_t resampled_sample_count{expected_size};
    if(!this->resampler_->process(this->input_buffer.write_ptr(), input, sample_count, resampled_sample_count)) {
        log_error(category::audio, tr("Failed to resample audio input."));
        return;
    }

    this->input_buffer.advance_write_ptr(resampled_sample_count * this->channel_count_ * sizeof(float));
	audio::encode_event_loop->schedule(this->event_loop_entry);
}


void AudioInput::process_audio() {
    const auto chunk_sample_count = (kChunkSizeMs * this->sample_rate_) / 1000;
    while(true) {
        auto available_sample_count = this->input_buffer.fill_count() / this->channel_count_ / sizeof(float);
        if(available_sample_count < chunk_sample_count) {
            break;
        }

        auto input = this->input_buffer.read_ptr();
        /*
         * It's save to mutate the current memory.
         * If overflows occur it could lead to wired artifacts but all memory access is save.
         */
        this->process_audio_chunk((float*) input);
        this->input_buffer.advance_read_ptr(chunk_sample_count * this->channel_count_ * sizeof(float));
    }
}

void AudioInput::process_audio_chunk(float *chunk) {
    constexpr static auto kTempSampleBufferSize{1024 * 8};
    constexpr static auto kMaxChannelCount{32};

    const auto chunk_sample_count = this->chunk_sample_count();
    float temp_sample_buffer[kTempSampleBufferSize];
    float out_sample_buffer[kTempSampleBufferSize];
    assert(memset(temp_sample_buffer, 0, sizeof(float) * kTempSampleBufferSize));
    assert(memset(out_sample_buffer, 0, sizeof(float) * kTempSampleBufferSize));

    AudioInputBufferInfo buffer_info{};
    buffer_info.sample_count = chunk_sample_count;

    this->invoke_level_meter(true, chunk, this->channel_count_, chunk_sample_count);
    if(auto processor{this->audio_processor_}; processor) {
        assert(kTempSampleBufferSize >= chunk_sample_count * this->channel_count_ * sizeof(float));

        audio::deinterleave(temp_sample_buffer, chunk, this->channel_count_, chunk_sample_count);
        webrtc::StreamConfig stream_config{(int) this->sample_rate_, this->channel_count_};

        float* channel_ptr[kMaxChannelCount];
        assert(memset(channel_ptr, 0, sizeof(float*) * kMaxChannelCount));
        for(size_t channel{0}; channel < this->channel_count_; channel++) {
            channel_ptr[channel] = temp_sample_buffer + (channel *  chunk_sample_count);
        }

        auto process_statistics = processor->process_stream(stream_config, channel_ptr);
        if(!process_statistics.has_value()) {
            /* TODO: Some kind of error message? */
            return;
        }

        audio::interleave_vec(out_sample_buffer, channel_ptr, this->channel_count_, chunk_sample_count);
        chunk = out_sample_buffer;

        if(process_statistics->voice_detected.has_value()) {
            buffer_info.vad_detected.emplace(*process_statistics->voice_detected);
        }
    }

    audio::apply_gain(chunk, this->channel_count_, chunk_sample_count, this->volume_);
    this->invoke_level_meter(false, out_sample_buffer, this->channel_count_, chunk_sample_count);

    auto begin = std::chrono::system_clock::now();
    for(const auto& consumer : this->consumers()) {
        consumer->handle_buffer(buffer_info, out_sample_buffer);
    }

    auto end = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
    if(ms > 5) {
        log_warn(category::audio, tr("Processing of audio input needed {}ms. This could be an issue!"), std::chrono::duration_cast<chrono::milliseconds>(end - begin).count());
    }
}

void AudioInput::EventLoopCallback::event_execute(const chrono::system_clock::time_point &point) {
    this->input->process_audio();
}

void AudioInput::invoke_level_meter(bool preprocess, const float *buffer, size_t channel_count, size_t sample_size) {
    std::vector<std::shared_ptr<AudioInputAudioLevelMeter>> level_meters{};
    level_meters.reserve(10);

    {
        std::lock_guard lock{this->consumers_mutex};
        auto& list = preprocess ? this->level_meter_preprocess : this->level_meter_postprocess;
        level_meters.reserve(list.size());

        list.erase(std::remove_if(list.begin(), list.end(), [&](const std::weak_ptr<AudioInputAudioLevelMeter>& weak_meter) {
            auto meter = weak_meter.lock();
            if(!meter) {
                return true;
            }

            level_meters.push_back(meter);
            return false;
        }), list.end());
    }

    for(const auto& level_meter : level_meters) {
        if(!level_meter->active) {
            continue;
        }

        level_meter->analyze_buffer(buffer, channel_count, sample_size);
    }
}