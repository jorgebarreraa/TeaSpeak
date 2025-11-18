//
// Created by wolverindev on 26.01.20.
//

#include <memory>

#include <spdlog/sinks/rotating_file_sink.h>

#include <iostream>
#include <bitset>
#include <algorithm>
#include <openssl/sha.h>
#include "../../build.h"
#include "../ConnectedClient.h"
#include "../InternalClient.h"
#include "../../server/VoiceServer.h"
#include "../voice/VoiceClient.h"
#include "PermissionManager.h"
#include "../../InstanceHandler.h"
#include "../../server/QueryServer.h"
#include "../music/MusicClient.h"
#include "../query/QueryClient.h"
#include "../../manager/ConversationManager.h"
#include "../../manager/PermissionNameMapper.h"
#include <experimental/filesystem>
#include <cstdint>
#include <StringVariable.h>

#include "helpers.h"
#include "./bulk_parsers.h"

#include <Properties.h>
#include <log/LogUtils.h>
#include <misc/sassert.h>
#include <misc/base64.h>
#include <misc/hex.h>
#include <misc/digest.h>
#include <misc/rnd.h>
#include <misc/timer.h>
#include <misc/strobf.h>
#include <misc/scope_guard.h>
#include <bbcode/bbcodes.h>
#include <src/music/MusicPlaylist.h>

namespace fs = std::experimental::filesystem;
using namespace std::chrono;
using namespace std;
using namespace ts;
using namespace ts::server;

