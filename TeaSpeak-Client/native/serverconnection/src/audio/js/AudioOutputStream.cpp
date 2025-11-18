#include <cmath>
#include "AudioOutputStream.h"
#include "../AudioOutput.h"
#include "../AudioResampler.h"

using namespace std;
using namespace tc;
using namespace tc::audio;

NAN_MODULE_INIT(AudioOutputStreamWrapper::Init) {
	auto klass = Nan::New<v8::FunctionTemplate>(AudioOutputStreamWrapper::NewInstance);
	klass->SetClassName(Nan::New("AudioOutputStream").ToLocalChecked());
	klass->InstanceTemplate()->SetInternalFieldCount(1);

	Nan::SetPrototypeMethod(klass, "write_data", AudioOutputStreamWrapper::_write_data);

	Nan::SetPrototypeMethod(klass, "get_buffer_latency", AudioOutputStreamWrapper::_get_buffer_latency);
	Nan::SetPrototypeMethod(klass, "set_buffer_latency", AudioOutputStreamWrapper::_set_buffer_latency);

	Nan::SetPrototypeMethod(klass, "get_buffer_max_latency", AudioOutputStreamWrapper::_get_buffer_max_latency);
	Nan::SetPrototypeMethod(klass, "set_buffer_max_latency", AudioOutputStreamWrapper::_set_buffer_max_latency);

	Nan::SetPrototypeMethod(klass, "flush_buffer", AudioOutputStreamWrapper::_flush_buffer);

	Nan::SetPrototypeMethod(klass, "write_data", AudioOutputStreamWrapper::_write_data);
	Nan::SetPrototypeMethod(klass, "write_data_rated", AudioOutputStreamWrapper::_write_data_rated);
	Nan::SetPrototypeMethod(klass, "deleted", AudioOutputStreamWrapper::_deleted);
	Nan::SetPrototypeMethod(klass, "delete", AudioOutputStreamWrapper::_delete);
	Nan::SetPrototypeMethod(klass, "clear", AudioOutputStreamWrapper::_clear);

	constructor().Reset(Nan::GetFunction(klass).ToLocalChecked());
}

NAN_METHOD(AudioOutputStreamWrapper::NewInstance) {
	if(!info.IsConstructCall()) {
        Nan::ThrowError("invalid invoke!");
	}
}


AudioOutputStreamWrapper::AudioOutputStreamWrapper(const std::shared_ptr<tc::audio::AudioOutputSource> &stream, bool owns) {
	this->_handle = stream;
	if(owns) {
		this->_own_handle = stream;
	}
}

AudioOutputStreamWrapper::~AudioOutputStreamWrapper() {
	this->drop_stream();
}

void AudioOutputStreamWrapper::drop_stream() {
	if(this->_own_handle) {
		this->_own_handle->on_underflow = nullptr;
		this->_own_handle->on_overflow = nullptr;
	}

	this->_handle.reset();
	this->_own_handle = nullptr;
}

void AudioOutputStreamWrapper::do_wrap(const v8::Local<v8::Object> &obj) {
	this->Wrap(obj);

	auto handle = this->_handle.lock();
	if(!handle) {
		Nan::ThrowError("weak handle");
		return;
	}

	Nan::DefineOwnProperty(this->handle(), Nan::New<v8::String>("sample_rate").ToLocalChecked(), Nan::New<v8::Number>((uint32_t) handle->sample_rate()), v8::ReadOnly);
	Nan::DefineOwnProperty(this->handle(), Nan::New<v8::String>("channels").ToLocalChecked(), Nan::New<v8::Number>((uint32_t) handle->channel_count()), v8::ReadOnly);

	if(this->_own_handle) {
		this->call_underflow = Nan::async_callback([&]{

			Nan::HandleScope scope;
			auto handle = this->handle();
			auto callback = Nan::Get(handle, Nan::New<v8::String>("callback_underflow").ToLocalChecked()).ToLocalChecked();
			if(callback->IsFunction())
				callback.As<v8::Function>()->Call(Nan::GetCurrentContext(), Nan::Undefined(), 0, nullptr);
		});
		this->call_overflow = Nan::async_callback([&]{
			Nan::HandleScope scope;
			auto handle = this->handle();
			auto callback = Nan::Get(handle, Nan::New<v8::String>("callback_overflow").ToLocalChecked()).ToLocalChecked();
			if(callback->IsFunction())
				callback.As<v8::Function>()->Call(Nan::GetCurrentContext(), Nan::Undefined(), 0, nullptr);
		});

		this->_own_handle->on_overflow = [&](size_t){ this->call_overflow(); };
		this->_own_handle->on_underflow = [&](size_t){ this->call_underflow(); return false; };
	}
}


