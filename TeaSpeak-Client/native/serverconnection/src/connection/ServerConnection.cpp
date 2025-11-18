#include "ServerConnection.h"
#include "ProtocolHandler.h"
#include "Socket.h"
#include "audio/VoiceConnection.h"
#include "audio/AudioSender.h"
#include "../logger.h"
#include "../hwuid.h"

#include <sstream>
#include <thread>
#include <iostream>
#include <misc/net.h>
#include <misc/base64.h>
#include <misc/endianness.h>
#include <misc/strobf.h>
#include <iomanip>
#include <include/NanGet.h>

//#define FUZZ_VOICE
//#define SHUFFLE_VOICE

using namespace std;
using namespace std::chrono;
using namespace tc::connection;

string ErrorHandler::get_error_message(ErrorHandler::ErrorId id) const {
	if(id == 0) {
        return "success";
	}

	auto index = (-id - 1) % this->kErrorBacklog;
	assert(index >= 0 && index < this->kErrorBacklog);
	return this->error_messages[index];
}

ErrorHandler::ErrorId ErrorHandler::register_error(const string &message) {
	auto index = this->error_index++ % this->kErrorBacklog;
	this->error_messages[index] = message;
	return -index - 1;
}

ServerConnection::ServerConnection() {
	logger::debug(category::connection, tr("Allocated ServerConnection {}."), (void*) this);
}

ServerConnection::~ServerConnection() {
	logger::debug(category::connection, tr("Begin deallocating ServerConnection {}."), (void*) this);
	if(this->protocol_handler && this->protocol_handler->connection_state == connection_state::CONNECTED) {
        this->protocol_handler->disconnect("server connection has been destoryed");
	}
	this->close_connection();
	this->finalize();

	logger::debug(category::connection, tr("Finished deallocating ServerConnection {}."), (void*) this);
}

NAN_MODULE_INIT(ServerConnection::Init) {
	auto klass = Nan::New<v8::FunctionTemplate>(ServerConnection::new_instance);
	klass->SetClassName(Nan::New("NativeServerConnection").ToLocalChecked());
	klass->InstanceTemplate()->SetInternalFieldCount(1);

	Nan::SetPrototypeMethod(klass, "connect", ServerConnection::_connect);
	Nan::SetPrototypeMethod(klass, "connected", ServerConnection::_connected);
	Nan::SetPrototypeMethod(klass, "disconnect", ServerConnection::_disconnect);
	Nan::SetPrototypeMethod(klass, "error_message", ServerConnection::_error_message);
	Nan::SetPrototypeMethod(klass, "send_command", ServerConnection::_send_command);
	Nan::SetPrototypeMethod(klass, "send_voice_data", ServerConnection::_send_voice_data);
	Nan::SetPrototypeMethod(klass, "send_voice_data_raw", ServerConnection::_send_voice_data_raw);
	Nan::SetPrototypeMethod(klass, "current_ping", ServerConnection::_current_ping);
    Nan::SetPrototypeMethod(klass, "statistics", ServerConnection::statistics);

	constructor().Reset(Nan::GetFunction(klass).ToLocalChecked());
}

NAN_METHOD(ServerConnection::new_instance) {
	//info.GetReturnValue().Set(Nan::New<v8::String>("Hello World").ToLocalChecked());

	if (info.IsConstructCall()) {
		auto instance = new ServerConnection();
		instance->Wrap(info.This());
		instance->initialize();
		//Nan::Set(info.This(), New<String>("type").ToLocalChecked(), New<Number>(type));
		info.GetReturnValue().Set(info.This());
	} else {
		v8::Local<v8::Function> cons = Nan::New(constructor());

		Nan::TryCatch try_catch;
		auto result = Nan::NewInstance(cons, 0, nullptr);
		if(try_catch.HasCaught()) {
			try_catch.ReThrow();
			return;
		}
		info.GetReturnValue().Set(result.ToLocalChecked());
	}
}

