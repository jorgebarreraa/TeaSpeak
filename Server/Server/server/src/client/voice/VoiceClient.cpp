#include <algorithm>
#include <memory>
#include <tomcrypt.h>
#include <arpa/inet.h>
#include <ThreadPool/Timer.h>
#include <misc/memtracker.h>
#include <log/LogUtils.h>

#include "VoiceClient.h"
#include "src/InstanceHandler.h"
#include "src/manager/ActionLogger.h"

using namespace std;
using namespace std::chrono;
using namespace ts::server;
using namespace ts::protocol;

constexpr static auto kMaxWhisperClientNameLength{30};
constexpr static auto kWhisperClientUniqueIdLength{28}; /* base64 encoded SHA1 hash */

VoiceClient::VoiceClient(const std::shared_ptr<VoiceServer>& server, const sockaddr_storage* address) :
    SpeakingClient{server->get_server()->sql, server->get_server()},
    voice_server(server) {
    assert(address);
    memtrack::allocated<VoiceClient>(this);
    memcpy(&this->remote_address, address, sizeof(sockaddr_storage));

    debugMessage(this->server->getServerId(), " Creating VoiceClient instance at {}", (void*) this);
}

void VoiceClient::initialize() {
    auto ref_self = dynamic_pointer_cast<VoiceClient>(this->ref());
    assert(ref_self);
    this->ref_self_voice = ref_self;

    this->server_command_queue_ = std::make_unique<ServerCommandQueue>(
            serverInstance->server_command_executor(),
            std::make_unique<VoiceClientCommandHandler>(ref_self)
    );

    this->properties()[property::CLIENT_TYPE] = ClientType::CLIENT_TEAMSPEAK;
    this->properties()[property::CLIENT_TYPE_EXACT] = ClientType::CLIENT_TEAMSPEAK;

    this->state = ConnectionState::INIT_HIGH;
    this->connection = new connection::VoiceClientConnection(this);
}

VoiceClient::~VoiceClient() {
    debugMessage(this->getServerId(), " Deleting VoiceClient instance at {}", (void*) this);

    this->state = ConnectionState::DISCONNECTED;
    delete this->connection;
    this->connection = nullptr;

    if(this->flush_task) {
        logCritical(this->getServerId(), "VoiceClient deleted with an active pending flush task!");
        serverInstance->general_task_executor()->cancel_task(this->flush_task);
    }

    memtrack::freed<VoiceClient>(this);
}

void VoiceClient::sendCommand0(const std::string_view& cmd, bool low, std::unique_ptr<std::function<void(bool)>> listener) {
    this->connection->send_command(cmd, low, std::move(listener));

#ifdef PKT_LOG_CMD
    logTrace(this->getServerId(), "{}[Command][Server -> Client] Sending command {}. Command low: {}. Full command: {}", CLIENT_STR_LOG_PREFIX, cmd.substr(0, cmd.find(' ')), low, cmd);
#endif
}

void VoiceClient::tick_server(const std::chrono::system_clock::time_point &time) {
    SpeakingClient::tick_server(time);
    {
        ALARM_TIMER(A1, "VoiceClient::tick", milliseconds(3));
        if(this->state == ConnectionState::CONNECTED) {
            this->connection->ping_handler().tick(time);
            this->connection->packet_statistics().tick();
        } else if(this->state == ConnectionState::INIT_LOW || this->state == ConnectionState::INIT_HIGH) {
            auto last_command = this->connection->crypt_setup_handler().last_handled_command();
            if(last_command.time_since_epoch().count() != 0) {
                if(time - last_command > seconds(5)) {
                    debugMessage(this->getServerId(), "{} Got handshake timeout. {}. State: {} Time: {}", CLIENT_STR_LOG_PREFIX,
                                 this->getLoggingPeerIp() + ":" + to_string(this->getPeerPort()),
                                 this->state == ConnectionState::INIT_HIGH ? "INIT_HIGH" : "INIT_LOW",
                                 duration_cast<seconds>(time - last_command).count()
                    );
                    this->close_connection(system_clock::now() + seconds(1));
                }
            }
        }
    }
}

std::chrono::milliseconds VoiceClient::current_ping() {
    return this->connection->ping_handler().current_ping();
}

bool VoiceClient::disconnect(const std::string &reason) {
    return this->disconnect(VREASON_SERVER_KICK, reason, this->server->serverRoot, true);
}

