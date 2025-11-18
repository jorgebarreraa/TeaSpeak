#include "./AudioOutput.h"
#include "./AudioResampler.h"
#include "../logger.h"
#include <cstring>
#include <algorithm>

using namespace std;
using namespace tc;
using namespace tc::audio;

void AudioOutputSource::clear() {
    std::lock_guard buffer_lock{this->buffer_mutex};
    this->buffer.clear();
    this->buffer_state = BufferState::buffering;
    this->fadeout_samples_left = 0;
}

void AudioOutputSource::apply_fadeout() {
    const auto samples_available = this->currently_buffered_samples();
    auto fade_samples = std::min(samples_available, this->fadeout_frame_samples_);
    if(!fade_samples) {
        this->fadeout_samples_left = 0;
        return;
    }

    const auto sample_byte_size = this->channel_count_ * sizeof(float) * fade_samples;
    assert(this->buffer.fill_count() >= sample_byte_size);
    auto write_ptr = (float*) ((char*) this->buffer.read_ptr() + (this->buffer.fill_count() - sample_byte_size));

    for(size_t index{0}; index < fade_samples; index++) {
        const auto offset = (float) ((float) (index + 1) / (float) fade_samples);
        const auto volume = std::min(log10f(offset) / -2.71828182845904f, 1.f);

        for(int channel{0}; channel < this->channel_count_; channel++) {
            *write_ptr++ *= volume;
        }
    }

    this->fadeout_samples_left = fade_samples;
}

void AudioOutputSource::apply_fadein() {
    assert(this->currently_buffered_samples() >= this->fadeout_samples_left);
    const auto samples_available = this->currently_buffered_samples();
    auto fade_samples = std::min(samples_available - this->fadeout_samples_left, this->fadein_frame_samples_);
    if(!fade_samples) {
        return;
    }

    /*
     * Note: We're using the read_ptr() here in order to correctly apply the effect.
     *       This isn't really best practice but works.
     */
    auto write_ptr = (float*) this->buffer.read_ptr() + this->fadeout_samples_left * this->channel_count_;
    for(size_t index{0}; index < fade_samples; index++) {
        const auto offset = (float) ((float) (index + 1) / (float) fade_samples);
        const auto volume = std::min(log10f(1 - offset) / -2.71828182845904f, 1.f);
        for(int channel{0}; channel < this->channel_count_; channel++) {
            *write_ptr++ *= volume;
        }
    }
}

bool AudioOutputSource::pop_samples(void *target_buffer, size_t target_sample_count) {
    std::unique_lock buffer_lock{this->buffer_mutex};
    auto result = this->pop_samples_(target_buffer, target_sample_count);
    buffer_lock.unlock();

    if(auto callback{this->on_read}; callback) {
        callback();
    }
    return result;
}

bool AudioOutputSource::pop_samples_(void *target_buffer, size_t target_sample_count) {
    switch(this->buffer_state) {
        case BufferState::fadeout: {
            /* Write as much we can */
            const auto write_samples = std::min(this->fadeout_samples_left, target_sample_count);
            const auto write_byte_size = write_samples * this->channel_count_ * sizeof(float);
            memcpy(target_buffer, this->buffer.read_ptr(), write_byte_size);
            this->buffer.advance_read_ptr(write_byte_size);

            /* Fill the rest with silence */
            const auto empty_samples = target_sample_count - write_samples;
            const auto empty_byte_size = empty_samples * this->channel_count_ * sizeof(float);
            memset((char*) target_buffer + write_byte_size, 0, empty_byte_size);

            this->fadeout_samples_left -= write_samples;
            if(!this->fadeout_samples_left) {
                log_trace(category::audio, tr("{} Successfully replayed fadeout sequence."), (void*) this);
                this->buffer_state = BufferState::buffering;
            }
            return true;
        }

        case BufferState::playing: {
            const auto buffered_samples = this->currently_buffered_samples();
            if(buffered_samples < target_sample_count + this->fadeout_frame_samples_) {
                const auto missing_samples = target_sample_count + this->fadeout_frame_samples_ - buffered_samples;
                if(auto callback{this->on_underflow}; callback) {
                    if(callback(missing_samples)) {
                        /* We've been filled up again. Trying again to fill the output buffer. */
                        return this->pop_samples_(target_buffer, target_sample_count);
                    }
                }

                /*
                 * When consuming target_sample_count amount samples of our buffer we could not
                 * apply the fadeout effect any more. Instead we're applying it now and returning to buffering state.
                 */
                this->apply_fadeout();

                /* Write the rest of unmodified buffer */
                const auto write_samples = buffered_samples - this->fadeout_samples_left;
                assert(write_samples <= target_sample_count);
                const auto write_byte_size = write_samples * this->channel_count_ * sizeof(float);
                memcpy(target_buffer, this->buffer.read_ptr(), write_byte_size);
                this->buffer.advance_read_ptr(write_byte_size);

                log_trace(category::audio, tr("{} Starting stream fadeout. Requested samples {}, Buffered samples: {}, Fadeout frame samples: {}, Returned normal samples: {}"),
                          (void*) this, target_sample_count, buffered_samples, this->fadeout_frame_samples_, write_samples
                );

                this->buffer_state = BufferState::fadeout;
                if(write_samples < target_sample_count) {
                    /* Fill the rest of the buffer with the fadeout content */
                    this->pop_samples_((char*) target_buffer + write_byte_size, target_sample_count - write_samples);
                }
            } else {
                /* We can just normally copy the buffer */
                const auto write_byte_size = target_sample_count * this->channel_count_ * sizeof(float);
                memcpy(target_buffer, this->buffer.read_ptr(), write_byte_size);
                this->buffer.advance_read_ptr(write_byte_size);
            }

            return true;
        }

        case BufferState::buffering:
            /* Nothing to replay */
            return false;

        default:
            assert(false);
            return false;
    }
}

