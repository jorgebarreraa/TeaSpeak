#include "AudioFilter.h"

#include <utility>
#include "../filter/FilterVad.h"
#include "../filter/FilterThreshold.h"
#include "../filter/FilterState.h"
#include "../../logger.h"

using namespace std;
using namespace tc::audio;
using namespace tc::audio::recorder;


NAN_MODULE_INIT(AudioFilterWrapper::Init) {
	auto klass = Nan::New<v8::FunctionTemplate>(AudioFilterWrapper::NewInstance);
	klass->SetClassName(Nan::New("AudioFilter").ToLocalChecked());
	klass->InstanceTemplate()->SetInternalFieldCount(1);

	Nan::SetPrototypeMethod(klass, "get_name", AudioFilterWrapper::_get_name);

	Nan::SetPrototypeMethod(klass, "get_margin_time", AudioFilterWrapper::_get_margin_time);
	Nan::SetPrototypeMethod(klass, "set_margin_time", AudioFilterWrapper::_set_margin_time);

	Nan::SetPrototypeMethod(klass, "get_level", AudioFilterWrapper::_get_level);

	Nan::SetPrototypeMethod(klass, "get_threshold", AudioFilterWrapper::_get_threshold);
	Nan::SetPrototypeMethod(klass, "set_threshold", AudioFilterWrapper::_set_threshold);

	Nan::SetPrototypeMethod(klass, "get_attack_smooth", AudioFilterWrapper::_get_attack_smooth);
	Nan::SetPrototypeMethod(klass, "set_attack_smooth", AudioFilterWrapper::_set_attack_smooth);

	Nan::SetPrototypeMethod(klass, "get_release_smooth", AudioFilterWrapper::_get_release_smooth);
	Nan::SetPrototypeMethod(klass, "set_release_smooth", AudioFilterWrapper::_set_release_smooth);

	Nan::SetPrototypeMethod(klass, "set_analyze_filter", AudioFilterWrapper::_set_analyze_filter);

	Nan::SetPrototypeMethod(klass, "is_consuming", AudioFilterWrapper::_is_consuming);
	Nan::SetPrototypeMethod(klass, "set_consuming", AudioFilterWrapper::_set_consuming);

	constructor_template().Reset(klass);
	constructor().Reset(Nan::GetFunction(klass).ToLocalChecked());
}

NAN_METHOD(AudioFilterWrapper::NewInstance) {
	if(!info.IsConstructCall())
		Nan::ThrowError("invalid invoke!");
}

AudioFilterWrapper::AudioFilterWrapper(std::string name, std::shared_ptr<tc::audio::filter::Filter> filter) : _filter{std::move(filter)}, _name{std::move(name)} {
	auto threshold_filter = dynamic_pointer_cast<filter::ThresholdFilter>(this->_filter);
	if(threshold_filter) {
		this->_call_analyzed = Nan::async_callback([&](float value) {
			Nan::HandleScope scope;

			if(!this->_callback_analyzed.IsEmpty()) {
				auto cb = Nan::New<v8::Function>(this->_callback_analyzed);

				v8::Local<v8::Value> argv[1];
				argv[0] = Nan::New<v8::Number>(value);

                (void) cb->Call(Nan::GetCurrentContext(), Nan::Undefined(), 1, argv);
			}
		});
	}
	log_allocate("AudioFilterWrapper", this);
}
AudioFilterWrapper::~AudioFilterWrapper() {
	log_free("AudioFilterWrapper", this);

	auto threshold_filter = dynamic_pointer_cast<filter::ThresholdFilter>(this->_filter);
	if(threshold_filter) {
        threshold_filter->on_analyze = nullptr;
	}

	this->_callback_analyzed.Reset();
}

void AudioFilterWrapper::do_wrap(const v8::Local<v8::Object> &obj) {
	this->Wrap(obj);
}

NAN_METHOD(AudioFilterWrapper::_get_name) {
	auto handle = ObjectWrap::Unwrap<AudioFilterWrapper>(info.Holder());
	if(!handle->_filter) {
		Nan::ThrowError("invalid handle");
		return;
	}

	info.GetReturnValue().Set(Nan::New<v8::String>(handle->_name).ToLocalChecked());
}

