#include "./AudioConsumer.h"
#include "./AudioRecorder.h"
#include "./AudioFilter.h"
#include "../filter/Filter.h"
#include "../filter/FilterVad.h"
#include "../filter/FilterThreshold.h"
#include "../filter/FilterState.h"
#include "../../logger.h"

using namespace std;
using namespace tc::audio;
using namespace tc::audio::recorder;

inline v8::PropertyAttribute operator|(const v8::PropertyAttribute& a, const v8::PropertyAttribute& b) {
    return (v8::PropertyAttribute) ((unsigned) a | (unsigned) b);
}

NAN_MODULE_INIT(AudioConsumerWrapper::Init) {
	auto klass = Nan::New<v8::FunctionTemplate>(AudioConsumerWrapper::NewInstance);
	klass->SetClassName(Nan::New("AudioConsumer").ToLocalChecked());
	klass->InstanceTemplate()->SetInternalFieldCount(1);

	Nan::SetPrototypeMethod(klass, "get_filters", AudioConsumerWrapper::_get_filters);
	Nan::SetPrototypeMethod(klass, "unregister_filter", AudioConsumerWrapper::_unregister_filter);

	Nan::SetPrototypeMethod(klass, "create_filter_vad", AudioConsumerWrapper::_create_filter_vad);
	Nan::SetPrototypeMethod(klass, "create_filter_threshold", AudioConsumerWrapper::_create_filter_threshold);
	Nan::SetPrototypeMethod(klass, "create_filter_state", AudioConsumerWrapper::_create_filter_state);

    Nan::SetPrototypeMethod(klass, "get_filter_mode", AudioConsumerWrapper::_get_filter_mode);
    Nan::SetPrototypeMethod(klass, "set_filter_mode", AudioConsumerWrapper::_set_filter_mode);

	constructor_template().Reset(klass);
	constructor().Reset(Nan::GetFunction(klass).ToLocalChecked());
}

NAN_METHOD(AudioConsumerWrapper::NewInstance) {
	if(!info.IsConstructCall())
		Nan::ThrowError("invalid invoke!");
}

AudioConsumerWrapper::AudioConsumerWrapper(const std::shared_ptr<AudioInput> &input) :
    sample_rate_{input->sample_rate()},
    channel_count_{input->channel_count()},
    js_queue{}
{
    log_allocate("AudioConsumerWrapper", this);

    this->consumer_handle = std::make_shared<InputConsumer>();
    this->consumer_handle->wrapper = this;

    input->register_consumer(this->consumer_handle);
}

AudioConsumerWrapper::~AudioConsumerWrapper() {
	log_free("AudioConsumerWrapper", this);

    {
        std::lock_guard lock{this->consumer_handle->wrapper_mutex};
        this->consumer_handle->wrapper = nullptr;
    }
    this->consumer_handle = nullptr;
}

