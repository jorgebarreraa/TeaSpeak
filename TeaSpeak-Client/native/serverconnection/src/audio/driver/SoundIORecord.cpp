//
// Created by wolverindev on 07.02.20.
//

#include "SoundIO.h"
#include <algorithm>
#include "../../logger.h"

using namespace tc::audio;

SoundIORecord::SoundIORecord(struct ::SoundIoDevice *device) : device_handle{device} {
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

SoundIORecord::~SoundIORecord() {
    {
        std::lock_guard slock{this->state_lock};
        if(this->running) this->impl_stop();
    }
    soundio_device_unref(this->device_handle);
}

size_t SoundIORecord::sample_rate() const {
    return this->_sample_rate;
}

void SoundIORecord::execute_recovery() {
    if(this->fail_recover_thread.joinable()) return;

    this->fail_recover_thread = std::thread([&]{
        _fail_begin:
        {
            std::unique_lock cv_lock{this->fail_cv_mutex};
            auto fc = this->failed_count;
            if(fc == 0) fc = 1;
            else if(fc > 10) return;
            this->fail_cv.wait_for(cv_lock, std::chrono::seconds{(fc - 1) * 10});
            if(!this->running || this->stop_requested) return;
        }

        std::unique_lock slock{this->state_lock, std::defer_lock};
        if(!slock.try_lock_for(std::chrono::milliseconds{20})) {
            log_info(category::audio, tr("Failed lock state mutex. Exit input recover thread."));
            return;
        }
        if(!this->running) return;

        std::string error{};
        this->impl_stop();
        if(!this->impl_start(error)) {
            log_info(category::audio, tr("Failed to recover from device fail: {}. Trying again later."), error);
            this->failed_count++;
            goto _fail_begin;
        }
    });
}

bool SoundIORecord::impl_start(std::string &error) {
    assert(this->device_handle);
    this->buffer = soundio_ring_buffer_create(nullptr, kChunkSize * sizeof(float) * 2); //2 Channels
    if(!buffer) {
        error = "failed to allocate the buffer";
        return false;
    }
    soundio_ring_buffer_clear(this->buffer);

    this->stream = soundio_instream_create(this->device_handle);
    if(!this->stream) {
        error = "out of memory";
        return false;
    }

    this->stream->userdata = this;
    this->stream->format = SoundIoFormatFloat32LE;
    this->stream->sample_rate = this->_sample_rate;
    this->stream->software_latency = 0.02;

    this->stream->overflow_callback = [](auto str) {
        auto handle = reinterpret_cast<SoundIORecord*>(str->userdata);
        log_info(category::audio, tr("Having an overflow on {}"), handle->device_handle->id);

        handle->failed_count++;
    };

    this->stream->error_callback = [](auto str, int err) {
        auto handle = reinterpret_cast<SoundIORecord*>(str->userdata);
        log_info(category::audio, tr("Having an error on {}: {}. Aborting recording and trying again later."), handle->device_handle->id, soundio_strerror(err));
        std::lock_guard slock{handle->state_lock};
        if(!handle->running) return;

        handle->stream_invalid = true;
        handle->execute_recovery();
    };

    this->stream->read_callback = [](struct SoundIoInStream *str, int frame_count_min, int frame_count_max) {
        auto handle = reinterpret_cast<SoundIORecord*>(str->userdata);
        handle->read_callback(frame_count_min, frame_count_max);
    };

    if(auto err = soundio_instream_open(this->stream); err) {
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

    if(auto err = soundio_instream_start(this->stream); err) {
        error = soundio_strerror(err) + std::string{" (start)"};
        goto error_cleanup;
    }

    //TODO: Test for interleaved channel layout!

    this->stop_requested = false;
    return true;

    error_cleanup:

    if(this->stream) soundio_instream_destroy(this->stream);
    this->stream = nullptr;

    if(this->buffer) soundio_ring_buffer_destroy(this->buffer);
    this->buffer = nullptr;

    return false;
}

void SoundIORecord::impl_stop() {
    if(!this->stream) return;

    if(this->fail_recover_thread.joinable() && std::this_thread::get_id() != this->fail_recover_thread.get_id()) {
        {
            std::lock_guard flock{this->fail_cv_mutex};
            this->stop_requested = true;
        }
        this->fail_cv.notify_all();
        this->fail_recover_thread.join();
    }

    soundio_instream_destroy(this->stream);
    this->stream = nullptr;

    soundio_ring_buffer_destroy(this->buffer);
    this->buffer = nullptr;
}

void SoundIORecord::read_callback(int frame_count_min, int frame_count_max) {
    const struct SoundIoChannelLayout *layout = &this->stream->layout;

    struct SoundIoChannelArea *areas;

    int frames_left{frame_count_max};
    int buffer_samples = kChunkSize - soundio_ring_buffer_fill_count(this->buffer) / (sizeof(float) * layout->channel_count);

    while(frames_left > 0) {
        int frame_count{frames_left};
        if(frame_count > buffer_samples)
            frame_count = buffer_samples;
        if(auto err = soundio_instream_begin_read(this->stream, &areas, &frame_count); err) {
            log_error(category::audio, tr("Failed to begin read from input stream buffer: {}"), soundio_strerror(err));
            return;
        }

        if(!areas) {
            log_warn(category::audio, tr("Input audio underflow for {} ({} samples)"),  this->device_handle->name, frame_count_max);
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

        const auto length = sizeof(float) * layout->channel_count * frame_count;
        memcpy(soundio_ring_buffer_write_ptr(this->buffer), areas[0].ptr, length);
        soundio_ring_buffer_advance_write_ptr(this->buffer, length);
        buffer_samples -= frame_count;
        frames_left -= frame_count;

        if(buffer_samples == 0) {
            std::lock_guard consumer{this->consumer_lock};
            const auto byte_count = soundio_ring_buffer_fill_count(this->buffer);
            const auto buffer_frame_count = byte_count / (sizeof(float) * layout->channel_count);

            for(auto& consumer : this->_consumers)
                consumer->consume(soundio_ring_buffer_read_ptr(this->buffer), buffer_frame_count, layout->channel_count);

            soundio_ring_buffer_advance_read_ptr(this->buffer, byte_count);
            buffer_samples = kChunkSize;
        }

        if(auto err = soundio_instream_end_read(this->stream); err) {
            log_error(category::audio, tr("Failed to close input stream buffer: {}"), soundio_strerror(err));
            return;
        }
    }
}