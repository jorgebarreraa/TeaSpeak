#pragma once

#include <nan.h>
#include <string_view>
#include <v8.h>

namespace Nan {
	/* UTF-8 Helpers */
	inline v8::Local<v8::String> LocalStringUTF8(std::string_view buffer) {
		auto isolate = Nan::GetCurrentContext()->GetIsolate();
		v8::EscapableHandleScope scope{isolate};
		auto data = v8::String::NewFromUtf8(isolate, buffer.data(), v8::NewStringType::kNormal, (int) buffer.length());

		v8::Local<v8::String> response{};
		if(!data.ToLocal(&response))
			throw std::bad_alloc{};
		return scope.Escape(response);
	}

	inline v8::Local<v8::String> LocalStringUTF8(const std::string& buffer) {
		return LocalStringUTF8(std::string_view{buffer});
	}

	inline v8::Local<v8::String> LocalStringUTF8(const char* buffer, size_t length) {
		return Nan::LocalStringUTF8(std::string_view{buffer, length});
	}

	template <size_t S>
	inline v8::Local<v8::String> LocalStringUTF8(const char (&buffer)[S]) {
		return Nan::LocalStringUTF8((const char*) buffer, S - 1);
	}

	/* Latin1 Helpers */
	inline v8::Local<v8::String> LocalString(std::string_view buffer) {
		auto isolate = Nan::GetCurrentContext()->GetIsolate();
		//v8::EscapableHandleScope scope{isolate};
		auto data = v8::String::NewFromOneByte(isolate, (uint8_t*) buffer.data(), v8::NewStringType::kNormal, (int) buffer.length());

		v8::Local<v8::String> response{};
		if(!data.ToLocal(&response))
			throw std::bad_alloc{};
		return response;
		//eturn scope.Escape(response);
	}

	inline v8::Local<v8::String> LocalString(const std::string& buffer) {
		return LocalString(std::string_view{buffer});
	}

	inline v8::Local<v8::String> LocalString(const char* buffer, size_t length) {
		return Nan::LocalString(std::string_view{buffer, length});
	}

	template <size_t S>
	inline v8::Local<v8::String> LocalString(const char (&buffer)[S]) {
		return Nan::LocalString((const char*) buffer, S - 1);
	}

	/* Wide char helpers */
	inline v8::Local<v8::String> LocalString(std::wstring_view buffer) {
		auto isolate = Nan::GetCurrentContext()->GetIsolate();
		v8::EscapableHandleScope scope{isolate};
		auto data = v8::String::NewFromTwoByte(isolate, (uint16_t*) buffer.data(), v8::NewStringType::kNormal, (int) buffer.length());

		v8::Local<v8::String> response{};
		if(!data.ToLocal(&response))
			throw std::bad_alloc{};
		return scope.Escape(response);
	}

	inline v8::Local<v8::String> LocalString(const std::wstring& buffer) {
		return LocalString(std::wstring_view{buffer});
	}

	template <size_t S>
	inline v8::Local<v8::String> LocalString(const wchar_t (&buffer)[S]) {
		return Nan::LocalString(buffer, S - 1);
	}

	inline v8::Local<v8::String> LocalString(const wchar_t* buffer, size_t length) {
		return Nan::LocalString(std::wstring_view{buffer, length});
	}
}