void AudioConsumerWrapper::do_wrap(const v8::Local<v8::Object> &obj) {
	this->Wrap(obj);

#if 0
	this->_call_data = Nan::async_callback([&] {
		Nan::HandleScope scope;

		auto handle = this->handle();
		v8::Local<v8::Value> callback_function = Nan::Get(handle, Nan::New<v8::String>("callback_data").ToLocalChecked()).FromMaybe(v8::Local<v8::Value>{});
		if(callback_function.IsEmpty() || callback_function->IsNullOrUndefined() || !callback_function->IsFunction()) {
			lock_guard lock(this->_data_lock);
			this->_data_entries.clear();
		}

		std::unique_ptr<DataEntry> buffer;
		while(true) {
			{
				lock_guard lock{this->_data_lock};
				if(this->_data_entries.empty()) {
                    break;
				}

				buffer = move(this->_data_entries.front());
				this->_data_entries.pop_front();
			}

			const auto byte_length = buffer->sample_count * this->channel_count_ * 4;
			auto js_buffer = v8::ArrayBuffer::New(Nan::GetCurrentContext()->GetIsolate(), byte_length);
			auto js_fbuffer = v8::Float32Array::New(js_buffer, 0, byte_length / 4);

			memcpy(js_buffer->GetContents().Data(), buffer->buffer, byte_length);

			v8::Local<v8::Value> argv[1];
			argv[0] = js_fbuffer;
            (void) callback_function.As<v8::Function>()->Call(Nan::GetCurrentContext(), Nan::Undefined(), 1, argv);
		}
	});
#endif

	this->_call_ended = Nan::async_callback([&]{
		Nan::HandleScope scope;

		auto handle = this->handle();
		v8::Local<v8::Value> callback_function = Nan::Get(handle, Nan::New<v8::String>("callback_ended").ToLocalChecked()).FromMaybe(v8::Local<v8::Value>{});
		if(callback_function.IsEmpty() || callback_function->IsNullOrUndefined() || !callback_function->IsFunction())
			return;
        (void) callback_function.As<v8::Function>()->Call(Nan::GetCurrentContext(), Nan::Undefined(), 0, nullptr);
	});

	this->_call_started = Nan::async_callback([&]{
		Nan::HandleScope scope;

		auto handle = this->handle();
		v8::Local<v8::Value> callback_function = Nan::Get(handle, Nan::New<v8::String>("callback_started").ToLocalChecked()).FromMaybe(v8::Local<v8::Value>{});
		if(callback_function.IsEmpty() || callback_function->IsNullOrUndefined() || !callback_function->IsFunction())
			return;
        (void) callback_function.As<v8::Function>()->Call(Nan::GetCurrentContext(), Nan::Undefined(), 0, nullptr);
	});

    Nan::DefineOwnProperty(this->handle(), Nan::New<v8::String>("sampleRate").ToLocalChecked(), Nan::New<v8::Number>((uint32_t) this->sample_rate_), v8::ReadOnly | v8::DontDelete);
    Nan::DefineOwnProperty(this->handle(), Nan::New<v8::String>("channelCount").ToLocalChecked(), Nan::New<v8::Number>((uint32_t) this->channel_count_), v8::ReadOnly | v8::DontDelete);
}

void AudioConsumerWrapper::delete_consumer() {
    if(this->consumer_handle) {
        std::lock_guard lock{this->consumer_handle->wrapper_mutex};
        this->consumer_handle->wrapper = nullptr;
    }
}

std::shared_ptr<AudioFilterWrapper> AudioConsumerWrapper::create_filter(const std::string& name, const std::shared_ptr<filter::Filter> &impl) {
	auto result = shared_ptr<AudioFilterWrapper>(new AudioFilterWrapper(name, impl), [](AudioFilterWrapper* ptr) {
		assert(v8::Isolate::GetCurrent());
		ptr->Unref();
	});

	/* wrap into object */
	{
		auto js_object = Nan::NewInstance(Nan::New(AudioFilterWrapper::constructor()), 0, nullptr).ToLocalChecked();
		result->do_wrap(js_object);
		result->Ref();
	}

	{
		lock_guard lock(this->filter_mutex_);
		this->filter_.push_back(result);
	}

	return result;
}

void AudioConsumerWrapper::delete_filter(const AudioFilterWrapper* filter) {
    std::shared_ptr<AudioFilterWrapper> handle; /* need to keep the handle 'till everything has been finished */

	{
		std::lock_guard lock(this->filter_mutex_);
		for(auto& c : this->filter_) {
			if(&*c == filter) {
				handle = c;
				break;
			}
		}

		if(!handle) {
            return;
		}

		{
			auto it = find(this->filter_.begin(), this->filter_.end(), handle);
			if(it != this->filter_.end()) {
                this->filter_.erase(it);
			}
		}
	}

	{
        /* ensure that the filter isn't used right now */
		lock_guard lock{this->consumer_handle->wrapper_mutex};
		handle->_filter = nullptr;
	}
}


NAN_METHOD(AudioConsumerWrapper::_get_filters) {
	auto handle = ObjectWrap::Unwrap<AudioConsumerWrapper>(info.Holder());
	auto filters = handle->filters();

	auto result = Nan::New<v8::Array>((uint32_t) filters.size());

	for(uint32_t index = 0; index < filters.size(); index++)
		Nan::Set(result, index, filters[index]->handle());

	info.GetReturnValue().Set(result);
}

NAN_METHOD(AudioConsumerWrapper::_unregister_filter) {
	auto handle = ObjectWrap::Unwrap<AudioConsumerWrapper>(info.Holder());

	if(info.Length() != 1 || !info[0]->IsObject()) {
		Nan::ThrowError("invalid argument");
		return;
	}

	if(!Nan::New(AudioFilterWrapper::constructor_template())->HasInstance(info[0])) {
		Nan::ThrowError("invalid consumer");
		return;
	}

	auto consumer = ObjectWrap::Unwrap<AudioFilterWrapper>(info[0]->ToObject(Nan::GetCurrentContext()).ToLocalChecked());
	handle->delete_filter(consumer);
}


