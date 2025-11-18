#include <misc/digest.h>
#include <include/NanStrings.h>
#include "AudioPlayer.h"
#include "../AudioOutput.h"
#include "AudioOutputStream.h"
#include "../../logger.h"

using namespace tc;
using namespace tc::audio;

extern tc::audio::AudioOutput* global_audio_output;

NAN_MODULE_INIT(player::init_js) {
	Nan::Set(target, Nan::LocalString("current_device"), Nan::GetFunction(Nan::New<v8::FunctionTemplate>(player::current_playback_device)).ToLocalChecked());
	Nan::Set(target, Nan::LocalString("set_device"), Nan::GetFunction(Nan::New<v8::FunctionTemplate>(player::set_playback_device)).ToLocalChecked());

	Nan::Set(target, Nan::LocalString("create_stream"), Nan::GetFunction(Nan::New<v8::FunctionTemplate>(player::create_stream)).ToLocalChecked());

	Nan::Set(target, Nan::LocalString("get_master_volume"), Nan::GetFunction(Nan::New<v8::FunctionTemplate>(player::get_master_volume)).ToLocalChecked());
	Nan::Set(target, Nan::LocalString("set_master_volume"), Nan::GetFunction(Nan::New<v8::FunctionTemplate>(player::set_master_volume)).ToLocalChecked());
}

NAN_METHOD(audio::available_devices) {
    if(!audio::initialized()) {
        Nan::ThrowError(tr("audio hasn't been initialized!"));
        return;
    }

	auto devices = audio::devices();
	auto result = Nan::New<v8::Array>((int) devices.size());

	for(size_t index = 0; index < devices.size(); index++) {
		auto device_info = Nan::New<v8::Object>();
		auto device = devices[index];

		Nan::Set(device_info, Nan::LocalString("name"), Nan::LocalStringUTF8(device->name()));
		Nan::Set(device_info, Nan::LocalString("driver"), Nan::LocalStringUTF8(device->driver()));
		Nan::Set(device_info, Nan::LocalString("device_id"), Nan::New<v8::String>(device->id()).ToLocalChecked());

		Nan::Set(device_info, Nan::LocalString("input_supported"), Nan::New<v8::Boolean>(device->is_input_supported()));
		Nan::Set(device_info, Nan::LocalString("output_supported"), Nan::New<v8::Boolean>(device->is_output_supported()));

		Nan::Set(device_info, Nan::LocalString("input_default"), Nan::New<v8::Boolean>(device->is_input_default()));
		Nan::Set(device_info, Nan::LocalString("output_default"), Nan::New<v8::Boolean>(device->is_output_default()));

		Nan::Set(result, (uint32_t) index, device_info);
	}

	info.GetReturnValue().Set(result);
}

NAN_METHOD(audio::await_initialized_js) {
    if(info.Length() != 1 || !info[0]->IsFunction()) {
        Nan::ThrowError(tr("Invalid arguments"));
        return;
    }

    if(audio::initialized()) {
        (void) info[0].As<v8::Function>()->Call(Nan::GetCurrentContext(), Nan::Undefined(), 0, nullptr);
    } else {
        auto _callback = std::make_unique<Nan::Persistent<v8::Function>>(info[0].As<v8::Function>());

        auto _async_callback = Nan::async_callback([call = std::move(_callback)] {
            Nan::HandleScope scope;
            auto callback_function = call->Get(Nan::GetCurrentContext()->GetIsolate());

            (void) callback_function->Call(Nan::GetCurrentContext(), Nan::Undefined(), 0, nullptr);
            call->Reset();
        }).option_destroyed_execute(true);

        audio::initialize([_async_callback] {
            _async_callback.call();
        });
    }
}


NAN_METHOD(audio::initialized_js) {
    info.GetReturnValue().Set(audio::initialized());
}

NAN_METHOD(player::current_playback_device) {
	if(!global_audio_output) {
		info.GetReturnValue().Set(Nan::Undefined());
		return;
	}

	auto device = global_audio_output->current_device();
	if(!device) {
        info.GetReturnValue().Set(Nan::Undefined());
        return;
	}
	info.GetReturnValue().Set(Nan::LocalString(device->id()));
}

NAN_METHOD(player::set_playback_device) {
	if(!global_audio_output) {
		Nan::ThrowError("Global audio output hasn't been yet initialized!");
		return;
	}

	if(info.Length() != 1 || !info[0]->IsString()) {
		Nan::ThrowError("invalid arguments");
		return;
	}

	auto device = audio::find_device_by_id(*Nan::Utf8String(info[0]), false);
    if(!device) {
        Nan::ThrowError("invalid device id");
        return;
    }

	std::string error;

	global_audio_output->set_device(device);
	if(!global_audio_output->playback(error)) {
		Nan::ThrowError(error.c_str());
		return;
	}
}

NAN_METHOD(player::create_stream) {
    if(!audio::initialized()) {
        Nan::ThrowError(tr("audio hasn't been initialized yet"));
        return;
    }

	if(!global_audio_output) {
		Nan::ThrowError("Global audio output hasn't been yet initialized!");
		return;
	}

	auto wrapper = new audio::AudioOutputStreamWrapper(global_audio_output->create_source(), true);
	auto object = Nan::NewInstance(Nan::New(audio::AudioOutputStreamWrapper::constructor()), 0, nullptr).ToLocalChecked();
	wrapper->do_wrap(object);
	info.GetReturnValue().Set(object);
}


NAN_METHOD(player::get_master_volume) {
	if(!global_audio_output) {
		Nan::ThrowError("Global audio output hasn't been yet initialized!");
		return;
	}
	info.GetReturnValue().Set(global_audio_output->volume());
}

NAN_METHOD(player::set_master_volume) {
	if(!global_audio_output) {
		Nan::ThrowError("Global audio output hasn't been yet initialized!");
		return;
	}

	if(info.Length() != 1 || !info[0]->IsNumber()) {
		Nan::ThrowError("invalid arguments");
		return;
	}
	global_audio_output->set_volume((float) info[0]->NumberValue(Nan::GetCurrentContext()).FromMaybe(0));
}