bool VoiceClient::disconnect(ts::ViewReasonId reason_id, const std::string &reason, const std::shared_ptr<ts::server::ConnectedClient>& invoker, bool notify_viewer) {
    task_id disconnect_id;

    auto weak_client{this->weak_ref()};
    serverInstance->general_task_executor()->schedule(disconnect_id, "voice client disconnect of " + this->getLoggingPrefix(), [
            weak_client,
            reason_id,
            reason,
            invoker,
            notify_viewer
    ]{
        auto client = dynamic_pointer_cast<VoiceClient>(weak_client.lock());
        if(!client) {
            /* client has already been deallocated */
            return;
        }

        bool await_disconnect_ack{false};
        {
            std::lock_guard state_lock{client->state_lock};
            switch (client->state) {
                case ConnectionState::DISCONNECTING:
                case ConnectionState::DISCONNECTED:
                    /* somebody else already disconnected the client */
                    return;

                case ConnectionState::INIT_HIGH:
                case ConnectionState::INIT_LOW:
                    /* disconnect the client and close the connection */
                    break;

                case ConnectionState::CONNECTED:
                    await_disconnect_ack = true;
                    break;

                case ConnectionState::UNKNWON:
                default:
                    assert(false);
                    return;
            }

            client->state = ConnectionState::DISCONNECTING;
        }

        /* server should never be a nullptr */
        assert(client->server);

        ChannelId client_channel{client->getChannelId()};
        {
            std::lock_guard command_lock{client->command_lock};
            std::unique_lock server_tree_lock{client->server->channel_tree_mutex};
            if(client->currentChannel) {
                if(notify_viewer) {
                    client->server->client_move(client->ref(), nullptr, invoker, reason, reason_id, false, server_tree_lock);
                } else {
                    auto server_channel = dynamic_pointer_cast<ServerChannel>(client->currentChannel);
                    assert(server_channel);
                    server_channel->unregister_client(client);

                    client->currentChannel = nullptr;
                }
            }
        }

        {
            ts::command_builder notify{"notifyclientleftview"};
            notify.put_unchecked(0, "reasonmsg", reason);
            notify.put_unchecked(0, "reasonid", reason_id);
            notify.put_unchecked(0, "clid", client->getClientId());
            notify.put_unchecked(0, "cfid", client_channel);
            notify.put_unchecked(0, "ctid", "0");

            if (invoker) {
                notify.put_unchecked(0, "invokerid", invoker->getClientId());
                notify.put_unchecked(0, "invokername", invoker->getDisplayName());
                notify.put_unchecked(0, "invokeruid", invoker->getUid());
            }

            if(await_disconnect_ack) {
                {
                    std::lock_guard flush_lock{client->flush_mutex};
                    if(!client->disconnect_acknowledged.has_value()) {
                        client->disconnect_acknowledged = std::make_optional(false);
                    } else {
                        /*
                         * The disconnect acknowledged flag might already has been set to true in cases
                         * where we know that the client is aware of the disconnect.
                         * An example scenario would be when the client sends the `clientdisconnect` command.
                         */
                    }
                }

                client->sendCommand0(notify.build(), false, std::make_unique<std::function<void(bool)>>([weak_client](bool success){
                    auto self = dynamic_pointer_cast<VoiceClient>(weak_client.lock());
                    if(!self) {
                        return;
                    }

                    if(!success) {
                        /* In theory we have no need to disconnect the client any more since a failed acknowledge would do the trick for us */
                        debugMessage(self->getServerId(), "{} Failed to receive disconnect acknowledge!", CLIENT_STR_LOG_PREFIX_(self));
                    } else {
                        debugMessage(self->getServerId(), "{} Received disconnect acknowledge!", CLIENT_STR_LOG_PREFIX_(self));
                    }

                    std::lock_guard flush_lock{self->flush_mutex};
                    assert(self->disconnect_acknowledged.has_value());
                    *self->disconnect_acknowledged = true;
                }));
            } else {
                client->sendCommand(notify, false);
            }
        }

        /* close the connection after 5 seconds regardless if we've received a disconnect acknowledge */
        client->close_connection(chrono::system_clock::now() + chrono::seconds{5});
    });

    return true;
}

bool VoiceClient::connection_flushed() {
    /* Ensure that we've at least send everything we wanted to send */
    if(!this->connection->wait_empty_write_and_prepare_queue(std::chrono::system_clock::time_point{})) {
        /* we still want to write data */
        return false;
    }

    {
        std::lock_guard flush_lock{this->flush_mutex};
        if(this->disconnect_acknowledged.has_value()) {
            /* We've send a disconnect command to the client. If the client acknowledges this command we've nothing more to send. */
            return *this->disconnect_acknowledged;
        }
    }

    if(this->connection->packet_encoder().acknowledge_manager().awaiting_acknowledge() > 0) {
        /* We're still waiting for some acknowledges from the client (for example the disconnect/kick packet) */
        return false;
    }

    return true;
}