NAN_METHOD(AudioOutputStreamWrapper::_clear) {
	auto client = ObjectWrap::Unwrap<AudioOutputStreamWrapper>(info.Holder());

	auto handle = client->_own_handle;
	if(!handle) {
		Nan::ThrowError("invalid handle");
		return;
	}

	handle->clear();
}

NAN_METHOD(AudioOutputStreamWrapper::_deleted) {
	auto client = ObjectWrap::Unwrap<AudioOutputStreamWrapper>(info.Holder());

	info.GetReturnValue().Set(!client->_own_handle);
}

NAN_METHOD(AudioOutputStreamWrapper::_delete) {
	auto client = ObjectWrap::Unwrap<AudioOutputStreamWrapper>(info.Holder());
	client->drop_stream();
}

ssize_t AudioOutputStreamWrapper::write_data(const std::shared_ptr<AudioOutputSource>& handle, void *source, size_t samples, bool interleaved) {
	if(interleaved) {
        return handle->enqueue_samples(source, samples);
	} else {
        return handle->enqueue_samples_no_interleave(source, samples);
    }
}

NAN_METHOD(AudioOutputStreamWrapper::_write_data) {
	auto client = ObjectWrap::Unwrap<AudioOutputStreamWrapper>(info.Holder());

	auto handle = client->_own_handle;
	if(!handle) {
		Nan::ThrowError("invalid handle");
		return;
	}

	if(info.Length() != 2 || !info[0]->IsArrayBuffer() || !info[1]->IsBoolean()) {
		Nan::ThrowError("Invalid arguments");
		return;
	}

	auto interleaved = info[1]->BooleanValue(info.GetIsolate());
	auto js_buffer = info[0].As<v8::ArrayBuffer>()->GetContents();

	if(js_buffer.ByteLength() % (handle->channel_count() * 4) != 0) {
		Nan::ThrowError("input buffer invalid size");
		return;
	}

	auto samples = js_buffer.ByteLength() / handle->channel_count() / 4;
	info.GetReturnValue().Set((int32_t) write_data(handle, js_buffer.Data(), samples, interleaved));
}