command_result ConnectedClient::handleCommandMusicBotCreate(Command& cmd) {
    if(!config::music::enabled) return command_result{error::music_disabled};

    CMD_REQ_SERVER;
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(25);

    if(this->server->music_manager_->max_bots() != -1 && this->server->music_manager_->max_bots() <= this->server->music_manager_->current_bot_count()){
        if(config::license->isPremium())
            return command_result{error::music_limit_reached};
        else
            return command_result{error::music_limit_reached, strobf("You reached the server music bot limit. You could increase this limit by extend your server with a premium license.").string()};
    }


    auto permissions_list = this->calculate_permissions({
                                                    permission::i_client_music_limit,
                                                    permission::b_client_music_create_permanent,
                                                    permission::b_client_music_create_semi_permanent,
                                                    permission::b_client_music_create_temporary,
                                                    permission::i_channel_join_power,
                                                    permission::i_client_music_delete_power,
                                                    permission::i_client_music_create_modify_max_volume
                                            }, this->getChannelId());

    auto permissions = std::map<permission::PermissionType, permission::v2::PermissionFlaggedValue>(permissions_list.begin(), permissions_list.end());

    auto max_bots = permissions[permission::i_client_music_limit];
    if(max_bots.has_value) {
        auto ownBots = this->server->music_manager_->listBots(this->getClientDatabaseId());
        if(!permission::v2::permission_granted(ownBots.size() + 1, max_bots))
            return command_result{error::music_client_limit_reached};
    }

    MusicClient::Type::value create_type;
    if(cmd[0].has("type")) {
        create_type = cmd["type"].as<MusicClient::Type::value>();
        switch(create_type) {
            case MusicClient::Type::PERMANENT:
                if(!permission::v2::permission_granted(1, permissions[permission::b_client_music_create_permanent]))
                    return command_result{permission::b_client_music_create_permanent};
                break;
            case MusicClient::Type::SEMI_PERMANENT:
                if(!permission::v2::permission_granted(1, permissions[permission::b_client_music_create_semi_permanent]))
                    return command_result{permission::b_client_music_create_semi_permanent};
                break;
            case MusicClient::Type::TEMPORARY:
                if(!permission::v2::permission_granted(1, permissions[permission::b_client_music_create_temporary]))
                    return command_result{permission::b_client_music_create_temporary};
                break;
            default:
                return command_result{error::vs_critical};
        }
    } else {
        if(permission::v2::permission_granted(1, permissions[permission::b_client_music_create_permanent]))
            create_type = MusicClient::Type::PERMANENT;
        else if(permission::v2::permission_granted(1, permissions[permission::b_client_music_create_semi_permanent]))
            create_type = MusicClient::Type::SEMI_PERMANENT;
        else if(permission::v2::permission_granted(1, permissions[permission::b_client_music_create_temporary]))
            create_type = MusicClient::Type::TEMPORARY;
        else
            return command_result{permission::b_client_music_create_temporary};
    }

    shared_lock server_channel_lock(this->server->channel_tree_mutex);
    auto channel = cmd[0].has("cid") ? this->server->channelTree->findChannel(cmd["cid"]) : this->currentChannel;
    if(!channel) {
        if(cmd[0].has("cid")) return command_result{error::channel_invalid_id};
    } else {
        if(this->calculate_and_get_join_state(channel) != permission::ok)
            channel = nullptr;
    }
    if(!channel)
        channel = this->server->channelTree->getDefaultChannel();

    auto bot = this->server->music_manager_->createBot(this->getClientDatabaseId());
    if(!bot) return command_result{error::vs_critical};
    bot->set_bot_type(create_type);
    if(permissions[permission::i_client_music_create_modify_max_volume].has_value) {
        auto max_volume = min(100, max(0, permissions[permission::i_client_music_create_modify_max_volume].value));
        if(max_volume >= 0)
            bot->volume_modifier(max_volume / 100.f);
    }
    this->selectedBot = bot;

    {
        server_channel_lock.unlock();
        unique_lock server_channel_w_lock(this->server->channel_tree_mutex);
        this->server->client_move(
                bot,
                channel,
                nullptr,
                "music bot created",
                ViewReasonId::VREASON_USER_ACTION,
                false,
                server_channel_w_lock
        );
    }
    bot->properties()[property::CLIENT_LAST_CHANNEL] = channel ? channel->channelId() : 0;
    bot->properties()[property::CLIENT_COUNTRY] = config::geo::countryFlag;

    if(permissions[permission::i_client_music_delete_power].has_value && permissions[permission::i_client_music_delete_power].value >= 0) {
        bot->clientPermissions->set_permission(permission::i_client_music_needed_delete_power, {permissions[permission::i_client_music_delete_power].value,0}, permission::v2::set_value, permission::v2::do_nothing);
    }

    Command notify(this->getExternalType() == ClientType::CLIENT_TEAMSPEAK ? "notifymusiccreated" : "");
    notify["bot_id"] = bot->getClientDatabaseId();
    this->sendCommand(notify);

    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandMusicBotDelete(Command& cmd) {
    if(!config::music::enabled) return command_result{error::music_disabled};

    CMD_REQ_SERVER;
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(25);

    auto bot = this->server->music_manager_->findBotById(cmd["bot_id"]);
    if(!bot) return command_result{error::music_invalid_id};

    if(bot->getOwner() != this->getClientDatabaseId()) {
        ACTION_REQUIRES_PERMISSION(permission::i_client_music_delete_power, bot->calculate_permission(permission::i_client_music_needed_delete_power, bot->getChannelId()), this->getChannelId());
    }

    this->server->music_manager_->deleteBot(bot);
    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandMusicBotSetSubscription(ts::Command &cmd) {
    if(!config::music::enabled) return command_result{error::music_disabled};

    auto bot = this->server->music_manager_->findBotById(cmd["bot_id"]);
    if(!bot && cmd["bot_id"].as<ClientDbId>() != 0) return command_result{error::music_invalid_id};

    {
        auto old_bot = this->subscribed_bot.lock();
        if(old_bot)
            old_bot->remove_subscriber(this->ref());
    }

    if(bot) {
        bot->add_subscriber(this->ref());
        this->subscribed_bot = bot;
    }

    return command_result{error::ok};
}

void apply_song(Command& command, const std::shared_ptr<ts::music::SongInfo>& element, int index = 0) {
    if(!element) return;

    command[index]["song_id"] = element ? element->getSongId() : 0;
    command[index]["song_url"] = element ? element->getUrl() : "";
    command[index]["song_invoker"] = element ? element->getInvoker() : 0;
    command[index]["song_loaded"] = false;

    auto entry = dynamic_pointer_cast<ts::music::PlayableSong>(element);
    if(entry) {
        auto data = entry->song_loaded_data();
        command[index]["song_loaded"] = entry->song_loaded() && data;

        if(entry->song_loaded() && data) {
            command[index]["song_title"] = data->title;
            command[index]["song_description"] = data->description;
            command[index]["song_thumbnail"] = data->thumbnail;
            command[index]["song_length"] = data->length.count();
        }
    }
}

command_result ConnectedClient::handleCommandMusicBotPlayerInfo(Command& cmd) {
    if(!config::music::enabled) return command_result{error::music_disabled};

    CMD_REQ_SERVER;
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(5);

    auto bot = this->server->music_manager_->findBotById(cmd["bot_id"]);
    if(!bot) return command_result{error::music_invalid_id};

    Command result(this->getExternalType() == CLIENT_TEAMSPEAK ? "notifymusicplayerinfo" : "");
    result["bot_id"] = bot->getClientDatabaseId();

    result["player_state"] =(int) bot->player_state();
    auto player = bot->current_player();
    if(player) {
        result["player_buffered_index"] = player->bufferedUntil().count();
        result["player_replay_index"] = player->currentIndex().count();
        result["player_max_index"] = player->length().count();
        result["player_seekable"] = player->seek_supported();

        result["player_title"] = player->songTitle();
        result["player_description"] = player->songDescription();
    } else {
        result["player_buffered_index"] = 0;
        result["player_replay_index"] = 0;
        result["player_max_index"] = 0;
        result["player_seekable"] = 0;
        result["player_title"] = "";
        result["player_description"] = "";
    }

    apply_song(result, bot->current_song());
    this->sendCommand(result);

    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandMusicBotPlayerAction(Command& cmd) {
    if(!config::music::enabled) return command_result{error::music_disabled};

    CMD_REQ_SERVER;
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(25);

    auto bot = this->server->music_manager_->findBotById(cmd["bot_id"]);
    if(!bot) return command_result{error::music_invalid_id};
    ACTION_REQUIRES_PERMISSION(permission::i_client_music_play_power, bot->calculate_permission(permission::i_client_music_needed_play_power, bot->getChannelId()), this->getChannelId());

    auto player = bot->current_player();
    if(cmd["action"] == 0) {
        bot->stopMusic();
    } else if(cmd["action"] == 1) {
        bot->playMusic();
    } else if(cmd["action"] == 2) {
        bot->player_pause();
    } else if(cmd["action"] == 3) {
        bot->forwardSong();
    } else if(cmd["action"] == 4) {
        bot->rewindSong();
    } else if(cmd["action"] == 5) {
        if(!player) return command_result{error::music_no_player};
        player->forward(::music::PlayerUnits{(int64_t) cmd["units"].as<uint64_t>()});
    } else if(cmd["action"] == 6) {
        if(!player) return command_result{error::music_no_player};
        player->rewind(::music::PlayerUnits{(int64_t) cmd["units"].as<uint64_t>()});
    } else {
        return command_result{error::music_invalid_action};
    }

    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandPlaylistList(ts::Command &cmd) {
    CMD_REQ_SERVER;
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(25);

    auto self_ref = this->ref();
    auto playlists = this->server->music_manager_->playlists();

    playlists.erase(find_if(playlists.begin(), playlists.end(), [&](const shared_ptr<music::PlayablePlaylist>& playlist) {
        return playlist->client_has_permissions(self_ref, permission::i_playlist_needed_view_power, permission::i_playlist_view_power, music::PlaylistPermissions::do_no_require_granted) != permission::ok;
    }), playlists.end());

    if(playlists.empty())
        return command_result{error::database_empty_result};

    Command notify(this->notify_response_command("notifyplaylistlist"));

    size_t index = 0;
    for(const auto& entry : playlists) {
        notify[index]["playlist_id"] = entry->playlist_id();
        auto bot = entry->current_bot();
        notify[index]["playlist_bot_id"] = bot ? bot->getClientDatabaseId() : 0;
        notify[index]["playlist_title"] = entry->properties()[property::PLAYLIST_TITLE].value();
        notify[index]["playlist_type"] = entry->properties()[property::PLAYLIST_TYPE].value();
        notify[index]["playlist_owner_dbid"] = entry->properties()[property::PLAYLIST_OWNER_DBID].value();
        notify[index]["playlist_owner_name"] = entry->properties()[property::PLAYLIST_OWNER_NAME].value();

        auto permissions = entry->permission_manager();
        auto permission = permissions->permission_value_flagged(permission::i_playlist_needed_modify_power);
        notify[index]["needed_power_modify"] = permission.has_value ? permission.value : 0;

        permission = permissions->permission_value_flagged(permission::i_playlist_needed_permission_modify_power);
        notify[index]["needed_power_permission_modify"] = permission.has_value ? permission.value : 0;

        permission = permissions->permission_value_flagged(permission::i_playlist_needed_delete_power);
        notify[index]["needed_power_delete"] = permission.has_value ? permission.value : 0;

        permission = permissions->permission_value_flagged(permission::i_playlist_song_needed_add_power);
        notify[index]["needed_power_song_add"] = permission.has_value ? permission.value : 0;

        permission = permissions->permission_value_flagged(permission::i_playlist_song_needed_move_power);
        notify[index]["needed_power_song_move"] = permission.has_value ? permission.value : 0;

        permission = permissions->permission_value_flagged(permission::i_playlist_song_needed_remove_power);
        notify[index]["needed_power_song_remove"] = permission.has_value ? permission.value : 0;
        index++;
    }

    this->sendCommand(notify);
    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandPlaylistCreate(ts::Command &cmd) {
    CMD_REF_SERVER(ref_server);
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(25);
    ACTION_REQUIRES_GLOBAL_PERMISSION(permission::b_playlist_create, 1);

    {
        auto max_playlists = this->calculate_permission(permission::i_max_playlists, 0);
        if(max_playlists.has_value) {
            auto playlists = ref_server->music_manager_->find_playlists_by_client(this->getClientDatabaseId());
            if(!permission::v2::permission_granted(playlists.size(), max_playlists))
                return command_result{permission::i_max_playlists};
        }
    }

    auto playlist = ref_server->music_manager_->create_playlist(this->getClientDatabaseId(), this->getDisplayName());
    if(!playlist) return command_result{error::vs_critical};

    playlist->properties()[property::PLAYLIST_TYPE] = music::Playlist::Type::GENERAL;

    {
        auto max_songs = this->calculate_permission(permission::i_max_playlist_size, 0);
        if(max_songs.has_value && max_songs.value >= 0)
            playlist->properties()[property::PLAYLIST_MAX_SONGS] = max_songs.value;
    }

    auto power = this->calculate_permission(permission::i_playlist_song_remove_power, 0);
    if(power.has_value && power.value >= 0)
        playlist->permission_manager()->set_permission(permission::i_playlist_song_needed_remove_power, {power.value, 0}, permission::v2::set_value, permission::v2::do_nothing);

    power = this->calculate_permission(permission::i_playlist_delete_power, 0);
    if(power.has_value && power.value >= 0)
        playlist->permission_manager()->set_permission(permission::i_playlist_needed_delete_power, {power.value, 0}, permission::v2::set_value, permission::v2::do_nothing);

    power = this->calculate_permission(permission::i_playlist_modify_power, 0);
    if(power.has_value && power.value >= 0)
        playlist->permission_manager()->set_permission(permission::i_playlist_needed_modify_power, {power.value, 0}, permission::v2::set_value, permission::v2::do_nothing);

    power = this->calculate_permission(permission::i_playlist_permission_modify_power, 0);
    if(power.has_value && power.value >= 0)
        playlist->permission_manager()->set_permission(permission::i_playlist_needed_permission_modify_power, {power.value, 0}, permission::v2::set_value, permission::v2::do_nothing);

    Command notify(this->notify_response_command("notifyplaylistcreated"));
    notify["playlist_id"] = playlist->playlist_id();
    this->sendCommand(notify);

    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandPlaylistDelete(ts::Command &cmd) {
    CMD_REF_SERVER(ref_server);
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(25);

    auto playlist = ref_server->music_manager_->find_playlist(cmd["playlist_id"]);
    if(!playlist) return command_result{error::playlist_invalid_id};

    if(auto perr = playlist->client_has_permissions(this->ref(), permission::i_playlist_needed_delete_power, permission::i_playlist_delete_power); perr)
        return command_result{perr};

    string error;
    if(!ref_server->music_manager_->delete_playlist(playlist->playlist_id(), error)) {
        logError(this->getServerId(), "Failed to delete playlist with id {}. Error: {}", playlist->playlist_id(), error);
        return command_result{error::vs_critical};
    }

    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandPlaylistInfo(ts::Command &cmd) {
    CMD_REF_SERVER(ref_server);
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(25);

    auto playlist = ref_server->music_manager_->find_playlist(cmd["playlist_id"]);
    if(!playlist) return command_result{error::playlist_invalid_id};

    if(auto perr = playlist->client_has_permissions(this->ref(), permission::i_playlist_needed_view_power, permission::i_playlist_view_power, music::PlaylistPermissions::do_no_require_granted); perr)
        return command_result{perr};


    Command notify(this->notify_response_command("notifyplaylistinfo"));
    for(const auto& property : playlist->properties().list_properties(property::FLAG_PLAYLIST_VARIABLE)) {
        notify[property.type().name] = property.value();
    }
    this->sendCommand(notify);

    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandPlaylistEdit(ts::Command &cmd) {
    CMD_REF_SERVER(ref_server);
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(25);

    auto playlist = ref_server->music_manager_->find_playlist(cmd["playlist_id"]);
    if(!playlist) return command_result{error::playlist_invalid_id};

    if(auto perr = playlist->client_has_permissions(this->ref(), permission::i_playlist_needed_modify_power, permission::i_playlist_modify_power); perr)
        return command_result{perr};

    deque<pair<const property::PropertyDescription*, string>> properties;

    for(const auto& key : cmd[0].keys()) {
        if(key == "playlist_id") continue;
        if(key == "return_code") continue;

        const auto& property = property::find<property::PlaylistProperties>(key);
        if(property == property::PLAYLIST_UNDEFINED) {
            logError(this->getServerId(), R"([{}] Tried to edit a not existing playlist property "{}" to "{}")", CLIENT_STR_LOG_PREFIX, key, cmd[key].string());
            continue;
        }

        if((property.flags & property::FLAG_USER_EDITABLE) == 0) {
            logError(this->getServerId(), "[{}] Tried to change a playlist property which is not changeable. (Key: {}, Value: \"{}\")", CLIENT_STR_LOG_PREFIX, key, cmd[key].string());
            continue;
        }

        if(!property.validate_input(cmd[key].as<string>())) {
            logError(this->getServerId(), "[{}] Tried to change a playlist property to an invalid value. (Key: {}, Value: \"{}\")", CLIENT_STR_LOG_PREFIX, key, cmd[key].string());
            continue;
        }

        if(property == property::PLAYLIST_CURRENT_SONG_ID) {
            auto song_id = cmd[key].as<SongId>();
            auto song = song_id > 0 ? playlist->find_song(song_id) : nullptr;
            if(song_id != 0 && !song)
                return command_result{error::playlist_invalid_song_id};
        } else if(property == property::PLAYLIST_MAX_SONGS) {
            auto value = cmd[key].as<int32_t>();
            auto max_value = this->calculate_permission(permission::i_max_playlist_size, this->getChannelId());
            if(max_value.has_value && !permission::v2::permission_granted(value, max_value))
                return command_result{permission::i_max_playlist_size};
        }

        properties.emplace_back(&property, key);
    }
    for(const auto& property : properties) {
        if(*property.first == property::PLAYLIST_CURRENT_SONG_ID) {
            playlist->set_current_song(cmd[property.second]);
            continue;
        }

        playlist->properties()[property.first] = cmd[property.second].string();
    }
    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandPlaylistPermList(ts::Command &cmd) {
    CMD_REF_SERVER(ref_server);
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(25);

    auto playlist = ref_server->music_manager_->find_playlist(cmd["playlist_id"]);
    if(!playlist) return command_result{error::playlist_invalid_id};

    {
        auto value = playlist->calculate_client_specific_permissions(permission::b_virtualserver_playlist_permission_list, this->ref());
        if(!permission::v2::permission_granted(1, value))
            return command_result{permission::b_virtualserver_playlist_permission_list};
    }

    auto permissions = playlist->permission_manager()->permissions();
    if(permissions.empty())
        return command_result{error::database_empty_result};

    Command result(this->notify_response_command("notifyplaylistpermlist"));

    auto permission_mapper = serverInstance->getPermissionMapper();
    auto perm_sid = cmd.hasParm("permsid") || cmd.hasParm("names");
    int index = 0;
    result["playlist_id"] = playlist->playlist_id();
    for (const auto &[perm, value] : permissions) {
        if(value.flags.value_set) {
            if (perm_sid)
                result[index]["permsid"] = permission_mapper->permission_name(this->getType(), perm);
            else
                result[index]["permid"] = perm;

            result[index]["permvalue"] = value.values.value;
            result[index]["permnegated"] = value.flags.negate;
            result[index]["permskip"] = value.flags.skip;
            index++;
        }
        if(value.flags.grant_set) {
            if (perm_sid)
                result[index]["permsid"] = permission_mapper->permission_name_grant(this->getType(), perm);
            else
                result[index]["permid"] = perm | PERM_ID_GRANT;

            result[index]["permvalue"] = value.values.grant;
            result[index]["permnegated"] = 0;
            result[index]["permskip"] = 0;
            index++;
        }
    }
    this->sendCommand(result);

    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandPlaylistAddPerm(ts::Command &cmd) {
    CMD_REF_SERVER(ref_server);
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(5);

    auto playlist = ref_server->music_manager_->find_playlist(cmd["playlist_id"]);
    if(!playlist) return command_result{error::playlist_invalid_id};

    if(auto perr = playlist->client_has_permissions(this->ref(), permission::i_playlist_needed_permission_modify_power, permission::i_playlist_permission_modify_power); perr)
        return command_result{perr};

    ts::command::bulk_parser::PermissionBulksParser pparser{cmd, true};
    if(!pparser.validate(this->ref(), 0))
        return pparser.build_command_result();

    for(const auto& ppermission : pparser.iterate_valid_permissions()) {
        ppermission.apply_to(playlist->permission_manager(), permission::v2::PermissionUpdateType::set_value);
        ppermission.log_update(serverInstance->action_logger()->permission_logger,
                               this->getServerId(),
                               this->ref(),
                               log::PermissionTarget::PLAYLIST,
                               permission::v2::PermissionUpdateType::set_value,
                               playlist->playlist_id(), "",
                               0, ""
        );
    }

    return pparser.build_command_result();
}

command_result ConnectedClient::handleCommandPlaylistDelPerm(ts::Command &cmd) {
    CMD_REF_SERVER(ref_server);
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(5);

    auto playlist = ref_server->music_manager_->find_playlist(cmd["playlist_id"]);
    if(!playlist) return command_result{error::playlist_invalid_id};

    if(auto perr = playlist->client_has_permissions(this->ref(), permission::i_playlist_needed_permission_modify_power, permission::i_playlist_permission_modify_power); perr)
        return command_result{perr};

    ts::command::bulk_parser::PermissionBulksParser pparser{cmd, false};
    if(!pparser.validate(this->ref(), 0))
        return pparser.build_command_result();

    for(const auto& ppermission : pparser.iterate_valid_permissions()) {
        ppermission.apply_to(playlist->permission_manager(), permission::v2::PermissionUpdateType::delete_value);
        ppermission.log_update(serverInstance->action_logger()->permission_logger,
                               this->getServerId(),
                               this->ref(),
                               log::PermissionTarget::PLAYLIST,
                               permission::v2::PermissionUpdateType::delete_value,
                               playlist->playlist_id(), "",
                               0, ""
        );
    }

    return pparser.build_command_result();
}

command_result ConnectedClient::handleCommandPlaylistClientList(ts::Command &cmd) {
    CMD_REF_SERVER(ref_server);
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(25);

    auto playlist = ref_server->music_manager_->find_playlist(cmd["playlist_id"]);
    if(!playlist) return command_result{error::playlist_invalid_id};

    {
        auto value = playlist->calculate_client_specific_permissions(permission::b_virtualserver_playlist_permission_list, this->ref());
        if(!permission::v2::permission_granted(1, value))
            return command_result{permission::b_virtualserver_playlist_permission_list};
    }

    auto permissions = playlist->permission_manager()->channel_permissions();
    if(permissions.empty())
        return command_result{error::database_empty_result};


    Command result(this->notify_response_command("notifyplaylistclientlist"));
    auto permission_mapper = serverInstance->getPermissionMapper();

    int index = 0;
    ClientDbId last_client_id{0};
    result["playlist_id"] = playlist->playlist_id();
    for (const auto &[perm, client, value] : permissions) {
        if(last_client_id != client)
            result[index++]["cldbid"] = client;
    }
    if(index == 0)
        return command_result{error::database_empty_result};
    this->sendCommand(result);

    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandPlaylistClientPermList(ts::Command &cmd) {
    CMD_REF_SERVER(ref_server);
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(25);

    auto playlist = ref_server->music_manager_->find_playlist(cmd["playlist_id"]);
    if(!playlist) return command_result{error::playlist_invalid_id};

    {
        auto value = playlist->calculate_client_specific_permissions(permission::b_virtualserver_playlist_permission_list, this->ref());
        if(!permission::v2::permission_granted(1, value))
            return command_result{permission::b_virtualserver_playlist_permission_list};
    }

    auto permissions = playlist->permission_manager()->channel_permissions();
    if(permissions.empty())
        return command_result{error::database_empty_result};

    Command result(this->notify_response_command("notifyplaylistclientpermlist"));

    auto client_id = cmd.hasParm("cldbid") ? cmd[0]["cldbid"].as<ClientDbId>() : 0;
    auto permission_mapper = serverInstance->getPermissionMapper();
    auto perm_sid = cmd.hasParm("permsid") || cmd.hasParm("names");

    int index = 0;
    ClientDbId last_client_id{0};
    result["playlist_id"] = playlist->playlist_id();
    result["cldbid"] = client_id;
    for (const auto &[perm, client, value] : permissions) {
        if(client_id > 0 && client != client_id) continue;

        if(last_client_id != client)
            result[index]["cldbid"] = client;
        if(value.flags.value_set) {
            if (perm_sid)
                result[index]["permsid"] = permission_mapper->permission_name(this->getType(), perm);
            else
                result[index]["permid"] = perm;

            result[index]["permvalue"] = value.values.value;
            result[index]["permnegated"] = value.flags.negate;
            result[index]["permskip"] = value.flags.skip;
            index++;
        }
        if(value.flags.grant_set) {
            if (perm_sid)
                result[index]["permsid"] = permission_mapper->permission_name_grant(this->getType(), perm);
            else
                result[index]["permid"] = perm | PERM_ID_GRANT;

            result[index]["permvalue"] = value.values.grant;
            result[index]["permnegated"] = 0;
            result[index]["permskip"] = 0;
            index++;
        }
    }
    if(index == 0)
        return command_result{error::database_empty_result};
    this->sendCommand(result);

    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandPlaylistClientAddPerm(ts::Command &cmd) {
    CMD_REF_SERVER(ref_server);
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(5);

    auto playlist = ref_server->music_manager_->find_playlist(cmd["playlist_id"]);
    if(!playlist) return command_result{error::playlist_invalid_id};

    auto client_id = cmd[0]["cldbid"].as<ClientDbId>();
    if(client_id == 0) return command_result{error::client_invalid_id};

    if(auto perr = playlist->client_has_permissions(this->ref(), permission::i_playlist_needed_permission_modify_power, permission::i_playlist_permission_modify_power); perr)
        return command_result{perr};

    ts::command::bulk_parser::PermissionBulksParser pparser{cmd, true};
    if(!pparser.validate(this->ref(), this->getClientDatabaseId()))
        return pparser.build_command_result();

    for(const auto& ppermission : pparser.iterate_valid_permissions()) {
        ppermission.apply_to_channel(playlist->permission_manager(), permission::v2::PermissionUpdateType::set_value, client_id);
        ppermission.log_update(serverInstance->action_logger()->permission_logger,
                               this->getServerId(),
                               this->ref(),
                               log::PermissionTarget::PLAYLIST_CLIENT,
                               permission::v2::PermissionUpdateType::set_value,
                               playlist->playlist_id(), "",
                               client_id, ""
        );
    }

    return pparser.build_command_result();
}

command_result ConnectedClient::handleCommandPlaylistClientDelPerm(ts::Command &cmd) {
    CMD_REF_SERVER(ref_server);
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(5);

    auto playlist = ref_server->music_manager_->find_playlist(cmd["playlist_id"]);
    if(!playlist) return command_result{error::playlist_invalid_id};

    auto client_id = cmd[0]["cldbid"].as<ClientDbId>();
    if(client_id == 0) return command_result{error::client_invalid_id};

    if(auto perr = playlist->client_has_permissions(this->ref(), permission::i_playlist_needed_permission_modify_power, permission::i_playlist_permission_modify_power); perr)
        return command_result{perr};


    ts::command::bulk_parser::PermissionBulksParser pparser{cmd, false};
    if(!pparser.validate(this->ref(), this->getClientDatabaseId()))
        return pparser.build_command_result();

    for(const auto& ppermission : pparser.iterate_valid_permissions()) {
        ppermission.apply_to_channel(playlist->permission_manager(), permission::v2::PermissionUpdateType::delete_value, client_id);
        ppermission.log_update(serverInstance->action_logger()->permission_logger,
                               this->getServerId(),
                               this->ref(),
                               log::PermissionTarget::PLAYLIST_CLIENT,
                               permission::v2::PermissionUpdateType::delete_value,
                               playlist->playlist_id(), "",
                               client_id, ""
        );
    }

    return pparser.build_command_result();
}

constexpr auto max_song_meta_info = 1024 * 512;

inline size_t estimated_song_info_size(const std::shared_ptr<ts::music::PlaylistEntryInfo>& song, bool extract_metadata) {
    return 128 + std::min(song->metadata.json_string.length(), (size_t) max_song_meta_info) + extract_metadata * 256;
}

inline void fill_song_info(ts::command_builder_bulk bulk, const std::shared_ptr<ts::music::PlaylistEntryInfo>& song, bool extract_metadata) {
    bulk.reserve(estimated_song_info_size(song, extract_metadata));

    bulk.put("song_id", song->song_id);
    bulk.put("song_invoker", song->invoker);
    bulk.put("song_previous_song_id", song->previous_song_id);
    bulk.put("song_url", song->original_url);
    bulk.put("song_url_loader", song->url_loader);
    bulk.put("song_loaded", song->metadata.is_loaded());
    if(song->metadata.json_string.length() > 1024 * 1024 * 512) {
        logWarning(LOG_GENERAL, "Dropping song metadata because its way to big. ({}bytes)", song->metadata.json_string.size());
    } else {
        bulk.put("song_metadata", song->metadata.json_string);
    }

    if(extract_metadata) {
        bulk.reserve(256, true);
        auto metadata = song->metadata.loaded_data;
        if(extract_metadata && song->metadata.is_loaded() && metadata) {
            bulk.put("song_metadata_title", metadata->title);
            bulk.put("song_metadata_description", metadata->description);
            bulk.put("song_metadata_url", metadata->url); /* Internally resolved URL. Should not be externally used */
            bulk.put("song_metadata_length", metadata->length.count());
            if(auto thumbnail = static_pointer_cast<::music::ThumbnailUrl>(metadata->thumbnail); thumbnail && thumbnail->type() == ::music::THUMBNAIL_URL) {
                bulk.put("song_metadata_thumbnail_url", thumbnail->url());
            } else {
                bulk.put("song_metadata_thumbnail_url", "none");
            }
        }
    }
}

command_result ConnectedClient::handleCommandPlaylistSongList(ts::Command &cmd) {
    CMD_REF_SERVER(ref_server);
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(25);

    auto playlist = ref_server->music_manager_->find_playlist(cmd["playlist_id"]);
    if(!playlist) return command_result{error::playlist_invalid_id};

    if(auto perr = playlist->client_has_permissions(this->ref(), permission::i_playlist_needed_view_power, permission::i_playlist_view_power); perr)
        return command_result{perr};

    auto songs = playlist->list_songs();
    if(songs.empty())
        return command_result{error::database_empty_result};

    ts::command_builder result{this->notify_response_command("notifyplaylistsonglist")};
    result.put(0, "version", 2); /* to signalize that we're sending the response bulked */
    auto extract_metadata = cmd.hasParm("extract-metadata");

    size_t index{0};
    for(const auto& song : songs) {
        if(index == 0) {
            result.put(0, "playlist_id", playlist->playlist_id());
        }
        fill_song_info(result.bulk(index), song, extract_metadata);

        if(this->getType() != ClientType::CLIENT_QUERY && result.current_size() + estimated_song_info_size(song, extract_metadata) > 64 * 1024) {
            this->sendCommand(result);
            result.reset();
            index = 0;
        } else {
            index++;
        }
    }

    if(index > 0) {
        this->sendCommand(result);
    }

    /* This step is actiually not really needed... */
    if(this->getType() != ClientType::CLIENT_QUERY) {
        ts::command_builder finish{"notifyplaylistsonglistfinished"};
        finish.put(0, "playlist_id", playlist->playlist_id());
        this->sendCommand(finish);
    }

    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandPlaylistSongSetCurrent(ts::Command &cmd) {
    CMD_REF_SERVER(ref_server);
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(25);

    auto playlist = ref_server->music_manager_->find_playlist(cmd["playlist_id"]);
    if(!playlist) return command_result{error::playlist_invalid_id};

    if(auto perr = playlist->client_has_permissions(this->ref(), permission::i_playlist_song_needed_move_power, permission::i_playlist_song_move_power); perr)
        return command_result{perr};

    if(!playlist->set_current_song(cmd["song_id"]))
        return command_result{error::playlist_invalid_song_id};

    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandPlaylistSongAdd(ts::Command &cmd) {
    CMD_REF_SERVER(ref_server);
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(25);

    auto playlist = ref_server->music_manager_->find_playlist(cmd["playlist_id"]);
    if(!playlist) return command_result{error::playlist_invalid_id};

    if(auto perr = playlist->client_has_permissions(this->ref(), permission::i_playlist_song_needed_add_power, permission::i_playlist_song_add_power); perr)
        return command_result{perr};

    if(cmd[0].has("invoker"))
        cmd["type"] = "";
    else if(!cmd[0].has("type"))
        cmd["type"] = "";

    if(!cmd[0].has("previous")) {
        auto songs = playlist->list_songs();
        if(songs.empty())
            cmd["previous"] = "0";
        else
            cmd["previous"] = songs.back()->song_id;
    }

    auto& type = cmd[0]["type"];
    std::string loader_string{""};
    if((type.castable<int>() && type.as<int>() == 0) || type.as<string>() == "yt") {
        loader_string = "YouTube";
    } else if((type.castable<int>() && type.as<int>() == 1) || type.as<string>() == "ffmpeg") {
        loader_string = "FFMpeg";
    } else if((type.castable<int>() && type.as<int>() == 2) || type.as<string>() == "channel") {
        loader_string = "ChannelProvider";
    }

    auto song = playlist->add_song(this->ref(), cmd["url"], loader_string, cmd["previous"]);
    if(!song) return command_result{error::vs_critical};

    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandPlaylistSongReorder(ts::Command &cmd) {
    CMD_REF_SERVER(ref_server);
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(25);

    auto playlist = ref_server->music_manager_->find_playlist(cmd["playlist_id"]);
    if(!playlist) return command_result{error::playlist_invalid_id};

    if(auto perr = playlist->client_has_permissions(this->ref(), permission::i_playlist_song_needed_move_power, permission::i_playlist_song_move_power); perr)
        return command_result{perr};

    SongId song_id = cmd["song_id"];
    SongId previous_id = cmd["song_previous_song_id"];

    auto song = playlist->find_song(song_id);
    if(!song) return command_result{error::playlist_invalid_song_id};

    if(!playlist->reorder_song(song_id, previous_id))
        return command_result{error::vs_critical};

    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandPlaylistSongRemove(ts::Command &cmd) {
    CMD_REF_SERVER(ref_server);
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(25);

    auto playlist = ref_server->music_manager_->find_playlist(cmd["playlist_id"]);
    if(!playlist) return command_result{error::playlist_invalid_id};

    if(auto perr = playlist->client_has_permissions(this->ref(), permission::i_playlist_song_needed_remove_power, permission::i_playlist_song_remove_power); perr)
        return command_result{perr};

    SongId song_id = cmd["song_id"];

    auto song = playlist->find_song(song_id);
    if(!song) return command_result{error::playlist_invalid_song_id};

    if(!playlist->delete_song(song_id))
        return command_result{error::vs_critical};

    return command_result{error::ok};
}


/****** legacy ******/
command_result ConnectedClient::handleCommandMusicBotQueueList(Command& cmd) {
#if false
    CMD_REQ_SERVER;
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(25);

    auto bot = this->server->musicManager->findBotById(cmd["bot_id"]);
    if(!bot) return command_result{error::music_invalid_id};

    auto playlist = dynamic_pointer_cast<music::PlayablePlaylist>(bot->playlist());
    if(!playlist) return command_result{error::vs_critical};

    if(auto perr = playlist->client_has_permissions(this->ref(), permission::i_playlist_needed_view_power, permission::i_playlist_view_power); perr)
        return command_result{perr};

    bool bulked = cmd.hasParm("bulk") || cmd.hasParm("balk") || cmd.hasParm("pipe") || cmd.hasParm("bar") || cmd.hasParm("paypal");
    int command_index = 0;

    Command notify(this->notify_response_command("notifymusicqueueentry"));
    auto songs = playlist->list_songs();
    int begin_index{0};
    for(auto it = songs.begin(); it != songs.end(); it++)
        if((*it)->song_id == playlist->currently_playing())
            break;
        else
            begin_index--;

    for(auto it = songs.begin(); it != songs.end(); it++) {
        if(!bulked)
            notify = Command(this->notify_response_command("notifymusicqueueentry"));

        auto song = *it;
        notify[command_index]["song_id"] = song->song_id;
        notify[command_index]["song_url"] = song->original_url;
        notify[command_index]["song_invoker"] = song->invoker;
        notify[command_index]["song_loaded"] = song->metadata.is_loaded();

        auto entry = song->load_future && song->load_future->succeeded() ? song->load_future->getValue({}) : nullptr;
        if(entry) {
            notify[command_index]["song_loaded"] = true;
            if(entry->type != ::music::TYPE_STREAM && entry->type != ::music::TYPE_VIDEO)
                continue;

            auto info = reinterpret_pointer_cast<::music::UrlSongInfo>(entry);
            if(info) {
                notify[command_index]["song_title"] = info->title;
                notify[command_index]["song_description"] = info->description;
                if(auto thumbnail = reinterpret_pointer_cast<::music::ThumbnailUrl>(info->thumbnail); thumbnail && thumbnail->type() == ::music::THUMBNAIL_URL)
                    notify[command_index]["song_thumbnail"] = thumbnail->url();
                else
                    notify[command_index]["song_thumbnail"] = "";
                notify[command_index]["song_length"] = info->length.count();
            }
        }
        notify[command_index]["queue_index"] = begin_index++;

        if(!bulked)
            this->sendCommand(notify);
        else
            command_index++;
    }

    if(bulked) {
        if(command_index > 0) {
            this->sendCommand(notify);
        } else
            return command_result{error::database_empty_result};
    }

    if(this->getExternalType() == ClientType::CLIENT_TEAMSPEAK) {
        Command notify("notifymusicqueuefinish");
        notify["bot_id"] = bot->getClientDatabaseId();
        this->sendCommand(notify);
    }
    return command_result{error::ok};
#endif
    return command_result{error::not_implemented}; //FIXME
}

command_result ConnectedClient::handleCommandMusicBotQueueAdd(Command& cmd) {
    return command_result{error::not_implemented}; //FIXME

    /*
    CMD_REQ_SERVER;
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(25);

    auto bot = this->server->musicManager->findBotById(cmd["bot_id"]);
    if(!bot) return command_result{error::music_invalid_id};
    PERM_CHECK_CHANNELR(permission::i_client_music_play_power, bot->permissionValue(permission::i_client_music_needed_play_power, bot->currentChannel), this->currentChannel, true);

    MusicClient::loader_t loader;
    auto& type = cmd[0]["type"];
    if((type.castable<int>() && type.as<int>() == 0) || type.as<string>() == "yt") {
        loader = bot->ytLoader(this->getServer());
    } else if((type.castable<int>() && type.as<int>() == 1) || type.as<string>() == "ffmpeg") {
        loader = bot->ffmpegLoader(this->getServer());
    } else if((type.castable<int>() && type.as<int>() == 2) || type.as<string>() == "channel") {
        loader = bot->channelLoader(this->getServer());
    } else if((type.castable<int>() && type.as<int>() == -1) || type.as<string>() == "any") {
        loader = bot->providerLoader(this->getServer(), "");
    }
    if(!loader) return command_result{error::music_invalid_action};

    auto entry = bot->queue()->insertEntry(cmd["url"], this->ref(), loader);
    if(!entry) return command_result{error::vs_critical};

    this->server->forEachClient([&](shared_ptr<ConnectedClient> client) {
        client->notifyMusicQueueAdd(bot, entry, bot->queue()->queueEntries().size() - 1, this->ref());
    });

    return command_result{error::ok};
    */
}

command_result ConnectedClient::handleCommandMusicBotQueueRemove(Command& cmd) {
    return command_result{error::not_implemented}; //FIXME

    /*
    CMD_REQ_SERVER;
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(25);

    auto bot = this->server->musicManager->findBotById(cmd["bot_id"]);
    if(!bot) return command_result{error::music_invalid_id};
    PERM_CHECK_CHANNELR(permission::i_client_music_play_power, bot->permissionValue(permission::i_client_music_needed_play_power, bot->currentChannel), this->currentChannel, true);

    std::deque<std::shared_ptr<music::SongInfo>> songs;
    for(int index = 0; index < cmd.bulkCount(); index++) {
        auto entry = bot->queue()->find_queue(cmd["song_id"]);
        if(!entry) {
            if(cmd.hasParm("skip_error")) continue;
            return command_result{error::database_empty_result};
        }

        songs.push_back(move(entry));
    }

    for(const auto& entry : songs)
        bot->queue()->deleteEntry(dynamic_pointer_cast<music::PlayableSong>(entry));
    this->server->forEachClient([&](shared_ptr<ConnectedClient> client) {
        client->notifyMusicQueueRemove(bot, songs, this->ref());
    });
    return command_result{error::ok};
    */
}

command_result ConnectedClient::handleCommandMusicBotQueueReorder(Command& cmd) {
    return command_result{error::not_implemented}; //FIXME

    /*
    CMD_REQ_SERVER;
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(25);

    auto bot = this->server->musicManager->findBotById(cmd["bot_id"]);
    if(!bot) return command_result{error::music_invalid_id};
    PERM_CHECK_CHANNELR(permission::i_client_music_play_power, bot->permissionValue(permission::i_client_music_needed_play_power, bot->currentChannel), this->currentChannel, true);

    auto entry = bot->queue()->find_queue(cmd["song_id"]);
    if(!entry) return command_result{error::database_empty_result};

    auto order = bot->queue()->changeOrder(entry, cmd["index"]);
    if(order < 0) return command_result{error::vs_critical};
    this->server->forEachClient([&](shared_ptr<ConnectedClient> client) {
        client->notifyMusicQueueOrderChange(bot, entry, order, this->ref());
    });
    return command_result{error::ok};
     */
}

command_result ConnectedClient::handleCommandMusicBotPlaylistAssign(ts::Command &cmd) {
    CMD_REF_SERVER(ref_server);
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(25);

    auto bot = ref_server->music_manager_->findBotById(cmd["bot_id"]);
    if(!bot) return command_result{error::music_invalid_id};
    if(bot->getOwner() != this->getClientDatabaseId()) {
        ACTION_REQUIRES_GLOBAL_PERMISSION(permission::i_client_music_play_power, bot->calculate_permission(permission::i_client_music_needed_play_power, 0));
    }

    auto playlist = ref_server->music_manager_->find_playlist(cmd["playlist_id"]);
    if(!playlist && cmd["playlist_id"] != 0) return command_result{error::playlist_invalid_id};

    if(ref_server->music_manager_->find_bot_by_playlist(playlist))
        return command_result{error::playlist_already_in_use};

    if(auto perr = playlist->client_has_permissions(this->ref(), permission::i_playlist_needed_view_power, permission::i_playlist_view_power); perr)
        return command_result{perr};

    if(!ref_server->music_manager_->assign_playlist(bot, playlist))
        return command_result{error::vs_critical};

    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandPlaylistSetSubscription(ts::Command &cmd) {
    CMD_REF_SERVER(ref_server);
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(5);

    if(!config::music::enabled) return command_result{error::music_disabled};

    auto playlist = ref_server->music_manager_->find_playlist(cmd["playlist_id"]);
    if(!playlist && cmd["playlist_id"] != 0) {
        return command_result{error::playlist_invalid_id};
    }

    {
        auto old_playlist = this->subscribed_playlist_.lock();
        if(old_playlist) {
            old_playlist->remove_subscriber(this->ref());
        }
    }

    if(playlist) {
        if(auto perr = playlist->client_has_permissions(this->ref(), permission::i_playlist_needed_view_power, permission::i_playlist_view_power); perr) {
            return command_result{perr};
        }

        playlist->add_subscriber(this->ref());
        this->subscribed_playlist_ = playlist;
    }

    return command_result{error::ok};
}
















