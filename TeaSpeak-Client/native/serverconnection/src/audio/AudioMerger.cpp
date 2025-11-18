#include <cassert>
#include "AudioMerger.h"
#include "../logger.h"
#include <iostream>

using namespace std;
using namespace tc::audio;

/* technique based on http://www.vttoth.com/CMS/index.php/technical-notes/68 */
inline constexpr float merge_ab(float a, float b) {
    /*
     * Form: A,B := [0;n]
     *       Z = 2(A + B) - (2/n) * A * B - n
     *
     * For a range from 0 to 2: Z = 2(A + B) - AB - 2
     */

    a += 1;
    b += 1;

    float result{};
    if(a < 1 && b < 1) {
        result = a * b;
    } else {
        result = 2 * (a + b) - a * b - 2;
    }

    result -= 1;
    return result;
}

static_assert(merge_ab(1, 0) == 1);
static_assert(merge_ab(0, 1) == 1);
static_assert(merge_ab(1, 1) == 1);

bool merge::merge_sources(void *dest_, void *src_a_, void *src_b_, size_t channels, size_t samples) {
	auto dest = (float*) dest_;
	auto src_a = (float*) src_a_;
	auto src_b = (float*) src_b_;

	for(size_t index = 0; index < channels * samples; index++) {
        dest[index] = merge_ab(src_a[index], src_b[index]);
	}

	return true;
}

bool merge::merge_n_sources(void *dest, void **srcs, size_t src_length, size_t channels, size_t samples) {
	assert(src_length > 0);

	/* find the first non empty source */
	while(!srcs[0]) {
		srcs++;
		src_length--;

		if(src_length == 0) {
            return false;
		}
	}
	if(srcs[0] != dest) {
        memcpy(dest, srcs[0], channels * samples * 4);
	}

	srcs++;
	src_length--;

	while(src_length > 0) {
		/* only invoke is srcs is not null! */
		if(srcs[0] && !merge::merge_sources(dest, srcs[0], dest, channels, samples)) {
            return false;
		}

		srcs++;
		src_length--;
	}

	return true;
}

#define stack_buffer_length (1024 * 8)
bool merge::merge_channels_interleaved(void *target, size_t target_channels, const void *src, size_t src_channels, size_t samples) {
	assert(src_channels == 1 || src_channels == 2);
	assert(target_channels == 1 || target_channels == 2);

	if(src_channels == target_channels) {
		if(src == target) {
            return true;
		}

		memcpy(target, src, samples * src_channels * 4);
		return true;
	}

	if(src_channels == 1 && target_channels == 2) {
		auto source_array = (float*) src;
		auto target_array = (float*) target;

		if(source_array == target_array) {
			if(stack_buffer_length < samples * src_channels) {
				log_error(category::audio, tr("channel merge failed due to large inputs"));
				return false;
			}
			float stack_buffer[stack_buffer_length];
			memcpy(stack_buffer, src, src_channels * samples * 4);
			source_array = stack_buffer;

			while(samples-- > 0) {
				*(target_array++) = *source_array;
				*(target_array++) = *source_array;
				source_array++;
			}
		} else {
			while(samples-- > 0) {
				*(target_array++) = *source_array;
				*(target_array++) = *source_array;
				source_array++;
			}
		}
	} else if(src_channels == 2 && target_channels == 1) {
		auto source_array = (float*) src;
		auto target_array = (float*) target;

		while(samples-- > 0) {
			*(target_array++) = merge_ab(*(source_array), *(source_array + 1));
			source_array += 2;
		}
	} else {
        return false;
	}

	return true;
}