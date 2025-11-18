#pragma once

#include <soundio/soundio.h>
#include <memory>
#include <array>
#include <vector>
#include <mutex>
#include "./AudioDriver.h"

namespace tc::audio {
    struct BackendPriority {
        static constexpr std::array<int, (int) SoundIoBackendDummy + 1> mapping{
                /* SoundIoBackendNone */ -100,
                /* SoundIoBackendJack */ 100,
                /* SoundIoBackendPulseAudio */ 90,
                /* SoundIoBackendAlsa */ 50,
                /* SoundIoBackendCoreAudio */ 100,
                /* SoundIoBackendWasapi */ 100,
                /* SoundIoBackendDummy */ 0
        };

        [[nodiscard]] static constexpr auto priority(SoundIoBackend backend) {
            if(backend >= mapping.size())
                return 0;
            return mapping[backend];
        }
    };

    constexpr std::array kSampleRateOrder{48000, 44100};
    constexpr auto kDefaultSampleRate{kSampleRateOrder[0]};

    class SoundIOPlayback : public AudioDevicePlayback {
        public:
            constexpr static auto kChunkTime{0.005};

            explicit SoundIOPlayback(struct ::SoundIoDevice* /* handle */);
            virtual ~SoundIOPlayback();

            [[nodiscard]] size_t sample_rate() const override;
        protected:
            bool impl_start(std::string& /* error */) override;
            void impl_stop() override;

        private:
            size_t _sample_rate;
            bool stream_invalid{false};
            bool have_underflow{false};
            struct ::SoundIoDevice* device_handle{nullptr};
            struct ::SoundIoOutStream* stream{nullptr};

            struct ::SoundIoRingBuffer* buffer{nullptr};

#ifdef WIN32
            std::mutex write_mutex{};
            std::condition_variable write_cv{};
            bool write_exit{false};

            std::chrono::system_clock::time_point next_write{};

            std::chrono::system_clock::time_point last_stats{};
            size_t samples{0};

            bool priority_boost{false};
#endif

            void write_callback(int frame_count_min, int frame_count_max);
    };

    class SoundIORecord : public AudioDeviceRecord {
        public:
            constexpr static auto kChunkSize{960};

            explicit SoundIORecord(struct ::SoundIoDevice* /* handle */);
            virtual ~SoundIORecord();

            [[nodiscard]] size_t sample_rate() const override;
        protected:
            bool impl_start(std::string& /* error */) override;
            void impl_stop() override;

        private:
            size_t _sample_rate;
            struct ::SoundIoDevice* device_handle{nullptr};
            struct ::SoundIoInStream* stream{nullptr};

            size_t failed_count{0};
            std::thread fail_recover_thread{};
            std::mutex fail_cv_mutex{};
            std::condition_variable fail_cv{};

            struct ::SoundIoRingBuffer* buffer{nullptr};

            bool stop_requested{false}; /* protected via fail_cv_mutex */
            void execute_recovery();
            void read_callback(int frame_count_min, int frame_count_max);
    };

    class SoundIODevice : public AudioDevice {
        public:
            explicit SoundIODevice(struct ::SoundIoDevice* /* handle */, std::string /* driver */, bool /* default */, bool /* owned */);
            virtual ~SoundIODevice();

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
            std::string _device_id{};

            std::string driver_name{};
            struct ::SoundIoDevice* device_handle{nullptr};
            bool _default{false};

            std::mutex io_lock{};
            std::shared_ptr<SoundIOPlayback> _playback;
            std::shared_ptr<SoundIORecord> _record;
    };

    class SoundIOBackendHandler {
        public:
            /* its sorted by priority */
            static std::vector<std::shared_ptr<SoundIOBackendHandler>> all_backends() {
                std::lock_guard lock{backend_lock};
                return backends;;
            }
            static std::shared_ptr<SoundIOBackendHandler> get_backend(SoundIoBackend backend);

            static void initialize_all();
            static void connect_all();
            static void shutdown_all();

            explicit SoundIOBackendHandler(SoundIoBackend backed);
            virtual ~SoundIOBackendHandler();

            bool initialize(std::string& error);
            void shutdown();

            bool connect(std::string& /* error */, bool /* enforce */ = false);
            [[nodiscard]] inline bool connected() const { return this->_connected; }
            void disconnect();

            [[nodiscard]] inline int priority() const { return BackendPriority::priority(this->backend); }
            [[nodiscard]] inline const char* name() const { return soundio_backend_name(this->backend); }

            [[nodiscard]] inline std::vector<std::shared_ptr<SoundIODevice>> input_devices() const {
                std::lock_guard lock{this->device_lock};
                return this->cached_input_devices;
            }

            [[nodiscard]] inline std::vector<std::shared_ptr<SoundIODevice>> output_devices() const {
                std::lock_guard lock{this->device_lock};
                return this->cached_output_devices;
            }

            [[nodiscard]] inline std::shared_ptr<SoundIODevice> default_input_device() const {
                return this->_default_input_device;
            }

            [[nodiscard]] inline std::shared_ptr<SoundIODevice> default_output_device() const {
                return this->_default_output_device;
            }

            const SoundIoBackend backend;
        private:
            static std::mutex backend_lock;
            static std::vector<std::shared_ptr<SoundIOBackendHandler>> backends;

            void handle_backend_disconnect(int /* error */);
            void handle_device_change();

            bool _connected{false};
            struct SoundIo* soundio_handle{nullptr};

            mutable std::mutex device_lock{};
            std::vector<std::shared_ptr<SoundIODevice>> cached_input_devices{};
            std::vector<std::shared_ptr<SoundIODevice>> cached_output_devices{};
            std::shared_ptr<SoundIODevice> _default_output_device{nullptr};
            std::shared_ptr<SoundIODevice> _default_input_device{nullptr};
    };
}