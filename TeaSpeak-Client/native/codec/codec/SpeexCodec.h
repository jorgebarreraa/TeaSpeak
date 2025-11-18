#pragma once

#include "NativeCodec.h"

#ifdef HAVE_SPEEX
	#include <speex/speex.h>
#endif

namespace tc {
	class NativeCodec;


	class SpeexCodec : public NativeCodec  {
		public:
			static bool supported();

#ifdef HAVE_SPEEX
			explicit SpeexCodec(CodecType::value type);
			virtual ~SpeexCodec();

			virtual NAN_METHOD(initialize);
			virtual NAN_METHOD(finalize);
			virtual NAN_METHOD(encode);
			virtual NAN_METHOD(decode);
		private:
			int frame_size = 0;

			//TODO are two bits really necessary?
			std::mutex coder_lock;
			SpeexBits encoder_bits;
			void* encoder = nullptr;
			SpeexBits decoder_bits;
			void* decoder = nullptr;
#endif
	};
}