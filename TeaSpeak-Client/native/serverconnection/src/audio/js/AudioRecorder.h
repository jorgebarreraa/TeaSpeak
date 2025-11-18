#pragma once

#include <nan.h>
#include <mutex>
#include <deque>

namespace tc::audio {
    class AudioInput;

    namespace recorder {
        class AudioConsumerWrapper;

        extern NAN_MODULE_INIT(init_js);

        extern NAN_METHOD(create_recorder);

        class AudioRecorderWrapper : public Nan::ObjectWrap {
            public:
                static NAN_MODULE_INIT(Init);
                static NAN_METHOD(NewInstance);
                static inline Nan::Persistent<v8::Function> & constructor() {
                    static Nan::Persistent<v8::Function> my_constructor;
                    return my_constructor;
                }

                explicit AudioRecorderWrapper(std::shared_ptr<AudioInput> /* input */);
                ~AudioRecorderWrapper() override;

                static NAN_METHOD(_get_device);
                static NAN_METHOD(_set_device);

                static NAN_METHOD(_start);
                static NAN_METHOD(_started);
                static NAN_METHOD(_stop);

                static NAN_METHOD(_create_consumer);
                static NAN_METHOD(_get_consumers);
                static NAN_METHOD(_delete_consumer);

                static NAN_METHOD(_set_volume);
                static NAN_METHOD(_get_volume);

                static NAN_METHOD(get_audio_processor);
                static NAN_METHOD(create_level_meter);

                std::shared_ptr<AudioConsumerWrapper> create_consumer();
                void delete_consumer(const AudioConsumerWrapper*);

                inline std::deque<std::shared_ptr<AudioConsumerWrapper>> consumers() {
                    std::lock_guard lock{this->consumer_mutex};
                    return this->consumer_;
                }

                void do_wrap(const v8::Local<v8::Object>& /* obj */);

                inline void js_ref() { this->Ref(); }
                inline void js_unref() { this->Unref(); }
            private:
                std::shared_ptr<AudioInput> input_;

                /* javascript consumer */
                std::mutex consumer_mutex;
                std::deque<std::shared_ptr<AudioConsumerWrapper>> consumer_;
        };
    }
}