#pragma once

#include "NativeCodec.h"

#ifdef HAVE_CELT
	#include <celt/celt.h>
#endif

namespace tc {
	class NativeCodec;


	class CeltCodec : public NativeCodec  {
		public:
			static bool supported();

#ifdef HAVE_CELT
			explicit CeltCodec(CodecType::value type);
			virtual ~CeltCodec();

			virtual NAN_METHOD(initialize);
			virtual NAN_METHOD(finalize);
			virtual NAN_METHOD(encode);
			virtual NAN_METHOD(decode);
		private:
			std::mutex coder_lock;
			int max_frame_size = 512;
			int channels = 1; /* fixed */

			CELTEncoder* encoder = nullptr;
			CELTDecoder* decoder = nullptr;
#endif
	};
}