void ServerConnection::initialize() {
	this->event_loop_exit = false;
	this->event_thread = thread(&ServerConnection::event_loop, this);
	this->protocol_handler = make_unique<ProtocolHandler>(this);
	this->voice_connection = make_shared<VoiceConnection>(this);
	this->voice_connection->_ref = this->voice_connection;
	this->voice_connection->initialize_js_object();

	this->execute_pending_commands = Nan::async_callback([&]{
		Nan::HandleScope scope;
		this->_execute_callback_commands();
	});
	this->execute_pending_voice = Nan::async_callback([&]{
		Nan::HandleScope scope;
		this->_execute_callback_voice();
	});
	this->execute_callback_disconnect = Nan::async_callback([&](std::string reason){
		Nan::HandleScope scope;
		this->_execute_callback_disconnect(reason);
	});


	this->call_connect_result = Nan::async_callback([&](ErrorHandler::ErrorId error_id) {
		Nan::HandleScope scope;
		/* lets update the server type */
		{
			auto js_this = this->handle();
			Nan::Set(js_this, Nan::New<v8::String>("server_type").ToLocalChecked(), Nan::New<v8::Number>(this->protocol_handler->server_type));
		}

		/* lets call the connect callback */
		{
			v8::Local<v8::Value> argv[1];
			argv[0] = Nan::New<v8::Number>(error_id);

			if(this->callback_connect)
                Nan::Call(*this->callback_connect, 1, argv);
			this->callback_connect = nullptr;
		}
	});


	this->call_disconnect_result = Nan::async_callback([&](ErrorHandler::ErrorId error_id) {
		Nan::HandleScope scope;
		v8::Local<v8::Value> argv[1];
		argv[0] = Nan::New<v8::Number>(error_id);

		if(this->callback_disconnect)
            Nan::Call(*this->callback_disconnect, 1, argv);
		this->callback_disconnect = nullptr;
	});

	auto js_this = this->handle();
	Nan::Set(js_this, Nan::New<v8::String>("_voice_connection").ToLocalChecked(), this->voice_connection->js_handle());

}

void ServerConnection::finalize() {
	this->event_loop_exit = true;
	this->event_condition.notify_all();
	this->event_thread.join();
}

void ServerConnection::event_loop() {
	auto eval_timeout = [&]{
		auto best = this->next_tick;
		if(this->next_resend < best)
			best = this->next_resend;
		if(this->event_loop_execute_connection_close)
			return system_clock::time_point{};
		return best;
	};

	while(!this->event_loop_exit) {
		auto timeout = eval_timeout();
		{
			unique_lock lock(this->event_lock);
			this->event_condition.wait_until(lock, timeout, [&]{
				if(eval_timeout() != timeout)
					return true;
				return this->event_loop_exit;
			});

			if(this->event_loop_exit)
				break;
		}
		if(this->event_loop_execute_connection_close) {
			this->close_connection();
			this->event_loop_execute_connection_close = false;
		}
		auto date = chrono::system_clock::now();
		if(this->next_tick <= date) {
			this->next_tick = date + chrono::milliseconds(500);
			this->execute_tick();
		}
		if(this->next_resend <= date) {
			this->next_resend = date + chrono::seconds(5);
			if(this->protocol_handler)
				this->protocol_handler->execute_resend();
		}
	}
}

void ServerConnection::schedule_resend(const std::chrono::system_clock::time_point &timeout) {
	if(this->next_resend > timeout) {
		this->next_resend = timeout;
		this->event_condition.notify_one();
	}
}

#ifdef WIN32
inline std::string wsa_error_str(int code) {
    int err;
    char msgbuf[256];   // for a message up to 255 bytes.
    msgbuf[0] = '\0';    // Microsoft doesn't guarantee this on man page.

    err = WSAGetLastError();
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,   // flags
                  NULL,                // lpsource
                  err,                 // message id
                  MAKELANGID (LANG_NEUTRAL, SUBLANG_DEFAULT),    // languageid
                  msgbuf,              // output buffer
                  sizeof(msgbuf),     // size of msgbuf, bytes
                  NULL);               // va_list of arguments

    if (!*msgbuf)
        sprintf(msgbuf, "%d", err);  // provide error # if no string available
    return std::string{msgbuf};
}
#endif

