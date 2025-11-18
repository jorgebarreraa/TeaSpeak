//
// Created by wolverindev on 07.02.20.
//

#include <algorithm>
#include <cmath>
#include "SoundIO.h"
#include "../../logger.h"

using namespace tc::audio;

SoundIOPlayback::SoundIOPlayback(struct ::SoundIoDevice *device) : device_handle{device} {
    soundio_device_ref(device);

    if(device->probe_error || !device->sample_rate_count)
        this->_sample_rate = kDefaultSampleRate;
    else {
        for(const auto& sample_rate : kSampleRateOrder) {
            for(size_t index{0}; index < device->sample_rate_count; index++) {
                auto supported_rate = device->sample_rates[index];
                if(supported_rate.min <= sample_rate && supported_rate.max >= sample_rate) {
                    this->_sample_rate = sample_rate;
                    goto _found;
                }
            }
        }

        this->_sample_rate = kDefaultSampleRate;
        _found:;
    }
}

SoundIOPlayback::~SoundIOPlayback() {
    soundio_device_unref(this->device_handle);
}

size_t SoundIOPlayback::sample_rate() const {
    return this->_sample_rate;
}

bool SoundIOPlayback::impl_start(std::string &error) {
    assert(this->device_handle);

    //TODO: Figure out how many channels!
    this->buffer = soundio_ring_buffer_create(nullptr, (int) (kChunkTime * this->_sample_rate * sizeof(float) * 2)); /* 2 channels */
    if(!buffer) {
        error = "failed to allocate the buffer";
        return false;
    }

    this->stream = soundio_outstream_create(this->device_handle);
    if(!this->stream) {
        error = "out of memory";
        return false;
    }

    this->write_exit = false;
    this->stream->userdata = this;
    this->stream->sample_rate = this->_sample_rate;
    this->stream->format = SoundIoFormatFloat32LE;
    this->stream->software_latency = 0.02;

    log_info(category::audio, tr("Open device with: {}"), this->_sample_rate);
    this->stream->underflow_callback = [](auto str) {
        auto handle = reinterpret_cast<SoundIOPlayback*>(str->userdata);
        log_info(category::audio, tr("Having an underflow on {}"), handle->device_handle->id);
    };

    this->stream->error_callback = [](auto str, int err) {
        auto handle = reinterpret_cast<SoundIOPlayback*>(str->userdata);
        log_info(category::audio, tr("Having an error on {}: {}. Aborting playback."), handle->device_handle->id, soundio_strerror(err));
        handle->stream_invalid = true;
    };

    this->stream->write_callback = [](struct SoundIoOutStream *str, int frame_count_min, int frame_count_max) {
        auto handle = reinterpret_cast<SoundIOPlayback*>(str->userdata);
        handle->write_callback(frame_count_min, frame_count_max);
    };

#ifdef WIN32
    this->next_write = std::chrono::system_clock::now();
#endif
    if(auto err = soundio_outstream_open(this->stream); err) {
        error = soundio_strerror(err) + std::string{" (open)"};
        goto error_cleanup;
    }

    if(this->_sample_rate != this->stream->sample_rate) {
        error = "sample rate mismatch (" + std::to_string(this->_sample_rate) + " <> " + std::to_string(this->stream->sample_rate) + ")";
        goto error_cleanup;
    }

    if(false && this->stream->layout_error) {
        error = std::string{"failed to set audio layout: "} + soundio_strerror(this->stream->layout_error);
        goto error_cleanup;
    }

    if(auto err = soundio_outstream_start(this->stream); err) {
        error = soundio_strerror(err) + std::string{" (start)"};
        goto error_cleanup;
    }

#ifdef WIN32
    this->priority_boost = false;
    if(!this->device_handle->is_raw)
        soundio_outstream_wasapi_set_sleep_divider(this->stream,0); /* basically while true loop */
#endif
    //TODO: Test for interleaved channel layout!

    return true;

    error_cleanup:
    if(this->stream) soundio_outstream_destroy(this->stream);
    this->stream = nullptr;

    if(this->buffer) soundio_ring_buffer_destroy(this->buffer);
    this->buffer = nullptr;
    return false;
}

