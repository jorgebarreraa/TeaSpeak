#include <iostream>
#include <chrono>
#include <thread>
#include <memory>
#include "SpeexCodec.h"
#include "NativeCodec.h"
#include "include/NanException.h"
#include "include/NanEventCallback.h"

using namespace std;
using namespace std::chrono;
using namespace tc;
using namespace v8;
using namespace Nan;

bool SpeexCodec::supported() {
#ifdef HAVE_SPEEX
	return true;
#endif
	return false;
}

#ifdef HAVE_SPEEX

SpeexCodec::SpeexCodec(NativeCodec::CodecType::value type) : NativeCodec(type) {
	cout << "Allocate speex instance" << endl;

}

SpeexCodec::~SpeexCodec() {
	cout << "Free speex instance" << endl;

	if(this->decoder || this->encoder) {
		NAN_THROW_EXCEPTION(Error, "please finalize before releasing!");
		return;
	}
}

NAN_METHOD(SpeexCodec::initialize) {
	lock_guard lock(this->coder_lock);

	const SpeexMode* speex_mode = nullptr;

	if(this->type == CodecType::SPEEX_NARROWBAND)
		speex_mode = &speex_nb_mode;
	else if(this->type == CodecType::SPEEX_WIDEBAND)
		speex_mode = &speex_wb_mode;
	else if(this->type == CodecType::SPEEX_ULTRA_WIDEBAND)
		speex_mode = &speex_uwb_mode;

	assert(speex_mode);
	{

		this->encoder = speex_encoder_init(speex_mode);
		if(!this->encoder) {
			NAN_THROW_EXCEPTION(Error, "Failed to create encoder");
			return;
		}

		/*Set the quality to 8 (15 kbps)*/
		int tmp = 8; //FIXME configurable
		speex_encoder_ctl(this->encoder, SPEEX_SET_QUALITY, &tmp);

		speex_encoder_ctl(this->encoder, SPEEX_GET_FRAME_SIZE, &this->frame_size);
	}
	{
		this->decoder = speex_decoder_init(speex_mode);
		if(!this->decoder) {
			speex_encoder_destroy(this->encoder);
			this->encoder = nullptr;

			NAN_THROW_EXCEPTION(Error, "Failed to create decoder");
			return;
		}

		int tmp = 1; //TODO What is this?
		speex_decoder_ctl(this->decoder, SPEEX_SET_ENH, &tmp);


		int tmp_frame_size;
		speex_encoder_ctl(this->encoder, SPEEX_GET_FRAME_SIZE, &tmp_frame_size);
		if(tmp_frame_size != this->frame_size) {
			NAN_THROW_EXCEPTION(Error, "Decoder and encoder have different frame sizes!");
			return;
		}
	}

	speex_bits_init(&this->encoder_bits);
	speex_bits_init(&this->decoder_bits);
}

NAN_METHOD(SpeexCodec::finalize) {
	lock_guard lock(this->coder_lock);

	if(this->encoder) {
		speex_encoder_destroy(this->encoder);
		this->encoder = nullptr;

		speex_bits_destroy(&this->encoder_bits);
	}

	if(this->decoder) {
		speex_decoder_destroy(this->decoder);
		this->decoder = nullptr;

		speex_bits_destroy(&this->decoder_bits);
	}
}