bool VoiceClient::close_connection(const system_clock::time_point &timeout) {
    std::lock_guard flush_lock{this->flush_mutex};
    if(this->flush_executed) {
        /* We've already scheduled a connection close. We only update the timeout. */
        this->flush_timeout = std::min(timeout, this->flush_timeout);
        return true;
    }

    this->flush_executed = true;
    this->flush_timeout = timeout;

    auto weak_client{this->weak_ref()};
    serverInstance->general_task_executor()->schedule_repeating(this->flush_task, "connection flush/close for " + this->getLoggingPrefix(), std::chrono::milliseconds{25}, [weak_client](const auto&){
        auto client = dynamic_pointer_cast<VoiceClient>(weak_client.lock());
        if(!client) {
            /* client has already been deallocated */
            return;
        }

        /* Dont execute any commands which might alter the current client state */
        std::lock_guard command_lock{client->command_lock};

        {
            std::lock_guard state_lock{client->state_lock};
            switch(client->state) {
                case ConnectionState::DISCONNECTED:
                    /* Client has successfully been disconnect. Our task should be removed soon. */
                    return;

                case ConnectionState::CONNECTED:
                case ConnectionState::INIT_HIGH:
                case ConnectionState::INIT_LOW:
                    /* It's the first call to this task. Setting the clients state to disconnecting */
                    break;

                case ConnectionState::DISCONNECTING:
                    /* We're just awaiting for the client to finish stuff */
                    break;

                case ConnectionState::UNKNWON:
                default:
                    assert(false);
                    return;
            }

            client->state = ConnectionState::DISCONNECTING;
        }

        auto timestamp_now = std::chrono::system_clock::now();
        auto flushed = client->connection_flushed();
        if(!flushed && client->flush_timeout >= timestamp_now) {
            /* connection hasn't yet been flushed */
            return;
        }

        if(flushed) {
            debugMessage(client->getServerId(), "{} Connection successfully flushed.", client->getLoggingPrefix());
        } else {
            debugMessage(client->getServerId(), "{} Connection flush timed out. Force closing connection.", client->getLoggingPrefix());
        }

        /* connection flushed or flush timed out. */
        client->finalDisconnect();
    });

    return true;
}

void VoiceClient::finalDisconnect() {
    auto ownLock = dynamic_pointer_cast<VoiceClient>(this->ref());
    assert(ownLock);

    {
        std::lock_guard state_lock{this->state_lock};
        if(this->state != ConnectionState::DISCONNECTING) {
            logCritical(this->getServerId(), "{} finalDisconnect called but state isn't disconnecting ({}).", this->getLoggingPrefix(), (uint32_t) this->state);
            return;
        }

        this->state = ConnectionState::DISCONNECTED;
    }

    {
        std::lock_guard flush_lock{this->flush_mutex};
        if(this->flush_task) {
            serverInstance->general_task_executor()->cancel_task(this->flush_task);
            this->flush_task = 0;
        }
    }

    /* TODO: Remove? (legacy) */
    {
        lock_guard disconnect_lock_final(this->finalDisconnectLock);
    }

    std::lock_guard command_lock{this->command_lock};
    this->processLeave();

    /* The voice server might be null if it's already gone */
    auto voice_server_{this->voice_server};
    if(voice_server_) {
        voice_server_->unregisterConnection(ownLock);
    }
}

void VoiceClient::send_voice_packet(const pipes::buffer_view &voice_buffer, const SpeakingClient::VoicePacketFlags &flags) {
    protocol::PacketFlags packet_flags{(uint8_t) PacketFlag::None};
    packet_flags |= flags.encrypted ? PacketFlag::None : PacketFlag::Unencrypted;
    packet_flags |= flags.head ? PacketFlag::Compressed : PacketFlag::None;
    packet_flags |= flags.fragmented ? PacketFlag::Fragmented : PacketFlag::None;
    packet_flags |= flags.new_protocol ? PacketFlag::NewProtocol : PacketFlag::None;

    this->connection->send_packet(PacketType::VOICE, packet_flags, voice_buffer.data_ptr<void>(), voice_buffer.length());
}

void VoiceClient::send_voice(const std::shared_ptr<SpeakingClient> &source_client, uint16_t seq_no, uint8_t codec, const void *payload, size_t payload_length) {
    /* TODO: Somehow set the head (compressed) flag for beginning voice packets? */
    auto packet = protocol::allocate_outgoing_server_packet(payload_length + 5);
    packet->type_and_flags_ = protocol::PacketType::VOICE;

    auto packet_payload = (uint8_t*) packet->payload;
    *((uint16_t*) packet_payload + 0) = htons(seq_no);
    *((uint16_t*) packet_payload + 1) = htons(source_client->getClientId());
    packet_payload[4] = codec;

    if(payload) {
        memcpy(packet->payload + 5, payload, payload_length);
    } else {
        assert(payload_length == 0);
    }

    this->getConnection()->send_packet(packet);
}

