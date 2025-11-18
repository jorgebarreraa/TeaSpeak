#pragma once

#include "NativeCodec.h"

#ifdef HAVE_OPUS
	#include <opus/opus.h>
#endif

namespace tc {
	class NativeCodec;

	class OpusCodec : public NativeCodec  {
		public:
			static bool supported();
#ifdef HAVE_OPUS
			explicit OpusCodec(CodecType::value type);
			virtual ~OpusCodec();

			virtual NAN_METHOD(initialize);
			virtual NAN_METHOD(finalize);
			virtual NAN_METHOD(encode);
			virtual NAN_METHOD(decode);
		private:
			uint16_t sampling_rate = 0;
			uint16_t frames = 0;
			uint32_t channels = 0;

			std::mutex coder_lock;
			OpusDecoder* decoder = nullptr;
			OpusEncoder* encoder = nullptr;
#endif
	};
}