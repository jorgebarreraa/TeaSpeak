#include "./FilterVad.h"
#include "../AudioMerger.h"
#include "../AudioInput.h"

#ifdef USE_FVAD
#include "../../logger.h"
#include <fvad.h>
#endif

using namespace std;
using namespace tc::audio;
using namespace tc::audio::filter;

VadFilter::VadFilter(size_t channels, size_t rate) : Filter{channels, rate} { }

#ifdef USE_FVAD
VadFilter::~VadFilter() {
	if(this->vad_handle_) {
		fvad_free(this->vad_handle_);
		this->vad_handle_ = nullptr;
	}
}

bool VadFilter::initialize(std::string &error, size_t mode, size_t margin) {
	this->vad_handle_ = fvad_new();
	if(!this->vad_handle_) {
		error = "failed to allocate handle";
		return false;
	}

	if(fvad_set_sample_rate(this->vad_handle_, (int) this->sample_rate_) != 0) {
		error = "invalid sample rate. Sample rate must be one of [8000, 16000, 32000 and 48000]";
		return false;
	}

	if(fvad_set_mode(this->vad_handle_, (int) mode) != 0) {
		error = "failed to set mode";
		return false;
	}

	this->mode_ = mode;
	this->margin_samples_ = margin;
	return true;
}

std::optional<size_t> VadFilter::mode() const {
    return std::make_optional(this->mode_);
}

constexpr static auto kMaxStackBufferSamples{1024 * 8};
bool VadFilter::contains_voice(const AudioInputBufferInfo& info, const float* buffer_) {
	if(!this->vad_handle_) {
		log_warn(category::audio, tr("Vad filter hasn't been initialized!"));
		return false;
	}

	float temp_sample_buffer[kMaxStackBufferSamples];
	if(info.sample_count > kMaxStackBufferSamples) {
        log_warn(category::audio, tr("Vad filter received too many samples {}, expected max {}."), info.sample_count, kMaxStackBufferSamples);
	    return false;
	}

	if(this->channels_ > 1) {
		if(!merge::merge_channels_interleaved(temp_sample_buffer, 1, buffer_, this->channels_, info.sample_count)) {
			log_warn(category::audio, tr("Failed to merge channels"));
			return false;
		}

        buffer_ = temp_sample_buffer;
	}

	/* convert float32 samples to signed int16 */
	{
		auto target = (int16_t*) temp_sample_buffer;
		auto source = buffer_;
		auto sample = info.sample_count;

		float tmp;
		while(sample-- > 0) {
			tmp = *source++;
			tmp *= 32768;

			if(tmp > 32767) {
                tmp = 32767;
			}

			if(tmp < -32768) {
                tmp = -32768;
			}

			*target++ = (int16_t) tmp;
		}
	}

	auto result = fvad_process(this->vad_handle_, (int16_t*) temp_sample_buffer, info.sample_count);
	if(result == -1) {
		log_warn(category::audio, tr("Invalid frame length"));
		return false;
	}

	return result == 1;
}
#else
VadFilter::~VadFilter() = default;

std::optional<size_t> VadFilter::mode() const {
    (void) this;
    return std::nullopt;
}

bool VadFilter::initialize(std::string &error, size_t mode, size_t margin) {
    this->mode_ = mode;
    this->margin_samples_ = margin;
    return true;
}

bool VadFilter::contains_voice(const AudioInputBufferInfo &info, const float *) {
    (void) this;
    return info.vad_detected.value_or(true);
}

#endif


bool VadFilter::process(const AudioInputBufferInfo &info, const float *buffer) {
    auto flag_vad{this->contains_voice(info, buffer)};
    if(!flag_vad) {
        this->margin_processed_samples_ += info.sample_count;
        return this->margin_processed_samples_ <= this->margin_samples_;
    } else {
        this->margin_processed_samples_ = 0;
    }
    return flag_vad;
}