NAN_METHOD(AudioFilterWrapper::_get_level) {
	auto handle = ObjectWrap::Unwrap<AudioFilterWrapper>(info.Holder());
	if(!handle->_filter) {
		Nan::ThrowError("invalid handle");
		return;
	}
	auto filter = dynamic_pointer_cast<filter::VadFilter>(handle->_filter);
	if(!filter) {
		Nan::ThrowError("filter does not support this method");
		return;
	}

	auto mode = filter->mode();
	if(mode.has_value()) {
        info.GetReturnValue().Set((int) *mode);
	} else {
	    /* We're using the preprocessor config */
	}
}


NAN_METHOD(AudioFilterWrapper::_get_margin_time) {
	auto handle = ObjectWrap::Unwrap<AudioFilterWrapper>(info.Holder());
	if(!handle->_filter) {
		Nan::ThrowError("invalid handle");
		return;
	}

	auto vad_filter = dynamic_pointer_cast<filter::VadFilter>(handle->_filter);
	auto threshold_filter = dynamic_pointer_cast<filter::ThresholdFilter>(handle->_filter);
	if(vad_filter) {
		info.GetReturnValue().Set((float) vad_filter->margin_release_time());
	} else if(threshold_filter) {
		info.GetReturnValue().Set((float) threshold_filter->margin_release_time());
	} else {
		Nan::ThrowError("invalid handle");
		return;
	}
}

NAN_METHOD(AudioFilterWrapper::_set_margin_time) {
	auto handle = ObjectWrap::Unwrap<AudioFilterWrapper>(info.Holder());
	if(!handle->_filter) {
		Nan::ThrowError("invalid handle");
		return;
	}

	if(info.Length() != 1 || !info[0]->IsNumber()) {
		Nan::ThrowError("invalid argument");
		return;
	}

	auto vad_filter = dynamic_pointer_cast<filter::VadFilter>(handle->_filter);
	auto threshold_filter = dynamic_pointer_cast<filter::ThresholdFilter>(handle->_filter);
	if(vad_filter) {
		vad_filter->set_margin_release_time(info[0]->NumberValue(Nan::GetCurrentContext()).FromMaybe(0));
	} else if(threshold_filter) {
		threshold_filter->set_margin_release_time(info[0]->NumberValue(Nan::GetCurrentContext()).FromMaybe(0));
	} else {
		Nan::ThrowError("invalid handle");
		return;
	}
}


NAN_METHOD(AudioFilterWrapper::_get_threshold) {
	auto handle = ObjectWrap::Unwrap<AudioFilterWrapper>(info.Holder());
	if(!handle->_filter) {
		Nan::ThrowError("invalid handle");
		return;
	}
	auto filter = dynamic_pointer_cast<filter::ThresholdFilter>(handle->_filter);
	if(!filter) {
		Nan::ThrowError("filter does not support this method");
		return;
	}

	info.GetReturnValue().Set((int) filter->threshold());
}


NAN_METHOD(AudioFilterWrapper::_set_threshold) {
	auto handle = ObjectWrap::Unwrap<AudioFilterWrapper>(info.Holder());
	if(!handle->_filter) {
		Nan::ThrowError("invalid handle");
		return;
	}

	if(info.Length() != 1 || !info[0]->IsNumber()) {
		Nan::ThrowError("invalid argument");
		return;
	}

	auto filter = dynamic_pointer_cast<filter::ThresholdFilter>(handle->_filter);
	if(!filter) {
		Nan::ThrowError("filter does not support this method");
		return;
	}

	filter->set_threshold((float) info[0]->Int32Value(Nan::GetCurrentContext()).FromMaybe(0));
}

NAN_METHOD(AudioFilterWrapper::_get_attack_smooth) {
	auto handle = ObjectWrap::Unwrap<AudioFilterWrapper>(info.Holder());
	if(!handle->_filter) {
		Nan::ThrowError("invalid handle");
		return;
	}
	auto filter = dynamic_pointer_cast<filter::ThresholdFilter>(handle->_filter);
	if(!filter) {
		Nan::ThrowError("filter does not support this method");
		return;
	}

	info.GetReturnValue().Set((int) filter->attack_smooth());
}