NAN_METHOD(ServerConnection::connect) {
	if(!this->protocol_handler) {
		Nan::ThrowError("ServerConnection not initialized");
		return;
	}
	if(info.Length() != 1) {
		Nan::ThrowError(tr("Invalid argument count"));
		return;
	}

	v8::Local arguments = info[0]->ToObject(Nan::GetCurrentContext()).ToLocalChecked();

	if(!arguments->IsObject()) {
		Nan::ThrowError(tr("Invalid argument"));
		return;
	}

	auto remote_host = Nan::Get(arguments, Nan::New<v8::String>("remote_host").ToLocalChecked()).ToLocalChecked();
	auto remote_port = Nan::Get(arguments, Nan::New<v8::String>("remote_port").ToLocalChecked()).ToLocalChecked();
	auto timeout = Nan::Get(arguments, Nan::New<v8::String>("timeout").ToLocalChecked()).ToLocalChecked();

	auto callback = Nan::Get(arguments, Nan::New<v8::String>("callback").ToLocalChecked()).ToLocalChecked();

	auto identity_key = Nan::Get(arguments, Nan::New<v8::String>("identity_key").ToLocalChecked()).ToLocalChecked();
	auto teamspeak = Nan::Get(arguments, Nan::New<v8::String>("teamspeak").ToLocalChecked()).ToLocalChecked();

	if(!identity_key->IsString() && !identity_key->IsNullOrUndefined()) {
		Nan::ThrowError(tr("Invalid identity"));
		return;
	}

	if(!remote_host->IsString() || !remote_port->IsNumber()) {
		Nan::ThrowError(tr("Invalid argument host/port"));
		return;
	}

	if(!callback->IsFunction()) {
		Nan::ThrowError(tr("Invalid callback"));
		return;
	}

	unique_lock _disconnect_lock(this->disconnect_lock, defer_lock);
	if(!_disconnect_lock.try_lock_for(chrono::milliseconds(500))) {
		Nan::ThrowError(tr("failed to acquire disconnect lock"));
		return;
	}

	this->callback_connect = make_unique<Nan::Callback>(callback.As<v8::Function>());

	this->voice_connection->reset();
	this->protocol_handler->reset();

	std::optional<std::string> identity{std::nullopt};
	if(identity_key->IsString()) {
        auto identity_encoded = Nan::Utf8String{identity_key->ToString(Nan::GetCurrentContext()).ToLocalChecked()};
        identity = std::make_optional(
                base64::decode(std::string_view{*identity_encoded, (size_t) identity_encoded.length()})
        );

	}

    if(!this->protocol_handler->initialize_identity(identity)) {
        Nan::ThrowError(tr("failed to initialize crypto identity"));
        return;
    }

	sockaddr_storage remote_address{};
	/* resolve address */
	{
		addrinfo hints{}, *result;
		memset(&hints, 0, sizeof(hints));

		hints.ai_family = AF_UNSPEC;

		auto remote_host_ = Nan::Utf8String{remote_host->ToString(Nan::GetCurrentContext()).ToLocalChecked()};
		if(getaddrinfo(*remote_host_, nullptr, &hints, &result) != 0 || !result) {
			this->call_connect_result(this->errors.register_error(tr("failed to resolve hostname")));
			return;
		}

		memcpy(&remote_address, result->ai_addr, result->ai_addrlen);
		freeaddrinfo(result);
	}

	switch(remote_address.ss_family) {
		case AF_INET:
			((sockaddr_in*) &remote_address)->sin_port = htons(remote_port->Int32Value(Nan::GetCurrentContext()).FromMaybe(0));
			break;

		case AF_INET6:
			((sockaddr_in6*) &remote_address)->sin6_port = htons(remote_port->Int32Value(Nan::GetCurrentContext()).FromMaybe(0));
            break;

		default:
		    break;
	}

    log_info(category::connection, tr("Connecting to {}."), net::to_string(remote_address));
	this->socket = make_shared<UDPSocket>(remote_address);
	if(!this->socket->initialize()) {
		this->call_connect_result(this->errors.register_error("failed to initialize socket"));
		this->socket = nullptr;
		return;
	}

	this->socket->on_data = [&](const pipes::buffer_view& buffer) { this->protocol_handler->progress_packet(buffer); };
    this->socket->on_fatal_error = [&](int code, int detail) {
#if WIN32
        auto message = wsa_error_str(detail);
#else
        auto message = std::string{strerror(detail)};
#endif
        this->execute_callback_disconnect.call((code == 1 ? tr("Failed to received data: ") : tr("Failed to send data: ")) + message, false);
        this->close_connection();
    };

	if(teamspeak->IsBoolean() && teamspeak->BooleanValue(info.GetIsolate())) {
        this->protocol_handler->server_type = server_type::TEAMSPEAK;
	}
	this->protocol_handler->connect();
}