void VoiceClient::send_voice_whisper(const std::shared_ptr<SpeakingClient> &source_client, uint16_t seq_no, uint8_t codec, const void *payload, size_t payload_length) {
    bool head{false};
    if(this->whisper_head_counter < 5) {
        head = true;
        this->whisper_head_counter++;
    }
    if(!payload) {
        this->whisper_head_counter = 0;
    }

    protocol::OutgoingServerPacket* packet;
    size_t payload_offset{0};

    if(head && this->getType() == ClientType::CLIENT_TEASPEAK) {
        auto uniqueId = source_client->getUid();
        auto nickname = source_client->getDisplayName();

        if(uniqueId.length() > kWhisperClientUniqueIdLength) {
            logCritical(LOG_GENERAL, "Clients unique id is longer than the expected max length of {}. Unique length: {}", kWhisperClientUniqueIdLength, uniqueId.length());
            return;
        }

        if(nickname.length() > kMaxWhisperClientNameLength) {
            logCritical(LOG_GENERAL, "Clients name is longer than the expected max length of {}. Name length: {}", kMaxWhisperClientNameLength, nickname.length());
            return;
        }

        packet = protocol::allocate_outgoing_server_packet(
                payload_length + 5 + kWhisperClientUniqueIdLength + 1 + nickname.length());
        packet->type_and_flags_ |= protocol::PacketFlag::Compressed;

        memset(packet->payload + payload_offset, 0, kWhisperClientUniqueIdLength);
        memcpy(packet->payload + payload_offset, uniqueId.data(), uniqueId.length());
        payload_offset += kWhisperClientUniqueIdLength;

        packet->payload[payload_offset++] = nickname.length();
        memcpy(packet->payload + payload_offset, nickname.data(), nickname.length());
        payload_offset += nickname.length();
    } else {
        packet = protocol::allocate_outgoing_server_packet(payload_length + 5);
    }
    packet->type_and_flags_ |= protocol::PacketType::VOICE_WHISPER;

    *((uint16_t*) &packet->payload[payload_offset]) = htons(seq_no);
    payload_offset += 2;

    *((uint16_t*) &packet->payload[payload_offset]) = htons(source_client->getClientId());
    payload_offset += 2;

    packet->payload[payload_offset] = codec;
    payload_offset += 1;

    if(payload) {
        memcpy(packet->payload + payload_offset, payload, payload_length);
        payload_offset += payload_length;
    } else {
        assert(payload_length == 0);
    }
    packet->payload_size = payload_offset;

    this->getConnection()->send_packet(packet);
}

float VoiceClient::current_ping_deviation() {
    return this->connection->packet_encoder().acknowledge_manager().current_rttvar();
}

float VoiceClient::current_packet_loss() const {
    return this->connection->packet_statistics().current_packet_loss();
}
void VoiceClient::processJoin() {
    SpeakingClient::processJoin();
    if(this->rtc_client_id > 0) {
        {
            /* Normal audio channel */
            auto sender = this->server->rtc_server().create_audio_source_supplier_sender(this->rtc_client_id, 1);
            assert(sender.has_value());
            this->rtc_audio_supplier.reset(*sender);
        }

        {
            /* Audio whisper channel */
            auto sender = this->server->rtc_server().create_audio_source_supplier_sender(this->rtc_client_id, 2);
            assert(sender.has_value());
            this->rtc_audio_whisper_supplier.reset(*sender);
        }
    }
}

void VoiceClient::clear_video_unsupported_message_flag() {
    this->video_unsupported_message_send = false;
}

constexpr static auto kUnsupportedMessage{"\n"
                                          "[b]Somebody in your channel started video sharing!\n"
                                          "[color=red]Your client doesn't support video sharing.[/color]\n"
                                          "\n"
                                          "Download the newest TeaSpeak client from [url]https://teaspeak.de[/url] or\n"
                                          "use the TeaSpeakWeb client at [url]%web-url%[/url].[/b]"};

void VoiceClient::send_video_unsupported_message() {
    if(this->video_unsupported_message_send) {
        return;
    }

    this->video_unsupported_message_send = true;
    ts::command_builder notify{"notifytextmessage"};
    notify.put_unchecked(0, "targetmode", ChatMessageMode::TEXTMODE_CHANNEL);
    notify.put_unchecked(0, "target", "0");
    notify.put_unchecked(0, "invokerid", "0");
    notify.put_unchecked(0, "invokername", "TeaSpeak - Video");
    notify.put_unchecked(0, "invokeruid", "000000000000000000000000000");
    std::string message{kUnsupportedMessage};
    if(auto index{message.find("%web-url%")}; index != std::string::npos) {
        /* TODO: generate connect url */
        message.replace(index, 9, "https://web.teaspeak.de");
    }
    notify.put_unchecked(0, "msg", message);

    this->sendCommand(notify, false);
}