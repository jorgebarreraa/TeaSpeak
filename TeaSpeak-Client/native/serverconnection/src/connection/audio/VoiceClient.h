#pragma once

#include <array>
#include <nan.h>
#include <include/NanEventCallback.h>
#include <functional>
#include <optional>
#include <pipes/buffer.h>
#include "../../audio/AudioResampler.h"
#include "../../audio/codec/Converter.h"
#include "../../audio/AudioOutput.h"
#include "../../EventLoop.h"
#include "../../logger.h"

namespace tc::connection {
    class ServerConnection;
    class VoiceConnection;
    class VoiceClient;

    class VoiceClient : public event::EventEntry {
            friend class VoiceConnection;
        public:
            struct state {
                enum value {
                    buffering, /* this state is never active */
                    playing,
                    stopping,
                    stopped
                };

                constexpr static std::array names = {
                    "buffering",
                    "playing",
                    "stopping",
                    "stopped"
                };
            };
            VoiceClient(const std::shared_ptr<VoiceConnection>& /* connection */, uint16_t /* client id */);
            virtual ~VoiceClient();

            void initialize();

            [[nodiscard]] inline uint16_t client_id() const { return this->client_id_; }

            void initialize_js_object();
            void finalize_js_object();

            v8::Local<v8::Object> js_handle() {
                assert(v8::Isolate::GetCurrent());
                return this->js_handle_.Get(Nan::GetCurrentContext()->GetIsolate());
            }

            inline std::shared_ptr<VoiceClient> ref() { return this->ref_.lock(); }

            void process_packet(uint16_t packet_id, const pipes::buffer_view& /* buffer */, uint8_t /* payload codec id */, bool /* head */);
            void execute_tick();

            inline float get_volume() const { return this->volume_; }
            inline void set_volume(float value) { this->volume_ = value; }

            inline state::value state() { return this->state_; }

            void cancel_replay();

            std::function<void()> on_state_changed;

            inline std::shared_ptr<audio::AudioOutputSource> output_stream() { return this->output_source; }
        private:
            struct EncodedBuffer {
                bool head{false};

                std::chrono::system_clock::time_point receive_timestamp;

                pipes::buffer buffer;
                uint8_t codec{0xFF};

                uint16_t packet_id{0};
                EncodedBuffer* next{nullptr};
            };

            struct {
                uint16_t last_packet_id{0xFFFF}; /* the first packet id is 0 so one packet before is 0xFFFF */
                std::chrono::system_clock::time_point last_packet_timestamp{};

                std::mutex pending_lock{};
                EncodedBuffer* pending_buffers{nullptr};

                /* forces all packets which are within the next chain to replay until (inclusive) force_replay is reached */
                EncodedBuffer* force_replay{nullptr};

                bool process_pending{false};

                [[nodiscard]] inline std::chrono::system_clock::time_point stream_timeout() const {
                    return this->last_packet_timestamp + std::chrono::milliseconds{1000};
                }
            } packet_queue;

            struct {
                /* the decoder has been initialized with fec data */
                bool decoder_initialized{false};
                audio::codec::AudioCodec current_codec{audio::codec::AudioCodec::Unknown};
                std::unique_ptr<audio::codec::AudioDecoder> decoder{};
                std::unique_ptr<audio::AudioResampler> resampler{};
            } decoder;

            /* might be null (if audio hasn't been initialized) */
            std::shared_ptr<audio::AudioOutputSource> output_source{};

            std::weak_ptr<VoiceClient> ref_{};
            v8::Persistent<v8::Object> js_handle_{};

            uint16_t client_id_{0};
            float volume_{1.f};

            state::value state_{state::stopped};
            inline void set_state(state::value value) {
                if(value == this->state_) {
                    return;
                }

                log_warn(category::audio, tr("Client {} state changed from {} to {}"), this->client_id_, state::names[this->state_], state::names[value]);
                this->state_ = value;
                if(this->on_state_changed) {
                    this->on_state_changed();
                }
            }

            /* Call only within the event loop or when execute lock is locked */
            void drop_enqueued_buffers();

            void event_execute(const std::chrono::system_clock::time_point &point) override;
            bool handle_output_underflow(size_t sample_count);

            /**
             * Reset the decoder.
             */
            void reset_decoder(bool /* deallocate */);

            /**
             * Decode and playback an audio packet.
             * If fec decode is active we try to decode the fec data within the packet and playback them instead of the packet data.
             * Note: If fec is set and the decoder hasn't been initialized we'll drop the buffer.
             */
            void playback_audio_packet(uint8_t /* codec protocol id */, const void* /* buffer */, size_t /* buffer length */, bool /* use fec data */);
    };


    class VoiceClientWrap : public Nan::ObjectWrap {
        public:
            static NAN_MODULE_INIT(Init);
            static NAN_METHOD(NewInstance);
            static inline Nan::Persistent<v8::Function> & constructor() {
                static Nan::Persistent<v8::Function> my_constructor;
                return my_constructor;
            }

            explicit VoiceClientWrap(const std::shared_ptr<VoiceClient>&);
            ~VoiceClientWrap() override;

            void do_wrap(const v8::Local<v8::Object>&);
        private:
            static NAN_METHOD(_get_state);
            static NAN_METHOD(_get_volume);
            static NAN_METHOD(_set_volume);
            static NAN_METHOD(_abort_replay);
            static NAN_METHOD(_get_stream);

            std::weak_ptr<VoiceClient> _handle;

            bool currently_playing_{false};
            Nan::callback_t<> call_state_changed;
            void call_state_changed_();
    };
}