NAN_METHOD(ServerConnection::_connected) {
	return ObjectWrap::Unwrap<ServerConnection>(info.Holder())->connected(info);
}


NAN_METHOD(ServerConnection::_connect) {
	return ObjectWrap::Unwrap<ServerConnection>(info.Holder())->connect(info);
}
NAN_METHOD(ServerConnection::connected) {
	bool result = this->protocol_handler && this->protocol_handler->connection_state >= connection_state::CONNECTING && this->protocol_handler->connection_state < connection_state::DISCONNECTING;
	info.GetReturnValue().Set(result);
}


NAN_METHOD(ServerConnection::_disconnect) {
	return ObjectWrap::Unwrap<ServerConnection>(info.Holder())->disconnect(info);
}
NAN_METHOD(ServerConnection::disconnect) {
	if(info.Length() != 2) {
		Nan::ThrowError("Invalid argument count");
		return;
	}

	if(!info[1]->IsFunction() || !info[0]->IsString()) {
		Nan::ThrowError("Invalid argument");
		return;
	}
	this->callback_disconnect = make_unique<Nan::Callback>(info[1].As<v8::Function>());
	if(!this->socket) {
		this->call_disconnect_result(0); /* this->errors.register_error("not connected") */
		return;
	}

	if(this->protocol_handler) {
		this->protocol_handler->disconnect(*Nan::Utf8String(info[0]));
	}
}

NAN_METHOD(ServerConnection::_error_message) {
		return ObjectWrap::Unwrap<ServerConnection>(info.Holder())->error_message(info);
}
NAN_METHOD(ServerConnection::error_message) {
	if(info.Length() != 1 || !info[0]->IsNumber()) {
		Nan::ThrowError("Invalid argument");
		return;
	}

	auto error = this->errors.get_error_message(
            (ErrorHandler::ErrorId) info[0]->IntegerValue(Nan::GetCurrentContext()).FromMaybe(0)
    );
	info.GetReturnValue().Set(Nan::New<v8::String>(error).ToLocalChecked());
}

//send_command(command: string, arguments: any[], switches: string[]);
template <typename T>
std::string _to_string(const T value) {
	std::string result(15, '\0');
	auto written = std::snprintf(&result[0], result.size(), "%.6f", value);
	if(written < 0) {
		log_warn(general, "Failed to format float value: {}; {}", value, written);
		return "0";
	}
	result.resize(written);
	return result;
}

NAN_METHOD(ServerConnection::_send_command) {
	return ObjectWrap::Unwrap<ServerConnection>(info.Holder())->send_command(info);
}

