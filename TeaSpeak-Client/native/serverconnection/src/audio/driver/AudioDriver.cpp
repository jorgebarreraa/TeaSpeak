//
// Created by wolverindev on 07.02.20.
//

#include <thread>
#include <condition_variable>
#include "../../logger.h"
#include "../../thread_helper.h"
#include "../AudioMerger.h"
#include "./AudioDriver.h"

#ifdef HAVE_SOUNDIO
    #include "./SoundIO.h"
#else
    #include "PortAudio.h"
#endif

using namespace tc::audio;

namespace tc::audio {
    std::deque<std::shared_ptr<AudioDevice>> devices() {
        std::deque<std::shared_ptr<AudioDevice>> result{};
#ifdef HAVE_SOUNDIO
        for(auto& backend : SoundIOBackendHandler::all_backends()) {
            auto input_devices = backend->input_devices();
            auto output_devices = backend->output_devices();

            result.insert(result.end(), input_devices.begin(), input_devices.end());
            result.insert(result.end(), output_devices.begin(), output_devices.end());
        }
#else
        auto devices = pa::devices();
        result.insert(result.end(), devices.begin(), devices.end());
#endif

        return result;
    }

    std::shared_ptr<AudioDevice> find_device_by_id(const std::string_view& id, bool input) {
#ifdef HAVE_SOUNDIO
        for(auto& backend : SoundIOBackendHandler::all_backends()) {
            for(auto& dev : input ? backend->input_devices() : backend->output_devices())
                if(dev->id() == id)
                    return dev;
        }
#else
        for(auto& device : devices()) {
            if(device->id() == id && (input ? device->is_input_supported() : device->is_output_supported())) {
                return device;
            }
        }
#endif
        return nullptr;
    }

    std::mutex initialize_lock{};
    std::deque<initialize_callback_t> initialize_callbacks{};
    int initialize_state{0}; /* 0 := not initialized | 1 := initialized | 2 := initializing */

#ifdef HAVE_SOUNDIO
    void _initialize() {
        SoundIOBackendHandler::initialize_all();
        SoundIOBackendHandler::connect_all();
    }

    void _finalize() {
        SoundIOBackendHandler::shutdown_all();
    }
#else
    void _initialize() {
        pa::initialize();
    }

    void _finalize() {
        pa::finalize();
    }
#endif

    void initialize(const initialize_callback_t& callback) {
        {
            std::unique_lock init_lock{initialize_lock};
            if(initialize_state == 2) {
                if(callback) {
                    initialize_callbacks.push_back(callback);
                }
                return;
            } else if(initialize_state == 1) {
                init_lock.unlock();
                callback();
                return;
            } else if(initialize_state != 0) {
                init_lock.unlock();
                callback();
                log_warn(category::audio, tr("Invalid initialize state ({})"), initialize_state);
                return;
            }

            initialize_state = 2;
        }
        std::thread init_thread([]{
            _initialize();

            std::unique_lock lock{initialize_lock};
            auto callbacks = std::move(initialize_callbacks);
            initialize_state = 1;
            lock.unlock();

            for(auto& callback : callbacks) {
                callback();
            }
        });
        threads::name(init_thread, tr("audio init"));
        init_thread.detach();
    }

    void await_initialized() {
        std::condition_variable cv{};
        std::mutex m{};

        std::unique_lock init_lock{initialize_lock};
        if(initialize_state != 2) {
            return;
        }
        initialize_callbacks.emplace_back([&]{ cv.notify_all(); });
        init_lock.unlock();

        std::unique_lock m_lock{m};
        cv.wait(m_lock);
    }

    bool initialized() {
        std::unique_lock init_lock{initialize_lock};
        return initialize_state == 1;
    }

    void finalize() {
        await_initialized();
        _finalize();
    }

