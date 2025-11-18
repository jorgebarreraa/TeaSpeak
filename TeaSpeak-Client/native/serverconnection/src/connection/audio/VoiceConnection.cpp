#include "VoiceConnection.h"
#include "VoiceClient.h"
#include "../ServerConnection.h"
#include "AudioSender.h"
#include "../../audio/js/AudioConsumer.h"
#include "../../audio/AudioInput.h"
#include "../../logger.h"
#include <misc/endianness.h> /* MUST be included as last file */

using namespace std;
using namespace tc;
using namespace tc::connection;
using namespace ts;
using namespace ts::protocol;
using namespace audio::recorder;

VoiceConnectionWrap::VoiceConnectionWrap(const std::shared_ptr<VoiceConnection>& handle) : handle(handle) {}
VoiceConnectionWrap::~VoiceConnectionWrap() {
	if(!this->_voice_recoder_handle.IsEmpty()) {
		auto old_consumer = this->_voice_recoder_ptr;
		assert(old_consumer);

		std::lock_guard read_lock{old_consumer->native_read_callback_lock};
        old_consumer->native_read_callback = nullptr;
	}
}

void VoiceConnectionWrap::do_wrap(const v8::Local<v8::Object> &object) {
	this->Wrap(object);
}

NAN_MODULE_INIT(VoiceConnectionWrap::Init) {
	auto klass = Nan::New<v8::FunctionTemplate>(VoiceConnectionWrap::NewInstance);
	klass->SetClassName(Nan::New("VoiceConnection").ToLocalChecked());
	klass->InstanceTemplate()->SetInternalFieldCount(1);

	Nan::SetPrototypeMethod(klass, "decoding_supported", VoiceConnectionWrap::_decoding_supported);
	Nan::SetPrototypeMethod(klass, "encoding_supported", VoiceConnectionWrap::_encoding_supported);

	Nan::SetPrototypeMethod(klass, "register_client", VoiceConnectionWrap::register_client);
	Nan::SetPrototypeMethod(klass, "available_clients", VoiceConnectionWrap::available_clients);
	Nan::SetPrototypeMethod(klass, "unregister_client", VoiceConnectionWrap::unregister_client);

	Nan::SetPrototypeMethod(klass, "audio_source", VoiceConnectionWrap::audio_source);
	Nan::SetPrototypeMethod(klass, "set_audio_source", VoiceConnectionWrap::set_audio_source);

	Nan::SetPrototypeMethod(klass, "get_encoder_codec", VoiceConnectionWrap::get_encoder_codec);
	Nan::SetPrototypeMethod(klass, "set_encoder_codec", VoiceConnectionWrap::set_encoder_codec);

	Nan::SetPrototypeMethod(klass, "enable_voice_send", VoiceConnectionWrap::enable_voice_send);

	constructor().Reset(Nan::GetFunction(klass).ToLocalChecked());
}

NAN_METHOD(VoiceConnectionWrap::NewInstance) {
	if(!info.IsConstructCall())
		Nan::ThrowError("invalid invoke!");
}

NAN_METHOD(VoiceConnectionWrap::_connected) {
	info.GetReturnValue().Set(true);
}

NAN_METHOD(VoiceConnectionWrap::_encoding_supported) {
	if(info.Length() != 1) {
		Nan::ThrowError("invalid argument count");
		return;
	}
	auto codec = info[0]->Uint32Value(Nan::GetCurrentContext()).FromMaybe(0);

	info.GetReturnValue().Set(codec >= 4 && codec <= 5); /* ignore SPEX currently :/ */
}

NAN_METHOD(VoiceConnectionWrap::_decoding_supported) {
	if(info.Length() != 1) {
		Nan::ThrowError("invalid argument count");
		return;
	}
	auto codec = info[0]->Uint32Value(Nan::GetCurrentContext()).FromMaybe(0);

	info.GetReturnValue().Set(codec >= 4 && codec <= 5); /* ignore SPEX currently :/ */
}

NAN_METHOD(VoiceConnectionWrap::register_client) {
    auto connection = ObjectWrap::Unwrap<VoiceConnectionWrap>(info.Holder());

	if(info.Length() != 1) {
		Nan::ThrowError("invalid argument count");
		return;
	}
	auto id = info[0]->Uint32Value(Nan::GetCurrentContext()).FromMaybe(0);
	auto handle = connection->handle.lock();
	if(!handle) {
		Nan::ThrowError("handle has been deallocated");
		return;
	}

	auto client = handle->register_client(id);
	if(!client) {
		Nan::ThrowError("failed to register client");
		return;
	}
	client->initialize_js_object();
	info.GetReturnValue().Set(client->js_handle());
}