struct TS3VersionSettings {
	std::string build;
	std::string platform;
	std::string sign;
};
NAN_METHOD(ServerConnection::send_command) {
	if(!this->protocol_handler) {
		Nan::ThrowError("ServerConnection not initialized");
		return;
	}

	if(info.Length() != 3) {
		Nan::ThrowError("invalid argument count");
		return;
	}

	if(!info[0]->IsString() || !info[1]->IsArray() || !info[2]->IsArray()) {
		Nan::ThrowError("invalid argument type");
		return;
	}

	auto begin = chrono::system_clock::now();
	auto command = info[0]->ToString(Nan::GetCurrentContext()).ToLocalChecked();
	auto arguments = info[1].As<v8::Array>();
	auto switches = info[2].As<v8::Array>();

	ts::Command cmd(*Nan::Utf8String(command));
	for(size_t index = 0; index < arguments->Length(); index++) {
		auto object = Nan::GetLocal<v8::Object>(arguments, (uint32_t) index);
		if(object.IsEmpty() || !object->IsObject()) {
			Nan::ThrowError(Nan::New<v8::String>("invalid parameter (" + to_string(index) + ")").ToLocalChecked());
			return;
		}

		v8::Local<v8::Array> properties = object->ToObject(Nan::GetCurrentContext()).ToLocalChecked()->GetOwnPropertyNames(Nan::GetCurrentContext()).ToLocalChecked();
		for(uint32_t i = 0; i < properties->Length(); i++) {
			auto key = Nan::GetStringLocal(properties, i);
			auto value = object->ToObject(Nan::GetCurrentContext()).ToLocalChecked()->Get(Nan::GetCurrentContext(), key).ToLocalChecked();

			string key_string = *Nan::Utf8String(key);
			if(value->IsInt32())
				cmd[index][key_string] = value->Int32Value(Nan::GetCurrentContext()).FromMaybe(0);
			else if(value->IsNumber() || value->IsNumberObject())
				cmd[index][key_string] = _to_string<double>(value->NumberValue(Nan::GetCurrentContext()).FromMaybe(0)); /* requires our own conversation because node overrides stuff to 0,0000*/
			else if(value->IsString())
				cmd[index][key_string] = *Nan::Utf8String(value->ToString(Nan::GetCurrentContext()).ToLocalChecked());
			else if(value->IsBoolean() || value->IsBooleanObject())
				cmd[index][key_string] = value->BooleanValue(info.GetIsolate());
			else if(value->IsNullOrUndefined())
				cmd[index][key_string] = "";
			else {
				Nan::ThrowError(Nan::New<v8::String>("invalid parameter (" + to_string(index) + ":" + key_string + ")").ToLocalChecked());
				return;
			}
		}
	}


	for(size_t index = 0; index < switches->Length(); index++) {
		auto object = Nan::GetStringLocal(switches, (uint32_t) index);
		if(object.IsEmpty()) {
			Nan::ThrowError(Nan::New<v8::String>("invalid switch (" + to_string(index) + ")").ToLocalChecked());
			return;
		}

		cmd.enableParm(*Nan::Utf8String(object));
	}

	if(this->protocol_handler->server_type == server_type::TEAMSPEAK) {
		if(cmd.command() == "clientinit") {
			/* If we have a return code here some strange stuff happens (Ghost client) */
			if(cmd[0].has("return_code"))
				cmd["return_code"] = nullptr;

			TS3VersionSettings ts_version{};
#ifdef WIN32
			/*
			ts_version = {
				"0.0.1 [Build: 1549713549]",
				"Linux",
				"7XvKmrk7uid2ixHFeERGqcC8vupeQqDypLtw2lY9slDNPojEv//F47UaDLG+TmVk4r6S0TseIKefzBpiRtLDAQ=="
			};
			*/

			ts_version = {
					"3.?.? [Build: 5680278000]",
					"Windows",
					"DX5NIYLvfJEUjuIbCidnoeozxIDRRkpq3I9vVMBmE9L2qnekOoBzSenkzsg2lC9CMv8K5hkEzhr2TYUYSwUXCg=="
			};
#else
			/*
			ts_version = {
				"0.0.1 [Build: 1549713549]",
				"Linux",
				"7XvKmrk7uid2ixHFeERGqcC8vupeQqDypLtw2lY9slDNPojEv//F47UaDLG+TmVk4r6S0TseIKefzBpiRtLDAQ=="
			};
			 */
			ts_version = {
					"3.?.? [Build: 5680278000]",
					"Linux",
					"Hjd+N58Gv3ENhoKmGYy2bNRBsNNgm5kpiaQWxOj5HN2DXttG6REjymSwJtpJ8muC2gSwRuZi0R+8Laan5ts5CQ=="
			};
#endif
			if(std::getenv("teaclient_ts3_build") && std::getenv("teaclient_ts3_platform") && std::getenv("teaclient_ts3_sign")) {
				ts_version = {
						std::getenv("teaclient_ts3_build"),
						std::getenv("teaclient_ts3_platform"),
						std::getenv("teaclient_ts3_sign")
				};
			}

			cmd["client_version"] = ts_version.build;
			cmd["client_platform"] = ts_version.platform;
			cmd["client_version_sign"] = ts_version.sign;
			cmd[strobf("hwid").string()] = system_uuid(); /* we dont want anybody to patch this out */
		}
	}

	this->protocol_handler->send_command(cmd, false);
	auto end = chrono::system_clock::now();
}
NAN_METHOD(ServerConnection::_send_voice_data) {
	return ObjectWrap::Unwrap<ServerConnection>(info.Holder())->send_voice_data(info);
}

