#pragma once

#include <nan.h>
#include <include/NanEventCallback.h>

namespace tc::audio {
    namespace filter {
        class Filter;
    }

    namespace recorder {
        class AudioConsumerWrapper;

        class AudioFilterWrapper : public Nan::ObjectWrap {
                friend class AudioConsumerWrapper;
            public:
                static NAN_MODULE_INIT(Init);
                static NAN_METHOD(NewInstance);
                static inline Nan::Persistent<v8::Function> & constructor() {
                    static Nan::Persistent<v8::Function> my_constructor;
                    return my_constructor;
                }
                static inline Nan::Persistent<v8::FunctionTemplate> & constructor_template() {
                    static Nan::Persistent<v8::FunctionTemplate> my_constructor_template;
                    return my_constructor_template;
                }

                AudioFilterWrapper(std::string  name, std::shared_ptr<filter::Filter>  /* handle */);
                ~AudioFilterWrapper() override;

                static NAN_METHOD(_get_name);

                /* VAD and Threshold */
                static NAN_METHOD(_get_margin_time);
                static NAN_METHOD(_set_margin_time);

                /* VAD relevant */
                static NAN_METHOD(_get_level);

                /* threshold filter relevant */
                static NAN_METHOD(_get_threshold);
                static NAN_METHOD(_set_threshold);

                static NAN_METHOD(_get_attack_smooth);
                static NAN_METHOD(_set_attack_smooth);

                static NAN_METHOD(_get_release_smooth);
                static NAN_METHOD(_set_release_smooth);

                static NAN_METHOD(_set_analyze_filter);

                /* consume filter */
                static NAN_METHOD(_is_consuming);
                static NAN_METHOD(_set_consuming);

                inline std::shared_ptr<filter::Filter> filter() { return this->_filter; }
            private:
                std::shared_ptr<filter::Filter> _filter;
                std::string _name;

                void do_wrap(const v8::Local<v8::Object>& /* object */);

                Nan::callback_t<float> _call_analyzed;
                Nan::Persistent<v8::Function> _callback_analyzed;
        };
    }
}