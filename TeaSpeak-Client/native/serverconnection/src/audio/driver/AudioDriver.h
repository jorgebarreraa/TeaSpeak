#pragma once

#include <mutex>
#include <deque>
#include <memory>
#include <iostream>
#include <functional>

namespace tc::audio {
    class AudioDeviceRecord {
        public:
            class Consumer {
                public:
                    virtual void consume(const void* /* buffer */, size_t /* samples */, size_t /* sample rate */, size_t /* channel count */) = 0;
            };

            [[nodiscard]] virtual size_t sample_rate() const = 0;

            [[nodiscard]] bool start(std::string& /* error */);
            void stop_if_possible();
            void stop();

            [[nodiscard]] inline std::vector<Consumer*> consumer() {
                std::lock_guard lock{this->consumer_lock};
                return this->_consumers;
            }

            void register_consumer(Consumer* /* source */);
            void remove_consumer(Consumer* /* source */);
        protected:
            virtual bool impl_start(std::string& /* error */) = 0;
            virtual void impl_stop() = 0;

            std::timed_mutex state_lock{};
            bool running{false};
            bool stream_invalid{false};

            std::mutex consumer_lock{};
            std::vector<Consumer*> _consumers{};
    };

    class AudioDevicePlayback {
        public:
            class Source {
                public:
                    virtual void fill_buffer(void* /* target */, size_t /* samples */, size_t /* sample rate */, size_t /* channel count */) = 0;
            };

            /**
             * Get the current playback sample rate.
             * Note: If the playback hasn't been started it might be zero.
             */
            [[nodiscard]] virtual size_t current_sample_rate() const = 0;

            [[nodiscard]] bool start(std::string& /* error */);
            void stop_if_possible();
            void stop();

            [[nodiscard]] inline std::vector<Source*> sources() {
                std::lock_guard lock{this->source_lock};
                return this->_sources;
            }
            void register_source(Source* /* source */);

            /* will and must be blocking until audio callback is done */
            void remove_source(Source* /* source */);

        protected:
            virtual bool impl_start(std::string& /* error */) = 0;
            virtual void impl_stop() = 0;

            void fill_buffer(void* /* target */, size_t /* samples */, size_t /* sample rate */, size_t /* channel count */);

            std::mutex state_lock{};
            bool running{false};

            std::mutex source_lock{};
            std::vector<Source*> _sources{};
    };

    class AudioDevice {
        public:
            /* information */
            [[nodiscard]] virtual std::string id() const = 0;
            [[nodiscard]] virtual std::string name() const = 0;
            [[nodiscard]] virtual std::string driver() const = 0;

            [[nodiscard]] virtual bool is_input_supported() const = 0;
            [[nodiscard]] virtual bool is_output_supported() const = 0;

            [[nodiscard]] virtual bool is_input_default() const = 0;
            [[nodiscard]] virtual bool is_output_default() const = 0;

            [[nodiscard]] virtual std::shared_ptr<AudioDevicePlayback> playback() = 0;
            [[nodiscard]] virtual std::shared_ptr<AudioDeviceRecord> record() = 0;
    };

    typedef std::function<void()> initialize_callback_t;

    extern void finalize();
    extern void initialize(const initialize_callback_t& /* callback */ = []{});
    extern void await_initialized();
    extern bool initialized();

    extern std::deque<std::shared_ptr<AudioDevice>> devices();
    extern std::shared_ptr<AudioDevice> find_device_by_id(const std::string_view& /* id */, bool /* input */);
}