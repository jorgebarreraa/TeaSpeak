#pragma once

#include <memory>
#include <array>
#include <vector>
#include <mutex>
#include <portaudio.h>
#include "./AudioDriver.h"

namespace tc::audio::pa {
    class PortAudioPlayback : public AudioDevicePlayback {
        public:
            static constexpr auto kDefaultChannelCount{2};
            static constexpr std::array<size_t, 2> kSupportedSampleRates{48000, 44100};

            explicit PortAudioPlayback(PaDeviceIndex index, const PaDeviceInfo* info);
            virtual ~PortAudioPlayback();

            [[nodiscard]] size_t current_sample_rate() const override;
        protected:
            bool impl_start(std::string& /* error */) override;
            void impl_stop() override;

        private:
            void write_callback(void *output, unsigned long frameCount, const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags);

            PaDeviceIndex index;
            const PaDeviceInfo* info;
            PaStream* stream{nullptr};

            size_t source_sample_rate{0};
            size_t source_channel_count{0};
    };

    class PortAudioRecord : public AudioDeviceRecord {
        public:
            static constexpr auto kDefaultChannelCount{2};
            static constexpr std::array<size_t, 2> kSupportedSampleRates{48000, 44100};

            explicit PortAudioRecord(PaDeviceIndex index, const PaDeviceInfo* info);
            virtual ~PortAudioRecord();

            [[nodiscard]] size_t sample_rate() const override;
        protected:
            bool impl_start(std::string& /* error */) override;
            void impl_stop() override;

        private:
            void read_callback(const void *input, unsigned long frameCount, const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags);

            size_t source_sample_rate{0};
            size_t source_channel_count{0};

            PaDeviceIndex index;
            const PaDeviceInfo* info;
            PaStream* stream{nullptr};
    };


    struct PaAudioDevice : public AudioDevice {
        public:
            explicit PaAudioDevice(PaDeviceIndex index, const PaDeviceInfo* info, const PaHostApiInfo* host);
            virtual ~PaAudioDevice() = default;

            [[nodiscard]] std::string id() const override;
            [[nodiscard]] std::string name() const override;
            [[nodiscard]] std::string driver() const override;

            [[nodiscard]] bool is_input_supported() const override;
            [[nodiscard]] bool is_output_supported() const override;

            [[nodiscard]] bool is_input_default() const override;
            [[nodiscard]] bool is_output_default() const override;

            [[nodiscard]] std::shared_ptr<AudioDevicePlayback> playback() override;
            [[nodiscard]] std::shared_ptr<AudioDeviceRecord> record() override;
        private:
            const PaDeviceIndex _index;
            const PaDeviceInfo* _info;
            const PaHostApiInfo* _host_info;

            std::mutex io_lock{};
            std::shared_ptr<PortAudioPlayback> _playback;
            std::shared_ptr<PortAudioRecord> _record;
    };


    extern void initialize();
    extern void finalize();

    extern std::deque<std::shared_ptr<PaAudioDevice>> devices();
}