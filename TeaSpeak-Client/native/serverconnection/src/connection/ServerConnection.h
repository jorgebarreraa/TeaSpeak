#pragma once

#include <array>
#include <string>
#include <thread>
#include <nan.h>
#include <include/NanEventCallback.h>
#include <condition_variable>
#include <pipes/buffer.h>

namespace ts {
	class Command;
}

namespace tc {
	namespace connection {
		namespace server_type {
			enum value : uint8_t {
				UNKNOWN,
				TEASPEAK,
				TEAMSPEAK
			};
		}

		class UDPSocket;
		class ProtocolHandler;
		class VoiceConnection;

		class ErrorHandler {
			public:
				typedef int16_t ErrorId;
				static constexpr ErrorId kErrorCodeSuccess = 0;
				static constexpr ErrorId kErrorBacklog = 5;

				[[nodiscard]] std::string get_error_message(ErrorId) const;
                [[nodiscard]] ErrorId register_error(const std::string& /* message */);

		    private:
                std::array<std::string, kErrorBacklog> error_messages{};
                ErrorId error_index{0};
		};

		class ServerConnection : public Nan::ObjectWrap {
				friend class ProtocolHandler;
			public:
				static NAN_MODULE_INIT(Init);

				static NAN_METHOD(new_instance);
				static inline Nan::Persistent<v8::Function> & constructor() {
					static Nan::Persistent<v8::Function> my_constructor;
					return my_constructor;
				}
				ServerConnection();
				~ServerConnection() override;

				NAN_METHOD(connect);
				NAN_METHOD(connected);
				NAN_METHOD(disconnect);

				NAN_METHOD(error_message);
				NAN_METHOD(send_command);
				NAN_METHOD(send_voice_data);
				void send_voice_data(const void* /* buffer */, size_t /* buffer length */, uint8_t /* codec */, bool /* head */);

				NAN_METHOD(send_voice_data_raw);

				void initialize();
				void finalize();

				void close_connection(); /* directly closes connection without notify etc */
				std::shared_ptr<UDPSocket> get_socket() { return this->socket; }
			private:
				struct VoicePacket {
					pipes::buffer voice_data;
					uint16_t client_id;
					uint16_t packet_id;
					uint8_t codec_id;
					bool flag_head;
				};

				static NAN_METHOD(_connect);
				static NAN_METHOD(_connected);
				static NAN_METHOD(_disconnect);
				static NAN_METHOD(_send_command);
				static NAN_METHOD(_send_voice_data);
				static NAN_METHOD(_send_voice_data_raw);
				static NAN_METHOD(_error_message);
				static NAN_METHOD(_current_ping);
                static NAN_METHOD(statistics);

				std::unique_ptr<Nan::Callback> callback_connect;
				std::unique_ptr<Nan::Callback> callback_disconnect;
				Nan::callback_t<ErrorHandler::ErrorId> call_connect_result;
				Nan::callback_t<ErrorHandler::ErrorId> call_disconnect_result;
				Nan::callback_t<> execute_pending_commands;
				Nan::callback_t<> execute_pending_voice;
				Nan::callback_t<std::string> execute_callback_disconnect;

				ErrorHandler errors;
				std::shared_ptr<UDPSocket> socket;
				std::unique_ptr<ProtocolHandler> protocol_handler;
				std::recursive_timed_mutex disconnect_lock;

				std::thread event_thread;
				std::mutex event_lock;
				std::condition_variable event_condition;
				bool event_loop_exit = false; /* set to true if we want to exit */
				void event_loop();

				bool event_loop_execute_connection_close = false;

				std::chrono::system_clock::time_point next_tick;
				std::chrono::system_clock::time_point next_resend;
				void execute_tick();

				void schedule_resend(const std::chrono::system_clock::time_point& /* timestamp */);

				std::mutex pending_commands_lock;
				std::deque<std::unique_ptr<ts::Command>> pending_commands;
				void _execute_callback_commands();

				std::mutex pending_voice_lock;
				std::deque<std::unique_ptr<VoicePacket>> pending_voice;
				void _execute_callback_voice();
				uint16_t voice_packet_id = 0;

				void _execute_callback_disconnect(const std::string&);

				std::shared_ptr<VoiceConnection> voice_connection;
		};
	}
}