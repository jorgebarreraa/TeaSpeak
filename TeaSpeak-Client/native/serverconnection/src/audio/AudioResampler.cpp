#include <cstring>
#include "AudioResampler.h"
#include "../logger.h"

using namespace std;
using namespace tc::audio;

AudioResampler::AudioResampler(size_t irate, size_t orate, size_t channels) : input_rate_{irate}, output_rate_{orate}, channels_{channels} {
	if(this->input_rate() != this->output_rate()) {
		soxr_error_t error;
		this->soxr_handle = soxr_create((double) this->input_rate_, (double) this->output_rate_, (unsigned) this->channels_, &error, nullptr, nullptr, nullptr);

		if(!this->soxr_handle) {
			log_error(category::audio, tr("Failed to create soxr resampler: {}. Input: {}; Output: {}; Channels: {}"), error, this->input_rate_, this->output_rate_, this->channels_);
		}
	}
}

AudioResampler::~AudioResampler() {
	if(this->soxr_handle)
		soxr_delete(this->soxr_handle);
}

bool AudioResampler::process(void *output, const void *input, size_t input_length, size_t& output_length) {
	if(this->output_rate_ == this->input_rate_) {
		if(input != output) {
            memcpy(output, input, input_length * this->channels_ * 4);
		}

		return input_length;
	}

	if(!this->soxr_handle) {
        return false;
	}

	assert(output_length > 0);
	auto error = soxr_process(this->soxr_handle, input, input_length, nullptr, output, output_length, &output_length);
	if(error) {
		log_error(category::audio, tr("Failed to process resample: {}"), error);
		return false;
	}

	return true;
}