NAN_METHOD(AudioOutputStreamWrapper::_write_data_rated) {
	auto client = ObjectWrap::Unwrap<AudioOutputStreamWrapper>(info.Holder());

	auto handle = client->_own_handle;
	if(!handle) {
		Nan::ThrowError("invalid handle");
		return;
	}

	if(info.Length() != 3 || !info[0]->IsArrayBuffer() || !info[1]->IsBoolean() || !info[2]->IsNumber()) {
		Nan::ThrowError("Invalid arguments");
		return;
	}

	auto sample_rate = info[2]->NumberValue(Nan::GetCurrentContext()).FromMaybe(0);
	auto interleaved = info[1]->BooleanValue(info.GetIsolate());
	auto js_buffer = info[0].As<v8::ArrayBuffer>()->GetContents();

	auto samples = js_buffer.ByteLength() / handle->channel_count() / 4;
	if(sample_rate == handle->sample_rate()) {
		info.GetReturnValue().Set((int32_t) write_data(handle, js_buffer.Data(), samples, interleaved));
	} else {
		if(!client->_resampler || client->_resampler->input_rate() != sample_rate)
			client->_resampler = make_unique<AudioResampler>((size_t) sample_rate, handle->sample_rate(), handle->channel_count());

		if(!client->_resampler || !client->_resampler->valid()) {
			Nan::ThrowError("Resampling failed (invalid resampler)");
			return;
		}

		//TODO: Use a tmp preallocated buffer here!
		size_t target_samples = client->_resampler->estimated_output_size(samples);
		auto buffer = SampleBuffer::allocate((uint8_t) handle->channel_count(), max((uint16_t) samples, (uint16_t) target_samples));
		auto source_buffer = js_buffer.Data();
		if(!interleaved) {
			auto src_buffer = (float*) js_buffer.Data();
			auto target_buffer = (float*) buffer->sample_data;

			auto samples_count = samples;
			while (samples_count-- > 0) {
				*target_buffer = *src_buffer;
				*(target_buffer + 1) = *(src_buffer + samples);

				target_buffer += 2;
				src_buffer++;
			}
			source_buffer = buffer->sample_data;
		}

		if(!client->_resampler->process(buffer->sample_data, source_buffer, samples, target_samples)) {
			Nan::ThrowError("Resampling failed");
			return;
		}

		buffer->sample_index = 0;
		buffer->sample_size = (uint16_t) target_samples;
		info.GetReturnValue().Set((int32_t) handle->enqueue_samples(buffer->sample_data, target_samples));
	}
}

NAN_METHOD(AudioOutputStreamWrapper::_get_buffer_latency) {
	auto client = ObjectWrap::Unwrap<AudioOutputStreamWrapper>(info.Holder());

	auto handle = client->_handle.lock();
	if(!handle) {
		Nan::ThrowError("weak handle");
		return;
	}

	info.GetReturnValue().Set((float) handle->min_buffered_samples() / (float) handle->sample_rate());
}

NAN_METHOD(AudioOutputStreamWrapper::_set_buffer_latency) {
	auto client = ObjectWrap::Unwrap<AudioOutputStreamWrapper>(info.Holder());

	auto handle = client->_handle.lock();
	if(!handle) {
		Nan::ThrowError("weak handle");
		return;
	}

	if(info.Length() != 1 || !info[0]->IsNumber()) {
		Nan::ThrowError("Invalid arguments");
		return;
	}

	handle->set_min_buffered_samples((size_t) ceil(handle->sample_rate() * info[0]->NumberValue(Nan::GetCurrentContext()).FromMaybe(0)));
}

NAN_METHOD(AudioOutputStreamWrapper::_get_buffer_max_latency) {
	auto client = ObjectWrap::Unwrap<AudioOutputStreamWrapper>(info.Holder());

	auto handle = client->_handle.lock();
	if(!handle) {
		Nan::ThrowError("weak handle");
		return;
	}

	info.GetReturnValue().Set((float) handle->max_buffering() / (float) handle->sample_rate());
}

NAN_METHOD(AudioOutputStreamWrapper::_set_buffer_max_latency) {
	auto client = ObjectWrap::Unwrap<AudioOutputStreamWrapper>(info.Holder());

	auto handle = client->_handle.lock();
	if(!handle) {
		Nan::ThrowError("weak handle");
		return;
	}

	if(info.Length() != 1 || !info[0]->IsNumber()) {
		Nan::ThrowError("Invalid arguments");
		return;
	}

	handle->set_max_buffered_samples((size_t) ceil(handle->sample_rate() * info[0]->NumberValue(Nan::GetCurrentContext()).FromMaybe(0)));
}

NAN_METHOD(AudioOutputStreamWrapper::_flush_buffer) {
	auto client = ObjectWrap::Unwrap<AudioOutputStreamWrapper>(info.Holder());

	auto handle = client->_handle.lock();
	if(!handle) {
		Nan::ThrowError("weak handle");
		return;
	}

	handle->clear();
}