NAN_METHOD(ServerConnection::send_voice_data) {
	if(!this->protocol_handler) {
		Nan::ThrowError("ServerConnection not initialized");
		return;
	}

	if(info.Length() != 3) {
		Nan::ThrowError("invalid argument count");
		return;
	}

	if(!info[0]->IsUint8Array() || !info[1]->IsInt32() || !info[2]->IsBoolean()) {
		Nan::ThrowError("invalid argument type");
		return;
	}


	auto voice_data = info[0].As<v8::Uint8Array>()->Buffer();
	this->send_voice_data(voice_data->GetContents().Data(), voice_data->GetContents().ByteLength(), (uint8_t) info[1]->Int32Value(Nan::GetCurrentContext()).FromMaybe(0), info[2]->BooleanValue(info.GetIsolate()));
}


NAN_METHOD(ServerConnection::_send_voice_data_raw) {
	return ObjectWrap::Unwrap<ServerConnection>(info.Holder())->send_voice_data_raw(info);
}

NAN_METHOD(ServerConnection::send_voice_data_raw) {
	//send_voice_data_raw(buffer: Float32Array, channels: number, sample_rate: number, header: boolean);

	if(info.Length() != 4) {
		Nan::ThrowError("invalid argument count");
		return;
	}

	if(!info[0]->IsFloat32Array() || !info[1]->IsInt32() || !info[2]->IsInt32()) {
		Nan::ThrowError("invalid argument type");
		return;
	}

	auto channels = info[1]->Int32Value(Nan::GetCurrentContext()).FromMaybe(0);
	auto sample_rate = info[2]->Int32Value(Nan::GetCurrentContext()).FromMaybe(0);
	auto flag_head = info[2]->BooleanValue(info.GetIsolate());

	auto voice_data = info[0].As<v8::Float32Array>()->Buffer();
	auto vs = this->voice_connection ? this->voice_connection->voice_sender() : nullptr;
	if(vs) {
        vs->send_data((float*) voice_data->GetContents().Data(), voice_data->GetContents().ByteLength() / (4 * channels), sample_rate, channels);
	}
}

