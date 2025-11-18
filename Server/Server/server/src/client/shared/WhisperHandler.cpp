//
// Created by WolverinDEV on 26/11/2020.
//

#include "WhisperHandler.h"
#include "src/client/voice/VoiceClientConnection.h"
#include <misc/endianness.h>
#include <log/LogUtils.h>

using namespace ts::server::whisper;

constexpr static auto kMaxWhisperTargets{1024};

WhisperHandler::WhisperHandler(SpeakingClient* handle) : handle{handle} {};
WhisperHandler::~WhisperHandler() {
    if(this->whisper_head_ptr) {
        ::free(this->whisper_head_ptr);
    }
}

bool WhisperHandler::validate_whisper_packet(const protocol::PacketParser &packet, bool& match_last_header, void *&payload_ptr, size_t &payload_length) {
    size_t head_length;
    if(packet.flags() & protocol::PacketFlag::NewProtocol) {
        if(packet.payload_length() < 3 + 10) {
            /* packet too short */
            return false;
        }

        head_length = 10;
    } else {
        if(packet.payload_length() < 3 + 2) {
            /* packet too short */
            return false;
        }

        auto channel_count = (uint8_t) packet.payload()[3];
        auto client_count = (uint8_t) packet.payload()[4];
        head_length = 2 + channel_count * 8 + client_count * 2;

        if(packet.payload_length() < 3 + head_length) {
            /* packet is too short */
            return false;
        }
    }

    auto head_ptr = packet.payload().data_ptr<uint8_t>() + 3;
    {
        std::lock_guard process_lock{this->whisper_head_mutex};
        match_last_header = this->whisper_head_length == head_length && memcmp(this->whisper_head_ptr, head_ptr, head_length) == 0;
        if(!match_last_header) {
            if(this->whisper_head_capacity < head_length) {
                if(this->whisper_head_ptr) {
                    ::free(this->whisper_head_ptr);
                }

                this->whisper_head_ptr = malloc(head_length);
                this->whisper_head_capacity = head_length;
            }

            this->whisper_head_length = head_length;
            memcpy(this->whisper_head_ptr, head_ptr, head_length);
        }
    }

    assert(packet.payload_length() >= head_length + 3);
    payload_ptr = (void*) (head_ptr + head_length);
    payload_length = packet.payload_length() - head_length - 3;

    return true;
}

bool WhisperHandler::process_packet(const protocol::PacketParser &packet, void *&payload_ptr, size_t &payload_length) {
    bool match_last_header;
    if(!this->validate_whisper_packet(packet, match_last_header, payload_ptr, payload_length)) {
        return false;
    }

    auto current_timestamp = std::chrono::system_clock::now();
    switch (this->session_state) {
        case SessionState::Uninitialized:
            /* Definitively initialize a new session */
            break;

        case SessionState::InitializeFailed: {
            if(!match_last_header) {
                /* Last header does not matches the current header, we need to reinitialize the session */
                break;
            }

            if(current_timestamp - std::chrono::milliseconds{500} < this->session_timestamp) {
                /* Last initialize failed and is less than 500ms ago */
                return false;
            }

            /* Lets try to initialize the whisper session again */
            break;
        }

        case SessionState::Initialized:
            if(!match_last_header) {
                /* Last header does not matches the current header, we need to reinitialize the session */
                break;
            }

            if(current_timestamp - std::chrono::seconds{5} > this->session_timestamp) {
                /* Last session member update is 5 seconds ago. Updating the session again */
                break;
            }

            /* We've nothing to change and everything is good */
            return true;
    }

    this->session_timestamp = current_timestamp;

    auto head_ptr = packet.payload().data_ptr<uint8_t>();
    size_t head_offset{3}; /* the first three bytes contain the voice sequence id and codec */

    if(packet.flags() & protocol::PacketFlag::NewProtocol) {
        auto type = head_ptr[head_offset++];
        auto target = head_ptr[head_offset++];
        auto type_id = be2le64(head_ptr, head_offset, &head_offset);

        auto result = this->initialize_session_new(2, type, target, type_id);
        if(result.has_error()) {
            this->session_state = SessionState::InitializeFailed;
            this->handle->notifyError(result);
        } else {
            this->session_state = SessionState::Initialized;
        }
        result.release_data();
    } else {
        auto channel_count = (uint8_t) head_ptr[head_offset++];
        auto client_count = (uint8_t) head_ptr[head_offset++];

        ChannelId channel_ids[channel_count];
        ClientId client_ids[client_count];

        for(uint8_t index = 0; index < channel_count; index++) {
            channel_ids[index] = be2le64(head_ptr, head_offset, &head_offset);
        }

        for(uint8_t index = 0; index < client_count; index++) {
            client_ids[index] = be2le16(head_ptr, head_offset, &head_offset);
        }

        auto result = this->initialize_session_old(2, client_ids, client_count, channel_ids, channel_count);
        if(result.has_error()) {
            this->session_state = SessionState::InitializeFailed;
            this->handle->notifyError(result);
        } else {
            this->session_state = SessionState::Initialized;
        }
        result.release_data();
    }

    return this->session_state == SessionState::Initialized;
}

