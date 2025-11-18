#include <v8.h>
#include <nan.h>
#include <iostream>
#include <mutex>
#include <include/NanStrings.h>

using namespace std;

#include "include/NanException.h"
#include "include/NanEventCallback.h"

#ifdef WIN32
	#include "src/Win32KeyboardHook.h"
#else
	#include "src/KeyboardHook.h"
    #include "src/X11KeyboardHook.h"
#endif


std::mutex callback_lock;
std::deque<std::shared_ptr<Nan::Callback>> callbacks;
std::deque<std::shared_ptr<KeyboardHook::KeyEvent>> queued_events;
std::unique_ptr<KeyboardHook> hook;

Nan::callback_t<> event_callback;

inline v8::Local<v8::Object> event_to_object(const std::shared_ptr<KeyboardHook::KeyEvent>& event) {
	Nan::EscapableHandleScope scope;

	auto object = Nan::New<v8::Object>();
	Nan::Set(object, Nan::LocalString("type"), Nan::New<v8::Number>(event->type));
	Nan::Set(object, Nan::LocalString("key_code"), Nan::New<v8::String>(event->code).ToLocalChecked());

	Nan::Set(object, Nan::LocalString("key_shift"), Nan::New<v8::Boolean>(event->key_shift));
	Nan::Set(object, Nan::LocalString("key_alt"), Nan::New<v8::Boolean>(event->key_alt));
	Nan::Set(object, Nan::LocalString("key_windows"), Nan::New<v8::Boolean>(event->key_windows));
	Nan::Set(object, Nan::LocalString("key_ctrl"), Nan::New<v8::Boolean>(event->key_ctrl));

	return scope.Escape(object);
}

NAN_METHOD(RegisterCallback) {
	if(info.Length() < 1 || !info[0]->IsFunction()) {
		NAN_THROW_EXCEPTION(Error, "argument must be a function!");
		return;
	}

	auto callback = make_shared<Nan::Callback>(info[0].As<v8::Function>());
	{
		lock_guard lock(callback_lock);
		callbacks.push_back(callback);
	}
}

NAN_METHOD(UnregisterCallback) {
	if(info.Length() < 1 || !info[0]->IsFunction()) {
		NAN_THROW_EXCEPTION(Error, "argument must be a function!");
		return;
	}

	{
		lock_guard lock(callback_lock);

		callbacks.erase(std::remove_if(callbacks.begin(), callbacks.end(), [&](const std::shared_ptr<Nan::Callback>& callback){
			return callback->GetFunction() == info[0];
		}), callbacks.end());
	}
}

NAN_MODULE_INIT(init) {
#ifdef WIN32
	hook = make_unique<hooks::Win32RawHook>();
#else
    hook = make_unique<hooks::X11KeyboardHook>();
#endif

	if(!hook->attach()) {
		NAN_THROW_EXCEPTION(Error, "Failed to attach hook!");
		return;
	}
	hook->callback_event = [&](const shared_ptr<KeyboardHook::KeyEvent>& event) {
		{
			lock_guard lock(callback_lock);
			queued_events.push_back(event);
		}

		event_callback();
	};


	event_callback = Nan::async_callback([](){
		Nan::HandleScope scope;

		unique_lock lock(callback_lock);
		auto events = queued_events;
		auto calls = callbacks;
		queued_events.clear();
		lock.unlock();


		for(const auto& event : events) {
			auto object = event_to_object(event);

			for(const auto& callback : calls) {
				v8::Local<v8::Value> args[] = {
						object
				};

				Nan::Call(*callback, Nan::Undefined().As<v8::Object>(), 1, args);
			}
		}
	});

	NAN_EXPORT(target, RegisterCallback);
	NAN_EXPORT(target, UnregisterCallback);

	node::AtExit([](auto){
	    hook->detach();

        std::unique_lock lock{callback_lock};
        callbacks.clear();
        queued_events.clear();
	});
}

NODE_MODULE(MODULE_NAME, init)