ssize_t AudioOutputSource::enqueue_samples(const void *source_buffer, size_t sample_count) {
    std::lock_guard buffer_lock{this->buffer_mutex};
    return this->enqueue_samples_(source_buffer, sample_count);
}

ssize_t AudioOutputSource::enqueue_samples_(const void *source_buffer, size_t sample_count) {
    switch(this->buffer_state) {
        case BufferState::fadeout:
        case BufferState::buffering: {
            assert(this->currently_buffered_samples() >= this->fadeout_samples_left);
            assert(this->min_buffered_samples_ >= this->currently_buffered_samples() - this->fadeout_samples_left);
            const auto missing_samples = this->min_buffered_samples_ - (this->currently_buffered_samples() - this->fadeout_samples_left);
            const auto write_sample_count = std::min(missing_samples, sample_count);
            const auto write_byte_size = write_sample_count * this->channel_count_ * sizeof(float);

            assert(write_sample_count <= this->max_supported_buffering());
            memcpy(this->buffer.write_ptr(), source_buffer, write_byte_size);
            this->buffer.advance_write_ptr(write_byte_size);

            if(sample_count < missing_samples) {
                /* we still need to buffer */
                return sample_count;
            }

            /*
             * Even though we still have fadeout samples left we don't declare them as such since we've already fulled
             * our future buffer.
             */
            this->fadeout_samples_left = 0;

            /* buffering finished */
            log_trace(category::audio, tr("{} Finished buffering {} samples. Fading them in."), (void*) this, this->min_buffered_samples_);
            this->apply_fadein();
            this->buffer_state = BufferState::playing;
            if(sample_count > missing_samples) {
                /* we've more data to write */
                return this->enqueue_samples_((const char*) source_buffer + write_byte_size, sample_count - missing_samples) + write_sample_count;
            } else {
                return write_sample_count;
            }
        }

        case BufferState::playing: {
            const auto buffered_samples = this->currently_buffered_samples();

            const auto write_sample_count = std::min(this->max_supported_buffering() - buffered_samples, sample_count);
            const auto write_byte_size = write_sample_count * this->channel_count_ * sizeof(float);

            memcpy(this->buffer.write_ptr(), source_buffer, write_byte_size);
            this->buffer.advance_write_ptr(write_byte_size);

            if(write_sample_count < sample_count) {
                if(auto callback{this->on_overflow}; callback) {
                    callback(sample_count - write_sample_count);
                }

                switch (this->overflow_strategy) {
                    case OverflowStrategy::discard_input:
                        return -2;

                    case OverflowStrategy::discard_buffer_all:
                        this->buffer.clear();
                        break;

                    case OverflowStrategy::discard_buffer_half:
                        /* FIXME: This implementation is wrong! */
                        this->buffer.advance_read_ptr(this->buffer.fill_count() / 2);
                        break;

                    case OverflowStrategy::ignore:
                        break;
                }
            }

            return write_sample_count;
        }

        default:
            assert(false);
            return false;
    }
}

constexpr static auto kMaxStackBuffer{1024 * 8 * sizeof(float)};
ssize_t AudioOutputSource::enqueue_samples_no_interleave(const void *source_buffer, size_t samples) {
    if(this->channel_count_ == 1) {
        return this->enqueue_samples(source_buffer, samples);
    } else if(this->channel_count_ == 2) {
        const auto buffer_byte_size = samples * this->channel_count_ * sizeof(float);
        if(buffer_byte_size > kMaxStackBuffer) {
            /* We can't convert to interleave */
            return 0;
        }

        uint8_t stack_buffer[kMaxStackBuffer];
        {
            auto src_buffer = (const float*) source_buffer;
            auto target_buffer = (float*) stack_buffer;

            auto samples_to_write = samples;
            while (samples_to_write-- > 0) {
                *target_buffer = *src_buffer;
                *(target_buffer + 1) = *(src_buffer + samples);

                target_buffer += 2;
                src_buffer++;
            }
        }

        return this->enqueue_samples(stack_buffer, samples);
    } else {
        /* TODO: Generalize to interleave algo */
        return 0;
    }
}

bool AudioOutputSource::set_max_buffered_samples(size_t samples) {
    samples = std::max(samples, (size_t) this->fadein_frame_samples_);
    if(samples > this->max_supported_buffering()) {
        samples = this->max_supported_buffering();
    }

    std::lock_guard buffer_lock{this->buffer_mutex};
    if(samples < this->min_buffered_samples_) {
        return false;
    }

    this->max_buffered_samples_ = samples;
    return true;
}

bool AudioOutputSource::set_min_buffered_samples(size_t samples) {
    samples = std::max(samples, (size_t) this->fadein_frame_samples_);

    std::lock_guard buffer_lock{this->buffer_mutex};
    if(samples > this->max_buffered_samples_) {
        return false;
    }

    this->min_buffered_samples_ = samples;
    switch(this->buffer_state) {
        case BufferState::fadeout:
        case BufferState::buffering: {
            assert(this->currently_buffered_samples() >= this->fadeout_samples_left);
            const auto buffered_samples = this->currently_buffered_samples() - this->fadeout_samples_left;
            if(buffered_samples > this->min_buffered_samples_) {
                log_trace(category::audio, tr("{} Finished buffering {} samples (due to min buffered sample reduce). Fading them in."), (void*) this, this->min_buffered_samples_);
                this->apply_fadein();
                this->buffer_state = BufferState::playing;
            }

            return true;
        }

        case BufferState::playing:
            return true;

        default:
            assert(false);
            return false;
    }
}