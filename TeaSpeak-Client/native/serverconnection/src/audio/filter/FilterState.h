#pragma once

#include "./Filter.h"
#include <mutex>

namespace tc {
	namespace audio {
		namespace filter {
			class StateFilter : public Filter {
				public:
                    StateFilter(size_t /* channel count */, size_t /* sample rate */);
					bool process(const AudioInputBufferInfo& /* info */, const float* /* buffer */) override;

					[[nodiscard]] inline auto consumes_input() const { return this->consume_; }
					inline void set_consume_input(bool state) { this->consume_ = state; }
				private:
					bool consume_{false};
			};
		}
	}
}