NAN_METHOD(AudioConsumerWrapper::_create_filter_vad) {
	auto handle = ObjectWrap::Unwrap<AudioConsumerWrapper>(info.Holder());

	if(info.Length() != 1 || !info[0]->IsNumber()) {
		Nan::ThrowError("invalid argument");
		return;
	}

	string error;
	auto filter = make_shared<filter::VadFilter>(handle->channel_count_, handle->sample_rate_);
	if(!filter->initialize(error, info[0]->Int32Value(Nan::GetCurrentContext()).FromMaybe(0), 2)) {
		Nan::ThrowError(Nan::New<v8::String>("failed to initialize filter (" + error + ")").ToLocalChecked());
		return;
	}

	auto object = handle->create_filter("vad", filter);
	info.GetReturnValue().Set(object->handle());
}

NAN_METHOD(AudioConsumerWrapper::_create_filter_threshold) {
	auto handle = ObjectWrap::Unwrap<AudioConsumerWrapper>(info.Holder());

	if(info.Length() != 1 || !info[0]->IsNumber()) {
		Nan::ThrowError("invalid argument");
		return;
	}

	auto filter = make_shared<filter::ThresholdFilter>(handle->channel_count_, handle->sample_rate_);
    filter->initialize((float) info[0]->Int32Value(Nan::GetCurrentContext()).FromMaybe(0), 2);

	auto object = handle->create_filter("threshold", filter);
	info.GetReturnValue().Set(object->handle());
}

NAN_METHOD(AudioConsumerWrapper::_create_filter_state) {
	auto handle = ObjectWrap::Unwrap<AudioConsumerWrapper>(info.Holder());

	auto filter = std::make_shared<filter::StateFilter>(handle->channel_count_, handle->sample_rate_);
	auto object = handle->create_filter("state", filter);
	info.GetReturnValue().Set(object->handle());
}

NAN_METHOD(AudioConsumerWrapper::_get_filter_mode) {
    auto handle = ObjectWrap::Unwrap<AudioConsumerWrapper>(info.Holder());
    info.GetReturnValue().Set((int) handle->filter_mode_);
}

NAN_METHOD(AudioConsumerWrapper::_set_filter_mode) {
    auto handle = ObjectWrap::Unwrap<AudioConsumerWrapper>(info.Holder());

    if(info.Length() != 1 || !info[0]->IsNumber()) {
        Nan::ThrowError("invalid argument");
        return;
    }

    auto value = info[0].As<v8::Number>()->Int32Value(info.GetIsolate()->GetCurrentContext()).FromMaybe(0);
    handle->filter_mode_ = (FilterMode) value;
}

void AudioConsumerWrapper::InputConsumer::handle_buffer(const AudioInputBufferInfo &info, const float *buffer) {
    std::lock_guard lock{this->wrapper_mutex};
    if(!this->wrapper) {
        return;
    }

    this->wrapper->handle_buffer(info, buffer);
}


void AudioConsumerWrapper::handle_buffer(const AudioInputBufferInfo &info, const float *buffer) {
    bool should_process;
    switch(this->filter_mode_) {

        case FilterMode::FILTER:
            should_process = true;
            for(const auto& filter : this->filters()) {
                auto filter_instance = filter->filter();
                if(!filter_instance) continue;

                if(!filter_instance->process(info, buffer)) {
                    should_process = false;
                    break;
                }
            }
            break;

        case FilterMode::BYPASS:
            should_process = true;
            break;

        case FilterMode::BLOCK:
        default:
            should_process = false;
            return;

    }

    if(!should_process) {
        if(!this->last_consumed) {
            this->last_consumed = true;
            this->_call_ended();

            std::unique_lock native_read_lock(this->native_read_callback_lock);
            if(this->native_read_callback) {
                auto callback = this->native_read_callback; /* copy */
                native_read_lock.unlock();
                callback(nullptr, 0); /* notify end */
            }
        }
    } else {
        if(this->last_consumed) {
            this->last_consumed = false;
            this->_call_started();
        }

        {
            unique_lock native_read_lock(this->native_read_callback_lock);
            if(this->native_read_callback) {
                auto callback = this->native_read_callback; /* copy */
                native_read_lock.unlock();

                callback(buffer, info.sample_count);
                return;
            }
        }

        /* TODO: Callback JavaScript if required */
    }
}