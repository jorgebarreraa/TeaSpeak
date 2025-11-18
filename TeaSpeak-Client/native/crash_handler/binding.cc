#include <v8.h>
#include <nan.h>
#include <node.h>
#include <iostream>
#include <mutex>

using namespace std;

#include "include/NanException.h"
#include "include/NanEventCallback.h"
#include "src/crash_handler.h"

#include <thread>

NAN_METHOD(setup_crash_handler) {
	if(info.Length() != 4) {
		Nan::ThrowError("invalid argument count");
		return;
	}

	if(!info[0]->IsString() || !info[1]->IsString() || !info[2]->IsString() || !info[3]->IsString()) {
		Nan::ThrowError("invalid argument types");
		return;
	}

	if(tc::signal::active()) {
		Nan::ThrowError("crash handler has already been initialized!");
		return;
	}

	auto context = make_unique<tc::signal::CrashContext>();
	context->component_name = *Nan::Utf8String(info[0]);
	context->crash_dump_folder = *Nan::Utf8String(info[1]);
	context->success_command_line = *Nan::Utf8String(info[2]);
	context->error_command_line = *Nan::Utf8String(info[3]);
	tc::signal::setup(context);
}

NAN_METHOD(crash_handler_active) {
	info.GetReturnValue().Set(tc::signal::active());
}

NAN_METHOD(finalize) {
	tc::signal::finalize();
}

NAN_METHOD(crash) {
    std::thread([] {
        while (true) {
            this_thread::sleep_for(chrono::milliseconds(100));
            cout << "crash bg thread" << endl;
        }
    }).detach();

    *(int*) (nullptr) = 0;
}

NAN_MODULE_INIT(init) {
	NAN_EXPORT(target, setup_crash_handler);
	NAN_EXPORT(target, crash_handler_active);
	NAN_EXPORT(target, finalize);
	NAN_EXPORT(target, crash);
}

NODE_MODULE(MODULE_NAME, init)