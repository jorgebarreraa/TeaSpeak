#pragma once

#include "ConnectedClient.h"
#include "./shared/WhisperHandler.h"
#include <json/json.h>
#include <src/rtc/lib.h>

namespace ts::server {
    class VirtualServer;
    class SpeakingClient : public ConnectedClient {
        public:
            struct VoicePacketFlags {
                bool encrypted      : 1;
                bool head           : 1;
                bool fragmented     : 1; /* used by MONO. IDK What this is */
                bool new_protocol   : 1;
                char _unused        : 4;

                VoicePacketFlags() : encrypted{false}, head{false}, fragmented{false}, new_protocol{false}, _unused{0} { }
            };
            static_assert(sizeof(VoicePacketFlags) == 1);

            enum HandshakeState {
                BEGIN,
                IDENTITY_PROOF,
                SUCCEEDED
            };
            enum IdentityType : uint8_t {
                TEASPEAK_FORUM,
                TEAMSPEAK,
                NICKNAME,

                UNSET = 0xff
            };

            SpeakingClient(sql::SqlManager* a, const std::shared_ptr<VirtualServer>& b);
            ~SpeakingClient() override;

            /* TODO: Remove this method. Currently only the music but uses that to broadcast his audio */
            virtual void send_voice_packet(const pipes::buffer_view& /* voice packet data */, const VoicePacketFlags& /* flags */) = 0;
            bool should_handle_voice_packet(size_t /* size */);

            virtual bool shouldReceiveVoice(const std::shared_ptr<ConnectedClient> &sender);
            bool shouldReceiveVoiceWhisper(const std::shared_ptr<ConnectedClient> &sender);

            inline std::chrono::milliseconds takeSpokenTime() {
                auto time = this->speak_time;
                this->speak_time = std::chrono::milliseconds(0);
                return time;
            }

            [[nodiscard]] inline whisper::WhisperHandler& whisper_handler() { return this->whisper_handler_; }
        protected:
            void tick_server(const std::chrono::system_clock::time_point &time) override;

        public:
            void updateChannelClientProperties(bool channel_lock, bool notify) override;

        protected:
            command_result handleCommand(Command &command) override;

        public:
            virtual void processJoin();
            void processLeave();

            virtual command_result handleCommandHandshakeBegin(Command&);
            virtual command_result handleCommandHandshakeIdentityProof(Command &);
            virtual command_result handleCommandClientInit(Command&);

            virtual command_result handleCommandRtcSessionDescribe(Command &command);
            virtual command_result handleCommandRtcSessionReset(Command &command);
            virtual command_result handleCommandRtcIceCandidate(Command &);
            virtual command_result handleCommandBroadcastAudio(Command &);
            virtual command_result handleCommandBroadcastVideo(Command &);
            virtual command_result handleCommandBroadcastVideoJoin(Command &);
            virtual command_result handleCommandBroadcastVideoLeave(Command &);
            virtual command_result handleCommandBroadcastVideoConfig(Command &);
            virtual command_result handleCommandBroadcastVideoConfigure(Command &);

            /* clientinit method helpers */
            command_result applyClientInitParameters(Command&);
            command_result resolveClientInitBan();

            void triggerVoiceEnd();
            void updateSpeak(bool onlyUpdate, const std::chrono::system_clock::time_point &time);
            std::chrono::milliseconds speak_accuracy{1000};

            std::mutex speak_mutex;
            std::chrono::milliseconds speak_time{0};
            std::chrono::system_clock::time_point speak_begin;
            std::chrono::system_clock::time_point speak_last_packet;

            std::chrono::system_clock::time_point speak_last_no_whisper_target;
            std::chrono::system_clock::time_point speak_last_too_many_whisper_targets;

            permission::v2::PermissionFlaggedValue max_idle_time{permission::v2::empty_permission_flagged_value};
            struct {
                HandshakeState state{HandshakeState::BEGIN};

                IdentityType identityType{IdentityType::UNSET};
                std::string proof_message;
                //TeamSpeak
                std::shared_ptr<ecc_key> identityKey;
                //TeaSpeak
                std::shared_ptr<Json::Value> identityData;
            } handshake;

            whisper::WhisperHandler whisper_handler_;

            bool rtc_session_pending_describe{false};
            rtc::RTCClientId rtc_client_id{0};
    };
}