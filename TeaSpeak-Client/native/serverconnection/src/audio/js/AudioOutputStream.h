#pragma once

#include <nan.h>
#include <include/NanEventCallback.h>

namespace tc::audio {
    class AudioResampler;
    class AudioOutputSource;

    class AudioOutputStreamWrapper : public Nan::ObjectWrap {
        public:
            static NAN_MODULE_INIT(Init);
            static NAN_METHOD(NewInstance);
            static inline Nan::Persistent<v8::Function> & constructor() {
                static Nan::Persistent<v8::Function> my_constructor;
                return my_constructor;
            }

            AudioOutputStreamWrapper(const std::shared_ptr<AudioOutputSource>& /* stream */, bool /* own */);
            ~AudioOutputStreamWrapper() override;

            void do_wrap(const v8::Local<v8::Object>&);
            void drop_stream();
        private:
            static ssize_t write_data(const std::shared_ptr<AudioOutputSource>&, void* source, size_t samples, bool interleaved);

            /* general methods */
            static NAN_METHOD(_get_buffer_latency);
            static NAN_METHOD(_set_buffer_latency);
            static NAN_METHOD(_get_buffer_max_latency);
            static NAN_METHOD(_set_buffer_max_latency);

            static NAN_METHOD(_flush_buffer);

            /* methods for owned streams only */
            static NAN_METHOD(_write_data);
            static NAN_METHOD(_write_data_rated);

            static NAN_METHOD(_clear);
            static NAN_METHOD(_deleted);
            static NAN_METHOD(_delete);

            std::unique_ptr<AudioResampler> _resampler;
            std::shared_ptr<AudioOutputSource> _own_handle;
            std::weak_ptr<AudioOutputSource> _handle;

            Nan::callback_t<> call_underflow;
            Nan::callback_t<> call_overflow;
    };
}