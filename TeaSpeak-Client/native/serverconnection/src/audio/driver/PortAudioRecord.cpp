//
// Created by WolverinDEV on 13/02/2020.
//


#include <algorithm>
#include <cmath>
#include "./PortAudio.h"
#include "../../logger.h"

using namespace tc::audio::pa;

PortAudioRecord::PortAudioRecord(PaDeviceIndex index, const PaDeviceInfo *info) : index{index}, info{info} {}
PortAudioRecord::~PortAudioRecord() {
    this->stop();
}

bool PortAudioRecord::impl_start(std::string &error) {
    {
        auto device_info = Pa_GetDeviceInfo(this->index);
        if(this->info != device_info) {
            error = "invalid info pointer";
            return false;
        }
    }

    const auto proxied_read_callback = [](
            const void *input, void *output,
            unsigned long frameCount,
            const PaStreamCallbackTimeInfo* timeInfo,
            PaStreamCallbackFlags statusFlags,
            void *userData) {
        assert(input);

        auto recorder = reinterpret_cast<PortAudioRecord*>(userData);
        assert(recorder);

        recorder->read_callback(input, frameCount, timeInfo, statusFlags);
        return 0;
    };

    PaStreamParameters parameters{};
    memset(&parameters, 0, sizeof(parameters));
    if(this->info->maxInputChannels < kDefaultChannelCount) {
        parameters.channelCount = (int) 1;
        this->source_channel_count = 1;
    } else {
        parameters.channelCount = (int) kDefaultChannelCount;
        this->source_channel_count = kDefaultChannelCount;
    }

    parameters.device = this->index;
    parameters.sampleFormat = paFloat32;
    parameters.suggestedLatency = this->info->defaultLowInputLatency;

    for(const auto& sample_rate : kSupportedSampleRates) {
        auto err = Pa_OpenStream(
                &this->stream,
                &parameters,
                nullptr,
                (double) sample_rate,
                paFramesPerBufferUnspecified,
                paClipOff,
                proxied_read_callback,
                this
        );
        log_debug(category::audio, "Open result for record device {} (MaxChannels: {}, Channels: {}, Sample rate: {}): {}", this->info->name, this->info->maxInputChannels, this->source_channel_count, sample_rate, err);

        if(err == paNoError) {
            this->source_sample_rate = sample_rate;
            break;
        }

        this->stream = nullptr;
        if(err == paInvalidSampleRate) {
            /* Try next sample rate */
            continue;
        }

        error = std::string{Pa_GetErrorText(err)} + " (open stream: " + std::to_string(err) + ")";
        return false;
    }

    if(!this->stream) {
        error = "no supported sample rate found";
        return false;
    }

    auto err = Pa_StartStream(this->stream);
    if(err != paNoError) {
        error = std::string{Pa_GetErrorText(err)} + "(start stream: " + std::to_string(err) + ")";
        err = Pa_CloseStream(this->stream);
        if(err != paNoError) {
            log_critical(category::audio, tr("Failed to close opened pa stream. This will cause memory leaks. Error: {}/{}"), err, Pa_GetErrorText(err));
        }
        return false;
    }
    
    log_debug(category::audio, tr("Opened audio record stream for {} ({})"), this->info->name, Pa_GetHostApiInfo(this->info->hostApi)->name);
    return true;
}

void PortAudioRecord::impl_stop() {
    if(Pa_IsStreamActive(this->stream))
        Pa_AbortStream(this->stream);

    auto error = Pa_CloseStream(this->stream);
    if(error != paNoError) {
        log_error(category::audio, tr("Failed to close PA stream: {}"), error);
    } else {
        log_debug(category::audio, tr("Closed audio record stream for {} ({})"), this->info->name, Pa_GetHostApiInfo(this->info->hostApi)->name);
    }
    this->stream = nullptr;
}

size_t PortAudioRecord::sample_rate() const {
    return this->source_sample_rate;
}

void PortAudioRecord::read_callback(const void *input, unsigned long frameCount,
                                    const PaStreamCallbackTimeInfo *timeInfo, PaStreamCallbackFlags statusFlags) {
    std::lock_guard consumer_lock{this->consumer_lock};
    for(auto& consumer : this->_consumers) {
        consumer->consume(input, frameCount, this->source_sample_rate, this->source_channel_count);
    }
}