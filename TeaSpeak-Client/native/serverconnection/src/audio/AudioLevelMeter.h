#pragma once

#include "./driver/AudioDriver.h"

namespace tc::audio {
    class AudioInput;

    /**
     * Note: Within the observer callback no methods of the level meter should be called nor the level meter should be destructed.
     */
    class AbstractAudioLevelMeter {
        public:
            struct Observer {
                public:
                    virtual void input_level_changed(float /* new level */) = 0;
            };

            explicit AbstractAudioLevelMeter();
            virtual ~AbstractAudioLevelMeter();


            [[nodiscard]] virtual bool start(std::string& /* error */) = 0;
            virtual void stop() = 0;
            [[nodiscard]] virtual bool running() const = 0;

            [[nodiscard]] inline float current_volume() const { return this->current_audio_volume; }

            void register_observer(Observer* /* observer */);
            bool unregister_observer(Observer* /* observer */);
        protected:
            mutable std::mutex mutex{};
            std::vector<Observer*> registered_observer{};

            float current_audio_volume{0.f};

            void analyze_buffer(const float* /* buffer */, size_t /* channel count */, size_t /* sample count */);
    };

    /**
     * This audio level meter operates directly on the raw input device without any processing.
     */
    class InputDeviceAudioLevelMeter : public AbstractAudioLevelMeter, public AudioDeviceRecord::Consumer {
        public:
            explicit InputDeviceAudioLevelMeter(std::shared_ptr<AudioDevice> /* target device */);

            ~InputDeviceAudioLevelMeter() override;

            bool start(std::string &string) override;

            void stop() override;

            bool running() const override;

        private:
            std::shared_ptr<AudioDevice> target_device{};
            std::shared_ptr<AudioDeviceRecord> recorder_instance{};

            void consume(const void *, size_t, size_t, size_t) override;
    };

    class AudioInputAudioLevelMeter : public AbstractAudioLevelMeter {
        friend class AudioInput;

        public:
            AudioInputAudioLevelMeter() = default;
            ~AudioInputAudioLevelMeter() override = default;

            bool start(std::string &string) override;
            void stop() override;
            bool running() const override;

        private:
            bool active{false};
    };
}