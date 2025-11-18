#pragma once

#include <v8.h>
#include <nan.h>
#include <memory>
#include <mutex>
#include <functional>
#include <protocol/Packet.h>
#include "../../audio/codec/Converter.h"

namespace tc {
	namespace audio::recorder {
        class AudioConsumerWrapper;
    }

	namespace connection {
		class ServerConnection;
		class VoiceConnection;
		class VoiceClient;
		class VoiceSender;

		class VoiceConnectionWrap : public Nan::ObjectWrap {
			public:
				static NAN_MODULE_INIT(Init);
				static NAN_METHOD(NewInstance);
				static inline Nan::Persistent<v8::Function> & constructor() {
					static Nan::Persistent<v8::Function> my_constructor;
					return my_constructor;
				}

				explicit VoiceConnectionWrap(const std::shared_ptr<VoiceConnection>&);
				~VoiceConnectionWrap() override;

				void do_wrap(const v8::Local<v8::Object>&);
			private:
				static NAN_METHOD(_connected);
				static NAN_METHOD(_encoding_supported);
				static NAN_METHOD(_decoding_supported);

				static NAN_METHOD(register_client);
				static NAN_METHOD(available_clients);
				static NAN_METHOD(unregister_client);

				static NAN_METHOD(audio_source);
				static NAN_METHOD(set_audio_source);

				static NAN_METHOD(get_encoder_codec);
				static NAN_METHOD(set_encoder_codec);
				static NAN_METHOD(enable_voice_send);

				void release_recorder();

				std::function<void(const void* /* buffer */, size_t /* samples */)> _read_callback;
				audio::recorder::AudioConsumerWrapper* _voice_recoder_ptr = nullptr;
				Nan::Persistent<v8::Object> _voice_recoder_handle;
				std::weak_ptr<VoiceConnection> handle;
		};

		class VoiceConnection {
				friend class ServerConnection;
				friend class VoiceConnectionWrap;
			public:
				explicit VoiceConnection(ServerConnection*);
				virtual ~VoiceConnection();

				void reset();
				void execute_tick();

				void initialize_js_object();
				void finalize_js_object();

				ServerConnection* handle() { return this->_handle; }
				v8::Local<v8::Object> js_handle() {
					assert(v8::Isolate::GetCurrent());
					return this->_js_handle.Get(Nan::GetCurrentContext()->GetIsolate());
				}

				inline std::shared_ptr<VoiceConnection> ref() { return this->_ref.lock(); }
				inline std::deque<std::shared_ptr<VoiceClient>> clients() {
					std::lock_guard lock(this->_clients_lock);
					return this->_clients;
				}

				inline std::shared_ptr<VoiceSender> voice_sender() { return this->_voice_sender; }

				std::shared_ptr<VoiceClient> find_client(uint16_t /* client id */);
				std::shared_ptr<VoiceClient> register_client(uint16_t /* client id */);
				void delete_client(const std::shared_ptr<VoiceClient>&);

				void process_packet(const ts::protocol::PacketParser&);

				void set_encoder_codec(const audio::codec::AudioCodec& /* target */);
                audio::codec::AudioCodec get_encoder_codec();
			private:
				ServerConnection* _handle;
				std::weak_ptr<VoiceConnection> _ref;

				v8::Persistent<v8::Object> _js_handle;

				std::recursive_mutex _clients_lock;
				std::deque<std::shared_ptr<VoiceClient>> _clients;

				std::shared_ptr<VoiceSender> _voice_sender;
		};
	}
}