    bool AudioDevicePlayback::start(std::string &error) {
        std::lock_guard lock{this->state_lock};
        if(this->running) {
            return true;
        }

        if(!this->impl_start(error)) {
            log_error(category::audio, tr("Failed to start playback: {}"), error);
            return false;
        }
        this->running = true;
        return true;
    }

    void AudioDevicePlayback::stop_if_possible() {
        std::lock_guard lock{this->state_lock};
        {
            std::lock_guard s_lock{this->source_lock};
            if(!this->_sources.empty()) return;
        }

        this->impl_stop();
        this->running = false;
    }

    void AudioDevicePlayback::stop() {
        std::lock_guard lock{this->state_lock};
        if(this->running) return;

        this->impl_stop();
        this->running = false;
    }

    void AudioDevicePlayback::register_source(Source* source) {
        std::lock_guard s_lock{this->source_lock};
        this->_sources.push_back(source);
    }

    void AudioDevicePlayback::remove_source(Source* source) {
        std::lock_guard s_lock{this->source_lock};
        auto index = find(this->_sources.begin(), this->_sources.end(), source);
        if(index == this->_sources.end()) return;

        this->_sources.erase(index);
    }

#define TMP_BUFFER_SIZE (4096 * 16) /* 64k */
    void AudioDevicePlayback::fill_buffer(void *buffer, size_t samples, size_t sample_rate, size_t channels) {
        std::lock_guard lock{this->source_lock};

        if(!buffer) {
            for(auto& source : this->_sources) {
                source->fill_buffer(nullptr, samples, sample_rate, channels);
            }
            return;
        }

        const auto size = this->_sources.size();
        if(size == 1) {
            this->_sources.front()->fill_buffer(buffer, samples, sample_rate, channels);
        } else if(size > 1) {
            this->_sources.front()->fill_buffer(buffer, samples, sample_rate, channels);
            uint8_t tmp_buffer[TMP_BUFFER_SIZE];
            if(sizeof(float) * samples * channels > TMP_BUFFER_SIZE) {
                log_warn(category::audio, tr("Dropping input source data because of too small merge buffer"));
                return;
            }

            for(auto it = this->_sources.begin() + 1; it != this->_sources.end(); it++) {
                (*it)->fill_buffer(tmp_buffer, samples, sample_rate, channels);
                merge::merge_sources(buffer, buffer, tmp_buffer, channels, samples);
            }
        } else {
            memset(buffer, 0, samples * channels * sizeof(float));
        }
    }

    bool AudioDeviceRecord::start(std::string &error) {
        std::lock_guard lock{this->state_lock};
        if(this->running && !this->stream_invalid) {
            return true;
        }

        if(this->stream_invalid) {
            this->impl_stop();
            this->running = false;
            this->stream_invalid = false;
        }

        if(!this->impl_start(error)) {
            log_error(category::audio, tr("Failed to start record: {}"), error);
            return false;
        }
        this->running = true;
        return true;
    }

    void AudioDeviceRecord::stop_if_possible() {
        std::lock_guard lock{this->state_lock};
        {
            std::lock_guard s_lock{this->consumer_lock};
            if(!this->_consumers.empty()) {
                return;
            }
        }

        this->impl_stop();
        this->running = false;
        this->stream_invalid = false;
    }

    void AudioDeviceRecord::stop() {
        std::lock_guard lock{this->state_lock};
        if(!this->running) {
            return;
        }

        this->impl_stop();
        this->running = false;
        this->stream_invalid = false;
    }

    void AudioDeviceRecord::register_consumer(Consumer* source) {
        std::lock_guard s_lock{this->consumer_lock};
        this->_consumers.push_back(source);
    }

    void AudioDeviceRecord::remove_consumer(Consumer* source) {
        std::lock_guard s_lock{this->consumer_lock};
        auto index = find(this->_consumers.begin(), this->_consumers.end(), source);
        if(index == this->_consumers.end()) {
            return;
        }

        this->_consumers.erase(index);
    }
}