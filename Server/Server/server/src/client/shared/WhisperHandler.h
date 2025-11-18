#pragma once

#include <string_view>
#include <chrono>
#include <mutex>

#include <Error.h>
#include <protocol/Packet.h>

namespace ts::connection {
    class VoiceClientConnection;
}

namespace ts::server {
    class VoiceClient;
    class SpeakingClient;
}

namespace ts::server::whisper {
    enum struct WhisperType {
        SERVER_GROUP = 0,
        CHANNEL_GROUP = 1,
        CHANNEL_COMMANDER = 2,
        ALL = 3,

        ECHO = 0x10,
    };

    enum struct WhisperTarget {
        CHANNEL_ALL = 0,
        CHANNEL_CURRENT = 1,
        CHANNEL_PARENT = 2,
        CHANNEL_ALL_PARENT = 3,
        CHANNEL_FAMILY = 4,
        CHANNEL_COMPLETE_FAMILY = 5,
        CHANNEL_SUBCHANNELS = 6
    };

    class WhisperHandler {
        public:
            explicit WhisperHandler(SpeakingClient* /* handle */);
            ~WhisperHandler();

            /*
             * Preprocess a whisper packet.
             * If the result is false the packet should not be send into the rtc source pipe.
             */
            bool process_packet(const protocol::PacketParser& /* packet */, void*& /* payload ptr */, size_t& /* payload length */);

            ts::command_result initialize_session_new(uint32_t /* stream id */, uint8_t /* type */, uint8_t /* target */, uint64_t /* type id */);
            ts::command_result initialize_session_old(uint32_t /* stream id */, const uint16_t* /* client ids */, size_t /* client count */, const uint64_t* /* channel ids */, size_t /* channel count */);

            void signal_session_reset();
            void handle_session_reset();

        private:
            enum struct SessionState {
                Uninitialized,
                InitializeFailed,
                Initialized
            };
            SpeakingClient* handle;

            SessionState session_state{SessionState::Uninitialized};
            std::chrono::system_clock::time_point session_timestamp{};

            std::mutex whisper_head_mutex{};
            void* whisper_head_ptr{nullptr};
            size_t whisper_head_length{0};
            size_t whisper_head_capacity{0};

            /**
             * This also updates the last head ptr.
             * @return True if the packet is a valid whisper packet. The payload ptr and payload length variables will be set
             */
            [[nodiscard]] bool validate_whisper_packet(const protocol::PacketParser& /* packet */, bool& /* matches last header */, void*& /* payload ptr */, size_t& /* payload length */);
            [[nodiscard]] ts::command_result configure_rtc_clients(uint32_t /* stream id */, const std::vector<std::shared_ptr<SpeakingClient>>& /* clients */);

            [[nodiscard]] size_t max_whisper_targets();
    };
}