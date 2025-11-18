#pragma once

#include <nan.h>
#include <mutex>
#include <functional>
#include <thread>
#include <condition_variable>

namespace tc {
	struct WorkerPool {
		public:
			WorkerPool();
			virtual ~WorkerPool();

			void initialize();
			void finalize();

			void enqueue_task(std::function<void()> /* function */);

			template <typename T>
			void enqueue_task(T&& closure) {
				auto handle = std::make_shared<T>(std::forward<T>(closure));
				this->enqueue_task(std::function<void()>([handle]{ (*handle)(); }));
			}

		private:
			bool _running = false;

			std::thread worker;
			std::mutex worker_lock;
			std::condition_variable worker_wait;

			std::deque<std::function<void()>> tasks;
	};
	extern WorkerPool* codec_workers;

	struct Chunk {
		char* memory;
		size_t length;
		size_t allocated_length;

		Chunk(size_t length) {
			memory = (char*) malloc(length);
			this->allocated_length = length;
			this->length = 0;
		}

		~Chunk() {
			free(memory);
		}
	};

	class NativeCodec : public Nan::ObjectWrap {
		public:
			struct CodecType {
				enum value {
					SPEEX_NARROWBAND,
					SPEEX_WIDEBAND,
					SPEEX_ULTRA_WIDEBAND,
					CELT_MONO,
					OPUS_VOICE,
					OPUS_MUSIC
				};

				static NAN_MODULE_INIT(Init);
				static NAN_METHOD(supported);
			};

			static NAN_MODULE_INIT(Init);
			static NAN_METHOD(NewInstance);

			static inline Nan::Persistent<v8::Function> & constructor() {
				static Nan::Persistent<v8::Function> my_constructor;
				return my_constructor;
			}

			explicit NativeCodec(CodecType::value type);
			virtual ~NativeCodec();

			virtual NAN_METHOD(initialize) = 0;
			virtual NAN_METHOD(finalize) = 0;

			virtual NAN_METHOD(encode) = 0;
			virtual NAN_METHOD(decode) = 0;

			static NAN_METHOD(function_encode);
			static NAN_METHOD(function_decode);
			static NAN_METHOD(function_initialize);
			static NAN_METHOD(function_finalize);

		protected:
			CodecType::value type;

	};
}