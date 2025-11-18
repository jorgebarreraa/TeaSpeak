#pragma once

#include <nan.h>
#include "../processing/AudioProcessor.h"

namespace tc::audio {
    class AudioProcessorWrapper : public Nan::ObjectWrap {
        public:
            static NAN_MODULE_INIT(Init);
            static NAN_METHOD(NewInstance);
            static inline Nan::Persistent<v8::Function> & constructor() {
                static Nan::Persistent<v8::Function> my_constructor;
                return my_constructor;
            }

            explicit AudioProcessorWrapper(const std::shared_ptr<AudioProcessor>& /* processor */);
            ~AudioProcessorWrapper() override;

            inline void wrap(v8::Local<v8::Object> object) {
                Nan::ObjectWrap::Wrap(object);
            }

            static NAN_METHOD(get_config);
            static NAN_METHOD(apply_config);

            static NAN_METHOD(get_statistics);
        private:
            struct Observer : public AudioProcessor::ProcessObserver {
                public:
                    explicit Observer(AudioProcessorWrapper* wrapper) : wrapper{wrapper} {}

                private:
                    AudioProcessorWrapper* wrapper;

                    void stream_processed(const AudioProcessor::Stats &stats) override;
            };

            std::weak_ptr<AudioProcessor> weak_processor{};
            Observer* registered_observer{nullptr};
    };
}