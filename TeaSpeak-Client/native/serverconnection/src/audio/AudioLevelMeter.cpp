//
// Created by WolverinDEV on 28/03/2021.
//

#include <cassert>
#include "./AudioLevelMeter.h"
#include "./processing/AudioVolume.h"
#include "../logger.h"

using namespace tc::audio;

AbstractAudioLevelMeter::AbstractAudioLevelMeter() {
    log_allocate("AbstractAudioLevelMeter", this);
}

AbstractAudioLevelMeter::~AbstractAudioLevelMeter() {
    log_free("AbstractAudioLevelMeter", this);
}

void AbstractAudioLevelMeter::register_observer(Observer *observer) {
    std::lock_guard lock{this->mutex};
    this->registered_observer.push_back(observer);
}

bool AbstractAudioLevelMeter::unregister_observer(Observer *observer) {
    std::lock_guard lock{this->mutex};
    auto index = std::find(this->registered_observer.begin(), this->registered_observer.end(), observer);
    if(index == this->registered_observer.end()) {
        return false;
    }

    this->registered_observer.erase(index);
    return true;
}

void AbstractAudioLevelMeter::analyze_buffer(const float *buffer, size_t channel_count, size_t sample_count) {
    auto volume = audio::audio_buffer_level(buffer, channel_count, sample_count);

    std::lock_guard lock{this->mutex};
    if(volume == this->current_audio_volume) {
        return;
    }
    this->current_audio_volume = volume;

    for(auto& observer : this->registered_observer) {
        observer->input_level_changed(volume);
    }
}

/* For a target device */
InputDeviceAudioLevelMeter::InputDeviceAudioLevelMeter(std::shared_ptr<AudioDevice> target_device) : target_device{std::move(target_device)} {
    assert(this->target_device);
}

InputDeviceAudioLevelMeter::~InputDeviceAudioLevelMeter() {
    this->stop();
}

bool InputDeviceAudioLevelMeter::start(std::string &error) {
    std::lock_guard lock{this->mutex};
    if(this->recorder_instance) {
        return true;
    }

    this->recorder_instance = this->target_device->record();
    if(!this->recorder_instance) {
        error = tr("failed to create device recorder");
        return false;
    }

    if(!this->recorder_instance->start(error)) {
        this->recorder_instance = nullptr;
        return false;
    }

    this->recorder_instance->register_consumer(this);
    return true;
}

bool InputDeviceAudioLevelMeter::running() const {
    std::lock_guard lock{this->mutex};
    return this->recorder_instance != nullptr;
}

void InputDeviceAudioLevelMeter::stop() {
    std::unique_lock lock{this->mutex};
    auto recorder_instance_ = std::exchange(this->recorder_instance, nullptr);
    if(recorder_instance_) {
        lock.unlock();
        /* The recorder might wait for us in this right moment */
        recorder_instance_->remove_consumer(this);
        recorder_instance_->stop_if_possible();
    }
}

/* Note the parameter order! */
void InputDeviceAudioLevelMeter::consume(const void *buffer, size_t sample_count, size_t /* sample rate */, size_t channel_count) {
    this->analyze_buffer((const float*) buffer, channel_count, sample_count);
}

bool AudioInputAudioLevelMeter::start(std::string &) {
    std::lock_guard lock{this->mutex};
    this->active = true;
    return true;
}

void AudioInputAudioLevelMeter::stop() {
    std::lock_guard lock{this->mutex};
    this->active = false;
}

bool AudioInputAudioLevelMeter::running() const {
    std::lock_guard lock{this->mutex};
    return this->active;
}