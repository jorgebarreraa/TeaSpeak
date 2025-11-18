//
// Created by WolverinDEV on 13/02/2020.
//


#include <algorithm>
#include <cmath>
#include "./PortAudio.h"
#include "../../logger.h"

using namespace tc::audio::pa;

PortAudioPlayback::PortAudioPlayback(PaDeviceIndex index, const PaDeviceInfo *info)
    : index{index}, info{info} { }

PortAudioPlayback::~PortAudioPlayback() {
    this->stop();
}

bool PortAudioPlayback::impl_start(std::string &error) {
    //TODO: Detect a supported sample rate and use that
    {
        auto device_info = Pa_GetDeviceInfo(this->index);
        if(this->info != device_info) {
            error = "invalid info pointer";
            return false;
        }
    }

    const auto proxied_write_callback = [](
            const void *input, void *output,
            unsigned long frameCount,
            const PaStreamCallbackTimeInfo* timeInfo,
            PaStreamCallbackFlags statusFlags,
            void *userData
    ) {
        assert(output);

        auto player = reinterpret_cast<PortAudioPlayback*>(userData);
        assert(player);

        player->write_callback(output, frameCount, timeInfo, statusFlags);
        return 0;
    };

    PaStreamParameters parameters{};
    memset(&parameters, 0, sizeof(parameters));
    parameters.device = this->index;
    parameters.sampleFormat = paFloat32;
    parameters.suggestedLatency = this->info->defaultLowInputLatency;

    if(this->info->maxOutputChannels < kDefaultChannelCount) {
        parameters.channelCount = (int) 1;
        this->source_channel_count = 1;
    } else {
        parameters.channelCount = (int) kDefaultChannelCount;
        this->source_channel_count = kDefaultChannelCount;
    }

    for(const auto& sample_rate : kSupportedSampleRates) {
        auto err = Pa_OpenStream(
                &this->stream,
                nullptr,
                &parameters,
                (double) sample_rate,
                paFramesPerBufferUnspecified,
                paClipOff,
                proxied_write_callback,
                this
        );
        log_debug(category::audio, "Open result for playback device {} (MaxChannels: {}, Channels: {}, Sample rate: {}): {}", this->info->name, this->info->maxOutputChannels, this->source_channel_count, sample_rate, err);

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
    return true;
}

void PortAudioPlayback::impl_stop() {
    if(Pa_IsStreamActive(this->stream)) {
        Pa_AbortStream(this->stream);
    }

    auto error = Pa_CloseStream(this->stream);
    if(error != paNoError) {
        log_error(category::audio, tr("Failed to close PA stream: {}"), error);
    }
    this->stream = nullptr;
}

size_t PortAudioPlayback::current_sample_rate() const {
    return this->source_sample_rate;
}

void PortAudioPlayback::write_callback(void *output, unsigned long frameCount,
                                    const PaStreamCallbackTimeInfo *timeInfo, PaStreamCallbackFlags statusFlags) {
    (void) timeInfo;
    (void) statusFlags;
    this->fill_buffer(output, frameCount, this->source_sample_rate, this->source_channel_count);
}