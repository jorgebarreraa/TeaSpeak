#include <utility>
#include <NanStrings.h>

#include "./AudioRecorder.h"
#include "./AudioConsumer.h"
#include "./AudioLevelMeter.h"
#include "./AudioProcessor.h"
#include "../AudioInput.h"
#include "../AudioLevelMeter.h"
#include "../../logger.h"

using namespace std;
using namespace tc::audio;
using namespace tc::audio::recorder;

NAN_MODULE_INIT(recorder::init_js) {
	Nan::Set(target, Nan::New<v8::String>("create_recorder").ToLocalChecked(), Nan::GetFunction(Nan::New<v8::FunctionTemplate>(recorder::create_recorder)).ToLocalChecked());
}

NAN_METHOD(recorder::create_recorder) {
    if(!audio::initialized()) {
        Nan::ThrowError(tr("audio hasn't been initialized yet"));
        return;
    }
	auto input = std::make_shared<AudioInput>(2, 48000);
	auto wrapper = new AudioRecorderWrapper(input);
	auto js_object = Nan::NewInstance(Nan::New(AudioRecorderWrapper::constructor())).ToLocalChecked();
	wrapper->do_wrap(js_object);
	info.GetReturnValue().Set(js_object);
}


NAN_MODULE_INIT(AudioRecorderWrapper::Init) {
	auto klass = Nan::New<v8::FunctionTemplate>(AudioRecorderWrapper::NewInstance);
	klass->SetClassName(Nan::New("AudioRecorder").ToLocalChecked());
	klass->InstanceTemplate()->SetInternalFieldCount(1);

	Nan::SetPrototypeMethod(klass, "get_device", AudioRecorderWrapper::_get_device);
	Nan::SetPrototypeMethod(klass, "set_device", AudioRecorderWrapper::_set_device);

	Nan::SetPrototypeMethod(klass, "start", AudioRecorderWrapper::_start);
	Nan::SetPrototypeMethod(klass, "started", AudioRecorderWrapper::_started);
	Nan::SetPrototypeMethod(klass, "stop", AudioRecorderWrapper::_stop);

	Nan::SetPrototypeMethod(klass, "get_volume", AudioRecorderWrapper::_get_volume);
	Nan::SetPrototypeMethod(klass, "set_volume", AudioRecorderWrapper::_set_volume);

	Nan::SetPrototypeMethod(klass, "get_consumers", AudioRecorderWrapper::_get_consumers);
	Nan::SetPrototypeMethod(klass, "create_consumer", AudioRecorderWrapper::_create_consumer);
	Nan::SetPrototypeMethod(klass, "delete_consumer", AudioRecorderWrapper::_delete_consumer);

    Nan::SetPrototypeMethod(klass, "get_audio_processor", AudioRecorderWrapper::get_audio_processor);
    Nan::SetPrototypeMethod(klass, "create_level_meter", AudioRecorderWrapper::create_level_meter);

	constructor().Reset(Nan::GetFunction(klass).ToLocalChecked());
}

NAN_METHOD(AudioRecorderWrapper::NewInstance) {
	if(!info.IsConstructCall())
		Nan::ThrowError("invalid invoke!");
}


AudioRecorderWrapper::AudioRecorderWrapper(std::shared_ptr<tc::audio::AudioInput> handle) : input_(std::move(handle)) {
	log_allocate("AudioRecorderWrapper", this);
}
AudioRecorderWrapper::~AudioRecorderWrapper() {
	if(this->input_) {
		this->input_->stop();
		this->input_->close_device();
		this->input_ = nullptr;
	}
	{
		lock_guard lock{this->consumer_mutex};
		this->consumer_.clear();
	}
	log_free("AudioRecorderWrapper", this);
}

std::shared_ptr<AudioConsumerWrapper> AudioRecorderWrapper::create_consumer() {
	auto result = std::shared_ptr<AudioConsumerWrapper>(new AudioConsumerWrapper(this->input_), [](AudioConsumerWrapper* ptr) {
		assert(v8::Isolate::GetCurrent());
		ptr->Unref();
	});

	/* wrap into object */
	{
		auto js_object = Nan::NewInstance(Nan::New(AudioConsumerWrapper::constructor()), 0, nullptr).ToLocalChecked();
		result->do_wrap(js_object);
		result->Ref();
	}

	{
		lock_guard lock(this->consumer_mutex);
		this->consumer_.push_back(result);
	}

	return result;
}

