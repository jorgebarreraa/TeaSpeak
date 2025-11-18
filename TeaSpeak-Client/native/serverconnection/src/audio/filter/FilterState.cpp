#include "./FilterState.h"

using namespace std;
using namespace tc::audio;
using namespace tc::audio::filter;

StateFilter::StateFilter(size_t a, size_t b) : Filter{a, b} {}

bool StateFilter::process(const AudioInputBufferInfo &, const float *) {
    return !this->consume_;
}