void SoundIOPlayback::impl_stop() {
    if(!this->stream) return;

#ifdef WIN32
    { /* exit the endless write loop when we're not in raw mode */
        std::lock_guard write_lock{this->write_mutex};
        this->write_exit = true;
        this->write_cv.notify_all();
    }
#endif
    soundio_outstream_destroy(this->stream);
    this->stream = nullptr;

    soundio_ring_buffer_destroy(this->buffer);
    this->buffer = nullptr;
}

typedef HANDLE ( __stdcall *TAvSetMmThreadCharacteristicsPtr )( LPCWSTR TaskName, LPDWORD TaskIndex );
void SoundIOPlayback::write_callback(int frame_count_min, int frame_count_max) {
    const struct SoundIoChannelLayout *layout = &this->stream->layout;
    struct SoundIoChannelArea *areas;

#ifdef WIN32
    if(!this->priority_boost) {
        this->priority_boost = true;

        // Attempt to assign "Pro Audio" characteristic to thread
        HMODULE AvrtDll = LoadLibrary((LPCTSTR) "AVRT.dll");
        if ( AvrtDll ) {
            DWORD taskIndex = 0;

            TAvSetMmThreadCharacteristicsPtr AvSetMmThreadCharacteristicsPtr =
            ( TAvSetMmThreadCharacteristicsPtr ) (void(*)()) GetProcAddress( AvrtDll, "AvSetMmThreadCharacteristicsW" );
            AvSetMmThreadCharacteristicsPtr( L"Pro Audio", &taskIndex );
            FreeLibrary( AvrtDll );
        }
    }

    /* shared windows devices */
    if(!this->device_handle->is_raw) {
        constexpr std::chrono::milliseconds jitter_ms{5};
        constexpr std::chrono::milliseconds kChunkMillis{(int64_t) (kChunkTime * 1000)};

        /* wait until the last stuff has been written */
        double latency{};
        {
            if(auto err = soundio_outstream_get_latency(this->stream, &latency); err) {
                log_warn(category::audio, tr("Failed to get auto stream latency: {}"), err);
                return;
            }

            std::chrono::microseconds software_latency{(int64_t) (this->device_handle->software_latency_current * 1e6)}; // latency for shared audio device
            std::chrono::microseconds buffered_duration{(int64_t) (latency * 1e6)};

            std::unique_lock cv_lock{this->write_mutex};
            auto now = std::chrono::system_clock::now();
            if(buffered_duration - jitter_ms > software_latency) {
                auto sleep_target = next_write - jitter_ms;
                if(sleep_target > now) {
                    this->write_cv.wait_until(cv_lock, sleep_target);
                    if(auto err = soundio_outstream_get_latency(this->stream, &latency); err) {
                        log_warn(category::audio, tr("Failed to get auto stream latency: {}"), err);
                        return;
                    }
                }
            } else {
                this->next_write = now - kChunkMillis; /* insert that chunk */
            }
            if(this->write_exit)
                return;
        }

        auto now = std::chrono::system_clock::now();
        auto overshoot = std::chrono::floor<std::chrono::milliseconds>(now - next_write).count();
        next_write = now + kChunkMillis;

        if(last_stats + std::chrono::seconds{1} < now) {
            last_stats = now;
            log_info(category::audio, tr("Samples: {}, lat: {}"), samples, latency);
            samples = 0;
        }

        double time_to_write{overshoot / 1000.0 + kChunkTime};
        bool drop_buffer{false};
        {
            const auto managed_latency = latency - this->device_handle->software_latency_current;
            if(managed_latency > 0.08) {
                drop_buffer = true;
                time_to_write = managed_latency * 1000 - 10;
            }
        }

        if(!drop_buffer) {
            auto frames_to_write = (int) (this->_sample_rate * time_to_write);
            if(frames_to_write <= 0) return;

            if(frames_to_write > frame_count_max) {
                log_warn(category::audio, tr("Supposed write chunk size is larger that supported max frame count. Reducing write chunk size."));
                frames_to_write = frame_count_max;
            }

            int frame_count{frames_to_write};
            if(auto err = soundio_outstream_begin_write(this->stream, &areas, &frame_count); err) {
                log_warn(category::audio, tr("Failed to begin a write to the soundio buffer: {}"), err);
                return;
            }

            if(frame_count != frames_to_write)
                log_warn(category::audio, tr("Allowed to write is not equal to the supposed value."));

            /* test for interleaved */
            {
                char* begin = areas[0].ptr - sizeof(float);
                for(size_t channel{0}; channel < layout->channel_count; channel++) {
                    if((begin += sizeof(float)) != areas[channel].ptr) {
                        log_error(category::audio, tr("Expected interleaved buffer, which it isn't"));
                        return;
                    }

                    if(areas[channel].step != sizeof(float) * layout->channel_count) {
                        log_error(category::audio, tr("Invalid step size for channel {}"), channel);
                        return;
                    }
                }
            }

            samples += frame_count;
            this->fill_buffer(areas[0].ptr, frame_count, layout->channel_count);
            if(auto err = soundio_outstream_end_write(this->stream); err) {
                log_warn(category::audio, tr("Failed to end a write to the soundio buffer: {}"), err);
                return;
            }
        } else {
            this->fill_buffer(nullptr, (int) (this->_sample_rate * time_to_write), layout->channel_count);
        }
    } else
#endif
    {
        int frames_left{frame_count_min}, err;

        /* time in second how much we want to fill the buffer */
        const auto min_interval = this->have_underflow ? 0.02 : 0.01;


        {
            const auto _min_interval_frames = (int) (min_interval * this->stream->sample_rate + .5);

            if(frames_left < _min_interval_frames)
                frames_left = _min_interval_frames;
            if(frames_left > frame_count_max)
                frames_left = frame_count_max;
            if(frame_count_max == 0) return;
        }

        while(frames_left > 0) {
            int frame_count{frames_left};
            auto buffered = soundio_ring_buffer_fill_count(this->buffer) / (sizeof(float) * layout->channel_count);
            if(frame_count > buffered) {
                if(buffered == 0) {
                    const auto fill_sample_count = (soundio_ring_buffer_free_count(this->buffer) / sizeof(float) / 2);
                    this->fill_buffer(soundio_ring_buffer_write_ptr(this->buffer), fill_sample_count, layout->channel_count);
                    soundio_ring_buffer_advance_write_ptr(this->buffer, fill_sample_count * sizeof(float) * 2);
                    buffered += fill_sample_count;
                } else
                    frame_count = buffered;
            }
            if((err = soundio_outstream_begin_write(this->stream, &areas, &frame_count))) {
                log_warn(category::audio, tr("Failed to begin a write to the soundio buffer: {}"), err);
                return;
            }

            /* test for interleaved */
            {
                char* begin = areas[0].ptr - sizeof(float);
                for(size_t channel{0}; channel < layout->channel_count; channel++) {
                    if((begin += sizeof(float)) != areas[channel].ptr) {
                        log_error(category::audio, tr("Expected interleaved buffer, which it isn't"));
                        return;
                    }

                    if(areas[channel].step != sizeof(float) * layout->channel_count) {
                        log_error(category::audio, tr("Invalid step size for channel {}"), channel);
                        return;
                    }
                }
            }

            const auto length = sizeof(float) * frame_count * layout->channel_count;
            memcpy(areas[0].ptr, soundio_ring_buffer_read_ptr(this->buffer), length);
            soundio_ring_buffer_advance_read_ptr(this->buffer, length);

            if((err = soundio_outstream_end_write(this->stream))) {
                log_warn(category::audio, tr("Failed to end a write to the soundio buffer: {}"), err);
                return;
            }

            frames_left -= frame_count;
        }
    }
}