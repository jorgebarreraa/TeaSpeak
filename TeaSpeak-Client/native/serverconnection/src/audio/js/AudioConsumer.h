#pragma once

#include <array>
#include <mutex>
#include <deque>
#include <include/NanEventCallback.h>
#include "../AudioInput.h"
#include "../AudioReframer.h"

namespace tc::audio {
    class AudioInput;

    namespace filter {
        class Filter;
    }

    struct AudioInputBufferInfo;
    namespace recorder {
        class AudioFilterWrapper;
        class AudioRecorderWrapper;

        enum FilterMode {
            BYPASS,
            FILTER,
            BLOCK
        };

        /* FIXME: Rechunk audio data to 20ms with the input frame rate (that's what we need for networking stuff) */
        class AudioConsumerWrapper : public Nan::ObjectWrap {
                friend class AudioRecorderWrapper;
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

                AudioConsumerWrapper(const std::shared_ptr<AudioInput>& /* input */);
                ~AudioConsumerWrapper() override;

                static NAN_METHOD(_get_filters);
                static NAN_METHOD(_unregister_filter);

                static NAN_METHOD(_create_filter_vad);
                static NAN_METHOD(_create_filter_threshold);
                static NAN_METHOD(_create_filter_state);

                static NAN_METHOD(_get_filter_mode);
                static NAN_METHOD(_set_filter_mode);

                std::shared_ptr<AudioFilterWrapper> create_filter(const std::string& /* name */, const std::shared_ptr<filter::Filter>& /* filter impl */);
                void delete_filter(const AudioFilterWrapper*);

                inline std::deque<std::shared_ptr<AudioFilterWrapper>> filters() {
                    std::lock_guard lock(this->filter_mutex_);
                    return this->filter_;
                }

                [[nodiscard]] inline auto channel_count() const { return this->channel_count_; };
                [[nodiscard]] inline auto sample_rate() const { return this->sample_rate_; };

                [[nodiscard]] inline FilterMode filter_mode() const { return this->filter_mode_; }

                std::mutex native_read_callback_lock;
                std::function<void(const float * /* buffer */, size_t /* samples */)> native_read_callback;
            private:
                struct InputConsumer : public AudioInputConsumer {
                    std::mutex wrapper_mutex{};
                    AudioConsumerWrapper* wrapper{nullptr};

                    void handle_buffer(const AudioInputBufferInfo &, const float *) override;
                };

                std::shared_ptr<InputConsumer> consumer_handle{};

                size_t const channel_count_;
                size_t const sample_rate_;

                Nan::JavaScriptQueue js_queue;

                std::mutex filter_mutex_;
                std::deque<std::shared_ptr<AudioFilterWrapper>> filter_;
                FilterMode filter_mode_{FilterMode::FILTER};
                bool last_consumed = false;

                void do_wrap(const v8::Local<v8::Object>& /* object */);
                void delete_consumer();

                void handle_buffer(const AudioInputBufferInfo& /* info */, const float* /* buffer */);

                Nan::callback_t<> _call_ended;
                Nan::callback_t<> _call_started;
        };
    }
}