NAN_METHOD(AudioFilterWrapper::_set_attack_smooth) {
	auto handle = ObjectWrap::Unwrap<AudioFilterWrapper>(info.Holder());
	if(!handle->_filter) {
		Nan::ThrowError("invalid handle");
		return;
	}

	if(info.Length() != 1 || !info[0]->IsNumber()) {
		Nan::ThrowError("invalid argument");
		return;
	}

	auto filter = dynamic_pointer_cast<filter::ThresholdFilter>(handle->_filter);
	if(!filter) {
		Nan::ThrowError("filter does not support this method");
		return;
	}

	filter->attack_smooth((float) info[0]->NumberValue(Nan::GetCurrentContext()).FromMaybe(0));
}

NAN_METHOD(AudioFilterWrapper::_get_release_smooth) {
	auto handle = ObjectWrap::Unwrap<AudioFilterWrapper>(info.Holder());
	if(!handle->_filter) {
		Nan::ThrowError("invalid handle");
		return;
	}
	auto filter = dynamic_pointer_cast<filter::ThresholdFilter>(handle->_filter);
	if(!filter) {
		Nan::ThrowError("filter does not support this method");
		return;
	}

	info.GetReturnValue().Set((int) filter->release_smooth());
}

NAN_METHOD(AudioFilterWrapper::_set_release_smooth) {
	auto handle = ObjectWrap::Unwrap<AudioFilterWrapper>(info.Holder());
	if(!handle->_filter) {
		Nan::ThrowError("invalid handle");
		return;
	}

	if(info.Length() != 1 || !info[0]->IsNumber()) {
		Nan::ThrowError("invalid argument");
		return;
	}

	auto filter = dynamic_pointer_cast<filter::ThresholdFilter>(handle->_filter);
	if(!filter) {
		Nan::ThrowError("filter does not support this method");
		return;
	}

	filter->release_smooth((float) info[0]->NumberValue(Nan::GetCurrentContext()).FromMaybe(0));
}

NAN_METHOD(AudioFilterWrapper::_set_analyze_filter) {
	auto handle = ObjectWrap::Unwrap<AudioFilterWrapper>(info.Holder());
	if(!handle->_filter) {
		Nan::ThrowError("invalid handle");
		return;
	}

	if(info.Length() != 1 || !(info[0]->IsFunction() || info[0]->IsNullOrUndefined())) {
		Nan::ThrowError("invalid argument");
		return;
	}

	auto filter = dynamic_pointer_cast<filter::ThresholdFilter>(handle->_filter);
	if(!filter) {
		Nan::ThrowError("filter does not support this method");
		return;
	}

	if(info[0]->IsNullOrUndefined()) {
		handle->_callback_analyzed.Reset();
		filter->on_analyze = nullptr;
	} else {
		handle->_callback_analyzed.Reset(info[0].As<v8::Function>());
		filter->on_analyze = [handle](float value){
			handle->_call_analyzed.call(std::forward<float>(value), true);
		};
	}
}

NAN_METHOD(AudioFilterWrapper::_is_consuming) {
	auto handle = ObjectWrap::Unwrap<AudioFilterWrapper>(info.Holder());
	if(!handle->_filter) {
		Nan::ThrowError("invalid handle");
		return;
	}
	auto filter = dynamic_pointer_cast<filter::StateFilter>(handle->_filter);
	if(!filter) {
		Nan::ThrowError("filter does not support this method");
		return;
	}

	info.GetReturnValue().Set(filter->consumes_input());
}
NAN_METHOD(AudioFilterWrapper::_set_consuming) {
	auto handle = ObjectWrap::Unwrap<AudioFilterWrapper>(info.Holder());
	if(!handle->_filter) {
		Nan::ThrowError("invalid handle");
		return;
	}

	if(info.Length() != 1 || !info[0]->IsBoolean()) {
		Nan::ThrowError("invalid argument");
		return;
	}

	auto filter = dynamic_pointer_cast<filter::StateFilter>(handle->_filter);
	if(!filter) {
		Nan::ThrowError("filter does not support this method");
		return;
	}

	filter->set_consume_input(info[0]->BooleanValue(info.GetIsolate()));
}