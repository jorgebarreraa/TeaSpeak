#pragma once

#include <nan.h>
#include <atomic>

namespace tc::audio {
    class AbstractAudioLevelMeter;
}

namespace tc::audio::recorder {
    class AudioLevelMeterWrapper : public Nan::ObjectWrap {
        public:
            /* Static JavaScript methods */
            static NAN_MODULE_INIT(Init);
            static NAN_METHOD(NewInstance);
            static NAN_METHOD(create_device_level_meter);

            static inline Nan::Persistent<v8::Function> & constructor() {
                static Nan::Persistent<v8::Function> my_constructor;
                return my_constructor;
            }

            explicit AudioLevelMeterWrapper(std::shared_ptr<AbstractAudioLevelMeter>);
            ~AudioLevelMeterWrapper() override;

            /* JavaScript member methods */
            static NAN_METHOD(start);
            static NAN_METHOD(running);
            static NAN_METHOD(stop);
            static NAN_METHOD(set_callback);

            inline void wrap(v8::Local<v8::Object> object) {
                Nan::ObjectWrap::Wrap(object);
            }
        private:
            static void timer_callback(uv_timer_t*);

            std::shared_ptr<AbstractAudioLevelMeter> handle{};

            /* Access only within the js event loop */
            uv_timer_t update_timer{};
            size_t update_timer_interval{50};

            Nan::Persistent<v8::Function> callback{};

            void test_timer();
    };
}