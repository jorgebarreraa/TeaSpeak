#pragma once

#include <memory>
#include <shared_mutex>
#include <optional>
#include <modules/audio_processing/include/audio_processing.h>

namespace tc::audio {
    class AudioProcessor {
        public:
            struct Config : public webrtc::AudioProcessing::Config {
                struct {
                    bool enabled{false};
                } rnnoise;

                int artificial_stream_delay{0};
            };

            struct Stats : public webrtc::AudioProcessingStats {
                // The RNNoise returned sample volume
                absl::optional<float> rnnoise_volume;
            };

            struct ProcessObserver {
                public:
                    virtual void stream_processed(const AudioProcessor::Stats&) = 0;
            };

            AudioProcessor();
            virtual ~AudioProcessor();

            [[nodiscard]] bool initialize();

            [[nodiscard]] Config get_config() const;
            void apply_config(const Config &/* config */);

            [[nodiscard]] AudioProcessor::Stats get_statistics() const;

            void register_process_observer(ProcessObserver* /* observer */);
            /**
             * Unregister a process observer.
             * Note: Never call this within the observer callback!
             *       This will cause a deadlock.
             * @return
             */
            bool unregister_process_observer(ProcessObserver* /* observer */);

            /* 10ms audio chunk */
            [[nodiscard]] std::optional<AudioProcessor::Stats> process_stream( const webrtc::StreamConfig& /* config */, float* const* /* buffer */);

            /**
             * Accepts deinterleaved float audio with the range [-1, 1]. Each element
             * of |data| points to a channel buffer, arranged according to
             * |reverse_config|.
             */
            void analyze_reverse_stream(const float* const* data,
                                        const webrtc::StreamConfig& reverse_config);

        private:
            constexpr static auto kMaxChannelCount{2};

            mutable std::shared_mutex processor_mutex{};

            Config current_config{};
            std::vector<ProcessObserver*> process_observer{};
            webrtc::AudioProcessing* processor{nullptr};

            absl::optional<float> rnnoise_volume{};
            std::array<void*, kMaxChannelCount> rnnoise_processor{nullptr};

            [[nodiscard]] AudioProcessor::Stats get_statistics_unlocked() const;
            void apply_config_unlocked(const Config &/* config */);
    };
}