#ifdef SHUFFLE_VOICE
static shared_ptr<ts::protocol::ClientPacket> shuffle_cached_packet;
#endif
void ServerConnection::send_voice_data(const void *buffer, size_t buffer_length, uint8_t codec, bool head) {
    auto packet = ts::protocol::allocate_outgoing_client_packet(buffer_length + 3);
    packet->type_and_flags_ = ts::protocol::PacketType::VOICE;

    auto packet_payload = (uint8_t*) packet->payload;
    *(uint16_t*) packet_payload = htons(this->voice_packet_id++);
    packet_payload[2] = (uint8_t) codec;
    if(buffer_length > 0 && buffer) {
        memcpy(&packet_payload[3], buffer, buffer_length);
    }

    if(head) {
        packet->type_and_flags_ |= ts::protocol::PacketFlag::Compressed;
    }
    packet->type_and_flags_ |= ts::protocol::PacketFlag::Unencrypted;

#ifdef FUZZ_VOICE
	if((rand() % 10) < 2) {
		log_info(category::connection, tr("Dropping voice packet"));
	} else {
		this->protocol_handler->send_packet(packet);
	}
#elif defined(SHUFFLE_VOICE)
	if(shuffle_cached_packet) {
		this->protocol_handler->send_packet(packet);
		this->protocol_handler->send_packet(std::exchange(shuffle_cached_packet, nullptr));
	} else {
		shuffle_cached_packet = packet;
	}
#else
    this->protocol_handler->send_packet(packet, false);
#endif
}

void ServerConnection::close_connection() {
	lock_guard lock{this->disconnect_lock};
	if(this->socket && this_thread::get_id() == this->socket->io_thread().get_id()) {
		logger::debug(category::connection, tr("close_connection() called in IO thread. Closing connection within event loop!"));
		if(!this->event_loop_execute_connection_close) {
			this->event_loop_execute_connection_close = true;
			this->event_condition.notify_one();
		}
		return;
	}

	this->event_loop_execute_connection_close = false;
	if(this->socket) {
	    this->protocol_handler->do_close_connection();
	}
	if(this->protocol_handler) {
        this->protocol_handler->do_close_connection();
	}
	this->socket = nullptr;

	this->call_disconnect_result.call(0, true);
}

void ServerConnection::execute_tick() {
	if(this->protocol_handler) {
        this->protocol_handler->execute_tick();
	}

	if(auto vc{this->voice_connection}; vc) {
        vc->execute_tick();
	}
}

void ServerConnection::_execute_callback_commands() {
	unique_ptr<ts::Command> next_command;
	v8::Local<v8::Function> callback;

	while(true) {
		{
			lock_guard lock(this->pending_commands_lock);
			if(this->pending_commands.empty())
				return;
			next_command = move(this->pending_commands.front());
			this->pending_commands.pop_front();
		}
		if(!next_command)
			continue;

		if(callback.IsEmpty()) {
			callback = Nan::Get(this->handle(), Nan::New<v8::String>("callback_command").ToLocalChecked()).ToLocalChecked().As<v8::Function>();
			if(callback.IsEmpty()) {
				logger::warn(category::connection, tr("Missing command callback! Dropping commands."));
				lock_guard lock(this->pending_commands_lock);
				this->pending_commands.clear();
				return;
			}
		}

		v8::Local<v8::Value> arguments[3];
		arguments[0] = Nan::New<v8::String>(next_command->command()).ToLocalChecked();

		auto parameters = Nan::New<v8::Array>((int) next_command->bulkCount());
		for(size_t index = 0; index < next_command->bulkCount(); index++) {
			auto object = Nan::New<v8::Object>();
			auto& bulk = next_command->operator[](index);

			for(const auto& key : bulk.keys())
				Nan::Set(object, Nan::New<v8::String>(key).ToLocalChecked(), Nan::New<v8::String>(bulk[key].string()).ToLocalChecked());

			Nan::Set(parameters, (uint32_t) index, object);
		}
		arguments[1] = parameters;


		auto switched = Nan::New<v8::Array>((int) next_command->parms().size());
		for(size_t index = 0; index < next_command->parms().size(); index++) {
			auto& key = next_command->parms()[index];
			Nan::Set(parameters, (uint32_t) index, Nan::New<v8::String>(key).ToLocalChecked());
		}
		arguments[2] = switched;

		callback->Call(Nan::GetCurrentContext(), Nan::Undefined(), 3, arguments);
	}
}

