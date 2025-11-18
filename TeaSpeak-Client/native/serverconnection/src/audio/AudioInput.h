#pragma once

#include <mutex>
#include <deque>
#include <memory>
#include <iostream>
#include <functional>
#include <optional>
#include "./AudioSamples.h"
#include "./driver/AudioDriver.h"
#include "../ring_buffer.h"
#include "../EventLoop.h"

namespace tc::audio {
    class AudioInput;
    class AudioReframer;
    class AudioResampler;
    class AudioProcessor;
    class AudioInputAudioLevelMeter;

    struct AudioInputBufferInfo {
        size_t sample_count{0};
        std::optional<bool> vad_detected{};
    };

    struct AudioInputConsumer {
        virtual void handle_buffer(const AudioInputBufferInfo& /* info */, const float* /* buffer */) = 0;
    };

    class AudioInput : public AudioDeviceRecord::Consumer {
        public:
            AudioInput(size_t /* channels */, size_t /* sample rate */);
            virtual ~AudioInput();

            void set_device(const std::shared_ptr<AudioDevice>& /* device */);
            [[nodiscard]] std::shared_ptr<AudioDevice> current_device() const { return this->input_device; }
            void close_device();

            [[nodiscard]] bool record(std::string& /* error */);
            [[nodiscard]] bool recording();
            void stop();

            [[nodiscard]] std::vector<std::shared_ptr<AudioInputConsumer>> consumers();
            void register_consumer(const std::shared_ptr<AudioInputConsumer>& /* consumer */);
            void remove_consumer(const std::shared_ptr<AudioInputConsumer>& /* consumer */);

            [[nodiscard]] std::shared_ptr<AudioInputAudioLevelMeter> create_level_meter(bool /* pre process */);

            [[nodiscard]] inline auto audio_processor() { return this->audio_processor_; }

            [[nodiscard]] inline size_t channel_count() const { return this->channel_count_; }
            [[nodiscard]] inline size_t sample_rate() const { return this->sample_rate_; }

            [[nodiscard]] inline float volume() const { return this->volume_; }
            inline void set_volume(float value) { this->volume_ = value; }
        private:
            constexpr static auto kInputBufferCapacityMs{750};
            constexpr static auto kChunkSizeMs{10}; /* Must be 10ms else the audio processor will fuck up */

            struct EventLoopCallback : public event::EventEntry {
                AudioInput* const input;

                explicit EventLoopCallback(AudioInput* input) : input{input} {}
                void event_execute(const std::chrono::system_clock::time_point &point) override;
            };

            struct AudioInitializeHook {
                std::mutex mutex{};
                AudioInput* input{nullptr};
            };

            void consume(const void *, size_t, size_t, size_t) override;
            void process_audio();
            void process_audio_chunk(float *);

            size_t const channel_count_;
            size_t const sample_rate_;

            std::mutex consumers_mutex{};
            std::deque<std::weak_ptr<AudioInputConsumer>> consumers_{};
            std::deque<std::weak_ptr<AudioInputAudioLevelMeter>> level_meter_preprocess{};
            std::deque<std::weak_ptr<AudioInputAudioLevelMeter>> level_meter_postprocess{};

            std::shared_ptr<AudioInitializeHook> initialize_hook_handle{};

            std::shared_ptr<EventLoopCallback> event_loop_entry{};
            std::shared_ptr<AudioProcessor> audio_processor_{};
            ring_buffer input_buffer;

            std::recursive_mutex input_source_lock{};
            std::shared_ptr<AudioDeviceRecord> input_recorder{};
            std::unique_ptr<AudioResampler> resampler_{nullptr};
            std::shared_ptr<AudioDevice> input_device{};

            size_t current_input_sample_rate{0};

            float volume_{1.f};

            void allocate_input_buffer_samples(size_t /* sample count */);
            [[nodiscard]] inline auto chunk_sample_count() const { return (kChunkSizeMs * this->sample_rate_) / 1000; }

            void invoke_level_meter(bool /* preprocess */, const float* /* buffer */, size_t /* channel count */, size_t /* sample size */);
    };
}