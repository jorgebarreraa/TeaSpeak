#pragma once

#ifdef COMPILE_WEB_CLIENT
#include <pipes/ssl.h>
#include <pipes/ws.h>
#include <src/client/SpeakingClient.h>
#include <src/client/ConnectedClient.h>
#include <protocol/buffers.h>
#include "misc/queue.h"
#include <opus/opus.h>
#include <json/json.h>
#include <EventLoop.h>
#include "../shared/WhisperHandler.h"
#include "../shared/ServerCommandExecutor.h"

namespace ts::server {
    class WebControlServer;
    class WebClientCommandHandler;

    class WebClient : public SpeakingClient {
            friend class WebControlServer;
            friend class WebClientCommandHandler;
        public:
            WebClient(WebControlServer*, int socketFd);
            ~WebClient() override;

            void sendJson(const Json::Value&);
            void sendCommand(const ts::Command &command, bool low) override;
            void sendCommand(const ts::command_builder &command, bool low) override;

            bool disconnect(const std::string &reason) override;
            bool close_connection(const std::chrono::system_clock::time_point& timeout = std::chrono::system_clock::time_point()) override;


            [[nodiscard]] inline std::chrono::nanoseconds client_ping() const { return this->client_ping_layer_7(); }
            [[nodiscard]] inline std::chrono::nanoseconds client_ping_layer_5() const { return this->ping.value; }
            [[nodiscard]] inline std::chrono::nanoseconds client_ping_layer_7() const { return this->js_ping.value; }

        protected:
            void tick_server(const std::chrono::system_clock::time_point&) override; /* Every 500ms */
        private:
            WebControlServer* handle;

            int file_descriptor;

            bool allow_raw_commands{false};
            bool ssl_detected{false};
            bool ssl_encrypted{true};
            pipes::SSL ssl_handler;
            pipes::WebSocket ws_handler;

            std::mutex event_mutex;
            ::event* readEvent;
            ::event* writeEvent;

            struct {
                uint8_t current_id{0};
                std::chrono::system_clock::time_point last_request;
                std::chrono::system_clock::time_point last_response;

                std::chrono::nanoseconds value{};
                std::chrono::nanoseconds timeout{2000};
            } ping;

            struct {
                uint8_t current_id{0};
                std::chrono::system_clock::time_point last_request;
                std::chrono::system_clock::time_point last_response;

                std::chrono::nanoseconds value{};
                std::chrono::nanoseconds timeout{2000};
            } js_ping;

            std::mutex queue_mutex;
            std::unique_ptr<ServerCommandQueue> command_queue{};
            std::deque<pipes::buffer> queue_write;
            threads::Mutex execute_mutex; /* needs to be recursive! */

            std::thread flush_thread;
            std::recursive_mutex close_lock;

            whisper::WhisperHandler whisper_handler_;
        private:
            void initialize();

            static void handleMessageRead(int, short, void*);
            static void handleMessageWrite(int, short, void*);
            void enqueue_raw_packet(const pipes::buffer_view& /* buffer */);

            /* TODO: Put the message processing part into the IO loop and not into command processing! */
            bool process_next_message(const std::string_view& buffer);

            //WS events
            void onWSConnected();
            void onWSDisconnected(const std::string& reason);
            void onWSMessage(const pipes::WSMessage&);
        protected:
            void disconnectFinal();
            void handleMessage(const pipes::buffer_view&);

        public:
            void send_voice_packet(const pipes::buffer_view &view, const VoicePacketFlags &flags) override;

        protected:

            command_result handleCommand(Command &command) override;
            command_result handleCommandClientInit(Command &command) override;

            command_result handleCommandWhisperSessionInitialize(Command &command);
            command_result handleCommandWhisperSessionReset(Command &command);
    };

    class WebClientCommandHandler : public ts::server::ServerCommandHandler {
        public:
            explicit WebClientCommandHandler(const std::shared_ptr<WebClient>& /* client */);

        protected:
            bool handle_command(const std::string_view &) override;

        private:
            std::weak_ptr<WebClient> client_ref;
    };
}
#endif