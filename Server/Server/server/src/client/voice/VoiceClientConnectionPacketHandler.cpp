#include <tommath.h>
#include <misc/endianness.h>
#include <log/LogUtils.h>
#include "../web/WebClient.h"
#include "VoiceClient.h"

using namespace std;
using namespace std::chrono;
using namespace ts::connection;
using namespace ts::protocol;

void VoiceClientConnection::handlePacketPong(const ts::protocol::PacketParser &packet) {
    if(packet.payload_length() < 2) {
        return;
    }

    this->ping_handler_.received_pong(be2le16((char*) packet.payload().data_ptr()));
}

void VoiceClientConnection::handlePacketPing(const protocol::PacketParser& packet) {
#ifdef PKT_LOG_PING
    logMessage(this->getServerId(), "{}[Ping] Sending pong for client requested ping {}", CLIENT_STR_LOG_PREFIX, packet->packetId());
#endif
    char buffer[2];
    le2be16(packet.packet_id(), buffer);
    this->send_packet(PacketType::PONG, (uint8_t) PacketFlag::Unencrypted, buffer, 2);
}

void VoiceClientConnection::handlePacketVoice(const protocol::PacketParser& packet) {
    auto client = this->getCurrentClient();
    if(!client) {
        return;
    }

    if(client->should_handle_voice_packet(packet.payload_length())) {
        auto& sink = client->rtc_audio_supplier;

        auto payload = packet.payload();
        uint16_t vpacketId = be2le16((char*) payload.data_ptr());
        auto codec = (uint8_t) payload[2];

        sink.send_audio(vpacketId, false, vpacketId * 960, codec, std::string_view{payload.data_ptr<char>() + 3, payload.length() - 3});
    }

    client->resetIdleTime();
    client->updateSpeak(false, std::chrono::system_clock::now());
}

void VoiceClientConnection::handlePacketVoiceWhisper(const ts::protocol::PacketParser &packet) {
    auto client = this->getCurrentClient();
    if(!client) return;

    void* payload;
    size_t payload_length;

    if(!client->whisper_handler().process_packet(packet, payload, payload_length)) {
        /* packet invalid or session failed to initialize */
        return;
    }

    auto voice_packet_id = ntohs(*packet.payload().data_ptr<uint16_t>());
    auto voice_codec = packet.payload().data_ptr<uint8_t>()[2];

    auto& sink = client->rtc_audio_whisper_supplier;
    sink.send_audio(voice_packet_id, false, voice_packet_id * 960, voice_codec, std::string_view{(const char*) payload, payload_length});

    client->resetIdleTime();
    client->updateSpeak(false, std::chrono::system_clock::now());
}

void VoiceClientConnection::handlePacketAck(const protocol::PacketParser& packet) {
    if(packet.payload_length() < 2) {
        return;
    }
    uint16_t target_id{be2le16(packet.payload().data_ptr<char>())};

    this->ping_handler_.received_command_acknowledged();
    this->packet_statistics().received_acknowledge((protocol::PacketType) packet.type(), target_id | (uint32_t) (packet.estimated_generation() << 16U));

    string error{};
    if(!this->packet_encoder().acknowledge_manager().process_acknowledge(packet.type(), target_id, error)) {
        debugMessage(this->virtual_server_id_, "{} Failed to handle acknowledge: {}", this->log_prefix(), error);
    }
}

void VoiceClientConnection::handlePacketAckLow(const ts::protocol::PacketParser &packet) {
    this->handlePacketAck(packet);
}

void VoiceClientConnection::handlePacketCommand(ReassembledCommand* command) {
    {
        using CommandHandleResult = CryptSetupHandler::CommandHandleResult;

        auto result = this->crypt_setup_handler_.handle_command(command->command_view());
        switch (result) {
            case CommandHandleResult::PASS_THROUGH:
                break;

            case CommandHandleResult::CONSUME_COMMAND:
                ReassembledCommand::free(command);
                return;

            case CommandHandleResult::CLOSE_CONNECTION:
                ReassembledCommand::free(command);
                auto client = this->getCurrentClient();
                assert(client); /* FIXME! */
                client->close_connection(std::chrono::system_clock::time_point{});
                return;
        }
    }

    auto client = this->getCurrentClient();
    if(!client) {
        ReassembledCommand::free(command);
        /* TODO! */
        return;
    }

    client->server_command_queue()->enqueue_command_execution(command);
}