void ServerConnection::_execute_callback_voice() {
	unique_ptr<VoicePacket> next_packet;
	v8::Local<v8::Function> callback;

	while(true) {
		{
			lock_guard lock(this->pending_voice_lock);
			if(this->pending_voice.empty())
				return;
			next_packet = move(this->pending_voice.front());
			this->pending_voice.pop_front();
		}
		if(!next_packet)
			continue;

		if(callback.IsEmpty()) {
			auto _callback =  Nan::Get(this->handle(), Nan::New<v8::String>("callback_voice_data").ToLocalChecked()).ToLocalChecked();
			if(_callback.IsEmpty() || _callback->IsUndefined()) {
				logger::warn(category::audio, tr("Missing voice callback! Dropping packets!"));
				lock_guard lock(this->pending_voice_lock);
				this->pending_voice.clear();
				return;
			}
			callback = _callback.As<v8::Function>();
		}

		v8::Local<v8::Value> arguments[5];

		v8::Local<v8::ArrayBuffer> buffer = v8::ArrayBuffer::New(
				Nan::GetCurrentContext()->GetIsolate(),
				next_packet->voice_data.length()
		);
		memcpy(buffer->GetContents().Data(), next_packet->voice_data.data_ptr(), next_packet->voice_data.length());
		arguments[0] = v8::Uint8Array::New(buffer, 0, buffer->ByteLength());
		arguments[1] = Nan::New<v8::Integer>(next_packet->client_id);
		arguments[2] = Nan::New<v8::Integer>(next_packet->codec_id);
		arguments[3] = Nan::New<v8::Boolean>(next_packet->flag_head);
		arguments[4] = Nan::New<v8::Integer>(next_packet->packet_id);
		callback->Call(Nan::GetCurrentContext(), Nan::Undefined(), 5, arguments);
	}
}

void ServerConnection::_execute_callback_disconnect(const std::string &reason) {
	auto callback = Nan::Get(this->handle(), Nan::New<v8::String>("callback_disconnect").ToLocalChecked()).ToLocalChecked().As<v8::Function>();
	if(callback.IsEmpty()) {
		cout << "Missing disconnect callback!" << endl;
		return;
	}

	v8::Local<v8::Value> arguments[1];
	arguments[0] = Nan::New<v8::String>(reason).ToLocalChecked();
	callback->Call(Nan::GetCurrentContext(), Nan::Undefined(), 1, arguments);

}

NAN_METHOD(ServerConnection::_current_ping) {
	auto connection = ObjectWrap::Unwrap<ServerConnection>(info.Holder());
    lock_guard lock{connection->disconnect_lock};

	auto& phandler = connection->protocol_handler;
	if(phandler)
		info.GetReturnValue().Set((uint32_t) chrono::floor<microseconds>(phandler->current_ping()).count());
	else
		info.GetReturnValue().Set(-1);
}

NAN_METHOD(ServerConnection::statistics) {
    auto connection = ObjectWrap::Unwrap<ServerConnection>(info.Holder());
    lock_guard lock{connection->disconnect_lock};

    auto& phandler = connection->protocol_handler;
    if(phandler) {
        auto statistics = phandler->statistics();

        auto result = Nan::New<v8::Object>();
        Nan::Set(result, Nan::LocalString("voice_bytes_received"), Nan::New<v8::Number>(statistics.voice_bytes_received));
        Nan::Set(result, Nan::LocalString("voice_bytes_send"), Nan::New<v8::Number>(statistics.voice_bytes_send));
        Nan::Set(result, Nan::LocalString("control_bytes_received"), Nan::New<v8::Number>(statistics.control_bytes_received));
        Nan::Set(result, Nan::LocalString("control_bytes_send"), Nan::New<v8::Number>(statistics.control_bytes_send));
        info.GetReturnValue().Set(result);
    } else {
        info.GetReturnValue().Set(Nan::Undefined());
    }
}