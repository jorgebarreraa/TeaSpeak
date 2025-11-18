#pragma once

#include <mutex>
#include <deque>
#include <memory>
#include <iostream>
#include <functional>
#include <cassert>
#include "./AudioSamples.h"
#include "./driver/AudioDriver.h"
#include "../ring_buffer.h"

#ifdef WIN32
    #define ssize_t int64_t
#endif

namespace tc::audio {
		class AudioOutput;
        class AudioResampler;
        class AudioProcessor;

		enum struct OverflowStrategy {
            ignore,
            discard_buffer_all,
            discard_buffer_half,
            discard_input
		};

        class AudioOutputSource {
			friend class AudioOutput;
			public:
                enum struct BufferState {
                    /* Awaiting enough samples to replay and apply the fadein effect */
                    buffering,
                    /* We have encountered a buffer underflow. Applying fadeout effect and changing state to buffering. */
                    fadeout,
                    /* We're just normally replaying audio */
                    playing
                };

                [[nodiscard]] inline auto channel_count() const -> size_t { return this->channel_count_; }
                [[nodiscard]] inline auto sample_rate() const -> size_t { return this->sample_rate_; }
                [[nodiscard]] inline auto state() const -> BufferState { return this->buffer_state; }

                /**
                 * The maximum amount of samples which could be buffered.
                 * @return
                 */
				[[nodiscard]] inline auto max_supported_buffering() const -> size_t {
                    return this->buffer.capacity() / this->channel_count_ / sizeof(float);
				}

                [[nodiscard]] inline auto max_buffering() const -> size_t {
				    const auto max_samples = this->max_supported_buffering();
				    if(this->max_buffered_samples_ && this->max_buffered_samples_ <= max_samples) {
                        return this->max_buffered_samples_;
				    }

                    return max_samples;
                }

                /**
                 * Sample count which still need to be replayed before newly emplaced buffers will be played.
                 * @return
                 */
                [[nodiscard]] inline size_t currently_buffered_samples() const {
				    return this->buffer.fill_count() / this->channel_count_ / sizeof(float);
				}


				[[nodiscard]] inline size_t min_buffered_samples() const { return this->min_buffered_samples_; }
                [[nodiscard]] inline size_t max_buffered_samples() const { return this->max_buffered_samples_; }

                bool set_min_buffered_samples(size_t /* target samples */);
                bool set_max_buffered_samples(size_t /* target samples */);

				OverflowStrategy overflow_strategy{OverflowStrategy::discard_buffer_half};

				/* if it returns true then the it means that the buffer has been refilled, we have to test again */
				std::function<bool(size_t /* sample count */)> on_underflow;
				std::function<void(size_t /* sample count */)> on_overflow;
				std::function<void()> on_read; /* will be invoked after sample read, e.g. for buffer fullup */

                void clear();
				ssize_t enqueue_samples(const void * /* input buffer */, size_t /* sample count */);
				ssize_t enqueue_samples_no_interleave(const void * /* input buffer */, size_t /* sample count */);

				/* Consume N samples */
                bool pop_samples(void* /* output buffer */, size_t /* sample count */);
			private:
				AudioOutputSource(size_t channel_count, size_t sample_rate, ssize_t max_buffer_sample_count = -1) :
                        channel_count_{channel_count}, sample_rate_{sample_rate},
                        buffer{max_buffer_sample_count == -1 ? channel_count * sample_rate * sizeof(float) : max_buffer_sample_count * channel_count * sizeof(float)}
                {
					this->clear();

					this->fadein_frame_samples_ = sample_rate * 0.02;
                    this->fadeout_frame_samples_ = sample_rate * 0.016;
				}

                size_t const channel_count_;
                size_t const sample_rate_;

				std::recursive_mutex buffer_mutex{};
				BufferState buffer_state{BufferState::buffering};
				tc::ring_buffer buffer;

                size_t min_buffered_samples_{0};
                size_t max_buffered_samples_{0};

				/*
				 * Fadeout and fadein properties.
				 * The fadeout sample count should always be lower than the fade in sample count.
				 */
                size_t fadein_frame_samples_;
				size_t fadeout_frame_samples_;
                size_t fadeout_samples_left{0};

				/* Methods bellow do not acquire the buffer_mutex mutex */
                ssize_t enqueue_samples_(const void * /* input buffer */, size_t /* sample count */);
                bool pop_samples_(void* /* output buffer */, size_t /* sample count */);

                void apply_fadeout();
                void apply_fadein();
		};

        class AudioOutput : public AudioDevicePlayback::Source {
			public:
				AudioOutput(size_t /* channels */, size_t /* rate */);
				virtual ~AudioOutput();

                void set_device(const std::shared_ptr<AudioDevice>& /* device */);
				bool playback(std::string& /* error */);
				void close_device();
                std::shared_ptr<AudioDevice> current_device() { return this->device; }

				std::deque<std::weak_ptr<AudioOutputSource>> sources() {
					std::lock_guard sources_lock{this->sources_mutex};
					return this->sources_;
				}

				std::shared_ptr<AudioOutputSource> create_source(ssize_t /* buffer sample size */ = -1);

				[[nodiscard]] inline size_t channel_count() const { return this->channel_count_; }
				[[nodiscard]] inline size_t sample_rate() const { return this->sample_rate_; }

				[[nodiscard]] inline float volume() const { return this->volume_modifier; }
				inline void set_volume(float value) { this->volume_modifier = value; }

				void register_audio_processor(const std::shared_ptr<AudioProcessor>&);
                bool unregister_audio_processor(const std::shared_ptr<AudioProcessor>&);
			private:
                /* One audio chunk should be 10ms long */
                constexpr static auto kChunkTimeMs{10};

				void fill_buffer(void *, size_t request_sample_count, size_t sample_rate, size_t out_channels) override;
                void fill_buffer_nochecks(void *, size_t request_sample_count, size_t out_channels);
				void fill_chunk_buffer();

				size_t const channel_count_;
				size_t const sample_rate_;

				std::mutex sources_mutex{};
				std::deque<std::weak_ptr<AudioOutputSource>> sources_{};

                std::mutex processors_mutex{};
				std::vector<std::shared_ptr<AudioProcessor>> audio_processors_{};

				std::recursive_mutex device_lock{};
                std::shared_ptr<AudioDevice> device{nullptr};
                std::shared_ptr<AudioDevicePlayback> playback_{nullptr};
                std::unique_ptr<AudioResampler> resampler_{nullptr};

                /* only access there buffers within the audio loop! */
				void** source_merge_buffer{nullptr};
                size_t source_merge_buffer_length{0};

                /*
                 * The chunk buffer will be large enough to hold
                 * a chunk of pcm data with our current configuration.
                 */
                void* chunk_buffer{nullptr};
				size_t chunk_buffer_length{0};
                size_t chunk_buffer_sample_length{0};
				size_t chunk_buffer_sample_offset{0};

				size_t current_output_sample_rate{0};
                size_t current_output_channels{0};

				void ensure_chunk_buffer_space(size_t /* output samples */);
				void cleanup_buffers();

				float volume_modifier{1.f};

                [[nodiscard]] inline auto chunk_local_sample_count() const {
                    assert(this->playback_);
                    return (this->sample_rate_ * kChunkTimeMs) / 1000;
                }
		};
	}