NAN_METHOD(VoiceConnectionWrap::available_clients) {
    auto connection = ObjectWrap::Unwrap<VoiceConnectionWrap>(info.Holder());

	auto handle = connection->handle.lock();
	if(!handle) {
		Nan::ThrowError("handle has been deallocated");
		return;
	}

	auto client = handle->clients();

	v8::Local<v8::Array> result = Nan::New<v8::Array>((int) client.size());
	for(uint32_t index{0}; index < client.size(); index++)
		Nan::Set(result, index, client[index]->js_handle());

	info.GetReturnValue().Set(result);
}

NAN_METHOD(VoiceConnectionWrap::unregister_client) {
    auto connection = ObjectWrap::Unwrap<VoiceConnectionWrap>(info.Holder());

	if(info.Length() != 1) {
		Nan::ThrowError("invalid argument count");
		return;
	}
	auto id = info[0]->Uint32Value(Nan::GetCurrentContext()).FromMaybe(0);
	auto handle = connection->handle.lock();
	if(!handle) {
		Nan::ThrowError("handle has been deallocated");
		return;
	}

	auto client = handle->find_client(id);
	if(!client) {
		Nan::ThrowError("missing client");
		return;
	}

	client->finalize_js_object();
	handle->delete_client(client);
}

NAN_METHOD(VoiceConnectionWrap::audio_source) {
	auto client = ObjectWrap::Unwrap<VoiceConnectionWrap>(info.Holder());
	info.GetReturnValue().Set(client->_voice_recoder_handle.Get(info.GetIsolate()));
}

NAN_METHOD(VoiceConnectionWrap::set_audio_source) {
    auto connection = ObjectWrap::Unwrap<VoiceConnectionWrap>(info.Holder());

	if(info.Length() != 1) {
		Nan::ThrowError("invalid argument count");
		return;
	}


	if(!Nan::New(AudioConsumerWrapper::constructor_template())->HasInstance(info[0]) && !info[0]->IsNullOrUndefined()) {
		Nan::ThrowError("invalid consumer (Consumer must be native!)");
		return;
	}

	auto handle = connection->handle.lock();
	if(!handle) {
		Nan::ThrowError("handle has been deallocated");
		return;
	}

    connection->release_recorder();
	if(!info[0]->IsNullOrUndefined()) {
        connection->_voice_recoder_ptr = ObjectWrap::Unwrap<audio::recorder::AudioConsumerWrapper>(info[0]->ToObject(Nan::GetCurrentContext()).ToLocalChecked());
        connection->_voice_recoder_handle.Reset(info[0]->ToObject(Nan::GetCurrentContext()).ToLocalChecked());

		weak_ptr weak_handle = handle;
        auto sample_rate = connection->_voice_recoder_ptr->sample_rate();
		auto channels = connection->_voice_recoder_ptr->channel_count();

		lock_guard read_lock{connection->_voice_recoder_ptr->native_read_callback_lock};
		connection->_voice_recoder_ptr->native_read_callback = [weak_handle, sample_rate, channels](const float* buffer, size_t sample_count) {
			auto handle = weak_handle.lock();
			if(!handle) {
				log_warn(category::audio, tr("Missing voice connection handle. Dropping input!"));
				return;
			}

			auto sender = handle->voice_sender();
			if(sender) {
				if(sample_count > 0 && buffer) {
                    sender->send_data(buffer, sample_count, sample_rate, channels);
				} else {
                    sender->send_stop();
				}
			} else {
				log_warn(category::audio, tr("Missing voice connection audio sender. Dropping input!"));
				return;
			}
		};
	}
}

NAN_METHOD(VoiceConnectionWrap::get_encoder_codec) {
	auto connection = ObjectWrap::Unwrap<VoiceConnectionWrap>(info.Holder());
	auto handle = connection->handle.lock();
	if(!handle) {
		Nan::ThrowError("handle has been deallocated");
		return;
	}

	auto codec = handle->get_encoder_codec();
	info.GetReturnValue().Set(audio::codec::audio_codec_to_protocol_id(codec).value_or(-1));
}

NAN_METHOD(VoiceConnectionWrap::set_encoder_codec) {
	auto connection = ObjectWrap::Unwrap<VoiceConnectionWrap>(info.Holder());
	auto handle = connection->handle.lock();
	if(!handle) {
		Nan::ThrowError("handle has been deallocated");
		return;
	}


	if(info.Length() != 1 || !info[0]->IsNumber()) {
		Nan::ThrowError("Invalid arguments");
		return;
	}

	auto codec = audio::codec::audio_codec_from_protocol_id(info[0]->NumberValue(Nan::GetCurrentContext()).FromMaybe(-1));
	if(!codec.has_value()) {
        Nan::ThrowError("unknown codec id");
        return;
	}
	handle->set_encoder_codec(*codec);
}

NAN_METHOD(VoiceConnectionWrap::enable_voice_send) {
	auto _this = ObjectWrap::Unwrap<VoiceConnectionWrap>(info.Holder());
	auto handle = _this->handle.lock();
	if(!handle) {
		Nan::ThrowError("handle has been deallocated");
		return;
	}

	if(info.Length() != 1 || !info[0]->IsBoolean()) {
		Nan::ThrowError("Invalid arguments");
		return;
	}

	auto sender = handle->voice_sender();
	if(!sender) {
		Nan::ThrowError("Voice sender has been deallocated");
		return;
	}

	sender->set_voice_send_enabled(info[0]->BooleanValue(info.GetIsolate()));
}