void AudioRecorderWrapper::delete_consumer(const AudioConsumerWrapper* consumer) {
	std::shared_ptr<AudioConsumerWrapper> handle; /* need to keep the handle 'till everything has been finished */
	{
		lock_guard lock(this->consumer_mutex);
		for(auto& c : this->consumer_) {
			if(&*c == consumer) {
				handle = c;
				break;
			}
		}

		if(!handle) {
            return;
		}

		{
			auto it = find(this->consumer_.begin(), this->consumer_.end(), handle);
			if(it != this->consumer_.end()) {
                this->consumer_.erase(it);
			}
		}
	}

    handle->delete_consumer();
}

void AudioRecorderWrapper::do_wrap(const v8::Local<v8::Object> &obj) {
	this->Wrap(obj);
}

NAN_METHOD(AudioRecorderWrapper::_get_device) {
	auto handle = ObjectWrap::Unwrap<AudioRecorderWrapper>(info.Holder());
	auto input = handle->input_;

	auto device = input->current_device();
	if(device)
	    info.GetReturnValue().Set(Nan::LocalString(device->id()));
	else
        info.GetReturnValue().Set(Nan::Undefined());
}

NAN_METHOD(AudioRecorderWrapper::_set_device) {
	auto handle = ObjectWrap::Unwrap<AudioRecorderWrapper>(info.Holder());
	auto input = handle->input_;

	const auto is_null_device = info[0]->IsNullOrUndefined();
	if(info.Length() != 2 || !(is_null_device || info[0]->IsString()) || !info[1]->IsFunction()) {
		Nan::ThrowError("invalid arguments");
		return;
	}

	if(!audio::initialized()) {
	    Nan::ThrowError("audio hasn't been initialized yet");
	    return;
	}

    unique_ptr<Nan::Persistent<v8::Function>> _callback = make_unique<Nan::Persistent<v8::Function>>(info[1].As<v8::Function>());
    unique_ptr<Nan::Persistent<v8::Object>> _recorder = make_unique<Nan::Persistent<v8::Object>>(info.Holder());
	auto call_callback = [call = std::move(_callback), recorder = move(_recorder)](const std::string& status) {
        Nan::HandleScope scope;
        auto callback_function = call->Get(Nan::GetCurrentContext()->GetIsolate());

        v8::Local<v8::Value> args[1];
        args[0] = Nan::LocalStringUTF8(status);
        (void) callback_function->Call(Nan::GetCurrentContext(), Nan::Undefined(), 1, args);

        recorder->Reset();
        call->Reset();
	};

	auto device = is_null_device ? nullptr : audio::find_device_by_id(*Nan::Utf8String(info[0]), true);
    if(!device && !is_null_device) {
        call_callback("invalid-device");
        return;
    }

	auto _async_callback = Nan::async_callback([callback = std::move(call_callback)] {
        callback("success");
	}).option_destroyed_execute(true);

	std::thread([_async_callback, input, device]{
		input->set_device(device);
		_async_callback();
	}).detach();
}

NAN_METHOD(AudioRecorderWrapper::_start) {
	if(info.Length() != 1) {
		Nan::ThrowError("missing callback");
		return;
	}

	if(!info[0]->IsFunction()) {
		Nan::ThrowError("not a function");
		return;
	}

    auto input = ObjectWrap::Unwrap<AudioRecorderWrapper>(info.Holder())->input_;
	std::string error{};

    v8::Local<v8::Value> argv[1];
    if(input->record(error)) {
        argv[0] = Nan::New<v8::Boolean>(true);
    } else {
        argv[0] = Nan::LocalString(error);
    }
    (void) info[0].As<v8::Function>()->Call(Nan::GetCurrentContext(), Nan::Undefined(), 1, argv);
}