void WhisperHandler::signal_session_reset() {
    auto server = this->handle->getServer();
    if(!server) {
        return;
    }

    server->rtc_server().reset_whisper_session(this->handle->rtc_client_id);
}

void WhisperHandler::handle_session_reset() {
    std::lock_guard process_lock{this->whisper_head_mutex};

    this->session_state = SessionState::Uninitialized;
    if(this->whisper_head_ptr) {
        ::free(this->whisper_head_ptr);
        this->whisper_head_ptr = nullptr;
        this->whisper_head_capacity = 0;
        this->whisper_head_length = 0;
    }
}

size_t WhisperHandler::max_whisper_targets() {
    auto server = this->handle->getServer();
    if(!server) {
        return false;
    }

    auto result = server->properties()[property::VIRTUALSERVER_MIN_CLIENTS_IN_CHANNEL_BEFORE_FORCED_SILENCE].as_or<size_t>(kMaxWhisperTargets);
    if(result > kMaxWhisperTargets) {
        result = kMaxWhisperTargets;
    }
    return result;
}

ts::command_result WhisperHandler::initialize_session_old(uint32_t stream_id, const uint16_t *client_ids, size_t client_count, const uint64_t *channel_ids, size_t channel_count) {
    auto server = this->handle->getServer();
    if(!server) {
        return ts::command_result{error::vs_critical, "missing server"};
    }

    std::vector<std::shared_ptr<SpeakingClient>> target_clients{};
    auto connected_clients = server->getClients();
    target_clients.reserve(connected_clients.size());

    for(const auto& connected_client : connected_clients) {
        auto speaking_client = dynamic_pointer_cast<SpeakingClient>(connected_client);
        if(!speaking_client || speaking_client == this->handle || !speaking_client->rtc_client_id) {
            continue;
        }

        auto client_channel_id = speaking_client->getChannelId();
        auto client_id = speaking_client->getClientId();

        for(size_t index = 0; index < channel_count; index++) {
            if(channel_ids[index] == client_channel_id) {
                goto add_client;
            }
        }

        for(size_t index = 0; index < client_count; index++) {
            if(client_ids[index] == client_id) {
                goto add_client;
            }
        }

        continue;

        add_client:
        target_clients.push_back(speaking_client);
    }

    return this->configure_rtc_clients(stream_id, target_clients);
}

