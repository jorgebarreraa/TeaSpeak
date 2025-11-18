#include <chrono>
#include <thread>
#include <memory>
#include "NativeCodec.h"
#include "OpusCodec.h"
#include "SpeexCodec.h"
#include "CeltCodec.h"
#include "include/NanException.h"
#include <iostream>

using namespace std;
using namespace std::chrono;
using namespace tc;
using namespace v8;
using namespace Nan;

#define DEFINE_ENUM_ENTRY(name, value) \
	Nan::ForceSet(types, Nan::New<v8::String>(name).ToLocalChecked(), Nan::New<v8::Number>(value), static_cast<PropertyAttribute>(ReadOnly|DontDelete)); \
	//Nan::ForceSet(types, Nan::New<v8::Number>(value), Nan::New<v8::String>(name).ToLocalChecked(), static_cast<PropertyAttribute>(ReadOnly|DontDelete));

NAN_MODULE_INIT(NativeCodec::CodecType::Init) {
	auto types = Nan::New<v8::Object>();

	DEFINE_ENUM_ENTRY("OPUS_VOICE", CodecType::OPUS_VOICE);
	DEFINE_ENUM_ENTRY("OPUS_MUSIC", CodecType::OPUS_MUSIC);
	DEFINE_ENUM_ENTRY("SPEEX_NARROWBAND", CodecType::SPEEX_NARROWBAND);
	DEFINE_ENUM_ENTRY("SPEEX_WIDEBAND", CodecType::SPEEX_WIDEBAND);
	DEFINE_ENUM_ENTRY("SPEEX_ULTRA_WIDEBAND", CodecType::SPEEX_ULTRA_WIDEBAND);
	DEFINE_ENUM_ENTRY("CELT_MONO", CodecType::CELT_MONO);

	Nan::Set(target, Nan::New<v8::String>("CodecTypes").ToLocalChecked(), types);
	Nan::Set(target, Nan::New<v8::String>("codec_supported").ToLocalChecked(), Nan::New<v8::Function>(NativeCodec::CodecType::supported));
}

NAN_METHOD(NativeCodec::CodecType::supported) {
	if(!info[0]->IsNumber()) {
		NAN_THROW_EXCEPTION(Error, "Argument 0 shall be a number!");
		return;
	}

	auto type = (CodecType::value) Nan::To<int>(info[0]).FromJust();
	if(type == CodecType::OPUS_MUSIC || type == CodecType::OPUS_VOICE) {
		info.GetReturnValue().Set(OpusCodec::supported());
	} else if(type == CodecType::SPEEX_NARROWBAND || type == CodecType::SPEEX_WIDEBAND || type == CodecType::SPEEX_ULTRA_WIDEBAND) {
		info.GetReturnValue().Set(SpeexCodec::supported());
	} else if(type == CodecType::CELT_MONO) {
		info.GetReturnValue().Set(CeltCodec::supported());
	} else {
		NAN_THROW_EXCEPTION(Error, "Invalid type!");
		return;
	}
}

NAN_MODULE_INIT(NativeCodec::Init) {
	auto klass = New<FunctionTemplate>(NewInstance);
	klass->SetClassName(Nan::New("NativeCodec").ToLocalChecked());
	klass->InstanceTemplate()->SetInternalFieldCount(1);

	Nan::SetPrototypeMethod(klass, "decode", NativeCodec::function_decode);
	Nan::SetPrototypeMethod(klass, "encode", NativeCodec::function_encode);
	Nan::SetPrototypeMethod(klass, "initialize", NativeCodec::function_initialize);
	Nan::SetPrototypeMethod(klass, "finalize", NativeCodec::function_finalize);

	constructor().Reset(Nan::GetFunction(klass).ToLocalChecked());
	//Nan::Set(target, Nan::New("NativeCodec").ToLocalChecked(), Nan::GetFunction(klass).ToLocalChecked());
}