NAN_METHOD(SpeexCodec::encode) {
	Nan::HandleScope scope;

	if(!info[0]->IsArrayBuffer()) {
		NAN_THROW_EXCEPTION(Error, "First argument isn't an array buffer!");
		return;
	}

	auto js_buffer = info[0].As<ArrayBuffer>()->GetContents();
	auto buffer = make_unique<Chunk>(max(js_buffer.ByteLength(), 256UL));
	buffer->length = js_buffer.ByteLength();
	memcpy(buffer->memory, js_buffer.Data(), js_buffer.ByteLength());

	auto callback_success = make_unique<Nan::Callback>(info[1].As<Function>());
	auto callback_error = make_unique<Nan::Callback>(info[2].As<Function>());

	auto codec = make_unique<v8::Persistent<Object>>(info.GetIsolate(), info.Holder());
	auto callback = Nan::async_callback([callback_success = move(callback_success), callback_error = move(callback_error), codec = move(codec)](std::unique_ptr<Chunk> result, std::string error) {
		Nan::HandleScope scope;
		if(result) {
			auto _buffer = v8::ArrayBuffer::New(v8::Isolate::GetCurrent(), result->length);
			memcpy(_buffer->GetContents().Data(), result->memory, result->length);

			Local<Value> argv[] = { _buffer }; //_buffer
			callback_success->Call(1, argv);
		} else  {
			Local<Value> argv[] = { Nan::New<String>(error).ToLocalChecked() }; //error
			callback_error->Call(1, argv);
		}
		codec->Reset();
	}).option_destroyed_execute(true);

	tc::codec_workers->enqueue_task(
			[this, callback, buffer = move(buffer)]() mutable {
				{
					lock_guard lock(this->coder_lock);
					if(!this->encoder) {
						callback(nullptr, "Please initialize first!");
						return;
					}

					if(buffer->length < this->frame_size * sizeof(float)) {
						callback(nullptr, "Input buffer to short! Received " + to_string(buffer->length) + ", Expected " + to_string(this->frame_size * sizeof(float)));
						return;
					}

					for(size_t frame = 0; frame < this->frame_size; frame++)
						((float*) buffer->memory)[frame] *= 8000; //We want a range between 0 and 8000

					speex_bits_reset(&this->encoder_bits);
					speex_encode(this->encoder, (float*) buffer->memory, &this->encoder_bits);
					auto nbytes = speex_bits_write(&this->encoder_bits, buffer->memory, buffer->allocated_length);

					if(nbytes < 0) {
						callback(nullptr, "Invalid write?");
						return;
					}
					buffer->length = nbytes;
				}
				callback(move(buffer), "");
			}
	);
}

NAN_METHOD(SpeexCodec::decode) {
	Nan::HandleScope scope;

	if(!info[0]->IsArrayBuffer()) {
		NAN_THROW_EXCEPTION(Error, "First argument isn't an array buffer!");
		return;
	}

	auto js_buffer = info[0].As<ArrayBuffer>()->GetContents();
	auto buffer = make_unique<Chunk>(max(js_buffer.ByteLength(), this->frame_size * sizeof(float)));
	buffer->length = js_buffer.ByteLength();
	memcpy(buffer->memory, js_buffer.Data(), js_buffer.ByteLength());

	auto callback_success = make_unique<Nan::Callback>(info[1].As<Function>());
	auto callback_error = make_unique<Nan::Callback>(info[2].As<Function>());

	auto codec = make_unique<v8::Persistent<Object>>(info.GetIsolate(), info.Holder());
	auto callback = Nan::async_callback([callback_success = move(callback_success), callback_error = move(callback_error), codec = move(codec)](std::unique_ptr<Chunk> result, std::string error) {
		Nan::HandleScope scope;
		if(result) {
			auto _buffer = v8::ArrayBuffer::New(v8::Isolate::GetCurrent(), result->length);
			memcpy(_buffer->GetContents().Data(), result->memory, result->length);
			Local<Value> argv[] = { _buffer };
			callback_success->Call(1, argv);
		} else  {
			Local<Value> argv[] = { Nan::New<String>(error).ToLocalChecked() };
			callback_error->Call(1, argv);
		}
		codec->Reset();
	}).option_destroyed_execute(true);

	tc::codec_workers->enqueue_task(
			[this, callback, buffer = move(buffer)]() mutable {
				int result;

				{
					lock_guard lock(this->coder_lock);
					if(!this->decoder) {
						callback(nullptr, "Please initialize first!");
						return;
					}

					speex_bits_reset(&this->decoder_bits);
					speex_bits_read_from(&this->decoder_bits, buffer->memory, buffer->length);
					auto state = speex_decode(this->decoder, &this->decoder_bits, (float*) buffer->memory);
					if(state != 0) {
						callback(nullptr, "decode failed (" + to_string(state) + ")");
						return;
					}
					buffer->length = this->frame_size * sizeof(float);
					for(size_t frame = 0; frame < this->frame_size; frame++)
						((float*) buffer->memory)[frame] /= 8000; //We want a range between 0 and 1
				}

				callback(move(buffer), "");
			}
	);
}

#endif