NAN_METHOD(AudioRecorderWrapper::_started) {
	auto handle = ObjectWrap::Unwrap<AudioRecorderWrapper>(info.Holder());
	auto input = handle->input_;

	info.GetReturnValue().Set(input->recording());
}

NAN_METHOD(AudioRecorderWrapper::_stop) {
	auto handle = ObjectWrap::Unwrap<AudioRecorderWrapper>(info.Holder());
	auto input = handle->input_;

	input->stop();
}

NAN_METHOD(AudioRecorderWrapper::_create_consumer) {
	auto handle = ObjectWrap::Unwrap<AudioRecorderWrapper>(info.Holder());
	auto consumer = handle->create_consumer();

	if(!consumer) {
		Nan::ThrowError("failed to create consumer");
		return;
	}

	info.GetReturnValue().Set(consumer->handle());
}

NAN_METHOD(AudioRecorderWrapper::_get_consumers) {
	auto handle = ObjectWrap::Unwrap<AudioRecorderWrapper>(info.Holder());
	auto consumers = handle->consumers();

	auto result = Nan::New<v8::Array>((uint32_t) consumers.size());

	for(uint32_t index = 0; index < consumers.size(); index++)
		Nan::Set(result, index, consumers[index]->handle());

	info.GetReturnValue().Set(result);
}

NAN_METHOD(AudioRecorderWrapper::_delete_consumer) {
	auto handle = ObjectWrap::Unwrap<AudioRecorderWrapper>(info.Holder());

	if(info.Length() != 1 || !info[0]->IsObject()) {
		Nan::ThrowError("invalid argument");
		return;
	}

	if(!Nan::New(AudioConsumerWrapper::constructor_template())->HasInstance(info[0])) {
		Nan::ThrowError("invalid consumer");
		return;
	}

	auto consumer = ObjectWrap::Unwrap<AudioConsumerWrapper>(info[0]->ToObject(Nan::GetCurrentContext()).ToLocalChecked());
	handle->delete_consumer(consumer);
}

NAN_METHOD(AudioRecorderWrapper::_set_volume) {
	auto handle = ObjectWrap::Unwrap<AudioRecorderWrapper>(info.Holder());

	if(info.Length() != 1 || !info[0]->IsNumber()) {
		Nan::ThrowError("invalid argument");
		return;
	}

	handle->input_->set_volume((float) info[0]->NumberValue(Nan::GetCurrentContext()).FromMaybe(0));
}

NAN_METHOD(AudioRecorderWrapper::_get_volume) {
	auto handle = ObjectWrap::Unwrap<AudioRecorderWrapper>(info.Holder());
	info.GetReturnValue().Set(handle->input_->volume());
}

NAN_METHOD(AudioRecorderWrapper::get_audio_processor) {
    auto handle = ObjectWrap::Unwrap<AudioRecorderWrapper>(info.Holder());

    auto processor = handle->input_->audio_processor();
    if(!processor) {
        return;
    }

    auto js_object = Nan::NewInstance(Nan::New(AudioProcessorWrapper::constructor()), 0, nullptr).ToLocalChecked();
    auto wrapper = new AudioProcessorWrapper(processor);
    wrapper->wrap(js_object);
    info.GetReturnValue().Set(js_object);
}

NAN_METHOD(AudioRecorderWrapper::create_level_meter) {
    if(info.Length() != 1 || !info[0]->IsString()) {
        Nan::ThrowError("invalid argument");
        return;
    }

    auto mode = *Nan::Utf8String{info[0]};
    bool preprocess;
    if(mode == std::string_view{"pre-process"}) {
        preprocess = true;
    } else if(mode == std::string_view{"post-process"}) {
        preprocess = false;
    } else {
        Nan::ThrowError("invalid first argument");
        return;
    }

    auto handle = ObjectWrap::Unwrap<AudioRecorderWrapper>(info.Holder());

    auto wrapper = new AudioLevelMeterWrapper(handle->input_->create_level_meter(preprocess));
    auto js_object = Nan::NewInstance(Nan::New(AudioLevelMeterWrapper::constructor())).ToLocalChecked();
    wrapper->wrap(js_object);
    info.GetReturnValue().Set(js_object);
}