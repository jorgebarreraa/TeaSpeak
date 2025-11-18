#pragma once

#define NAN_THROW_EXCEPTION(type, message)                                                                                                      \
do {                                                                                                                                            \
    auto isolate = Nan::GetCurrentContext()->GetIsolate();                                                                                      \
    auto exception = v8::Exception::type(v8::String::NewFromUtf8(isolate, message, v8::NewStringType::kNormal).ToLocalChecked());               \
    isolate->ThrowException(exception);                                                                                                         \
} while(0)

