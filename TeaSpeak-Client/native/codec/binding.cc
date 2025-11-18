#include <nan.h>
#include <node.h>
#include <v8.h>
#include <iostream>
#include "codec/NativeCodec.h"

using namespace std;


tc::WorkerPool* tc::codec_workers = nullptr;

NAN_METHOD(finalize) {
	auto pool = tc::codec_workers;
	if(!pool) return;

	tc::codec_workers = nullptr;

	pool->finalize();
	delete pool;
}

NAN_MODULE_INIT(init) {
	tc::codec_workers = new tc::WorkerPool();
	tc::codec_workers->initialize();

	Nan::Set(target, Nan::New<v8::String>("new_instance").ToLocalChecked(), Nan::GetFunction(Nan::New<v8::FunctionTemplate>(tc::NativeCodec::NewInstance)).ToLocalChecked());
	tc::NativeCodec::Init(target);
	tc::NativeCodec::CodecType::Init(target);
}

NODE_MODULE(MODULE_NAME, init)