ts::command_result WhisperHandler::initialize_session_new(uint32_t stream_id, uint8_t type_u8, uint8_t target_u8, uint64_t type_id) {
    auto type = (WhisperType) type_u8;
    auto target = (WhisperTarget) target_u8;

    auto server = this->handle->getServer();
    if(!server) {
        return ts::command_result{error::vs_critical, "missing server"};
    }

#ifdef PKT_LOG_WHISPER
    logTrace(this->getServerId(), "{} Whisper data length: {}. Type: {}. Target: {}. Target ID: {}.", CLIENT_STR_LOG_PREFIX, data_length, type, target, type_id);
#endif
    std::vector<std::shared_ptr<SpeakingClient>> target_clients{};
    if(type == WhisperType::ECHO) {
        target_clients.push_back(dynamic_pointer_cast<SpeakingClient>(this->handle->ref()));
    } else {
        auto connected_clients = server->getClients();
        target_clients.reserve(connected_clients.size());
        for (const auto &available_client : connected_clients) {
            auto speaking_client = dynamic_pointer_cast<SpeakingClient>(available_client);
            if (!speaking_client || this->handle == speaking_client || !speaking_client->rtc_client_id) {
                continue;
            }

            if (type == WhisperType::ALL) {
                target_clients.push_back(speaking_client);
            } else if (type == WhisperType::SERVER_GROUP) {
                if (type_id == 0) {
                    target_clients.push_back(speaking_client);
                } else {
                    std::shared_lock client_view_lock(speaking_client->get_channel_lock());
                    for (const auto &id : speaking_client->current_server_groups()) {
                        if (id == type_id) {
                            target_clients.push_back(speaking_client);
                            break;
                        }
                    }
                }
            } else if (type == WhisperType::CHANNEL_GROUP) {
                if (speaking_client->current_channel_group() == type_id) {
                    target_clients.push_back(speaking_client);
                }
            } else if (type == WhisperType::CHANNEL_COMMANDER) {
                if (speaking_client->properties()[property::CLIENT_IS_CHANNEL_COMMANDER].as_or<bool>(false)) {
                    target_clients.push_back(speaking_client);
                }
            } else {
                return ts::command_result{error::parameter_invalid, "type"};
            }
        }

        if (target == WhisperTarget::CHANNEL_CURRENT) {
            target_clients.erase(std::remove_if(target_clients.begin(), target_clients.end(), [&](const shared_ptr<SpeakingClient> &target) {
                                     return target->getChannel() != this->handle->getChannel();
                                 }),
                                 target_clients.end());
        } else if (target == WhisperTarget::CHANNEL_PARENT) {
            auto current_parent = this->handle->getChannel();
            if (current_parent && (current_parent = current_parent->parent())) {
                target_clients.erase(std::remove_if(target_clients.begin(), target_clients.end(), [&](const shared_ptr<SpeakingClient> &target) {
                                         return target->getChannel() != current_parent;
                                     }),
                                     target_clients.end());
            } else {
                target_clients.clear();
            }
        } else if (target == WhisperTarget::CHANNEL_ALL_PARENT) {
            auto current_parent = this->handle->getChannel();
            if (current_parent && (current_parent = current_parent->parent())) {
                target_clients.erase(std::remove_if(target_clients.begin(), target_clients.end(), [&](const shared_ptr<SpeakingClient> &target) {
                                         auto tmp_parent = current_parent;
                                         while (tmp_parent && tmp_parent != target->getChannel())
                                             tmp_parent = tmp_parent->parent();
                                         return target->getChannel() != tmp_parent;
                                     }),
                                     target_clients.end());
            } else {
                target_clients.clear();
            }
        } else if (target == WhisperTarget::CHANNEL_FAMILY) {
            auto current_channel = this->handle->getChannel();
            target_clients.erase(std::remove_if(target_clients.begin(), target_clients.end(), [&](const shared_ptr<SpeakingClient> &target) {
                                     auto tmp_current = target->getChannel();
                                     while (tmp_current && tmp_current != current_channel) {
                                         tmp_current = tmp_current->parent();
                                     }
                                     return current_channel != tmp_current;
                                 }),
                                 target_clients.end());
        } else if (target == WhisperTarget::CHANNEL_COMPLETE_FAMILY) {
            shared_ptr<BasicChannel> current = this->handle->getChannel();
            while (current && current->parent()) current = current->parent();
            target_clients.erase(std::remove_if(target_clients.begin(), target_clients.end(), [&](const shared_ptr<SpeakingClient> &target) {
                                     auto tmp_current = target->getChannel();
                                     while (tmp_current && tmp_current != current)
                                         tmp_current = tmp_current->parent();
                                     return current != tmp_current;
                                 }),
                                 target_clients.end());
        } else if (target == WhisperTarget::CHANNEL_SUBCHANNELS) {
            shared_ptr<BasicChannel> current = this->handle->getChannel();
            target_clients.erase(std::remove_if(target_clients.begin(), target_clients.end(), [&](const shared_ptr<SpeakingClient> &target) {
                                     return target->getChannel()->parent() != current;
                                 }),
                                 target_clients.end());
        } else if(target == WhisperTarget::CHANNEL_ALL) {
            /* nothing to filter out */
        } else {
            return ts::command_result{error::parameter_invalid, "target"};
        }
    }

    return this->configure_rtc_clients(stream_id, target_clients);
}

ts::command_result WhisperHandler::configure_rtc_clients(uint32_t stream_id, const std::vector<std::shared_ptr<SpeakingClient>>& target_clients) {
    auto max_clients = this->max_whisper_targets();
    assert(max_clients <= kMaxWhisperTargets);

    if(target_clients.size() >= max_clients) {
        return ts::command_result{error::whisper_too_many_targets};
    }

    if(target_clients.empty()) {
        return ts::command_result{error::whisper_no_targets};
    }

    auto server = this->handle->getServer();
    if(!server) {
        return ts::command_result{error::vs_critical, "missing server"};
    }

    uint32_t target_client_ids[kMaxWhisperTargets];
    size_t target_client_count{0};
    for(const auto& target_client : target_clients) {
        target_client_ids[target_client_count++] = target_client->rtc_client_id;
    }

    std::string error;
    if(!server->rtc_server().configure_whisper_session(error, this->handle->rtc_client_id, stream_id, target_client_ids, target_client_count)) {
        logCritical(server->getServerId(), "{} Failed to configure whisper session with {} participants.", CLIENT_STR_LOG_PREFIX_(this->handle), target_client_count);
        return ts::command_result{error::vs_critical, error};
    }

    logTrace(server->getServerId(), "{} Configured whisper session with {} participants.", CLIENT_STR_LOG_PREFIX_(this->handle), target_client_count);
    return ts::command_result{error::ok};
}