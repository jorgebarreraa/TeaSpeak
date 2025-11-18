#include <v8.h>
#include <nan.h>
#include <node.h>
#include <iostream>
#include <mutex>
#include <event2/thread.h>

#include "./src/resolver.h"
#include "./utils.h"

using namespace std;

#include "include/NanException.h"
#include "include/NanEventCallback.h"

std::unique_ptr<tc::dns::Resolver> resolver{nullptr};

NAN_METHOD(initialize) {
	if(resolver) {
		Nan::ThrowError("already initialized");
		return;
	}

#ifdef WIN32
    evthread_use_windows_threads();
#else
    evthread_use_pthreads();
#endif
	resolver = make_unique<tc::dns::Resolver>();

	string error;
	if(!resolver->initialize(error, true, true)) {
		Nan::ThrowError(error.c_str());
		return;
	}
}

NAN_METHOD(query_connect_address) {
	if(!resolver) {
		Nan::ThrowError("initialize resolver first!");
		return;
	}

	if(info.Length() != 3 || !info[0]->IsString() || !info[1]->IsNumber() || !info[2]->IsFunction()) {
		Nan::ThrowError("invalid arguments");
		return;
	}

	auto host = Nan::Utf8String{info[0]->ToString(Nan::GetCurrentContext()).ToLocalChecked()};
	auto port = info[1]->ToNumber(Nan::GetCurrentContext()).ToLocalChecked()->NumberValue(Nan::GetCurrentContext()).FromMaybe(0);

	auto js_callback = make_unique<Nan::Callback>(info[2].As<v8::Function>());
	auto begin = chrono::system_clock::now();
	auto callback = Nan::async_callback([js_callback = std::move(js_callback), begin] (bool success, std::string message, tc::dns::ServerAddress response) {
		Nan::HandleScope scope{};
		auto isolate = Nan::GetCurrentContext()->GetIsolate();

		v8::Local<v8::Value> argv[1];
		if(!success) {
			argv[0] = v8::String::NewFromOneByte(isolate, (uint8_t*) message.c_str()).ToLocalChecked();
		} else {
			auto js_data = Nan::New<v8::Object>();
			Nan::Set(js_data,
					v8::String::NewFromUtf8(isolate, "host").ToLocalChecked(),
				    v8::String::NewFromUtf8(isolate, response.host.c_str()).ToLocalChecked()
			);
			Nan::Set(js_data,
			         v8::String::NewFromUtf8(isolate, "port").ToLocalChecked(),
			         Nan::New<v8::Number>(response.port)
			);
			Nan::Set(js_data,
			         v8::String::NewFromUtf8(isolate, "timing").ToLocalChecked(),
			         Nan::New<v8::Number>(chrono::floor<chrono::milliseconds>(chrono::system_clock::now() - begin).count())
			);

			argv[0] = js_data;
		}
		js_callback->Call(1, argv);
	}).option_destroyed_execute(true);

	tc::dns::cr(*resolver,
			tc::dns::ServerAddress{ *host, (uint16_t) port },
			[callback = std::move(callback)] (bool success, std::variant<std::string, tc::dns::ServerAddress> data) mutable {
				callback.call_cpy(success, success ? "" : std::get<std::string>(data), !success ? tc::dns::ServerAddress{"", 0} : std::get<tc::dns::ServerAddress>(data), false);
	});
}

#ifndef WIN32
__attribute__((visibility("default")))
#endif
NAN_MODULE_INIT(init) {
	Nan::Set(target,
			v8::String::NewFromUtf8(Nan::GetCurrentContext()->GetIsolate(), "resolve_cr").ToLocalChecked(),
			Nan::GetFunction(Nan::New<v8::FunctionTemplate>(query_connect_address)).ToLocalChecked()
	);

	Nan::Set(target,
	         v8::String::NewFromUtf8(Nan::GetCurrentContext()->GetIsolate(), "initialize").ToLocalChecked(),
	         Nan::GetFunction(Nan::New<v8::FunctionTemplate>(initialize)).ToLocalChecked()
	);
}

NODE_MODULE(MODULE_NAME, init)