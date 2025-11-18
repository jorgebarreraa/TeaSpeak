#pragma once

#include <memory>
#include <optional>
#include <string>

namespace ts::server {
    class SpeakingClient;
}

struct VideoBroadcastOptions;

namespace ts::rtc {
    typedef uint32_t RTCClientId;
    typedef uint32_t RTCChannelId;
    typedef uint32_t RTCStreamId;

    extern std::string_view version();
    extern bool initialize(std::string& /* error */);

    enum struct ChannelAssignResult {
        Success,
        ClientUnknown,
        TargetChannelUnknown,
        UnknownError
    };

    enum struct BroadcastStartResult {
        Success,
        InvalidClient,
        ClientHasNoChannel,
        InvalidBroadcastType,
        InvalidStreamId,
        UnknownError
    };

    enum struct VideoBroadcastType {
        Camera = 0,
        Screen = 1
    };

    enum struct VideoBroadcastJoinResult {
        Success,
        InvalidClient,
        InvalidBroadcast,
        InvalidBroadcastType,
        UnknownError
    };

    enum struct VideoBroadcastConfigureResult {
        Success,
        InvalidClient,
        InvalidBroadcast, /* Client might not be broadcasting */
        InvalidBroadcastType,
        UnknownError
    };

    class NativeAudioSourceSupplier;
    class Server {
        public:
            Server();
            ~Server();

            RTCClientId create_client(const std::shared_ptr<server::SpeakingClient>& /* client */);
            bool initialize_rtc_connection(std::string& /* error */, RTCClientId /* client id */);
            bool initialize_native_connection(std::string& /* error */, RTCClientId /* client id */);
            void destroy_client(RTCClientId /* client id */);

            bool client_video_stream_count(uint32_t /* client id */, uint32_t* /* camera count */, uint32_t* /* screen count */);

            /* RTC client actions */
            void reset_rtp_session(RTCClientId /* client */);
            bool apply_remote_description(std::string& /* error */, RTCClientId /* client id */, uint32_t /* mode */, const std::string& /* description */);
            bool generate_local_description(RTCClientId /* client id */, std::string& /* result */);
            bool add_ice_candidate(std::string& /* error */, RTCClientId /* client id */, uint32_t /* media line */, const std::string& /* description */);
            void ice_candidates_finished(RTCClientId /* client id */);

            /* Native client actions */
            std::optional<NativeAudioSourceSupplier> create_audio_source_supplier_sender(RTCClientId /* client id */, uint32_t /* stream id */);

            /* channel actions */
            uint32_t create_channel();
            ChannelAssignResult assign_channel(RTCClientId /* client id */, RTCChannelId /* channel id */);
            void destroy_channel(RTCChannelId /* channel id */);

            /* Audio */
            BroadcastStartResult start_broadcast_audio(RTCClientId /* client id */, RTCStreamId /* stream id */);

            bool configure_whisper_session(std::string& /* error */, RTCClientId /* client id */, uint32_t /* source stream id */, RTCClientId* /* session members */, size_t /* session member count */);
            void reset_whisper_session(RTCClientId /* client id */);

            /* Video */
            BroadcastStartResult start_broadcast_video(RTCClientId /* client id */, VideoBroadcastType /* broadcast type */, RTCStreamId /* stream id */, const VideoBroadcastOptions* /* options */);
            VideoBroadcastConfigureResult client_broadcast_video_configure(RTCClientId /* client id */, VideoBroadcastType /* broadcast type */, const VideoBroadcastOptions* /* options */);
            VideoBroadcastConfigureResult client_broadcast_video_config(RTCClientId /* client id */, VideoBroadcastType /* broadcast type */, VideoBroadcastOptions* /* options */);

            VideoBroadcastJoinResult join_video_broadcast(RTCClientId /* client id */, RTCClientId /* target id */, VideoBroadcastType /* broadcast type */);
            void leave_video_broadcast(RTCClientId /* client id */, RTCClientId /* target id */, VideoBroadcastType /* broadcast type */);
        private:
            void* server_ptr{nullptr};
    };

    class NativeAudioSourceSupplier {
        public:
            explicit NativeAudioSourceSupplier() : sender_ptr{nullptr} {}
            explicit NativeAudioSourceSupplier(void*);
            virtual ~NativeAudioSourceSupplier() noexcept;

            NativeAudioSourceSupplier(const NativeAudioSourceSupplier&) = delete;
            NativeAudioSourceSupplier(NativeAudioSourceSupplier&& other) noexcept;

            inline void reset(NativeAudioSourceSupplier& other) {
                std::swap(other.sender_ptr, this->sender_ptr);
            }
            void send_audio(uint16_t /* seq no */, bool /* marked */, uint32_t /* timestamp */, uint8_t /* codec */, const std::string_view& /* data */);
        private:
            void* sender_ptr;
    };
}