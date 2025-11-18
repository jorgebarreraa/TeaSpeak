#include <cmath>
#include "./FilterThreshold.h"
#include "../AudioInput.h"
#include "../processing/AudioVolume.h"

using namespace std;
using namespace tc::audio;
using namespace tc::audio::filter;

ThresholdFilter::ThresholdFilter(size_t a, size_t b) : Filter(a, b) {}
ThresholdFilter::~ThresholdFilter() = default;

void ThresholdFilter::initialize(float threshold, size_t margin) {
	this->threshold_ = threshold;
	this->margin_samples_ = margin;
}

bool ThresholdFilter::process(const AudioInputBufferInfo& info, const float* buffer) {
    auto analyze_callback = this->on_analyze;
	float value = audio::audio_buffer_level(buffer, this->channels_, info.sample_count);

	auto last_level = this->current_level_;
	float smooth;
	if(this->margin_processed_samples_ == 0) {
        /* we're in release */
        smooth = this->release_smooth_;
	} else {
        smooth = this->attack_smooth_;
	}

	this->current_level_ = last_level * smooth + value * (1 - smooth);
	//log_trace(category::audio, "Vad level: before: {}, edit: {}, now: {}, smooth: {}", last_level, value, this->_current_level, smooth);

	if(analyze_callback) {
        analyze_callback(this->current_level_);
	}

	if(this->current_level_ >= this->threshold_) {
		this->margin_processed_samples_ = 0;
		return true;
	}

	return (this->margin_processed_samples_ += info.sample_count) < this->margin_samples_;
}