void VoiceConnectionWrap::release_recorder() {
	if(!this->_voice_recoder_handle.IsEmpty()) {
		assert(v8::Isolate::GetCurrent());

		auto old_consumer = this->_voice_recoder_ptr;
		assert(old_consumer);

		lock_guard read_lock(this->_voice_recoder_ptr->native_read_callback_lock);
		this->_voice_recoder_ptr->native_read_callback = nullptr;
	} else {
		assert(!this->_voice_recoder_ptr);
	}

	this->_voice_recoder_ptr = nullptr;
	this->_voice_recoder_handle.Reset();
}


VoiceConnection::VoiceConnection(ServerConnection *handle) : _handle(handle) {
	this->_voice_sender = make_shared<VoiceSender>(this);
	this->_voice_sender->_ref = this->_voice_sender;
	this->_voice_sender->set_codec(audio::codec::AudioCodec::OpusMusic);
}
VoiceConnection::~VoiceConnection() {
	if(v8::Isolate::GetCurrent())
		this->finalize_js_object();
	else {
		assert(this->_js_handle.IsEmpty());
	}

	this->_voice_sender->finalize();
}

void VoiceConnection::reset() {
	lock_guard lock{this->_clients_lock};
	this->_clients.clear();
}

void VoiceConnection::execute_tick() {
    decltype(this->_clients) clients{};
    {
        std::lock_guard lg{this->_clients_lock};
        clients = this->_clients;
    }
    for(auto& client : clients)
        client->execute_tick();
}

void VoiceConnection::initialize_js_object() {
	auto object_wrap = new VoiceConnectionWrap(this->ref());
	auto object = Nan::NewInstance(Nan::New(VoiceConnectionWrap::constructor()), 0, nullptr).ToLocalChecked();
	object_wrap->do_wrap(object);

	this->_js_handle.Reset(Nan::GetCurrentContext()->GetIsolate(), object);
}

void VoiceConnection::finalize_js_object() {
	this->_js_handle.Reset();
}

std::shared_ptr<VoiceClient> VoiceConnection::find_client(uint16_t client_id) {
	lock_guard lock{this->_clients_lock};
	for(const auto& client : this->_clients)
		if(client->client_id() == client_id)
			return client;

	return nullptr;
}

std::shared_ptr<VoiceClient> VoiceConnection::register_client(uint16_t client_id) {
	lock_guard lock{this->_clients_lock};
	auto client = this->find_client(client_id);
	if(client) return client;

	client = make_shared<VoiceClient>(this->ref(), client_id);
	client->ref_ = client;
    client->initialize();
	this->_clients.push_back(client);
	return client;
}

void VoiceConnection::delete_client(const std::shared_ptr<tc::connection::VoiceClient> &client) {
	{
		lock_guard lock(this->_clients_lock);
		auto it = find(this->_clients.begin(), this->_clients.end(), client);
		if(it != this->_clients.end()) {
			this->_clients.erase(it);
		}
	}

	//TODO deinitialize client
}

void VoiceConnection::process_packet(const ts::protocol::PacketParser &packet) {
	if(packet.type() == ts::protocol::PacketType::VOICE) {
		if(packet.payload_length() < 5) {
			//TODO log invalid voice packet
			return;
		}

		auto payload = packet.payload();
		auto packet_id = be2le16(&payload[0]);
		auto client_id = be2le16(&payload[2]);
		auto codec_id = (uint8_t) payload[4];
		auto flag_head = packet.has_flag(ts::protocol::PacketFlag::Compressed);
		//container->voice_data = packet->data().length() > 5 ? packet->data().range(5) : pipes::buffer{};

		//log_info(category::voice_connection, tr("Received voice packet from {}. Packet ID: {}"), client_id, packet_id);
		auto client = this->find_client(client_id);
		if(!client) {
			log_warn(category::voice_connection, tr("Received voice packet from unknown client {}. Dropping packet!"), client_id);
			return;
		}

		if(payload.length() > 5) {
            client->process_packet(packet_id, payload.view(5), codec_id, flag_head);
		} else {
            client->process_packet(packet_id, pipes::buffer_view{nullptr, 0}, codec_id, flag_head);
		}
	} else {
		//TODO implement whisper
	}
}

void VoiceConnection::set_encoder_codec(const audio::codec::AudioCodec &codec) {
	auto vs = this->_voice_sender;
	if(vs) {
        vs->set_codec(codec);
	}
}

audio::codec::AudioCodec VoiceConnection::get_encoder_codec() {
	auto vs = this->_voice_sender;
	return vs ? vs->target_codec() : audio::codec::AudioCodec::Unknown;
}