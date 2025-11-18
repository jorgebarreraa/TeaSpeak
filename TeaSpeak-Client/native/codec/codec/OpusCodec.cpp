#include <iostream>
#include <chrono>
#include <thread>
#include <memory>
#include "OpusCodec.h"
#include "NativeCodec.h"
#include "include/NanException.h"
#include "include/NanEventCallback.h"

using namespace std;
using namespace std::chrono;
using namespace tc;
using namespace v8;
using namespace Nan;

bool OpusCodec::supported() {
#ifdef HAVE_OPUS
	return true;
#endif
	return false;
}

#ifdef HAVE_OPUS
OpusCodec::OpusCodec(NativeCodec::CodecType::value type) : NativeCodec(type) {
	cout << "New opus instance" << endl;

	this->sampling_rate = 48000;
	this->frames = 960;
	if(type == CodecType::OPUS_MUSIC) {
		this->channels = 2;
	} else {
		this->channels = 1;
	}
}

OpusCodec::~OpusCodec() {
	cout << "Free opus instance" << endl;

	if(this->decoder || this->encoder) {
		NAN_THROW_EXCEPTION(Error, "please finalize before releasing!");
		return;
	}
}

NAN_METHOD(OpusCodec::initialize) {
	int error = 0;
	lock_guard lock(this->coder_lock);

	this->encoder = opus_encoder_create(this->sampling_rate, this->channels, this->type == CodecType::OPUS_MUSIC ? OPUS_APPLICATION_AUDIO : OPUS_APPLICATION_VOIP, &error);
	if(!this->encoder || error) {
		NAN_THROW_EXCEPTION(Error, ("Failed to create encoder (" + to_string(error) + ")").c_str());
		return;
	}

	this->decoder = opus_decoder_create(this->sampling_rate, this->channels, &error);
	if(!this->encoder || error) {
		opus_encoder_destroy(this->encoder);
		this->encoder = nullptr;

		NAN_THROW_EXCEPTION(Error, ("Failed to create decoder (" + to_string(error) + ")").c_str());
		return;
	}
}

NAN_METHOD(OpusCodec::finalize) {
	lock_guard lock(this->coder_lock);

	opus_encoder_destroy(this->encoder);
	this->encoder = nullptr;

	opus_decoder_destroy(this->decoder);
	this->decoder = nullptr;
}

NAN_METHOD(OpusCodec::encode) {
	Nan::HandleScope scope;

	if(info.Length() != 3 || !info[0]->IsArrayBuffer()) {
		NAN_THROW_EXCEPTION(Error, "Invalid arguments");
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
				int result;
				{
					lock_guard lock(this->coder_lock);
					if(!this->encoder) {
						callback(nullptr, "Please initialize first!");
						return;
					}

					if(this->channels == 1) {
						result = opus_encode_float(this->encoder, (float*) buffer->memory, (int) (buffer->length / sizeof(float) / this->channels), (u_char*) buffer->memory, (opus_int32) buffer->allocated_length);
					} else {
						auto samples = buffer->length / sizeof(float) / this->channels;
						float* local_buffer = new float[samples * this->channels];
						for(size_t channel = 0; channel < this->channels; channel++)
							for(size_t sample = 0; sample < samples; sample++)
								local_buffer[sample * this->channels + channel] = ((float*) buffer->memory)[channel * samples + sample];

						result = opus_encode_float(this->encoder, local_buffer, samples, (u_char*) buffer->memory, (opus_int32) buffer->allocated_length);
						delete[] local_buffer;
					}
				}

				if(result <= 0) {
					callback(nullptr, "Invalid return code (" + to_string(result) + ")");
				} else {
					buffer->length = result;
					callback(move(buffer), "");
				}
			}
	);
}

NAN_METHOD(OpusCodec::decode) {
	Nan::HandleScope scope;

	if(!info[0]->IsArrayBuffer()) {
		NAN_THROW_EXCEPTION(Error, "First argument isn't an array buffer!");
		return;
	}

	auto js_buffer = info[0].As<ArrayBuffer>()->GetContents();
	auto buffer = make_unique<Chunk>(max(js_buffer.ByteLength(), this->channels * this->frames * sizeof(float)));
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
			[this, buffer = move(buffer), callback]() mutable {
				int result;

				{
					lock_guard lock(this->coder_lock);
					if(!this->decoder) {
						callback(nullptr, "Please initialize first!");
						return;
					}

					if(this->channels == 1) {
						result = opus_decode_float(this->decoder, (u_char*) buffer->memory, (opus_int32) buffer->length, (float*) buffer->memory, this->frames, 0);
					} else {
						float* local_buffer = new float[this->frames * this->channels];
						result = opus_decode_float(this->decoder, (u_char*) buffer->memory, (opus_int32) buffer->length, (float*) local_buffer, this->frames, 0);

						for(size_t channel = 0; channel < this->channels; channel++)
							for(size_t sample = 0; sample < result; sample++)
								((float*) buffer->memory)[channel * this->frames + sample] = local_buffer[sample * this->channels + channel];
						delete[] local_buffer;
					}
				}

				if(result <= 0) {
					callback(nullptr, "Invalid return code (" + to_string(result) + ")");
				} else {
					buffer->length = result * sizeof(float) * this->channels;
					callback(move(buffer), "");
				}
			}
	);
}
#endif