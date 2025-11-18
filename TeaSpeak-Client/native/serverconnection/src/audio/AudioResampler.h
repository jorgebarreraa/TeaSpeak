#pragma once

#include <memory>
#include <cmath>
#include <soxr.h>
#include <deque>
#include <mutex>

#if defined(WIN32) && !defined(ssize_t)
    #define ssize_t int64_t
#endif

namespace tc::audio {
		class AudioResampler {
			public:
				AudioResampler(size_t /* input rate */, size_t /* output rate */, size_t /* channels */);
				virtual ~AudioResampler();

                [[nodiscard]] inline size_t channels() const { return this->channels_; }
                [[nodiscard]] inline size_t input_rate() const { return this->input_rate_; }
                [[nodiscard]] inline size_t output_rate() const { return this->output_rate_; }

                [[nodiscard]] inline long double io_ratio() const { return (long double) this->output_rate_ / (long double) this->input_rate_; }
                [[nodiscard]] inline size_t estimated_output_size(size_t input_length) {
                    if(!this->soxr_handle) return input_length; /* no resembling needed */
					return (size_t) ceill(this->io_ratio() * input_length + *soxr_num_clips(this->soxr_handle)) + 1;
				}
                [[nodiscard]] inline size_t input_size(size_t output_length) const {
                    return (size_t) ceill((long double) this->input_rate_ / (long double) this->output_rate_ * output_length);
                }

                [[nodiscard]] inline bool valid() { return this->io_ratio() == 1 || this->soxr_handle != nullptr; }

                [[nodiscard]] bool process(void* /* output */, const void* /* input */, size_t /* input length */, size_t& /* output length */);
			private:
				size_t const channels_{0};
				size_t const input_rate_{0};
				size_t const output_rate_{0};

				soxr_t soxr_handle{nullptr};
		};
	}