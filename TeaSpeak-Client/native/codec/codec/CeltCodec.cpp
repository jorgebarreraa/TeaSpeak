#include <iostream>
#include <chrono>
#include <thread>
#include <memory>
#include "CeltCodec.h"
#include "NativeCodec.h"
#include "include/NanException.h"
#include "include/NanEventCallback.h"

using namespace std;
using namespace std::chrono;
using namespace tc;
using namespace v8;
using namespace Nan;

bool CeltCodec::supported() {
#ifdef HAVE_CELT
	return true;
#endif
	return false;
}

#ifdef HAVE_CELT

CeltCodec::CeltCodec(NativeCodec::CodecType::value type) : NativeCodec(type) {
	cout << "Allocate celt instance" << endl;

}

CeltCodec::~CeltCodec() {
	cout << "Free celt instance" << endl;

	if(this->decoder || this->encoder) {
		NAN_THROW_EXCEPTION(Error, "please finalize before releasing!");
		return;
	}
}

NAN_METHOD(CeltCodec::initialize) {
	lock_guard lock(this->coder_lock);

	//(≥20kHz; sample rates from 8 kHz to 48 kHz)
	int error = 0;

	int sample_rate = 48000;
	this->encoder = celt_encoder_create(sample_rate, this->channels, &error);
	if(!this->encoder || error) {
		cout << this->encoder << " - " << error << endl;
		if(this->encoder)
			celt_encoder_destroy(this->encoder);
		this->encoder = nullptr;

		NAN_THROW_EXCEPTION(Error, ("Failed to create encoder (" + to_string(error) + ")").c_str());
		return;
	}

	this->decoder = celt_decoder_create(sample_rate, this->channels, &error);
	if(!this->decoder || error) {
		if(this->decoder)
			celt_decoder_destroy(this->decoder);
		this->decoder = nullptr;
		if(this->encoder)
			celt_encoder_destroy(this->encoder);

		this->encoder = nullptr;
		NAN_THROW_EXCEPTION(Error, ("Failed to create decoder (" + to_string(error) + ")").c_str());
		return;
	}
}

NAN_METHOD(CeltCodec::finalize) {
	lock_guard lock(this->coder_lock);

	if(this->encoder) {
		celt_encoder_destroy(this->encoder);
		this->encoder = nullptr;
	}

	if(this->decoder) {
		celt_decoder_destroy(this->decoder);
		this->decoder = nullptr;
	}
}


NAN_METHOD(CeltCodec::encode) {
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

	auto frame_size = buffer->length / sizeof(float) / this->channels;
	if(frame_size != 64 && frame_size != 128 && frame_size != 256 && frame_size != 512) {
		NAN_THROW_EXCEPTION(Error, ("Invalid frame size (" + to_string(frame_size) + "). Only allow 64, 128, 256, 512bytes").c_str());
		return;
	}

	tc::codec_workers->enqueue_task(
			[this, callback, buffer = move(buffer)]() mutable {
				{
					lock_guard lock(this->coder_lock);
					if(!this->encoder) {
						callback(nullptr, "Please initialize first!");
						return;
					}

					auto nbytes = celt_encode_float(this->encoder, (float*) buffer->memory, buffer->length / sizeof(float) / this->channels, (u_char*) buffer->memory, buffer->allocated_length);
					if(nbytes < 0) {
						callback(nullptr, "Invalid encode return code (" + to_string(nbytes) + ")");
						return;
					}

					buffer->length = nbytes;
				}
				callback(move(buffer), "");
			}
	);
}

NAN_METHOD(CeltCodec::decode) {
	Nan::HandleScope scope;

	if(!info[0]->IsArrayBuffer()) {
		NAN_THROW_EXCEPTION(Error, "First argument isn't an array buffer!");
		return;
	}

	auto js_buffer = info[0].As<ArrayBuffer>()->GetContents();
	auto buffer = make_unique<Chunk>(max(js_buffer.ByteLength(), this->max_frame_size * this->channels * sizeof(float)));
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

					auto code = celt_decode_float(this->decoder, (u_char*) buffer->memory, buffer->length, (float*) buffer->memory, buffer->allocated_length / sizeof(float) / this->channels);
					if(code < 0) {
						callback(nullptr, "Invalid decode return code (" + to_string(code) + ")");
						return;
					}
					cout << code << endl;

					buffer->length = this->channels * this->max_frame_size * sizeof(float);
				}

				callback(move(buffer), "");
			}
	);
}

#endif