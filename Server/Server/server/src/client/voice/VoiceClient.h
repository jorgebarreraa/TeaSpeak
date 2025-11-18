#pragma once

#include <cstdint>
#include <ThreadPool/Thread.h>
#include <ThreadPool/Mutex.h>
#include <protocol/buffers.h>
#include <netinet/in.h>
#include <deque>
#include <cstdint>
#include <src/server/VoiceServer.h>
#include <EventLoop.h>
#include "../SpeakingClient.h"
#include "../ConnectedClient.h"
#include "protocol/CryptHandler.h"
#include "VoiceClientConnection.h"
#include "src/server/PrecomputedPuzzles.h"
#include "../../lincense/TeamSpeakLicense.h"

//#define LOG_INCOMPING_PACKET_FRAGMENTS
//#define LOG_AUTO_ACK_AUTORESPONSE
//#define LOG_AUTO_ACK_REQUEST
//#define LOG_AUTO_ACK_RESPONSE

#define PKT_LOG_CMD
//#define PKT_LOG_VOICE
//#define PKT_LOG_WHISPER
//#define PKT_LOG_PING

//For CLion
#ifndef CLIENT_LOG_PREFIX
    #define CLIENT_LOG_PREFIX "Undefined CLIENT_LOG_PREFIX"
#endif

namespace ts {
    namespace connection {
        class VoiceClientConnection;
    }
    namespace server {
        namespace server::udp {
            class CryptSetupHandler;
        }

        class VirtualServer;
        class VoiceClientCommandHandler;

        class VoiceClient : public SpeakingClient {
                friend class VirtualServer;
                friend class VoiceServer;
                friend class POWHandler;
                friend class ts::connection::VoiceClientConnection;
                friend class ConnectedClient;
                friend class server::udp::CryptSetupHandler;
                friend class VoiceClientCommandHandler;
            public:
                VoiceClient(const std::shared_ptr<VoiceServer>& server, const sockaddr_storage*);
                ~VoiceClient() override;

                bool close_connection(const std::chrono::system_clock::time_point &timeout) override;
                bool disconnect(const std::string&) override;

                [[nodiscard]] inline const auto& get_remote_address() const { return this->remote_address; }
                /*
                 * TODO: Use a helper class called InvokerDescription containing the invoker properties and not holding a whole connected client reference
                 * 2. May use some kind of class to easily set the disconnect reason?
                 */
                bool disconnect(ViewReasonId /* reason type */, const std::string& /* reason */, const std::shared_ptr<ts::server::ConnectedClient>& /* invoker */, bool /* notify viewer */);

                void sendCommand(const ts::Command &command, bool low = false) override { return this->sendCommand0(command.build(), low, nullptr); }
                void sendCommand(const ts::command_builder &command, bool low) override { return this->sendCommand0(command.build(), low, nullptr); }

                /* Note: Order is only guaranteed if progressDirectly is on! */
                virtual void sendCommand0(const std::string_view& /* data */, bool low, std::unique_ptr<std::function<void(bool)>> listener);

                connection::VoiceClientConnection* getConnection(){ return connection; }

                [[nodiscard]] std::chrono::milliseconds current_ping();
                [[nodiscard]] float current_ping_deviation();

                [[nodiscard]] float current_packet_loss() const;

                [[nodiscard]] const auto& server_command_queue() { return this->server_command_queue_; }

                void send_video_unsupported_message();
                void clear_video_unsupported_message_flag();
            private:
                connection::VoiceClientConnection* connection;

            protected:
                /* Might be null when the server has been deleted but the client hasn't yet fully disconnected */
                std::shared_ptr<VoiceServer> voice_server;

                void initialize();
                virtual void tick_server(const std::chrono::system_clock::time_point &time) override;

                /* Attention these handle callbacks are not thread save! */
                void handlePacketCommand(const std::string_view&);
            public:
                void send_voice_packet(const pipes::buffer_view &packet, const VoicePacketFlags &flags) override;
                void send_voice(const std::shared_ptr<SpeakingClient>& /* source client */, uint16_t /* seq no */, uint8_t /* codec */, const void* /* payload */, size_t /* payload length */);
                void send_voice_whisper(const std::shared_ptr<SpeakingClient>& /* source client */, uint16_t /* seq no */, uint8_t /* codec */, const void* /* payload */, size_t /* payload length */);

                void processJoin() override;
            protected:
                virtual command_result handleCommand(Command &command) override;

            private:
                /*
                 * Use to schedule a network write.
                 * If we don't have a proper weak ref we have,
                 * every time we want to schedule a write,
                 * to lock the weak_ptr and dynamic ptr cast it
                 */
                std::weak_ptr<VoiceClient> ref_self_voice{};

                rtc::NativeAudioSourceSupplier rtc_audio_supplier{};
                rtc::NativeAudioSourceSupplier rtc_audio_whisper_supplier{};

                uint16_t stop_seq_counter{0};
                uint16_t whisper_head_counter{0};

                std::mutex flush_mutex{};
                task_id flush_task{0};
                bool flush_executed{false};
                std::chrono::system_clock::time_point flush_timeout{};
                std::optional<bool> disconnect_acknowledged{}; /* locked by flush_mutex */

                std::unique_ptr<ServerCommandQueue> server_command_queue_{};

                bool video_unsupported_message_send{false};
                /* This method should only be called from the close connection method!  */
                void finalDisconnect();

                /* Used by close_connection to determine if we've successfully flushed the connection */
                [[nodiscard]] bool connection_flushed();

                command_result handleCommandClientInit(Command&) override;
                command_result handleCommandClientDisconnect(Command&);
        };

        class VoiceClientCommandHandler : public ts::server::ServerCommandHandler {
            public:
                explicit VoiceClientCommandHandler(const std::shared_ptr<VoiceClient>& /* client */);

            protected:
                bool handle_command(const std::string_view &) override;

            private:
                std::weak_ptr<VoiceClient> client_ref;
        };
    }
}