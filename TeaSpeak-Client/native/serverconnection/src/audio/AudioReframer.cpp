#include <cassert>
#include <cstring>
#include "AudioReframer.h"

using namespace tc::audio;

AudioReframer::AudioReframer(size_t channels, size_t target_frame_size) : frame_size_(target_frame_size), channels_(channels) {
	this->buffer = nullptr;
	this->buffer_index_ = 0;
}

AudioReframer::~AudioReframer() {
	if(this->buffer) {
        free(this->buffer);
	}
}

void AudioReframer::process(const void *source, size_t samples) {
	if(!this->buffer) {
        this->buffer = (float*) malloc(this->channels_ * this->frame_size_ * sizeof(float));
	}
	assert(this->on_frame);

	if(this->buffer_index_ > 0) {
		if(this->buffer_index_ + samples > this->frame_size_) {
			auto required = this->frame_size_ - this->buffer_index_;
			auto length = required * this->channels_ * sizeof(float);

			memcpy((char*) this->buffer + this->buffer_index_ * sizeof(float) * this->channels_, source, length);
			samples -= required;
			source = (char*) source + length;

			this->on_frame((float*) this->buffer);
		} else {
			memcpy((char*) this->buffer + this->buffer_index_ * sizeof(float) * this->channels_, source, samples * this->channels_ * sizeof(float));
			this->buffer_index_ += samples;
			return;
		}
	}

	auto _on_frame = this->on_frame;
	while(samples > this->frame_size_) {
		if(_on_frame) {
            _on_frame((float*) source);
		}

		samples -= this->frame_size_;
		source = (char*) source + this->frame_size_ * this->channels_ * sizeof(float);
	}
	if(samples > 0) {
        memcpy((char*) this->buffer, source, samples * this->channels_ * sizeof(float));
	}
	
	this->buffer_index_ = samples;
}

void AudioReframer::flush() {
    if(this->buffer_index_ == 0) {
        return;
    }

    if(!this->on_flush) {
        this->buffer_index_ = 0;
        return;
    }

    this->on_flush((const float*) this->buffer, this->buffer_index_);
    this->buffer_index_ = 0;
}