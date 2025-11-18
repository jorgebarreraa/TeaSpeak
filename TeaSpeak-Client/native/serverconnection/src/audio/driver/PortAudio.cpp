//
// Created by WolverinDEV on 13/02/2020.
//

#include "PortAudio.h"
#include "../../logger.h"
#include <portaudio.h>

namespace tc::audio::pa {
    std::mutex _audio_devices_lock;
    std::unique_ptr<std::deque<std::shared_ptr<PaAudioDevice>>> _audio_devices{};

    void initialize_devices() {
        std::lock_guard dev_lock{_audio_devices_lock};
        _audio_devices = std::make_unique<std::deque<std::shared_ptr<PaAudioDevice>>>();

        /* query devices */
        auto device_count = Pa_GetDeviceCount();
        if(device_count < 0) {
            log_error(category::audio, tr("Pa_GetDeviceCount() returned {}"), device_count);
            return;
        }

        for(PaDeviceIndex device_index = 0; device_index < device_count; device_index++) {
            auto device_info = Pa_GetDeviceInfo(device_index);
            if(!device_info) {
                log_warn(category::audio, tr("Pa_GetDeviceInfo(...) failed for device {}"), device_index);
                continue;
            }

            auto device_host_info = Pa_GetHostApiInfo(device_info->hostApi);
            if(!device_host_info) {
                log_warn(category::audio, tr("Pa_GetHostApiInfo(...) failed for device {} with host api {}"), device_index, device_info->hostApi);
                continue;
            }

            _audio_devices->push_back(std::make_shared<PaAudioDevice>(device_index, device_info, device_host_info));
        }

    }

    void initialize() {
        Pa_Initialize();
        initialize_devices();
    }

    void finalize() {
        Pa_Terminate();
    }

    std::deque<std::shared_ptr<PaAudioDevice>> devices() {
        std::lock_guard dev_lock{_audio_devices_lock};
        return *_audio_devices;
    }

    /* device class */
    PaAudioDevice::PaAudioDevice(PaDeviceIndex index, const PaDeviceInfo* info, const PaHostApiInfo* host)
        : _index{index}, _info{info}, _host_info{host} { }

    std::string PaAudioDevice::id() const {
        return std::string{this->_info->name} + "_" + this->_host_info->name;
    }

    std::string PaAudioDevice::name() const {
        return this->_info->name;
    }

    std::string PaAudioDevice::driver() const {
        return this->_host_info->name;
    }

    bool PaAudioDevice::is_input_supported() const {
        return this->_info->maxInputChannels > 0;
    }

    bool PaAudioDevice::is_output_supported() const {
        return this->_info->maxOutputChannels > 0;
    }

    bool PaAudioDevice::is_input_default() const {
        return this->_index == Pa_GetDefaultInputDevice();
    }

    bool PaAudioDevice::is_output_default() const {
        return this->_index == Pa_GetDefaultOutputDevice();
    }

    std::shared_ptr<AudioDevicePlayback> PaAudioDevice::playback() {
        if(!this->is_output_supported()) {
            log_warn(category::audio, tr("Tried to create playback manager for device which does not supports it."));
            return nullptr;
        }

        std::lock_guard lock{this->io_lock};
        if(!this->_playback) {
            this->_playback = std::make_shared<PortAudioPlayback>(this->_index, this->_info);
        }
        return this->_playback;
    }

    std::shared_ptr<AudioDeviceRecord> PaAudioDevice::record() {
        if(!this->is_input_supported()) {
            log_warn(category::audio, tr("Tried to create record manager for device which does not supports it."));
            return nullptr;
        }

        std::lock_guard lock{this->io_lock};
        if(!this->_record) {
            this->_record = std::make_shared<PortAudioRecord>(this->_index, this->_info);
        }
        return this->_record;
    }
}