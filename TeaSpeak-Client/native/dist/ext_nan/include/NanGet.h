#pragma once

#include "./NanStrings.h"
#include <v8.h>
#include <type_traits>

namespace Nan {
	inline v8::MaybeLocal<v8::Object> GetObject(v8::Local<v8::Object> object, v8::Local<v8::Value> key) {
		auto result = Nan::Get(object, key);
		if(result.IsEmpty())
			return v8::Local<v8::Object>{};

		return result.ToLocalChecked()->ToObject(v8::Isolate::GetCurrent()->GetCurrentContext());
	}

	inline v8::MaybeLocal<v8::Value> Get(v8::Local<v8::Object> object, const std::string_view& key) {
		return Nan::Get(object, Nan::LocalString(key));
	}

	template <class T, typename K, typename std::enable_if<!std::is_same<v8::Value, T>::value, int>::type * = nullptr>
	inline v8::MaybeLocal<T> Get(v8::Local<v8::Object> object, K key) {
		MaybeLocal<v8::Value> result{Nan::Get(object, key)};
		if(result.IsEmpty())
			return v8::Local<T>{};

		return result.ToLocalChecked().As<T>();
	}

	template <class T, typename K>
	inline v8::Local<T> GetLocal(v8::Local<v8::Object> object, K key, const v8::Local<T>& default_value = v8::Local<T>{}) {
		return Nan::Get<T, K>(object, key).FromMaybe(default_value);
	}

	template <typename T>
	inline v8::MaybeLocal<v8::String> GetString(v8::Local<v8::Object> object, T key) { return Get<v8::String>(object, key); }

	template <typename T>
	inline v8::Local<v8::String> GetStringLocal(v8::Local<v8::Object> object, T key) { return GetLocal<v8::String>(object, key); }
}