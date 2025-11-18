//
// Created by WolverinDEV on 28/03/2021.
//

#include <include/NanEventCallback.h>
#include <include/NanStrings.h>
#include "./AudioLevelMeter.h"
#include "../AudioLevelMeter.h"
#include <thread>

using namespace tc::audio;
using namespace tc::audio::recorder;

NAN_MODULE_INIT(AudioLevelMeterWrapper::Init) {
    auto klass = Nan::New<v8::FunctionTemplate>(AudioLevelMeterWrapper::NewInstance);
    klass->SetClassName(Nan::New("AudioLevelMeter").ToLocalChecked());
    klass->InstanceTemplate()->SetInternalFieldCount(1);

    Nan::SetPrototypeMethod(klass, "start", AudioLevelMeterWrapper::start);
    Nan::SetPrototypeMethod(klass, "running", AudioLevelMeterWrapper::running);
    Nan::SetPrototypeMethod(klass, "stop", AudioLevelMeterWrapper::stop);
    Nan::SetPrototypeMethod(klass, "set_callback", AudioLevelMeterWrapper::set_callback);

    Nan::Set(target, Nan::LocalStringUTF8("create_device_level_meter"), Nan::GetFunction(Nan::New<v8::FunctionTemplate>(AudioLevelMeterWrapper::create_device_level_meter)).ToLocalChecked());

    constructor().Reset(Nan::GetFunction(klass).ToLocalChecked());
}

NAN_METHOD(AudioLevelMeterWrapper::NewInstance) {
    if(!info.IsConstructCall()) {
        Nan::ThrowError("invalid invoke!");
    }
}

NAN_METHOD(AudioLevelMeterWrapper::create_device_level_meter) {
    if(info.Length() != 1 || !info[0]->IsString()) {
        Nan::ThrowError("invalid arguments");
        return;
    }

    auto target_device_id = *Nan::Utf8String(info[0]);
    std::shared_ptr<AudioDevice> target_device{};
    for(const auto& device : audio::devices()) {
        if(device->id() == target_device_id) {
            target_device = device;
            break;
        }
    }

    if(!target_device || !target_device->is_input_supported()) {
        Nan::ThrowError("invalid target device");
        return;
    }

    auto wrapper = new AudioLevelMeterWrapper(std::make_shared<InputDeviceAudioLevelMeter>(target_device));
    auto js_object = Nan::NewInstance(Nan::New(AudioLevelMeterWrapper::constructor())).ToLocalChecked();
    wrapper->wrap(js_object);
    info.GetReturnValue().Set(js_object);
}

AudioLevelMeterWrapper::AudioLevelMeterWrapper(std::shared_ptr<AbstractAudioLevelMeter> handle) : handle{std::move(handle)} {
    assert(this->handle);

    memset(&this->update_timer, 0, sizeof(this->update_timer));
    this->update_timer.data = this;

    uv_timer_init(Nan::GetCurrentEventLoop(), &this->update_timer);
}

AudioLevelMeterWrapper::~AudioLevelMeterWrapper() noexcept {
    uv_timer_stop(&this->update_timer);
    this->update_timer.data = nullptr;
    this->callback.Reset();
}

NAN_METHOD(AudioLevelMeterWrapper::start) {
    auto handle = ObjectWrap::Unwrap<AudioLevelMeterWrapper>(info.Holder());

    if(info.Length() != 1 || !info[0]->IsFunction()) {
        Nan::ThrowError("invalid arguments");
        return;
    }

    auto js_queue = std::make_unique<Nan::JavaScriptQueue>();
    auto js_callback = std::make_unique<Nan::Persistent<v8::Function>>(info[0].As<v8::Function>());

    handle->Ref();
    std::thread{[handle, js_queue = std::move(js_queue), js_callback = std::move(js_callback)]() mutable {
        std::string error{};
        auto result = handle->handle->start(error);

        js_queue->enqueue([handle, result, error = std::move(error), js_callback = std::move(js_callback)]{
            auto isolate = Nan::GetCurrentContext()->GetIsolate();
            auto start_callback = js_callback->Get(isolate);
            if(!result) {
                auto js_error = Nan::LocalStringUTF8(error);
                (void) start_callback->Call(isolate->GetCurrentContext(), Nan::Undefined(), 1, (v8::Local<v8::Value>*) &js_error);
            } else {
                (void) start_callback->Call(isolate->GetCurrentContext(), Nan::Undefined(), 0, nullptr);
            }

            handle->test_timer();
            js_callback->Reset();
            handle->Unref();
        });
    }}.detach();
}

NAN_METHOD(AudioLevelMeterWrapper::running) {
    auto handle = ObjectWrap::Unwrap<AudioLevelMeterWrapper>(info.Holder())->handle;
    info.GetReturnValue().Set(handle->running());
}

NAN_METHOD(AudioLevelMeterWrapper::stop) {
    auto handle = ObjectWrap::Unwrap<AudioLevelMeterWrapper>(info.Holder());
    handle->handle->stop();
    handle->test_timer();
}

NAN_METHOD(AudioLevelMeterWrapper::set_callback) {
    auto handle = ObjectWrap::Unwrap<AudioLevelMeterWrapper>(info.Holder());

    if(info.Length() < 1) {
        Nan::ThrowError("invalid arguments");
        return;
    }

    if(info[0]->IsFunction()) {
        handle->update_timer_interval = 50;
        if(info.Length() >= 2 && info[1]->IsNumber()) {
            auto value = info[1]->NumberValue(Nan::GetCurrentContext()).FromMaybe(0);
            if(value < 1) {
                Nan::ThrowError("invalid update interval");
                return;
            }

            handle->update_timer_interval = (size_t) value;
        }

        handle->callback.Reset(info[0].As<v8::Function>());
    } else if(info[0]->IsNullOrUndefined()) {
        handle->callback.Reset();
    } else {
        Nan::ThrowError("invalid arguments");
        return;
    }
    handle->test_timer();
}

void AudioLevelMeterWrapper::test_timer() {
    if(this->handle->running() && !this->callback.IsEmpty()) {
        uv_timer_start(&this->update_timer, AudioLevelMeterWrapper::timer_callback, 0, this->update_timer_interval);
    } else {
        uv_timer_stop(&this->update_timer);
    }
}

void AudioLevelMeterWrapper::timer_callback(uv_timer_t *callback) {
    Nan::HandleScope scope{};

    auto level_meter = (AudioLevelMeterWrapper*) callback->data;
    if(level_meter->callback.IsEmpty()) {
        return;
    }

    auto isolate = Nan::GetCurrentContext()->GetIsolate();
    assert(isolate);
    auto timer_callback = level_meter->callback.Get(isolate);

    auto level = Nan::New(level_meter->handle->current_volume());
    (void) timer_callback->Call(Nan::GetCurrentContext(), Nan::Undefined(), 1, (v8::Local<v8::Value>*) &level);
}