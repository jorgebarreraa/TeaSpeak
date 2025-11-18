#include <memory>

#include <vector>
#include <bitset>
#include <algorithm>
#include "../ConnectedClient.h"
#include "../InternalClient.h"
#include "../voice/VoiceClient.h"
#include "PermissionManager.h"
#include "../../InstanceHandler.h"
#include "../../server/QueryServer.h"
#include "../music/MusicClient.h"
#include "../query/QueryClient.h"
#include "../../manager/ConversationManager.h"
#include "../../manager/PermissionNameMapper.h"
#include "../../manager/ActionLogger.h"
#include "../../PermissionCalculator.h"
#include <cstdint>

#include "helpers.h"
#include "./bulk_parsers.h"

#include <Properties.h>
#include <log/LogUtils.h>
#include <misc/base64.h>
#include <misc/hex.h>
#include <misc/rnd.h>
#include <bbcode/bbcodes.h>
#include <misc/utf8.h>

using namespace std::chrono;
using namespace std;
using namespace ts;
using namespace ts::server;

#define QUERY_PASSWORD_LENGTH 12

command_result ConnectedClient::handleCommandClientGetVariables(Command &cmd) {
    CMD_REQ_SERVER;
    ConnectedLockedClient client{this->server->find_client_by_id(cmd["clid"].as<ClientId>())};
    {
        shared_lock tree_lock(this->channel_tree_mutex);

        if (!client || (client.client != this && !this->isClientVisible(client.client, false)))
            return command_result{error::client_invalid_id, ""};

        deque<const property::PropertyDescription*> props;
        for (auto &prop : client->properties()->list_properties(property::FLAG_CLIENT_VARIABLE, this->getType() == CLIENT_TEAMSPEAK ? property::FLAG_NEW : (uint16_t) 0)) {
            props.push_back(&prop.type());
        }

        this->notifyClientUpdated(client.client, props, false);
    }
    if(client.client == this && this->getType() == ClientType::CLIENT_TEAMSPEAK) {
        this->subscribeChannel({this->currentChannel}, true, true); /* lets show the clients in the current channel because we've not done that while joining ("speed improvement" ;)) */
    }
    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandClientKick(Command &cmd) {
    CMD_REQ_SERVER;
    CMD_CHK_AND_INC_FLOOD_POINTS(25);

    command_result_bulk result{};
    result.reserve(cmd.bulkCount());

    std::vector<ConnectedLockedClient<ConnectedClient>> clients{};
    clients.reserve(cmd.bulkCount());

    auto type = cmd["reasonid"].as<ViewReasonId>();
    auto target_channel = type == ViewReasonId::VREASON_CHANNEL_KICK ? this->server->channelTree->getDefaultChannel() : nullptr;

    for(size_t index = 0; index < cmd.bulkCount(); index++) {
        ConnectedLockedClient<ConnectedClient> client{this->server->find_client_by_id(cmd[index]["clid"].as<ClientId>())};

        if (!client) {
            result.emplace_result(error::client_invalid_id);
            continue;
        }

        if (client->getType() == CLIENT_MUSIC) {
            result.emplace_result(error::client_invalid_type);
            continue;
        }

        if(type == ViewReasonId::VREASON_CHANNEL_KICK) {
            auto kick_power = this->calculate_permission(permission::i_client_kick_from_channel_power, client->getChannelId()).zero_if_unset();
            if(!permission::v2::permission_granted(client->calculate_permission(permission::i_client_needed_kick_from_channel_power, client->getChannelId()), kick_power)) {
                result.emplace_result(permission::i_client_needed_kick_from_channel_power);
                continue;
            }
        } else {
            auto kick_power = this->calculate_permission(permission::i_client_kick_from_server_power, client->getChannelId()).zero_if_unset();
            if(!permission::v2::permission_granted(client->calculate_permission(permission::i_client_needed_kick_from_server_power, client->getChannelId()), kick_power)) {
                result.emplace_result(permission::i_client_needed_kick_from_server_power);
                continue;
            }
        }

        clients.emplace_back(std::move(client));
        result.emplace_result(error::ok);
    }

    for(auto& client : clients) {
        auto old_channel = client->getChannel();
        if(!old_channel) continue;

        if (target_channel) {
            this->server->notify_client_kick(client.client, this->ref(), cmd["reasonmsg"].as<std::string>(), target_channel);
            serverInstance->action_logger()->client_channel_logger.log_client_kick(this->getServerId(), this->ref(), client->ref(), target_channel->channelId(), target_channel->name(), old_channel->channelId(), old_channel->name());
        } else {
            this->server->notify_client_kick(client.client, this->ref(), cmd["reasonmsg"].as<std::string>(), nullptr);
            client->close_connection(system_clock::now() + seconds(1));
            serverInstance->action_logger()->client_channel_logger.log_client_kick(this->getServerId(), this->ref(), client->ref(), 0, "", old_channel->channelId(), old_channel->name());
        }
    }

    return command_result{std::forward<command_result_bulk>(result)};
}

command_result ConnectedClient::handleCommandClientGetIds(Command &cmd) {
    CMD_REQ_SERVER;

    bool error = false;
    bool found = false;
    auto client_list = this->server->getClients();

    Command notify(this->notify_response_command("notifyclientids"));
    int result_index = 0;

    for(int index = 0; index < cmd.bulkCount(); index++) {
        auto unique_id = cmd[index]["cluid"].as<string>();
        for(const auto& entry : client_list) {
            if(entry->getUid() == unique_id) {
                if(!config::server::show_invisible_clients_as_online && !this->channel_tree->channel_visible(entry->currentChannel, nullptr))
                    continue;

                notify[result_index]["name"] = entry->getDisplayName();
                notify[result_index]["clid"] = entry->getClientId();
                notify[result_index]["cluid"] = entry->getUid();
                result_index++;
                found = true;
            }
        }
        if(found) found = false;
        else error = false;
    }

    if(result_index > 0) {
        this->sendCommand(notify);
    }
    if(error) {
        return command_result{error::database_empty_result};
    }
    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandClientMove(Command &cmd) {
    CMD_REQ_SERVER;
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(10);

    std::unique_lock server_channel_lock{this->server->channel_tree_mutex};

    auto target_channel = this->server->channelTree->findChannel(cmd["cid"].as<ChannelId>());
    if (!target_channel) {
        return command_result{error::channel_invalid_id};
    }

    auto& channel_whitelist = this->join_whitelisted_channel;
    auto whitelist_entry = std::find_if(channel_whitelist.begin(), channel_whitelist.end(), [&](const auto& entry) { return entry.first == target_channel->channelId(); });
    if(whitelist_entry != channel_whitelist.end()) {
        debugMessage(this->getServerId(), "{} Allowing client to join channel {} because the token he used earlier explicitly allowed it.", this->getLoggingPrefix(), target_channel->channelId());
        if(whitelist_entry->second != "ignore") {
            if (!target_channel->verify_password(cmd["cpw"].optional_string(), this->getType() != ClientType::CLIENT_QUERY)) {
                if (!permission::v2::permission_granted(1, this->calculate_permission(permission::b_channel_join_ignore_password, target_channel->channelId()))) {
                    return command_result{error::channel_invalid_password};
                }
            }
        }
    } else {
        if(!cmd[0].has("cpw")) {
            cmd["cpw"] = "";
        }

        if (!target_channel->verify_password(cmd["cpw"].optional_string(), this->getType() != ClientType::CLIENT_QUERY)) {
            if (!permission::v2::permission_granted(1, this->calculate_permission(permission::b_channel_join_ignore_password, target_channel->channelId()))) {
                return command_result{error::channel_invalid_password};
            }
        }

        auto permission_error = this->calculate_and_get_join_state(target_channel);
        if(permission_error != permission::unknown) {
            return command_result{permission_error};
        }
    }
    channel_whitelist.clear();

    command_result_bulk result{};
    result.reserve(cmd.bulkCount());

    std::vector<ConnectedLockedClient<ConnectedClient>> target_clients{};

    for(size_t index{0}; index < cmd.bulkCount(); index++) {
        auto target_client_id = cmd[index]["clid"].as<ClientId>();
        ConnectedLockedClient target_client{target_client_id == 0 ? this->ref() : this->server->find_client_by_id(target_client_id)};
        if(!target_client) {
            result.emplace_result(error::client_invalid_id);
            continue;
        }

        if(!target_client->getChannel()) {
            if(target_client.client != this) {
                result.emplace_result(error::client_invalid_id);
                continue;
            }
        }
        if(target_client->getChannel() == target_channel) {
            result.emplace_result(error::ok);
            continue;
        }

        if(target_client.client != this) {
            if(!permission::v2::permission_granted(target_client->calculate_permission(permission::i_client_needed_move_power, target_client->getChannelId()).zero_if_unset(), this->calculate_permission(permission::i_client_move_power, target_client->getChannelId()))) {
                result.emplace_result(permission::i_client_move_power);
                continue;
            }
            if(!permission::v2::permission_granted(target_client->calculate_permission(permission::i_client_needed_move_power, target_channel->channelId()).zero_if_unset(), this->calculate_permission(permission::i_client_move_power, target_channel->channelId()))) {
                result.emplace_result(permission::i_client_move_power);
                continue;
            }
        }
        target_clients.emplace_back(std::move(target_client));
        result.emplace_result(error::ok);
    }

    ClientPermissionCalculator own_permission_calculator{this, target_channel};

    /* FIXME: Some kind of invite key frags to prevent limit checking! */
    if (!target_channel->properties()[property::CHANNEL_FLAG_MAXCLIENTS_UNLIMITED].as_or<bool>(true)) {
        auto max_clients = target_channel->properties()[property::CHANNEL_MAXCLIENTS].as_or<uint32_t>(0);
        auto channel_clients = this->server->getClientsByChannel(target_channel);
        if(channel_clients.size() >= max_clients && !own_permission_calculator.permission_granted(permission::b_channel_join_ignore_maxclients, 1)) {
            return command_result{error::channel_maxclients_reached};
        }
    }

    if(!target_channel->properties()[property::CHANNEL_FLAG_MAXFAMILYCLIENTS_UNLIMITED].as_or<bool>(true)) {
        auto base_channel{target_channel};
        while(base_channel && base_channel->properties()[property::CHANNEL_FLAG_MAXFAMILYCLIENTS_INHERITED].as_or<bool>(false)) {
            base_channel = base_channel->parent();
        }

        if(base_channel) {
            auto max_clients = target_channel->properties()[property::CHANNEL_MAXFAMILYCLIENTS].as_or<uint32_t>(0);
            auto current_client_count = this->server->countChannelRootClients(target_channel, max_clients, false);
            if(current_client_count >= max_clients && !own_permission_calculator.permission_granted(permission::b_channel_join_ignore_maxclients, 1)) {
                return command_result{error::channel_maxfamily_reached};
            }
        } else {
            /* This is kinda odd, I guess we've moved the channel and haven't cleared the inherited flag */
        }
    }

    std::vector<std::shared_ptr<ServerChannel>> channels{};
    channels.reserve(target_clients.size());

    for(auto& client : target_clients) {
        auto client_old_channel = dynamic_pointer_cast<ServerChannel>(client->getChannel());
        if(!client_old_channel) {
            continue;
        }

        this->server->client_move(
                client.client,
                target_channel,
                client.client == this ? nullptr : this->ref(),
                "",
                client.client == this ? ViewReasonId::VREASON_USER_ACTION : ViewReasonId::VREASON_MOVED,
                true,
                server_channel_lock
        );

        serverInstance->action_logger()->client_channel_logger.log_client_move(this->getServerId(), this->ref(), client->ref(), target_channel->channelId(), target_channel->name(), client_old_channel->channelId(), client_old_channel->name());

        if(std::find_if(channels.begin(), channels.end(), [&](const std::shared_ptr<ServerChannel>& channel) { return &*channel == &*client_old_channel; }) == channels.end()) {
            channels.push_back(client_old_channel);
        }
    }

    for(const auto& oldChannel : channels) {
        if(!server_channel_lock.owns_lock()) {
            server_channel_lock.lock();
        }

        if(oldChannel->channelType() != ChannelType::temporary) {
            continue;
        }

        if(oldChannel->properties()[property::CHANNEL_DELETE_DELAY].as_unchecked<int64_t>() > 0) {
            continue;
        }

        if(!this->server->isChannelRootEmpty(oldChannel, false)) {
            continue;
        }

        this->server->delete_channel(oldChannel, this->ref(), "temporary auto delete", server_channel_lock, true);
    }

    return command_result{std::move(result)};
}

command_result ConnectedClient::handleCommandClientPoke(Command &cmd) {
    CMD_REQ_SERVER;
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(25);

    command_result_bulk result{};
    result.reserve(cmd.bulkCount());

    std::vector<ConnectedLockedClient<ConnectedClient>> clients{};
    clients.reserve(cmd.bulkCount());

    for(size_t index{0}; index < cmd.bulkCount(); index++) {
        ConnectedLockedClient client{ this->server->find_client_by_id(cmd[index]["clid"].as<ClientId>())};
        if (!client) {
            result.emplace_result(error::client_invalid_id);
            continue;
        }
        if (client->getType() == CLIENT_MUSIC) {
            result.emplace_result(error::client_invalid_type);
            continue;
        }

        auto own_permission = this->calculate_permission(permission::i_client_poke_power, client->getChannelId());
        if(!permission::v2::permission_granted(client->calculate_permission(permission::i_client_needed_poke_power, client->getChannelId()), own_permission)) {
            result.emplace_result(permission::i_client_poke_power);
            continue;
        }

        clients.push_back(std::move(client));
        result.emplace_result(error::ok);
    }

    /* clients might be empty ;) */

    if(clients.size() > 1) {
        auto max_clients = this->calculate_permission(permission::i_client_poke_max_clients, 0);
        if(!permission::v2::permission_granted(clients.size(), max_clients)) {
            return command_result{permission::i_client_poke_max_clients};
        }
    }

    auto message = cmd["msg"].string();
    if(utf8::count_characters(message) > ts::config::server::limits::poke_message_length) {
        return command_result{error::parameter_invalid_size, "msg"};
    }

    for(auto& client : clients) {
        client->notifyClientPoke(this->ref(), message);
    }

    return command_result{std::forward<command_result_bulk>(result)};
}


command_result ConnectedClient::handleCommandClientChatComposing(Command &cmd) {
    CMD_REQ_SERVER;
    CMD_CHK_AND_INC_FLOOD_POINTS(0);

    ConnectedLockedClient client{this->server->find_client_by_id(cmd["clid"].as<ClientId>())};
    if (!client) return command_result{error::client_invalid_id};

    client->notifyClientChatComposing(this->ref());
    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandClientChatClosed(Command &cmd) {
    CMD_REQ_SERVER;
    CMD_CHK_AND_INC_FLOOD_POINTS(5);

    ConnectedLockedClient<ConnectedClient> client{this->server->find_client_by_id(cmd["clid"].as<ClientId>())};
    if (!client) return command_result{error::client_invalid_id};
    {
        unique_lock channel_lock(this->channel_tree_mutex);
        this->open_private_conversations.erase(remove_if(this->open_private_conversations.begin(), this->open_private_conversations.end(), [&](const weak_ptr<ConnectedClient>& weak) {
            return weak.lock() == client;
        }), this->open_private_conversations.end());
    }
    {
        unique_lock channel_lock(client->get_channel_lock());
        client->open_private_conversations.erase(remove_if(client->open_private_conversations.begin(), client->open_private_conversations.end(), [&](const weak_ptr<ConnectedClient>& weak) {
            return weak.lock().get() == this;
        }), client->open_private_conversations.end());
    }
    client->notifyClientChatClosed(this->ref());
    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandClientDbList(Command &cmd) {
    CMD_REQ_SERVER;
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(25);
    ACTION_REQUIRES_GLOBAL_PERMISSION(permission::b_virtualserver_client_dblist, 1);

    size_t offset = cmd[0].has("start") ? cmd["start"].as<size_t>() : 0;
    size_t limit = cmd[0].has("duration") ? cmd["duration"].as<int>() : 0;
    if(limit > 2000 || limit < 1)
        limit = 2000;

    ts::command_builder result{this->notify_response_command("notifyclientdblist")};
    result.reserve_bulks(limit);

    struct CallbackArgument {
        ts::command_builder& result;
        bool show_ip{false};
        size_t command_index{0};
    };

    CallbackArgument callback_argument{result};
    callback_argument.show_ip = permission::v2::permission_granted(1, this->calculate_permission(permission::b_client_remoteaddress_view, 0));
    serverInstance->databaseHelper()->listDatabaseClients(this->getServerId(), { offset }, { limit }, [](void* ptr_data, const DatabaseClient& client) {
        auto argument = (CallbackArgument*) ptr_data;

        auto bulk = argument->result.bulk(argument->command_index++);
        bulk.reserve(300);

        bulk.put_unchecked("cldbid", client.client_database_id);
        bulk.put_unchecked("client_unique_identifier", client.client_unique_id);
        bulk.put_unchecked("client_nickname", client.client_nickname);
        bulk.put_unchecked("client_lastip", argument->show_ip ? client.client_ip : "hidden");
        bulk.put_unchecked("client_lastconnected", client.client_last_connected);
        bulk.put_unchecked("client_created", client.client_created);
        bulk.put_unchecked("client_totalconnections", client.client_total_connections);
        bulk.put_unchecked("client_login_name", client.client_login_name);
        bulk.put_unchecked("client_description", client.client_description);
    }, &callback_argument);

    if (callback_argument.command_index == 0)
        return command_result{error::database_empty_result};

    if (cmd.hasParm("count")) {
        size_t count{0};
        sql::command(this->server->getSql(), "SELECT COUNT(`client_database_id`) AS `count` FROM `clients_server` WHERE `server_id` = :sid", variable{":sid", this->server->getServerId()})
            .query([&](int, std::string* v, std::string*) {
                count = stoll(v[0]);
        });
        result.put_unchecked(0, "count", count);
    }

    this->sendCommand(result);
    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandClientDBEdit(Command &cmd) {
    CMD_REQ_SERVER;
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(5);
    ACTION_REQUIRES_GLOBAL_PERMISSION(permission::b_client_modify_dbproperties, 1);

    if (!serverInstance->databaseHelper()->validClientDatabaseId(this->server, cmd["cldbid"])) return command_result{error::database_empty_result, "invalid cldbid"};
    auto props = serverInstance->databaseHelper()->loadClientProperties(this->server, cmd["cldbid"], ClientType::CLIENT_TEAMSPEAK);

    for (auto &elm : cmd[0].keys()) {
        if (elm == "cldbid") continue;

        const auto& info = property::find<property::ClientProperties>(elm);
        if(info == property::CLIENT_UNDEFINED) {
            logError(this->getServerId(), "Client " + this->getDisplayName() + " tried to change someone's db entry, but the entry in unknown: " + elm);
            continue;
        }
        if(!info.validate_input(cmd[elm].as<string>())) {
            logError(this->getServerId(), "Client " + this->getDisplayName() + " tried to change a property to an invalid value. (Value: '" + cmd[elm].as<string>() + "', Property: '" + std::string{info.name} + "')");
            continue;
        }
        (*props)[info] = cmd[elm].string();
    }

    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandClientEdit(ts::Command &cmd) {
    CMD_REQ_SERVER;

    ConnectedLockedClient client{this->server->find_client_by_id(cmd["clid"].as<ClientId>())};
    if (!client) return command_result{error::client_invalid_id};
    return this->handleCommandClientEdit(cmd, client.client);
}

command_result ConnectedClient::handleCommandClientEdit(Command &cmd, const std::shared_ptr<ConnectedClient>& client) {
    assert(client);
    auto self = client == this;
    CMD_CHK_AND_INC_FLOOD_POINTS(self ? 15 : 25);
    CMD_RESET_IDLE;


    bool update_talk_rights = false;
    unique_ptr<lock_guard<std::recursive_mutex>> nickname_lock;
    std::deque<std::pair<const property::PropertyDescription*, std::string>> keys;
    for(const auto& key : cmd[0].keys()) {
        if(key == "return_code") {
            continue;
        }

        if(key == "clid") {
            continue;
        }

        const auto &info = property::find<property::ClientProperties>(key);
        if(info == property::CLIENT_UNDEFINED) {
            logError(this->getServerId(), R"([{}] Tried to change a not existing client property for {}. (Key: "{}", Value: "{}"))", CLIENT_STR_LOG_PREFIX, CLIENT_STR_LOG_PREFIX_(client), key, cmd[key].string());
            continue;
        }

        if((info.flags & property::FLAG_USER_EDITABLE) == 0) {
            logError(this->getServerId(), R"([{}] Tried to change a not user editable client property for {}. (Key: "{}", Value: "{}"))", CLIENT_STR_LOG_PREFIX, CLIENT_STR_LOG_PREFIX_(client), key, cmd[key].string());
            continue;
        }

        if(!info.validate_input(cmd[key].as<string>())) {
            logError(this->getServerId(), R"([{}] Tried to change a client property to an invalid value for {}. (Key: "{}", Value: "{}"))", CLIENT_STR_LOG_PREFIX, CLIENT_STR_LOG_PREFIX_(client), key, cmd[key].string());
            continue;
        }

        if(client->properties()[&info].as_unchecked<string>() == cmd[key].as<string>()) {
            continue;
        }

        if (info == property::CLIENT_DESCRIPTION) {
            if (self) {
                ACTION_REQUIRES_PERMISSION(permission::b_client_modify_own_description, 1, client->getChannelId());
            } else if(client->getType() == ClientType::CLIENT_MUSIC) {
                if(client->properties()[property::CLIENT_OWNER] != this->getClientDatabaseId()) {
                    ACTION_REQUIRES_PERMISSION(permission::i_client_music_modify_power, client->calculate_permission(permission::i_client_music_needed_modify_power, client->getChannelId()), client->getChannelId());
                }
            } else {
                ACTION_REQUIRES_PERMISSION(permission::b_client_modify_description, 1, client->getChannelId());
            }

            string value = cmd["client_description"].string();
            auto value_length = utf8::count_characters(value);
            if (value_length < 0 || value_length > 200) {
                return command_result{error::parameter_invalid, "Invalid description length. A maximum of 200 characters is allowed!"};
            }
        } else if (info == property::CLIENT_IS_TALKER) {
            ACTION_REQUIRES_PERMISSION(permission::b_client_set_flag_talker, 1, client->getChannelId());
            cmd["client_is_talker"] = cmd["client_is_talker"].as<bool>();
            cmd["client_talk_request"] = 0;
            update_talk_rights = true;

            keys.emplace_back(&property::describe(property::CLIENT_IS_TALKER), "client_is_talker");
            keys.emplace_back(&property::describe(property::CLIENT_TALK_REQUEST), "client_talk_request");
            continue;
        } else if(info == property::CLIENT_NICKNAME) {
            if(!self) {
                if(client->getType() != ClientType::CLIENT_MUSIC) return command_result{error::client_invalid_type};
                if(client->properties()[property::CLIENT_OWNER] != this->getClientDatabaseId()) {
                    ACTION_REQUIRES_PERMISSION(permission::i_client_music_rename_power, client->calculate_permission(permission::i_client_music_needed_rename_power, client->getChannelId()), client->getChannelId());
                }
            }

            string name = cmd["client_nickname"].string();
            auto name_length = utf8::count_characters(name);
            if (name_length < 3) {
                return command_result{error::parameter_invalid, "Invalid name length. A minimum of 3 characters is required!"};
            }
            if (name_length > 30) {
                return command_result{error::parameter_invalid, "Invalid name length. A maximum of 30 characters is allowed!"};
            }

            if (!permission::v2::permission_granted(1, this->calculate_permission(permission::b_client_ignore_bans, client->getClientId()))) {
                auto banRecord = serverInstance->banManager()->findBanByName(this->getServerId(), name);
                if (banRecord)
                    return command_result{error::client_nickname_inuse, string() + "This nickname is " + (banRecord->serverId == 0 ? "globally " : "") + "banned for the reason: " + banRecord->reason};
            }
            if (this->server) {
                nickname_lock = std::make_unique<lock_guard<recursive_mutex>>(this->server->client_nickname_lock);
                bool self = false;
                for (const auto &cl : this->server->getClients()) {
                    if (cl->getDisplayName() == cmd["client_nickname"].string()) {
                        if(cl == this)
                            self = true;
                        else
                            return command_result{error::client_nickname_inuse, "This nickname is already in use"};
                    }
                }
                if(self) {
                    nickname_lock.reset();
                    continue;
                }
            }
        } else if(info == property::CLIENT_NICKNAME_PHONETIC) {
            if(!self) {
                if(client->getType() != ClientType::CLIENT_MUSIC) return command_result{error::client_invalid_type};
                if(client->properties()[property::CLIENT_OWNER] != this->getClientDatabaseId()) {
                    ACTION_REQUIRES_PERMISSION(permission::i_client_music_rename_power, client->calculate_permission(permission::i_client_music_needed_rename_power, client->getChannelId()), client->getChannelId());
                }
            }

            string name = cmd["client_nickname_phonetic"].string();
            auto name_length = utf8::count_characters(name);
            if (name_length < 0 || name_length > 30) {
                return command_result{error::parameter_invalid, "Invalid name length. A maximum of 30 characters is allowed!"};
            }
        } else if(info == property::CLIENT_PLAYER_VOLUME) {
            if(client->getType() != ClientType::CLIENT_MUSIC) return command_result{error::client_invalid_type};
            if(client->properties()[property::CLIENT_OWNER] != this->getClientDatabaseId()) {
                ACTION_REQUIRES_PERMISSION(permission::i_client_music_modify_power, client->calculate_permission(permission::i_client_music_needed_modify_power, client->getChannelId()), client->getChannelId());
            }
            auto bot = dynamic_pointer_cast<MusicClient>(client);
            assert(bot);

            auto volume = cmd["player_volume"].as<float>();

            auto max_volume = this->calculate_permission(permission::i_client_music_create_modify_max_volume, client->getClientId());
            if(max_volume.has_value && !permission::v2::permission_granted(volume * 100, max_volume))
                return command_result{permission::i_client_music_create_modify_max_volume};

            bot->volume_modifier(cmd["player_volume"]);
        } else if(info == property::CLIENT_IS_CHANNEL_COMMANDER) {
            if(!self) {
                if(client->getType() != ClientType::CLIENT_MUSIC) return command_result{error::client_invalid_type};
                if(client->properties()[property::CLIENT_OWNER] != this->getClientDatabaseId()) {
                    ACTION_REQUIRES_PERMISSION(permission::i_client_music_modify_power, client->calculate_permission(permission::i_client_music_needed_modify_power, client->getChannelId()), client->getChannelId());
                }
            }

            if(cmd["client_is_channel_commander"].as<bool>())
                ACTION_REQUIRES_PERMISSION(permission::b_client_use_channel_commander, 1, client->getChannelId());
        } else if(info == property::CLIENT_IS_PRIORITY_SPEAKER) {
            //FIXME allow other to remove this thing
            if(!self) {
                if(client->getType() != ClientType::CLIENT_MUSIC)
                    return command_result{error::client_invalid_type};
                if(client->properties()[property::CLIENT_OWNER] != this->getClientDatabaseId())
                    ACTION_REQUIRES_PERMISSION(permission::i_client_music_modify_power, client->calculate_permission(permission::i_client_music_needed_modify_power, client->getClientId()), client->getClientId());
            }

            if(cmd["client_is_priority_speaker"].as<bool>())
                ACTION_REQUIRES_PERMISSION(permission::b_client_use_priority_speaker, 1, client->getChannelId());
        } else if (self && key == "client_talk_request") {
            CMD_CHK_AND_INC_FLOOD_POINTS(20);
            ACTION_REQUIRES_PERMISSION(permission::b_client_request_talker, 1, client->getChannelId());

            if (cmd["client_talk_request"].as<bool>())
                cmd["client_talk_request"] = duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
            else
                cmd["client_talk_request"] = 0;
            keys.emplace_back(&property::describe(property::CLIENT_TALK_REQUEST), "client_talk_request");
            continue;
        } else if (self && key == "client_badges") {
            std::string str = cmd[key];
            size_t index = 0;
            int badgesTags = 0;
            do {
                index = str.find("badges", index);
                if (index < str.length()) badgesTags++;
                index++;
            } while (index < str.length() && index != 0);
            if (badgesTags >= 3) {
                if (!permission::v2::permission_granted(1, this->calculate_permission(permission::b_client_allow_invalid_badges, client->getClientId()))) {
                    ((VoiceClient *) this)->disconnect(VREASON_SERVER_KICK, config::messages::kick_invalid_badges, this->server ? this->server->serverAdmin : dynamic_pointer_cast<ConnectedClient>(serverInstance->getInitialServerAdmin()), true);
                }
            }
            continue;
        } else if(!self && key == "client_version") {
            if(client->getType() != ClientType::CLIENT_MUSIC) return command_result{error::client_invalid_type};
            if(client->properties()[property::CLIENT_OWNER] != this->getClientDatabaseId()) {
                ACTION_REQUIRES_PERMISSION(permission::i_client_music_modify_power, client->calculate_permission(permission::i_client_music_needed_modify_power, client->getChannelId()), client->getChannelId());
            }
        } else if(!self && key == "client_platform") {
            if(client->getType() != ClientType::CLIENT_MUSIC) return command_result{error::client_invalid_type};
            if(client->properties()[property::CLIENT_OWNER] != this->getClientDatabaseId()) {
                ACTION_REQUIRES_PERMISSION(permission::i_client_music_modify_power, client->calculate_permission(permission::i_client_music_needed_modify_power, client->getChannelId()), client->getChannelId());
            }
        } else if(!self && key == "client_country") {
            if(client->getType() != ClientType::CLIENT_MUSIC) return command_result{error::client_invalid_type};
            if(client->properties()[property::CLIENT_OWNER] != this->getClientDatabaseId()) {
                ACTION_REQUIRES_PERMISSION(permission::i_client_music_modify_power, client->calculate_permission(permission::i_client_music_needed_modify_power, client->getChannelId()), client->getChannelId());
            }
        } else if(!self && (info == property::CLIENT_FLAG_NOTIFY_SONG_CHANGE/* || info == property::CLIENT_NOTIFY_SONG_MESSAGE*/)) {
            if(client->getType() != ClientType::CLIENT_MUSIC) return command_result{error::client_invalid_type};
            if(client->properties()[property::CLIENT_OWNER] != this->getClientDatabaseId()) {
                ACTION_REQUIRES_PERMISSION(permission::i_client_music_modify_power, client->calculate_permission(permission::i_client_music_needed_modify_power, client->getChannelId()), client->getChannelId());
            }
        } else if(!self && key == "client_uptime_mode") {
            if(client->getType() != ClientType::CLIENT_MUSIC) return command_result{error::client_invalid_type};
            if(client->properties()[property::CLIENT_OWNER] != this->getClientDatabaseId()) {
                ACTION_REQUIRES_PERMISSION(permission::i_client_music_modify_power, client->calculate_permission(permission::i_client_music_needed_modify_power, client->getChannelId()), client->getChannelId());
            }

            if(cmd[key].as<MusicClient::UptimeMode::value>() == MusicClient::UptimeMode::TIME_SINCE_SERVER_START) {
                cmd["client_lastconnected"] = duration_cast<seconds>(this->server->startTimestamp.time_since_epoch()).count();
            } else {
                string value = client->properties()[property::CLIENT_CREATED];
                if(value.empty())
                    value = "0";
                cmd["client_lastconnected"] = value;
            }

            keys.emplace_back(&property::describe(property::CLIENT_LASTCONNECTED), "client_lastconnected");
        } else if(!self && info == property::CLIENT_BOT_TYPE) {
            ACTION_REQUIRES_PERMISSION(permission::i_client_music_modify_power, client->calculate_permission(permission::i_client_music_needed_modify_power, client->getChannelId()), client->getChannelId());
            auto type = cmd["client_bot_type"].as<MusicClient::Type::value>();
            if(type == MusicClient::Type::TEMPORARY) {
                ACTION_REQUIRES_PERMISSION(permission::b_client_music_modify_temporary, 1, client->getChannelId());
            } else if(type == MusicClient::Type::SEMI_PERMANENT) {
                ACTION_REQUIRES_PERMISSION(permission::b_client_music_modify_semi_permanent, 1, client->getChannelId());
            } else if(type == MusicClient::Type::PERMANENT) {
                ACTION_REQUIRES_PERMISSION(permission::b_client_music_modify_permanent, 1, client->getChannelId());
            } else
                return command_result{error::parameter_invalid};
        } else if(info == property::CLIENT_AWAY_MESSAGE) {
            if(!self) continue;

            if(cmd["client_away_message"].string().length() > ts::config::server::limits::afk_message_length)
                return command_result{error::parameter_invalid};
        } else if(!self) { /* dont edit random properties of other clients. For us self its allowed to edit the rest without permissions */
            continue;
        } else if(info == property::CLIENT_TALK_REQUEST_MSG) {
            if(cmd["client_talk_request_msg"].string().length() > ts::config::server::limits::talk_power_request_message_length)
                return command_result{error::parameter_invalid};
        }

        keys.emplace_back(&info, key);
    }

    deque<const property::PropertyDescription*> updates;
    for(const auto& key : keys) {
        if(*key.first == property::CLIENT_IS_PRIORITY_SPEAKER) {
            client->clientPermissions->set_permission(permission::b_client_is_priority_speaker, {1, 0}, cmd["client_is_priority_speaker"].as<bool>() ? permission::v2::PermissionUpdateType::set_value : permission::v2::PermissionUpdateType::delete_value, permission::v2::PermissionUpdateType::do_nothing);
        }

        auto property = client->properties()[key.first];
        auto old_value = property.value();
        auto new_value = cmd[0][key.second].value();
        if(old_value == new_value)
            continue;

        property = new_value;
        updates.push_back(key.first);

        serverInstance->action_logger()->client_edit_logger.log_client_edit(
                this->getServerId(),
                this->ref(),
                client,
                *key.first,
                old_value,
                new_value
        );
    }
    if(update_talk_rights) {
        client->updateTalkRights(client->calculate_permission(permission::i_client_talk_power, client->getChannelId()));
    }

    if(this->server) {
        this->server->notifyClientPropertyUpdates(client, updates);
    }

    nickname_lock.reset();
    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandClientUpdate(Command &cmd) {
    return this->handleCommandClientEdit(cmd, this->ref());
}

command_result ConnectedClient::handleCommandClientMute(Command &cmd) {
    CMD_REQ_SERVER;
    CMD_RESET_IDLE;

    ConnectedLockedClient client{this->server->find_client_by_id(cmd["clid"].as<ClientId>())};
    if (!client || client->getClientId() == this->getClientId()) return command_result{error::client_invalid_id};

    {
        unique_lock channel_lock(this->channel_tree_mutex);
        for(const auto& weak : this->mutedClients)
            if(weak.lock() == client) return command_result{error::ok};
        this->mutedClients.push_back(client.client);
    }

    if (config::voice::notifyMuted)
        client->notifyTextMessage(ChatMessageMode::TEXTMODE_PRIVATE, this->ref(), client->getClientId(), 0, system_clock::now(), config::messages::mute_notify_message);

    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandClientUnmute(Command &cmd) {
    CMD_REQ_SERVER;
    CMD_RESET_IDLE;

    ConnectedLockedClient client{this->server->find_client_by_id(cmd["clid"].as<ClientId>())};
    if (!client || client->getClientId() == this->getClientId()) return command_result{error::client_invalid_id};

    {
        unique_lock channel_lock(this->channel_tree_mutex);
        this->mutedClients.erase(std::remove_if(this->mutedClients.begin(), this->mutedClients.end(), [&](const weak_ptr<ConnectedClient>& weak) {
            auto c = weak.lock();
            return !c || c  == client;
        }), this->mutedClients.end());
    }

    if (config::voice::notifyMuted)
        client->notifyTextMessage(ChatMessageMode::TEXTMODE_PRIVATE, this->ref(), client->getClientId(), 0, system_clock::now(), config::messages::unmute_notify_message);

    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandClientList(Command &cmd) {
    CMD_REQ_SERVER;
    CMD_RESET_IDLE;

    bool allow_ip = false;
    if (cmd.hasParm("ip"))
        allow_ip = permission::v2::permission_granted(1, this->calculate_permission(permission::b_client_remoteaddress_view, 0));
    Command result("");

    int index = 0;
    this->server->forEachClient([&](shared_ptr<ConnectedClient> client) {
        if (client->getType() == ClientType::CLIENT_INTERNAL) return;

        result[index]["clid"] = client->getClientId();
        if (client->getChannel())
            result[index]["cid"] = client->getChannel()->channelId();
        else result[index]["cid"] = 0;
        result[index]["client_database_id"] = client->getClientDatabaseId();
        result[index]["client_nickname"] = client->getDisplayName();
        result[index]["client_type"] = client->getType();

        if (cmd.hasParm("uid"))
            result[index]["client_unique_identifier"] = client->getUid();
        if (cmd.hasParm("away")) {
            result[index]["client_away"] = client->properties()[property::CLIENT_AWAY].as_unchecked<string>();
            result[index]["client_away_message"] = client->properties()[property::CLIENT_AWAY_MESSAGE].as_unchecked<string>();
        }
        if (cmd.hasParm("groups")) {
            result[index]["client_channel_group_id"] = client->properties()[property::CLIENT_CHANNEL_GROUP_ID].as_unchecked<string>();
            result[index]["client_servergroups"] = client->properties()[property::CLIENT_SERVERGROUPS].as_unchecked<string>();
            result[index]["client_channel_group_inherited_channel_id"] = client->properties()[property::CLIENT_CHANNEL_GROUP_INHERITED_CHANNEL_ID].as_unchecked<string>();
        }
        if (cmd.hasParm("times")) {
            result[index]["client_idle_time"] = duration_cast<milliseconds>(system_clock::now() - client->idleTimestamp).count();
            result[index]["client_total_online_time"] =
                    client->properties()[property::CLIENT_TOTAL_ONLINE_TIME].as_unchecked<int64_t>() + duration_cast<seconds>(system_clock::now() - client->lastOnlineTimestamp).count();
            result[index]["client_month_online_time"] =
                    client->properties()[property::CLIENT_MONTH_ONLINE_TIME].as_unchecked<int64_t>() + duration_cast<seconds>(system_clock::now() - client->lastOnlineTimestamp).count();
            result[index]["client_idle_time"] = duration_cast<milliseconds>(system_clock::now() - client->idleTimestamp).count();
            result[index]["client_created"] = client->properties()[property::CLIENT_CREATED].as_unchecked<string>();
            result[index]["client_lastconnected"] = client->properties()[property::CLIENT_LASTCONNECTED].as_unchecked<string>();
        }
        if (cmd.hasParm("info")) {
            result[index]["client_version"] = client->properties()[property::CLIENT_VERSION].as_unchecked<string>();
            result[index]["client_platform"] = client->properties()[property::CLIENT_PLATFORM].as_unchecked<string>();
        }

        if (cmd.hasParm("badges"))
            result[index]["client_badges"] = client->properties()[property::CLIENT_BADGES].as_unchecked<string>();
        if (cmd.hasParm("country"))
            result[index]["client_country"] = client->properties()[property::CLIENT_COUNTRY].as_unchecked<string>();
        if (cmd.hasParm("ip"))
            result[index]["connection_client_ip"] = allow_ip ? client->getPeerIp() : "hidden";
        if (cmd.hasParm("icon"))
            result[index]["client_icon_id"] = client->properties()[property::CLIENT_ICON_ID].as_unchecked<string>();

        if (cmd.hasParm("voice")) {
            result[index]["client_talk_power"] = client->properties()[property::CLIENT_TALK_POWER].as_unchecked<string>();
            result[index]["client_flag_talking"] = client->properties()[property::CLIENT_FLAG_TALKING].as_unchecked<string>();
            result[index]["client_input_muted"] = client->properties()[property::CLIENT_INPUT_MUTED].as_unchecked<string>();
            result[index]["client_output_muted"] = client->properties()[property::CLIENT_OUTPUT_MUTED].as_unchecked<string>();
            result[index]["client_input_hardware"] = client->properties()[property::CLIENT_INPUT_HARDWARE].as_unchecked<string>();
            result[index]["client_output_hardware"] = client->properties()[property::CLIENT_OUTPUT_HARDWARE].as_unchecked<string>();
            result[index]["client_is_talker"] = client->properties()[property::CLIENT_IS_TALKER].as_unchecked<string>();
            result[index]["client_is_priority_speaker"] = client->properties()[property::CLIENT_IS_PRIORITY_SPEAKER].as_unchecked<string>();
            result[index]["client_is_recording"] = client->properties()[property::CLIENT_IS_RECORDING].as_unchecked<string>();
            result[index]["client_is_channel_commander"] = client->properties()[property::CLIENT_IS_CHANNEL_COMMANDER].as_unchecked<string>();

        }
        index++;
    });
    this->sendCommand(result);
    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandClientGetDBIDfromUID(Command &cmd) {
    CMD_REQ_SERVER;
    CMD_RESET_IDLE;

    deque<string> unique_ids;
    for(int index = 0; index < cmd.bulkCount(); index++) {
        unique_ids.push_back(cmd[index]["cluid"]);
    }

    auto res = serverInstance->databaseHelper()->queryDatabaseInfoByUid(this->server, unique_ids);
    if (res.empty()) return command_result{error::database_empty_result};

    Command result(this->getExternalType() == CLIENT_TEAMSPEAK ? "notifyclientdbidfromuid" : "");
    int result_index = 0;
    for(auto& info : res) {
        result[result_index]["cluid"] = info->client_unique_id;
        result[result_index]["cldbid"] = info->client_database_id;
        result_index++;
    }
    this->sendCommand(result);
    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandClientGetNameFromDBID(Command &cmd) {
    CMD_REQ_SERVER;
    CMD_RESET_IDLE;

    deque<ClientDbId> dbids;
    for(int index = 0; index < cmd.bulkCount(); index++)
        dbids.push_back(cmd[index]["cldbid"].as<ClientDbId>());

    auto res = serverInstance->databaseHelper()->queryDatabaseInfo(this->server, dbids);
    if (res.empty()) return command_result{error::database_empty_result};

    Command result(this->getExternalType() == CLIENT_TEAMSPEAK ? "notifyclientgetnamefromdbid" : "");
    int result_index = 0;
    for(auto& info : res) {
        result[result_index]["cluid"] = info->client_unique_id;
        result[result_index]["cldbid"] = info->client_database_id;
        result[result_index]["name"] = info->client_nickname;
        result[result_index]["clname"] = info->client_nickname;
        result_index++;
    }
    this->sendCommand(result);
    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandClientGetNameFromUid(Command &cmd) {
    CMD_REQ_SERVER;
    CMD_RESET_IDLE;

    deque<string> unique_ids;
    for(int index = 0; index < cmd.bulkCount(); index++)
        unique_ids.push_back(cmd[index]["cluid"].as<string>());

    auto res = serverInstance->databaseHelper()->queryDatabaseInfoByUid(this->server, unique_ids);
    if (res.empty()) return command_result{error::database_empty_result};

    Command result(this->getExternalType() == CLIENT_TEAMSPEAK ? "notifyclientnamefromuid" : "");
    int result_index = 0;
    for(auto& info : res) {
        result[result_index]["cluid"] = info->client_unique_id;
        result[result_index]["cldbid"] = info->client_database_id;
        result[result_index]["name"] = info->client_nickname;
        result[result_index]["clname"] = info->client_nickname;
        result_index++;
    }
    this->sendCommand(result);
    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandClientGetUidFromClid(Command &cmd) {
    CMD_REQ_SERVER;
    CMD_RESET_IDLE;

    bool error = false;
    bool found = false;
    auto client_list = this->server->getClients();

    Command notify(this->getExternalType() == CLIENT_TEAMSPEAK ? "notifyclientuidfromclid" : "");
    int result_index = 0;

    for(int index = 0; index < cmd.bulkCount(); index++) {
        auto client_id = cmd[index]["clid"].as<ClientId>();
        for(const auto& entry : client_list) {
            if(entry->getClientId() == client_id) {
                notify[result_index]["clname"] = entry->getDisplayName();
                notify[result_index]["clid"] = entry->getClientId();
                notify[result_index]["cluid"] = entry->getUid();
                notify[result_index]["cldbid"] = entry->getClientDatabaseId();
                result_index++;
                found = true;
            }
        }
        if(found) found = false;
        else error = false;
    }

    if(result_index > 0)
        this->sendCommand(notify);
    if(error)
        return command_result{error::database_empty_result};
    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandClientAddPerm(Command &cmd) {
    CMD_REQ_SERVER;
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(5);

    auto cldbid = cmd["cldbid"].as<ClientDbId>();
    if(!serverInstance->databaseHelper()->validClientDatabaseId(this->server, cldbid))
        return command_result{error::client_invalid_id};
    auto mgr = serverInstance->databaseHelper()->loadClientPermissionManager(this->getServerId(), cldbid);

    ACTION_REQUIRES_GLOBAL_PERMISSION(permission::i_client_permission_modify_power, this->server->calculate_permission(permission::i_client_needed_permission_modify_power, cldbid, ClientType::CLIENT_TEAMSPEAK, 0));

    ts::command::bulk_parser::PermissionBulksParser pparser{cmd, true};
    if(!pparser.validate(this->ref(), 0))
        return pparser.build_command_result();

    bool update_channels{false};
    for(const auto& ppermission : pparser.iterate_valid_permissions()) {
        ppermission.apply_to(mgr, permission::v2::PermissionUpdateType::set_value);
        ppermission.log_update(serverInstance->action_logger()->permission_logger,
                               this->getServerId(),
                               this->ref(),
                               log::PermissionTarget::CLIENT,
                               permission::v2::PermissionUpdateType::set_value,
                               cldbid, "",
                               0, ""
        );

        update_channels |= ppermission.is_client_view_property();
    }

    serverInstance->databaseHelper()->saveClientPermissions(this->server, cldbid, mgr);
    auto onlineClients = this->server->findClientsByCldbId(cldbid);
    if (!onlineClients.empty())
        for (const auto &elm : onlineClients) {
            elm->task_update_needed_permissions.enqueue();
            if(update_channels)
                elm->task_update_channel_client_properties.enqueue();
            elm->join_state_id++; /* join permission may changed, all channels need to be recalculate dif needed */
        }

    return pparser.build_command_result();
}

command_result ConnectedClient::handleCommandClientDelPerm(Command &cmd) {
    CMD_REQ_SERVER;
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(5);

    auto cldbid = cmd["cldbid"].as<ClientDbId>();
    if(!serverInstance->databaseHelper()->validClientDatabaseId(this->server, cldbid))
        return command_result{error::client_invalid_id};
    auto mgr = serverInstance->databaseHelper()->loadClientPermissionManager(this->getServerId(), cldbid);
    ACTION_REQUIRES_GLOBAL_PERMISSION(permission::i_client_permission_modify_power, this->server->calculate_permission(permission::i_client_needed_permission_modify_power, cldbid, ClientType::CLIENT_TEAMSPEAK, 0));

    ts::command::bulk_parser::PermissionBulksParser pparser{cmd, false};
    if(!pparser.validate(this->ref(), 0))
        return pparser.build_command_result();

    bool update_channels{false};
    for(const auto& ppermission : pparser.iterate_valid_permissions()) {
        ppermission.apply_to(mgr, permission::v2::PermissionUpdateType::delete_value);
        ppermission.log_update(serverInstance->action_logger()->permission_logger,
                               this->getServerId(),
                               this->ref(),
                               log::PermissionTarget::CLIENT,
                               permission::v2::PermissionUpdateType::delete_value,
                               cldbid, "",
                               0, ""
        );
        update_channels |= ppermission.is_client_view_property();
    }

    serverInstance->databaseHelper()->saveClientPermissions(this->server, cldbid, mgr);
    auto onlineClients = this->server->findClientsByCldbId(cldbid);
    if (!onlineClients.empty())
        for (const auto &elm : onlineClients) {
            elm->task_update_needed_permissions.enqueue();
            if(update_channels)
                elm->task_update_channel_client_properties.enqueue();
            elm->join_state_id++; /* join permission may changed, all channels need to be recalculate dif needed */
        }

    return pparser.build_command_result();
}

command_result ConnectedClient::handleCommandClientPermList(Command &cmd) {
    CMD_REQ_SERVER;
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(5);
    ACTION_REQUIRES_GLOBAL_PERMISSION(permission::b_virtualserver_client_permission_list, 1);

    if(!serverInstance->databaseHelper()->validClientDatabaseId(this->server, cmd["cldbid"])) return command_result{error::client_invalid_id};
    auto mgr = serverInstance->databaseHelper()->loadClientPermissionManager(this->getServerId(), cmd["cldbid"]);
    if (!this->notifyClientPermList(cmd["cldbid"], mgr, cmd.hasParm("permsid"))) return command_result{error::database_empty_result};
    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandClientDbInfo(Command &cmd) {
    CMD_REQ_SERVER;
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(5);
    ACTION_REQUIRES_GLOBAL_PERMISSION(permission::b_virtualserver_client_dbinfo, 1);

    std::deque<ClientDbId> cldbids;
    for(int index = 0; index < cmd.bulkCount(); index++) {
        cldbids.push_back(cmd[index]["cldbid"]);
    }

    auto basic = serverInstance->databaseHelper()->queryDatabaseInfo(this->server, cldbids);
    if (basic.empty()) {
        return command_result{error::database_empty_result};
    }

    auto allow_ip = permission::v2::permission_granted(1, this->calculate_permission(permission::b_client_remoteaddress_view, 0));
    ts::command_builder result{this->getExternalType() == ClientType::CLIENT_TEAMSPEAK ? "notifyclientdbinfo" : ""};
    result.reserve_bulks(basic.size());

    size_t index = 0;
    for(const auto& info : basic) {
        auto bulk = result.bulk(index++);
        bulk.reserve(800);

        bulk.put_unchecked("client_base64HashClientUID", hex::hex(base64::validate(info->client_unique_id) ? base64::decode(info->client_unique_id) : info->client_unique_id, 'a', 'q'));
        bulk.put_unchecked(property::CLIENT_UNIQUE_IDENTIFIER, info->client_unique_id);
        bulk.put_unchecked(property::CLIENT_NICKNAME, info->client_nickname);
        bulk.put_unchecked(property::CLIENT_DATABASE_ID, info->client_database_id);
        bulk.put_unchecked(property::CLIENT_CREATED, chrono::duration_cast<chrono::seconds>(info->client_created.time_since_epoch()).count());
        bulk.put_unchecked(property::CLIENT_LASTCONNECTED, chrono::duration_cast<chrono::seconds>(info->client_last_connected.time_since_epoch()).count());
        bulk.put_unchecked(property::CLIENT_TOTALCONNECTIONS, info->client_total_connections);
        bulk.put_unchecked(property::CLIENT_DATABASE_ID, info->client_database_id);

        auto props = serverInstance->databaseHelper()->loadClientProperties(this->server, info->client_database_id, ClientType::CLIENT_TEAMSPEAK);
        if(allow_ip) {
            bulk.put_unchecked("client_lastip", info->client_ip);
        } else {
            bulk.put_unchecked("client_lastip", "hidden");
        }

#define ASSIGN_PROPERTY(property) \
        bulk.put_unchecked(property, (*props)[property].value());

        ASSIGN_PROPERTY(property::CLIENT_ICON_ID);
        ASSIGN_PROPERTY(property::CLIENT_BADGES);
        ASSIGN_PROPERTY(property::CLIENT_VERSION);
        ASSIGN_PROPERTY(property::CLIENT_PLATFORM);
        ASSIGN_PROPERTY(property::CLIENT_HARDWARE_ID);
        ASSIGN_PROPERTY(property::CLIENT_TOTAL_BYTES_DOWNLOADED);
        ASSIGN_PROPERTY(property::CLIENT_TOTAL_BYTES_UPLOADED);
        ASSIGN_PROPERTY(property::CLIENT_MONTH_BYTES_DOWNLOADED);
        ASSIGN_PROPERTY(property::CLIENT_MONTH_BYTES_DOWNLOADED);
        ASSIGN_PROPERTY(property::CLIENT_DESCRIPTION);
        ASSIGN_PROPERTY(property::CLIENT_FLAG_AVATAR);

        ASSIGN_PROPERTY(property::CLIENT_MONTH_ONLINE_TIME);
        ASSIGN_PROPERTY(property::CLIENT_TOTAL_ONLINE_TIME);
#undef ASSIGN_PROPERTY

        index++;
    }

    this->sendCommand(result);

    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandClientDBDelete(Command &cmd) {
    CMD_REQ_SERVER;
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(5);
    ACTION_REQUIRES_GLOBAL_PERMISSION(permission::b_client_delete_dbproperties, 1);

    ClientDbId id = cmd["cldbid"];
    if (!serverInstance->databaseHelper()->validClientDatabaseId(this->server, id)) return command_result{error::database_empty_result};
    serverInstance->databaseHelper()->deleteClient(this->server, id);

    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandClientDBFind(Command &cmd) {
    CMD_REQ_SERVER;
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(5);
    ACTION_REQUIRES_GLOBAL_PERMISSION(permission::b_virtualserver_client_dbsearch, 1);

    bool uid = cmd.hasParm("uid");
    string pattern = cmd["pattern"];

    const auto detailed = cmd.hasParm("details");
    const auto show_ip = permission::v2::permission_granted(1, this->calculate_permission(permission::b_client_remoteaddress_view, 0));

    size_t command_index{0};
    ts::command_builder result{this->notify_response_command("notifyclientdbfind")};
    result.reserve_bulks(50);

    constexpr static auto kBaseCommand{"SELECT `client_database_id`, `client_unique_id`, `client_nickname`, `client_ip`, `client_last_connected`, `client_total_connections` FROM `clients_server` WHERE "};

    auto sql_result = sql::command{this->sql, std::string{kBaseCommand} + "`server_id` = :sid AND " + (uid ? "`client_unique_id`" : "`client_nickname`") + " LIKE :pattern LIMIT 50", variable{":sid", this->getServerId()}, variable{":pattern", pattern}}
        .query([&](int length, std::string* values, std::string* names) {
            auto bulk = result.bulk(command_index++);
            bulk.reserve(300);

            auto index{0};
            ClientDbId client_database_id;
            try {
                assert(names[index] == "client_database_id");
                client_database_id = std::stoull(values[index]);
                bulk.put_unchecked("cldbid", values[index++]);

                assert(names[index] == "client_unique_id");
                bulk.put_unchecked("client_unique_identifier", values[index++]);

                assert(names[index] == "client_nickname");
                bulk.put_unchecked("client_nickname", values[index++]);

                assert(names[index] == "client_ip");
                if(detailed) {
                    bulk.put_unchecked("client_lastip", show_ip ? values[index++] : "hidden");
                } else {
                    index++;
                }

                assert(names[index] == "client_last_connected");
                bulk.put_unchecked("client_lastconnected", values[index++]);

                assert(names[index] == "client_total_connections");
                bulk.put_unchecked("client_totalconnections", values[index++]);

                assert(index == length);
            } catch (std::exception& ex) {
                command_index--;
                logError(this->getServerId(), "Failed to parse client base properties at index {}: {}. Search pattern: {}",
                         index - 1,
                         ex.what(),
                         pattern
                );
                return;
            }

            if(detailed) {
                auto props = serverInstance->databaseHelper()->loadClientProperties(this->server, client_database_id, ClientType::CLIENT_TEAMSPEAK);
                if (props) {
                    auto& properties = *props;
                    bulk.put_unchecked("client_badges", properties[property::CLIENT_BADGES].as_unchecked<std::string>());
                    bulk.put_unchecked("client_version",
                                       properties[property::CLIENT_VERSION].as_unchecked<std::string>());
                    bulk.put_unchecked("client_platform",
                                       properties[property::CLIENT_PLATFORM].as_unchecked<std::string>());
                    bulk.put_unchecked("client_hwid",
                                       properties[property::CLIENT_HARDWARE_ID].as_unchecked<std::string>());
                }
            }
    });

    if(command_index == 0)
        return command_result{error::database_empty_result};

    this->sendCommand(result);
    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandClientInfo(Command &cmd) {
    CMD_REQ_SERVER;
    CMD_RESET_IDLE;

    ts::command_result_bulk result{};
    ts::command_builder notify{this->notify_response_command("notifyclientinfo")};
    bool view_remote = permission::v2::permission_granted(1, this->calculate_permission(permission::b_client_remoteaddress_view, 0));

    int result_index = 0;
    for(int index{0}; index < cmd.bulkCount(); index++) {
        auto client_id = cmd[index]["clid"].as<ClientId>();
        if(client_id == 0) {
            result.emplace_result(error::client_invalid_id);
            continue;
        }

        ConnectedLockedClient client{this->server->find_client_by_id(client_id)};
        if(!client) {
            result.emplace_result(error::client_invalid_id);
            continue;
        }

        auto notify_bulk = notify.bulk(index);
        for (const auto &key : client->properties()->list_properties(property::FLAG_CLIENT_VIEW | property::FLAG_CLIENT_VARIABLE | property::FLAG_CLIENT_INFO, this->getType() == CLIENT_TEAMSPEAK ? property::FLAG_NEW : (uint16_t) 0)) {
            notify_bulk.put_unchecked(key.type().name, key.value());
        }

        notify_bulk.put_unchecked("cid", client->getChannelId());
        if(view_remote) {
            notify_bulk.put_unchecked(property::CONNECTION_CLIENT_IP, client->getPeerIp());
        } else {
            notify_bulk.put_unchecked(property::CONNECTION_CLIENT_IP, "hidden");
        }
        notify_bulk.put_unchecked(property::CONNECTION_CONNECTED_TIME, duration_cast<milliseconds>(system_clock::now() - client->connectTimestamp).count());

        auto total_stats = this->getConnectionStatistics()->total_stats();
        notify_bulk.put_unchecked(property::CONNECTION_PACKETS_SENT_TOTAL, std::accumulate(total_stats.connection_packets_sent.begin(), total_stats.connection_packets_sent.end(), (size_t) 0U));
        notify_bulk.put_unchecked(property::CONNECTION_BYTES_SENT_TOTAL, std::accumulate(total_stats.connection_packets_sent.begin(), total_stats.connection_packets_sent.end(), (size_t) 0U));
        notify_bulk.put_unchecked(property::CONNECTION_PACKETS_RECEIVED_TOTAL, std::accumulate(total_stats.connection_packets_received.begin(), total_stats.connection_packets_received.end(), (size_t) 0U));
        notify_bulk.put_unchecked(property::CONNECTION_BYTES_RECEIVED_TOTAL, std::accumulate(total_stats.connection_bytes_received.begin(), total_stats.connection_bytes_received.end(), (size_t) 0U));

        auto report_second = this->getConnectionStatistics()->second_stats();
        auto report_minute = this->getConnectionStatistics()->minute_stats();
        notify_bulk.put_unchecked(property::CONNECTION_BANDWIDTH_SENT_LAST_SECOND_TOTAL, std::accumulate(report_second.connection_bytes_sent.begin(), report_second.connection_bytes_sent.end(), (size_t) 0U));
        notify_bulk.put_unchecked(property::CONNECTION_BANDWIDTH_SENT_LAST_MINUTE_TOTAL, std::accumulate(report_minute.connection_bytes_sent.begin(), report_minute.connection_bytes_sent.end(), (size_t) 0U));
        notify_bulk.put_unchecked(property::CONNECTION_BANDWIDTH_RECEIVED_LAST_SECOND_TOTAL, std::accumulate(report_second.connection_bytes_received.begin(), report_second.connection_bytes_received.end(), (size_t) 0U));
        notify_bulk.put_unchecked(property::CONNECTION_BANDWIDTH_RECEIVED_LAST_MINUTE_TOTAL, std::accumulate(report_minute.connection_bytes_received.begin(), report_minute.connection_bytes_received.end(), (size_t) 0U));

        notify_bulk.put_unchecked(property::CONNECTION_FILETRANSFER_BANDWIDTH_SENT, report_second.file_bytes_sent);
        notify_bulk.put_unchecked(property::CONNECTION_FILETRANSFER_BANDWIDTH_RECEIVED, report_second.file_bytes_received);
        notify_bulk.put_unchecked(property::CONNECTION_FILETRANSFER_BYTES_SENT_TOTAL, total_stats.file_bytes_sent);
        notify_bulk.put_unchecked(property::CONNECTION_FILETRANSFER_BYTES_RECEIVED_TOTAL, total_stats.file_bytes_received);

        float server2client_packetloss{0};
        float client2server_packetloss{0}; /* TODO: Parse from the client connect parameters? */
        if(auto vc = dynamic_pointer_cast<VoiceClient>(this->ref()); vc) {
            server2client_packetloss = vc->current_packet_loss();
        }

        if(auto data{this->connection_info.data}; data) {
            try {
                client2server_packetloss = std::stof(data->properties["connection_server2client_packetloss_total"]);
            } catch(std::exception&) {}
        }

        notify_bulk.put_unchecked(property::CONNECTION_PACKETLOSS_TOTAL, (server2client_packetloss + client2server_packetloss) / 2);
        notify_bulk.put_unchecked(property::CONNECTION_SERVER2CLIENT_PACKETLOSS_TOTAL, server2client_packetloss);
        notify_bulk.put_unchecked(property::CONNECTION_CLIENT2SERVER_PACKETLOSS_TOTAL, client2server_packetloss);

        /* TODO: Is this really right? It might be property::CONNECTION_IDLE_TIME. It might also be that CONNECTION_IDLE_TIME should be client_idle_time */
        notify_bulk.put_unchecked("client_idle_time", duration_cast<milliseconds>(system_clock::now() - client->idleTimestamp).count());
        result_index++;

        result.emplace_result(error::ok);
    }


    if(result_index > 0) {
        this->sendCommand(notify);
    }

    return ts::command_result{std::move(result)};
}

command_result ConnectedClient::handleCommandClientFind(Command &cmd) {
    CMD_REQ_SERVER;
    CMD_CHK_AND_INC_FLOOD_POINTS(5);

    string pattern = cmd["pattern"];
    std::transform(pattern.begin(), pattern.end(), pattern.begin(), ::tolower);

    Command res("");
    int index = 0;
    for (const auto &cl : this->server->getClients()) {
        string name = cl->getDisplayName();
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);
        if (name.find(pattern) != std::string::npos) {
            res[index]["clid"] = cl->getClientId();
            res[index]["client_nickname"] = cl->getDisplayName();
            index++;
        }
    }
    if (index == 0) return command_result{error::database_empty_result};
    this->sendCommand(res);

    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandClientSetServerQueryLogin(Command &cmd) {
    ACTION_REQUIRES_GLOBAL_PERMISSION(permission::b_client_create_modify_serverquery_login, 1);

    if(!cmd[0].has("client_login_password")) cmd["client_login_password"] = "";

    std::string password = cmd["client_login_password"];
    if(password.empty())
        password = rnd_string(QUERY_PASSWORD_LENGTH);

    auto old = serverInstance->getQueryServer()->find_query_account_by_name(cmd["client_login_name"]);
    if (old) {
        if(old->unique_id == this->getUid()) {
            serverInstance->getQueryServer()->change_query_password(old, password);
        } else {
            return command_result{error::client_not_logged_in};
        }
    } else {
        serverInstance->getQueryServer()->create_query_account(cmd["client_login_name"], this->getServerId(), this->getUid(), password);
    }

    Command res(this->notify_response_command("notifyclientserverqueryloginpassword"));
    res["client_login_password"] = password;
    this->sendCommand(res);

    return command_result{error::ok};
}







