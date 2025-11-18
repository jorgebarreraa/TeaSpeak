#include "AudioSamples.h"
#include <cstdlib>

using namespace std;
using namespace tc;
using namespace tc::audio;

std::shared_ptr<SampleBuffer> SampleBuffer::allocate(uint8_t channels, uint16_t samples) {
	auto buffer = (SampleBuffer*) malloc(sizeof(SampleBuffer) + channels * samples * 4);
	if(!buffer) {
        return nullptr;
	}

    buffer->sample_size = samples;
    buffer->sample_index = 0;
	return shared_ptr<SampleBuffer>(buffer, ::free);
}