NAN_METHOD(NativeCodec::NewInstance) {
	if (info.IsConstructCall()) {
		if(!info[0]->IsNumber()) {
			NAN_THROW_EXCEPTION(Error, "Argument 0 shall be a number!");
			return;
		}

		auto type = (CodecType::value) Nan::To<int>(info[0]).FromJust();
		std::unique_ptr<NativeCodec> instance;
		if(type == CodecType::OPUS_MUSIC || type == CodecType::OPUS_VOICE) {
			#ifdef HAVE_OPUS
			instance = make_unique<OpusCodec>(type);
			#endif
		} else if(type == CodecType::SPEEX_NARROWBAND || type == CodecType::SPEEX_WIDEBAND || type == CodecType::SPEEX_ULTRA_WIDEBAND) {
			#ifdef HAVE_SPEEX
			instance = make_unique<SpeexCodec>(type);
			#endif
		} else if(type == CodecType::CELT_MONO) {
			#ifdef HAVE_CELT
			instance = make_unique<CeltCodec>(type);
			#endif
		} else {
			NAN_THROW_EXCEPTION(Error, "Invalid type!");
			return;
		}
		if(!instance) {
			NAN_THROW_EXCEPTION(Error, "Target codec isn't supported!");
			return;
		}
		instance.release()->Wrap(info.This());

		Nan::Set(info.This(), New<String>("type").ToLocalChecked(), New<Number>(type));
		info.GetReturnValue().Set(info.This());
	} else {
		const int argc = 1;
		v8::Local<v8::Value> argv[argc] = {info[0]};
		v8::Local<v8::Function> cons = Nan::New(constructor());

		Nan::TryCatch try_catch;
		auto result = Nan::NewInstance(cons, argc, argv);
		if(try_catch.HasCaught()) {
			try_catch.ReThrow();
			return;
		}
		info.GetReturnValue().Set(result.ToLocalChecked());
	}
}

NAN_METHOD(NativeCodec::function_decode) {
	if(info.Length() != 3) {
		NAN_THROW_EXCEPTION(Error, "Invalid argument count!");
		return;
	}
	if(!info[0]->IsArrayBuffer() || !info[1]->IsFunction() || !info[2]->IsFunction()) {
		NAN_THROW_EXCEPTION(Error, "Invalid argument types!");
		return;
	}

	auto codec = ObjectWrap::Unwrap<NativeCodec>(info.Holder());
	codec->decode(info);
}

NAN_METHOD(NativeCodec::function_encode) {
	if(info.Length() != 3) {
		NAN_THROW_EXCEPTION(Error, "Invalid argument count!");
		return;
	}
	if(!info[0]->IsArrayBuffer() || !info[1]->IsFunction() || !info[2]->IsFunction()) {
		NAN_THROW_EXCEPTION(Error, "Invalid argument types!");
		return;
	}

	auto codec = ObjectWrap::Unwrap<NativeCodec>(info.Holder());
	codec->encode(info);
}

NAN_METHOD(NativeCodec::function_initialize) {
	auto codec = ObjectWrap::Unwrap<NativeCodec>(info.Holder());
	codec->initialize(info);
}
NAN_METHOD(NativeCodec::function_finalize) {
	auto codec = ObjectWrap::Unwrap<NativeCodec>(info.Holder());
	codec->finalize(info);
}

NativeCodec::NativeCodec(tc::NativeCodec::CodecType::value type) : type(type) {}
NativeCodec::~NativeCodec() {}


WorkerPool::WorkerPool() {}
WorkerPool::~WorkerPool() {}

void WorkerPool::initialize() {
	assert(!this->_running);

	this->_running = true;
	this->worker = thread([&]{
		while(this->_running) {
			function<void()> worker;
			{
				unique_lock lock(this->worker_lock);
				this->worker_wait.wait_for(lock, minutes(1), [&]{ return !this->_running || !this->tasks.empty(); });
				if(!this->_running) break;
				if(this->tasks.empty()) continue;

				worker = move(this->tasks.front());
				this->tasks.pop_front();
			}

			try {
				worker();
			} catch(std::exception& ex) {
				cerr << "failed to invoke opus worker! message: " << ex.what() << endl;
			} catch (...) {
				cerr << "failed to invoke opus worker!" << endl;
			}
		}
	});
#ifndef WIN32
	auto worker_handle = this->worker.native_handle();
	pthread_setname_np(worker_handle, "Codec Worker");
#endif
}

void WorkerPool::finalize() {
	this->_running = false;
	this->worker_wait.notify_all();
	this->tasks.clear();

	if(this->worker.joinable())
		this->worker.join();
}

void WorkerPool::enqueue_task(std::function<void()> task) {
	lock_guard lock(this->worker_lock);
	this->tasks.push_back(std::move(task));
	this->worker_wait.notify_one();
}