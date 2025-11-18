//
// Created by wolverindev on 26.01.20.
//

#include <memory>

#include <bitset>
#include <algorithm>
#include <openssl/sha.h>
#include "../ConnectedClient.h"
#include "../InternalClient.h"
#include "../../server/VoiceServer.h"
#include "../voice/VoiceClient.h"
#include "../../InstanceHandler.h"
#include "../../server/QueryServer.h"
#include "../music/MusicClient.h"
#include "../query/QueryClient.h"
#include "../../manager/ConversationManager.h"
#include "../../manager/PermissionNameMapper.h"
#include "../../manager/ActionLogger.h"
#include "../../groups/GroupManager.h"
#include "../../PermissionCalculator.h"

#include "helpers.h"
#include "./bulk_parsers.h"

#include <log/LogUtils.h>
#include <misc/base64.h>
#include <misc/digest.h>
#include <misc/rnd.h>
#include <bbcode/bbcodes.h>

using namespace std::chrono;
using namespace std;
using namespace ts;
using namespace ts::server;

#define QUERY_PASSWORD_LENGTH 12

command_result ConnectedClient::handleCommandServerGetVariables(Command &cmd) {
    CMD_REQ_SERVER;
    this->notifyServerUpdated(this->ref());
    return command_result{error::ok};
}

#define SERVEREDIT_CHK_PROP_CACHED(name, perm, type)\
else if(key == name) { \
   ACTION_REQUIRES_GLOBAL_PERMISSION(perm, 1);  \
   if(toApplay.count(key) == 0) toApplay[key] = cmd[key].as<std::string>(); \
   if(!cmd[0][key].castable<type>()) return command_result{error::parameter_invalid};

#define SERVEREDIT_CHK_PROP2(name, perm, type_a, type_b)\
else if(key == name) { \
   ACTION_REQUIRES_GLOBAL_PERMISSION(perm, 1);  \
   if(toApplay.count(key) == 0) toApplay[key] = cmd[key].as<std::string>(); \
   if(!cmd[0][key].castable<type_a>() && !!cmd[0][key].castable<type_b>()) return command_result{error::parameter_invalid};

command_result ConnectedClient::handleCommandServerEdit(Command &cmd) {
    CMD_CHK_AND_INC_FLOOD_POINTS(5);

    if (cmd[0].has("sid") && this->getServerId() != cmd["sid"].as<ServerId>()) {
        return command_result{error::server_invalid_id};
    }

    auto target_server = this->server;
    if(cmd[0].has("sid")) {
        target_server = serverInstance->getVoiceServerManager()->findServerById(cmd["sid"]);
        if(!target_server && cmd["sid"].as<ServerId>() != 0) {
            return command_result{error::server_invalid_id};
        }
    }
    ServerId serverId = target_server ? target_server->serverId : 0;
    auto group_manager = target_server ? target_server->group_manager() : serverInstance->group_manager();

    map<string, string> toApplay;
    for (auto &key : cmd[0].keys()) {
        if (key == "sid") continue;
        SERVEREDIT_CHK_PROP_CACHED("virtualserver_name", permission::b_virtualserver_modify_name, string) }
        SERVEREDIT_CHK_PROP_CACHED("virtualserver_name_phonetic", permission::b_virtualserver_modify_name, string) }
        SERVEREDIT_CHK_PROP_CACHED("virtualserver_maxclients", permission::b_virtualserver_modify_maxclients, size_t)
            if (cmd["virtualserver_maxclients"].as<size_t>() > 1024)
                return command_result{error::accounting_slot_limit_reached, "Do you really need more that 1024 slots?"};
        } SERVEREDIT_CHK_PROP_CACHED("virtualserver_reserved_slots", permission::b_virtualserver_modify_reserved_slots, size_t) }
        SERVEREDIT_CHK_PROP_CACHED("virtualserver_max_channels", permission::b_virtualserver_modify_maxchannels, size_t)
            if(cmd["virtualserver_max_channels"].as<size_t>() > 8192)
                return command_result{error::channel_protocol_limit_reached};
        }
        SERVEREDIT_CHK_PROP_CACHED("virtualserver_icon_id", permission::b_virtualserver_modify_icon_id, int64_t) }
        SERVEREDIT_CHK_PROP_CACHED("virtualserver_channel_temp_delete_delay_default", permission::b_virtualserver_modify_channel_temp_delete_delay_default, uint32_t) }
        SERVEREDIT_CHK_PROP_CACHED("virtualserver_codec_encryption_mode", permission::b_virtualserver_modify_codec_encryption_mode, int) }
        SERVEREDIT_CHK_PROP_CACHED("virtualserver_default_server_group", permission::b_virtualserver_modify_default_servergroup, GroupId)
            if(target_server) {
                auto target_group = group_manager->server_groups()->find_group(groups::GroupCalculateMode::GLOBAL, cmd["virtualserver_default_server_group"].as<GroupId>());
                if (!target_group) {
                    return command_result{error::group_invalid_id};
                }
            }
        } SERVEREDIT_CHK_PROP_CACHED("virtualserver_default_channel_group", permission::b_virtualserver_modify_default_channelgroup, GroupId)
            if(target_server) {
                auto target_group = group_manager->channel_groups()->find_group(groups::GroupCalculateMode::GLOBAL, cmd["virtualserver_default_channel_group"].as<GroupId>());
                if (!target_group) {
                    return command_result{error::group_invalid_id};
                }
            }
        } SERVEREDIT_CHK_PROP_CACHED("virtualserver_default_channel_admin_group", permission::b_virtualserver_modify_default_channeladmingroup, GroupId)
            if(target_server) {
                auto target_group = group_manager->channel_groups()->find_group(groups::GroupCalculateMode::GLOBAL, cmd["virtualserver_default_channel_admin_group"].as<GroupId>());
                if (!target_group) {
                    return command_result{error::group_invalid_id};
                }
            }
        } SERVEREDIT_CHK_PROP_CACHED("virtualserver_default_music_group", permission::b_virtualserver_modify_default_musicgroup, GroupId)
            if(target_server) {
                auto target_group = group_manager->server_groups()->find_group(groups::GroupCalculateMode::GLOBAL, cmd["virtualserver_default_server_group"].as<GroupId>());
                if (!target_group) {
                    return command_result{error::group_invalid_id};
                }
            }
        }
        SERVEREDIT_CHK_PROP_CACHED("virtualserver_priority_speaker_dimm_modificator", permission::b_virtualserver_modify_priority_speaker_dimm_modificator, float) }

        SERVEREDIT_CHK_PROP_CACHED("virtualserver_port", permission::b_virtualserver_modify_port, uint16_t) }
        SERVEREDIT_CHK_PROP_CACHED("virtualserver_host", permission::b_virtualserver_modify_host, string) }
        SERVEREDIT_CHK_PROP_CACHED("virtualserver_web_host", permission::b_virtualserver_modify_port, uint16_t) }
        SERVEREDIT_CHK_PROP_CACHED("virtualserver_web_port", permission::b_virtualserver_modify_host, string) }

        SERVEREDIT_CHK_PROP_CACHED("virtualserver_hostbanner_url", permission::b_virtualserver_modify_hostbanner, string) }
        SERVEREDIT_CHK_PROP_CACHED("virtualserver_hostbanner_gfx_url", permission::b_virtualserver_modify_hostbanner, string) }
        SERVEREDIT_CHK_PROP_CACHED("virtualserver_hostbanner_gfx_interval", permission::b_virtualserver_modify_hostbanner, int) }
        SERVEREDIT_CHK_PROP_CACHED("virtualserver_hostbanner_mode", permission::b_virtualserver_modify_hostbanner, int) }

        SERVEREDIT_CHK_PROP_CACHED("virtualserver_hostbutton_tooltip", permission::b_virtualserver_modify_hostbutton, string) }
        SERVEREDIT_CHK_PROP_CACHED("virtualserver_hostbutton_url", permission::b_virtualserver_modify_hostbutton, string) }
        SERVEREDIT_CHK_PROP_CACHED("virtualserver_hostbutton_gfx_url", permission::b_virtualserver_modify_hostbutton, string) }

        SERVEREDIT_CHK_PROP_CACHED("virtualserver_hostmessage", permission::b_virtualserver_modify_hostmessage, string) }
        SERVEREDIT_CHK_PROP_CACHED("virtualserver_hostmessage_mode", permission::b_virtualserver_modify_hostmessage, int) }
        SERVEREDIT_CHK_PROP_CACHED("virtualserver_welcomemessage", permission::b_virtualserver_modify_welcomemessage, string) }

        SERVEREDIT_CHK_PROP_CACHED("virtualserver_needed_identity_security_level", permission::b_virtualserver_modify_needed_identity_security_level, int) }

        SERVEREDIT_CHK_PROP_CACHED("virtualserver_antiflood_points_tick_reduce", permission::b_virtualserver_modify_antiflood, uint64_t) }
        SERVEREDIT_CHK_PROP_CACHED("virtualserver_antiflood_points_needed_command_block", permission::b_virtualserver_modify_antiflood, uint64_t) }
        SERVEREDIT_CHK_PROP_CACHED("virtualserver_antiflood_points_needed_ip_block", permission::b_virtualserver_modify_antiflood, uint64_t) }

        SERVEREDIT_CHK_PROP_CACHED("virtualserver_complain_autoban_count", permission::b_virtualserver_modify_complain, uint64_t) }
        SERVEREDIT_CHK_PROP_CACHED("virtualserver_complain_autoban_time", permission::b_virtualserver_modify_complain, uint64_t) }
        SERVEREDIT_CHK_PROP_CACHED("virtualserver_complain_remove_time", permission::b_virtualserver_modify_complain, uint64_t) }

        SERVEREDIT_CHK_PROP_CACHED("virtualserver_autostart", permission::b_virtualserver_modify_autostart, bool) }
        SERVEREDIT_CHK_PROP_CACHED("virtualserver_min_clients_in_channel_before_forced_silence", permission::b_virtualserver_modify_channel_forced_silence, int) }

        SERVEREDIT_CHK_PROP_CACHED("virtualserver_log_client", permission::b_virtualserver_modify_log_settings, bool) }
        SERVEREDIT_CHK_PROP_CACHED("virtualserver_log_query", permission::b_virtualserver_modify_log_settings, bool) }
        SERVEREDIT_CHK_PROP_CACHED("virtualserver_log_channel", permission::b_virtualserver_modify_log_settings, bool) }
        SERVEREDIT_CHK_PROP_CACHED("virtualserver_log_permissions", permission::b_virtualserver_modify_log_settings, bool) }
        SERVEREDIT_CHK_PROP_CACHED("virtualserver_log_server", permission::b_virtualserver_modify_log_settings, bool) }
        SERVEREDIT_CHK_PROP_CACHED("virtualserver_log_filetransfer", permission::b_virtualserver_modify_log_settings, bool) }

        SERVEREDIT_CHK_PROP_CACHED("virtualserver_min_client_version", permission::b_virtualserver_modify_min_client_version, uint64_t) }
        SERVEREDIT_CHK_PROP_CACHED("virtualserver_min_android_version", permission::b_virtualserver_modify_min_client_version, uint64_t) }
        SERVEREDIT_CHK_PROP_CACHED("virtualserver_min_ios_version", permission::b_virtualserver_modify_min_client_version, uint64_t) }


        SERVEREDIT_CHK_PROP_CACHED("virtualserver_music_bot_limit", permission::b_virtualserver_modify_music_bot_limit, uint64_t) }
        SERVEREDIT_CHK_PROP_CACHED("virtualserver_flag_password", permission::b_virtualserver_modify_password, bool)
            if (cmd["virtualserver_flag_password"].as<bool>() && !cmd[0].has("virtualserver_password"))
                return command_result{error::parameter_invalid};
        } SERVEREDIT_CHK_PROP_CACHED("virtualserver_password", permission::b_virtualserver_modify_password, string)
            if(cmd["virtualserver_password"].string().empty()) {
                toApplay["virtualserver_flag_password"] = "0";
            } else {
                toApplay["virtualserver_flag_password"] = "1";
                if(this->getType() == CLIENT_QUERY)
                    toApplay["virtualserver_password"] = base64::encode(digest::sha1(cmd["virtualserver_password"].string()));
            }
        }
        SERVEREDIT_CHK_PROP_CACHED("virtualserver_default_client_description", permission::b_virtualserver_modify_default_messages, string) }
        SERVEREDIT_CHK_PROP_CACHED("virtualserver_default_channel_description", permission::b_virtualserver_modify_default_messages, string) }
        SERVEREDIT_CHK_PROP_CACHED("virtualserver_default_channel_topic", permission::b_virtualserver_modify_default_messages, string) }
        SERVEREDIT_CHK_PROP_CACHED("virtualserver_country_code", permission::b_virtualserver_modify_country_code, string) }
        SERVEREDIT_CHK_PROP2("virtualserver_max_download_total_bandwidth", permission::b_virtualserver_modify_ft_settings, uint64_t, int64_t)
            if(cmd["virtualserver_max_download_total_bandwidth"].string().find('-') == string::npos)
                toApplay["virtualserver_max_download_total_bandwidth"] = to_string((int64_t) cmd["virtualserver_max_download_total_bandwidth"].as<uint64_t>());
            else
                toApplay["virtualserver_max_download_total_bandwidth"] = to_string(cmd["virtualserver_max_download_total_bandwidth"].as<int64_t>());
        }
        SERVEREDIT_CHK_PROP2("virtualserver_max_upload_total_bandwidth", permission::b_virtualserver_modify_ft_settings, uint64_t, int64_t)
            if(cmd["virtualserver_max_upload_total_bandwidth"].string().find('-') == string::npos)
                toApplay["virtualserver_max_upload_total_bandwidth"] = to_string((int64_t) cmd["virtualserver_max_upload_total_bandwidth"].as<uint64_t>());
            else
                toApplay["virtualserver_max_upload_total_bandwidth"] = to_string(cmd["virtualserver_max_upload_total_bandwidth"].as<int64_t>());
        }
        SERVEREDIT_CHK_PROP2("virtualserver_download_quota", permission::b_virtualserver_modify_ft_quotas, uint64_t, int64_t)
            if(cmd["virtualserver_download_quota"].string().find('-') == string::npos)
                toApplay["virtualserver_download_quota"] = to_string((int64_t) cmd["virtualserver_download_quota"].as<uint64_t>());
            else
                toApplay["virtualserver_download_quota"] = to_string(cmd["virtualserver_download_quota"].as<int64_t>());
        }
        SERVEREDIT_CHK_PROP2("virtualserver_upload_quota", permission::b_virtualserver_modify_ft_quotas, uint64_t, int64_t)
            if(cmd["virtualserver_upload_quota"].string().find('-') == string::npos)
                toApplay["virtualserver_upload_quota"] = to_string((int64_t) cmd["virtualserver_upload_quota"].as<uint64_t>());
            else
                toApplay["virtualserver_upload_quota"] = to_string(cmd["virtualserver_upload_quota"].as<int64_t>());
        }
        else {
            logError(target_server ? target_server->getServerId() : 0, "Client " + this->getDisplayName() + " tried to change a not existing server properties. (" + key + ")");
            //return command_result{error::not_implemented};
        }
    }

    std::deque<std::string> keys;
    bool group_update = false;
    for (const auto& elm : toApplay) {
        const auto& info = property::find<property::VirtualServerProperties>(elm.first);
        if(info == property::VIRTUALSERVER_UNDEFINED) {
            logCritical(target_server ? target_server->getServerId() : 0, "Missing server property " + elm.first);
            continue;
        }

        if(!info.validate_input(elm.second)) {
            logError(target_server ? target_server->getServerId() : 0, "Client " + this->getDisplayName() + " tried to change a property to an invalid value. (Value: '" + elm.second + "', Property: '" + std::string{info.name} + "')");
            continue;
        }

        auto property = target_server ? target_server->properties()[info] : serverInstance->getDefaultServerProperties()[info];
        if(property.value() == elm.second)
            continue;
        auto old_value = property.value();
        property = elm.second;
        serverInstance->action_logger()->server_edit_logger.log_server_edit(serverId, this->ref(), info, old_value, elm.second);
        keys.push_back(elm.first);

        group_update |= info == property::VIRTUALSERVER_DEFAULT_SERVER_GROUP || info == property::VIRTUALSERVER_DEFAULT_CHANNEL_GROUP || info == property::VIRTUALSERVER_DEFAULT_MUSIC_GROUP;
    }

    if(target_server) {
        if (group_update) {
            target_server->forEachClient([&](const shared_ptr<ConnectedClient> &client) {
                bool groups_changed;
                client->update_displayed_client_groups(groups_changed, groups_changed);

                if (groups_changed) {
                    client->task_update_needed_permissions.enqueue();
                    client->task_update_channel_client_properties.enqueue();
                }
            });
        }

        if (!keys.empty()) {
            target_server->notifyServerEdited(this->ref(), keys);
        }
    }
    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandServerRequestConnectionInfo(Command &) {
    CMD_REQ_SERVER;
    ACTION_REQUIRES_GLOBAL_PERMISSION(permission::b_virtualserver_connectioninfo_view, 1);

    ts::command_builder result{"notifyserverconnectioninfo"};
    auto first_bulk = result.bulk(0);

    auto total_stats = this->server->getServerStatistics()->total_stats();
    auto minute_report = this->server->getServerStatistics()->minute_stats();
    auto second_report = this->server->getServerStatistics()->second_stats();
    auto network_report = this->server->generate_network_report();

    first_bulk.put_unchecked(property::CONNECTION_FILETRANSFER_BANDWIDTH_SENT, minute_report.file_bytes_sent);
    first_bulk.put_unchecked(property::CONNECTION_FILETRANSFER_BANDWIDTH_RECEIVED, minute_report.file_bytes_received);

    first_bulk.put_unchecked(property::CONNECTION_FILETRANSFER_BYTES_SENT_TOTAL,
                             this->server->properties()[property::VIRTUALSERVER_TOTAL_BYTES_DOWNLOADED].as_unchecked<string>());
    first_bulk.put_unchecked(property::CONNECTION_FILETRANSFER_BYTES_RECEIVED_TOTAL,
                             this->server->properties()[property::VIRTUALSERVER_TOTAL_BYTES_UPLOADED].as_unchecked<string>());

    first_bulk.put_unchecked("connection_filetransfer_bytes_sent_month",
                             this->server->properties()[property::VIRTUALSERVER_MONTH_BYTES_DOWNLOADED].as_unchecked<string>());
    first_bulk.put_unchecked("connection_filetransfer_bytes_received_month",
                             this->server->properties()[property::VIRTUALSERVER_MONTH_BYTES_UPLOADED].as_unchecked<string>());

    first_bulk.put_unchecked(property::CONNECTION_PACKETS_SENT_TOTAL, std::accumulate(total_stats.connection_packets_sent.begin(), total_stats.connection_packets_sent.end(), (size_t) 0U));
    first_bulk.put_unchecked(property::CONNECTION_BYTES_SENT_TOTAL, std::accumulate(total_stats.connection_bytes_sent.begin(), total_stats.connection_bytes_sent.end(), (size_t) 0U));
    first_bulk.put_unchecked(property::CONNECTION_PACKETS_RECEIVED_TOTAL, std::accumulate(total_stats.connection_packets_received.begin(), total_stats.connection_packets_received.end(), (size_t) 0U));
    first_bulk.put_unchecked(property::CONNECTION_BYTES_RECEIVED_TOTAL, std::accumulate(total_stats.connection_bytes_received.begin(), total_stats.connection_bytes_received.end(), (size_t) 0U));

    first_bulk.put_unchecked(property::CONNECTION_BANDWIDTH_SENT_LAST_SECOND_TOTAL, std::accumulate(second_report.connection_bytes_sent.begin(), second_report.connection_bytes_sent.end(), (size_t) 0U));
    first_bulk.put_unchecked(property::CONNECTION_BANDWIDTH_SENT_LAST_MINUTE_TOTAL, std::accumulate(minute_report.connection_bytes_sent.begin(), minute_report.connection_bytes_sent.end(), (size_t) 0U));
    first_bulk.put_unchecked(property::CONNECTION_BANDWIDTH_RECEIVED_LAST_SECOND_TOTAL, std::accumulate(second_report.connection_bytes_received.begin(), second_report.connection_bytes_received.end(), (size_t) 0U));
    first_bulk.put_unchecked(property::CONNECTION_BANDWIDTH_RECEIVED_LAST_MINUTE_TOTAL, std::accumulate(minute_report.connection_bytes_received.begin(), minute_report.connection_bytes_received.end(), (size_t) 0U));

    first_bulk.put_unchecked(property::CONNECTION_CONNECTED_TIME,
                             this->server->properties()[property::VIRTUALSERVER_UPTIME].as_unchecked<string>());
    first_bulk.put_unchecked(property::CONNECTION_PACKETLOSS_TOTAL, network_report.average_loss);
    first_bulk.put_unchecked(property::CONNECTION_PING, network_report.average_ping);

    this->sendCommand(result);
    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandServerGroupAdd(Command &cmd) {
    return this->handleCommandGroupAdd(cmd, GroupTarget::GROUPTARGET_SERVER);
}

command_result ConnectedClient::handleCommandServerGroupCopy(Command &cmd) {
    return this->handleCommandGroupCopy(cmd, GroupTarget::GROUPTARGET_SERVER);
}

command_result ConnectedClient::handleCommandServerGroupRename(Command &cmd) {
    return this->handleCommandGroupRename(cmd, GroupTarget::GROUPTARGET_SERVER);
}

command_result ConnectedClient::handleCommandServerGroupDel(Command &cmd) {
    return this->handleCommandGroupDel(cmd, GroupTarget::GROUPTARGET_SERVER);
}

command_result ConnectedClient::handleCommandServerGroupList(Command &) {
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(5);
    ACTION_REQUIRES_GLOBAL_PERMISSION(permission::b_virtualserver_servergroup_list, 1);

    std::optional<ts::command_builder> generated_command{};
    this->notifyServerGroupList(generated_command, this->getType() != ClientType::CLIENT_QUERY);
    this->command_times.servergrouplist = system_clock::now();
    return command_result{error::ok};
}

//servergroupclientlist sgid=2
//notifyservergroupclientlist sgid=6 cldbid=2 client_nickname=WolverinDEV client_unique_identifier=xxjnc14LmvTk+Lyrm8OOeo4tOqw=
command_result ConnectedClient::handleCommandServerGroupClientList(Command &cmd) {
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(5);

    ACTION_REQUIRES_GLOBAL_PERMISSION(permission::b_virtualserver_servergroup_client_list, 1);

    std::shared_ptr<VirtualServer> target_server{};
    if(cmd[0].has("sid")) {
        auto server_id = cmd["sid"].as<ServerId>();
        if(server_id == 0) {
            /* that's all right */
            target_server = this->server;
        } else if(server_id == this->getServerId()) {
            /* this is also allowed */
        } else {
            return ts::command_result{error::server_invalid_id};
        }
    } else {
        target_server = this->server;
    }

    auto group_manager = target_server ? target_server->group_manager() : serverInstance->group_manager();
    auto server_group = group_manager->server_groups()->find_group(groups::GroupCalculateMode::GLOBAL, cmd["sgid"].as<GroupId>());
    if (!server_group) {
        return command_result{error::group_invalid_id};
    }

    ts::command_builder notify{this->notify_response_command("notifyservergroupclientlist")};
    notify.put_unchecked(0, "sgid", server_group->group_id());

    int index{0};
    for (const auto &assignment : group_manager->assignments().server_group_clients(server_group->group_id(), true)) {
        auto bulk = notify.bulk(index++);
        bulk.put_unchecked("cldbid", assignment.client_database_id);
        bulk.put_unchecked("client_nickname", assignment.client_display_name.value_or(""));
        bulk.put_unchecked("client_unique_identifier", assignment.client_unique_id.value_or(""));
    }

    if(index == 0) {
        if(this->getType() != ClientType::CLIENT_TEAMSPEAK) {
            /* TS3 clients don't want a error here. They're fine with just not receiving a notify */
            return ts::command_result{error::database_empty_result};
        }
    } else {
        this->sendCommand(notify);
    }

    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandServerGroupAddClient(Command &cmd) {
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(25);

    std::shared_ptr<VirtualServer> target_server{};
    if(cmd[0].has("sid")) {
        auto server_id = cmd["sid"].as<ServerId>();
        if(server_id == 0) {
            /* that's all right */
        } else if(server_id == this->getServerId()) {
            /* this is also allowed */
            target_server = this->server;
        } else {
            return ts::command_result{error::server_invalid_id};
        }
    } else {
        target_server = this->server;
    }

    auto group_manager = target_server ? this->server->group_manager() : serverInstance->group_manager();

    auto target_cldbid = cmd["cldbid"].as<ClientDbId>();
    if (!serverInstance->databaseHelper()->validClientDatabaseId(target_server, cmd["cldbid"])) {
        return command_result{error::client_invalid_id, "invalid cldbid"};
    }

    ClientPermissionCalculator client_permissions{target_server, target_cldbid, ClientType::CLIENT_TEAMSPEAK, 0};
    if(!permission::v2::permission_granted(client_permissions.calculate_permission(permission::i_client_needed_permission_modify_power).zero_if_unset(), this->calculate_permission(permission::i_client_permission_modify_power, 0))) {
        return command_result{permission::i_client_needed_permission_modify_power};
    }

    std::vector<std::shared_ptr<groups::ServerGroup>> added_groups{};
    added_groups.reserve(cmd.bulkCount());

    ts::command_result_bulk result{};
    result.reserve(cmd.bulkCount());

    {
        auto permission_add_power = this->calculate_permission(permission::i_server_group_member_add_power, -1);
        auto permission_self_add_power = this->calculate_permission(permission::i_server_group_self_add_power, -1);

        for(auto index = 0; index < cmd.bulkCount(); index++) {
            auto group_id = cmd[index]["sgid"].as<GroupId>();
            auto group = group_manager->server_groups()->find_group(groups::GroupCalculateMode::GLOBAL, group_id);
            if(!group) {
                result.emplace_result(error::group_invalid_id);
                continue;
            }

            /* permission tests */
            if(!group->permission_granted(permission::i_server_group_needed_member_add_power, permission_add_power, true)) {
                if(target_cldbid != this->getClientDatabaseId()) {
                    result.emplace_result(permission::i_server_group_member_add_power);
                    continue;
                }

                if(!group->permission_granted(permission::i_server_group_needed_member_add_power, permission_self_add_power, true)) {
                    result.emplace_result(permission::i_server_group_self_add_power);
                    continue;
                }
            }

            switch (group_manager->assignments().add_server_group(target_cldbid, group_id, !group->is_permanent())) {
                case groups::GroupAssignmentResult::SUCCESS:
                    break;

                case groups::GroupAssignmentResult::ADD_ALREADY_MEMBER_OF_GROUP:
                    result.emplace_result(error::client_is_already_member_of_group);
                    continue;

                case groups::GroupAssignmentResult::SET_ALREADY_MEMBER_OF_GROUP:
                case groups::GroupAssignmentResult::REMOVE_NOT_MEMBER_OF_GROUP:
                default:
                    result.emplace_result(error::vs_critical);
                    continue;
            }

            added_groups.push_back(group);
            result.emplace_result(error::ok);
        }
    }

    std::deque<std::shared_ptr<VirtualServer>> updated_servers{};
    if(target_server) {
        updated_servers.push_back(target_server);
    } else {
        updated_servers = serverInstance->getVoiceServerManager()->serverInstances();
    }

    auto invoker = this->ref();
    std::shared_ptr<ConnectedClient> client_instance{};
    for(const auto& updated_server : updated_servers) {
        for (const auto &updated_client : updated_server->findClientsByCldbId(target_cldbid)) {
            client_instance = updated_client;

            bool groups_changed;
            updated_client->update_displayed_client_groups(groups_changed, groups_changed);

            /* join permissions have changed */
            updated_client->join_state_id++;

            if(groups_changed) {
                updated_client->task_update_needed_permissions.enqueue();
                updated_client->task_update_channel_client_properties.enqueue();
            }

            auto client_list = updated_server->getClients();
            for(const auto& group : added_groups) {
                std::optional<ts::command_builder> notify{};
                for(const auto& client : client_list) {
                    client->notifyServerGroupClientAdd(notify, invoker, updated_client, group->group_id());
                }
            }
        }
    }

    for(const auto& group : added_groups) {
        serverInstance->action_logger()->group_assignment_logger.log_group_assignment_add(target_server ? target_server->getServerId() : 0,
                                                                                          this->ref(), log::GroupTarget::SERVER,
                                                                                          group->group_id(), group->display_name(),
                                                                                          target_cldbid, client_instance ? client_instance->getDisplayName() : ""
        );
    }

    return ts::command_result{std::move(result)};
}

command_result ConnectedClient::handleCommandServerGroupDelClient(Command &cmd) {
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(25);

    std::shared_ptr<VirtualServer> target_server{};
    if(cmd[0].has("sid")) {
        auto server_id = cmd["sid"].as<ServerId>();
        if(server_id == 0) {
            /* that's all right */
        } else if(server_id == this->getServerId()) {
            /* this is also allowed */
            target_server = this->server;
        } else {
            return ts::command_result{error::server_invalid_id};
        }
    } else {
        target_server = this->server;
    }

    auto group_manager = target_server ? this->server->group_manager() : serverInstance->group_manager();

    auto target_cldbid = cmd["cldbid"].as<ClientDbId>();
    if (!serverInstance->databaseHelper()->validClientDatabaseId(target_server, cmd["cldbid"])) {
        return command_result{error::client_invalid_id, "invalid cldbid"};
    }

    ClientPermissionCalculator client_permissions{target_server, target_cldbid, ClientType::CLIENT_TEAMSPEAK, 0};
    if(!permission::v2::permission_granted(client_permissions.calculate_permission(permission::i_client_needed_permission_modify_power).zero_if_unset(), this->calculate_permission(permission::i_client_permission_modify_power, 0))) {
        return command_result{permission::i_client_needed_permission_modify_power};
    }

    std::vector<std::shared_ptr<groups::ServerGroup>> removed_groups{};
    removed_groups.reserve(cmd.bulkCount());

    ts::command_result_bulk result{};
    result.reserve(cmd.bulkCount());

    {
        auto permission_remove_power = this->calculate_permission(permission::i_server_group_member_remove_power, -1);
        auto permission_self_remove_power = this->calculate_permission(permission::i_server_group_self_remove_power, -1);

        for(auto index = 0; index < cmd.bulkCount(); index++) {
            auto group_id = cmd[index]["sgid"].as<GroupId>();
            auto group = group_manager->server_groups()->find_group(groups::GroupCalculateMode::GLOBAL, group_id);
            if(!group) {
                result.emplace_result(error::group_invalid_id);
                continue;
            }

            /* permission tests */
            if(!group->permission_granted(permission::i_server_group_needed_member_remove_power, permission_remove_power, true)) {
                if(target_cldbid != this->getClientDatabaseId()) {
                    result.emplace_result(permission::i_server_group_member_remove_power);
                    continue;
                }

                if(!group->permission_granted(permission::i_server_group_needed_member_remove_power, permission_self_remove_power, true)) {
                    result.emplace_result(permission::i_server_group_self_remove_power);
                    continue;
                }
            }

            switch (group_manager->assignments().remove_server_group(target_cldbid, group_id)) {
                case groups::GroupAssignmentResult::SUCCESS:
                    break;

                case groups::GroupAssignmentResult::REMOVE_NOT_MEMBER_OF_GROUP:
                    result.emplace_result(error::client_is_already_member_of_group);
                    continue;

                case groups::GroupAssignmentResult::ADD_ALREADY_MEMBER_OF_GROUP:
                case groups::GroupAssignmentResult::SET_ALREADY_MEMBER_OF_GROUP:
                default:
                    result.emplace_result(error::vs_critical);
                    continue;
            }

            removed_groups.push_back(group);
            result.emplace_result(error::ok);
        }
    }

    std::deque<std::shared_ptr<VirtualServer>> updated_servers{};
    if(target_server) {
        updated_servers.push_back(target_server);
    } else {
        updated_servers = serverInstance->getVoiceServerManager()->serverInstances();
    }

    auto invoker = this->ref();
    std::shared_ptr<ConnectedClient> client_instance{};
    for(const auto& updated_server : updated_servers) {
        for (const auto &updated_client : updated_server->findClientsByCldbId(target_cldbid)) {
            client_instance = updated_client;

            bool groups_changed;
            updated_client->update_displayed_client_groups(groups_changed, groups_changed);

            /* join permissions have changed */
            updated_client->join_state_id++;

            if(groups_changed) {
                updated_client->task_update_needed_permissions.enqueue();
                updated_client->task_update_channel_client_properties.enqueue();
            }

            auto client_list = updated_server->getClients();
            for(const auto& group : removed_groups) {
                std::optional<ts::command_builder> notify{};
                for(const auto& client : client_list) {
                    client->notifyServerGroupClientRemove(notify, invoker, updated_client, group->group_id());
                }
            }
        }
    }

    for(const auto& group : removed_groups) {
        serverInstance->action_logger()->group_assignment_logger.log_group_assignment_remove(target_server ? target_server->getServerId() : 0,
                                                                                          this->ref(), log::GroupTarget::SERVER,
                                                                                          group->group_id(), group->display_name(),
                                                                                          target_cldbid, client_instance ? client_instance->getDisplayName() : ""
        );
    }

    return ts::command_result{std::move(result)};
}

command_result ConnectedClient::handleCommandServerGroupPermList(Command &cmd) {
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(5);
    ACTION_REQUIRES_GLOBAL_PERMISSION(permission::b_virtualserver_servergroup_permission_list, 1);

    auto group_manager = this->server ? this->server->group_manager() : serverInstance->group_manager();
    auto serverGroup = group_manager->server_groups()->find_group(groups::GroupCalculateMode::GLOBAL, cmd["sgid"].as<GroupId>());
    if (!serverGroup) {
        return command_result{error::group_invalid_id};
    }

    if(this->getType() == ClientType::CLIENT_TEAMSPEAK && this->command_times.last_notify + this->command_times.notify_timeout < system_clock::now()) {
        this->sendTSPermEditorWarning();
    }

    if (!this->notifyGroupPermList(serverGroup, cmd.hasParm("permsid"))) {
        return command_result{error::database_empty_result};
    }

    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandServerGroupAddPerm(Command &cmd) {
    CMD_CHK_AND_INC_FLOOD_POINTS(5);

    auto group_id = cmd["sgid"].as<GroupId>();

    std::shared_ptr<groups::ServerGroupManager> owning_manager{};
    auto group_manager = this->server ? this->server->group_manager() : serverInstance->group_manager();
    auto group = group_manager->server_groups()->find_group_ext(owning_manager, groups::GroupCalculateMode::GLOBAL, group_id);

    if(!group) {
        return ts::command_result{error::group_invalid_id};
    }

    ACTION_REQUIRES_GROUP_PERMISSION(group, permission::i_server_group_needed_modify_power, permission::i_server_group_modify_power, true);

    auto target_server = group_manager->server_groups() == owning_manager ? this->server : nullptr;
    return this->executeGroupPermissionEdit(cmd, { group }, target_server, permission::v2::PermissionUpdateType::set_value);
}

command_result ConnectedClient::handleCommandServerGroupDelPerm(Command &cmd) {
    CMD_CHK_AND_INC_FLOOD_POINTS(5);

    auto group_id = cmd["sgid"].as<GroupId>();

    std::shared_ptr<groups::ServerGroupManager> owning_manager{};
    auto group_manager = this->server ? this->server->group_manager() : serverInstance->group_manager();
    auto group = group_manager->server_groups()->find_group_ext(owning_manager, groups::GroupCalculateMode::GLOBAL, group_id);

    if(!group) {
        return ts::command_result{error::group_invalid_id};
    }

    ACTION_REQUIRES_GROUP_PERMISSION(group, permission::i_server_group_needed_modify_power, permission::i_server_group_modify_power, true);

    auto target_server = group_manager->server_groups() == owning_manager ? this->server : nullptr;
    return this->executeGroupPermissionEdit(cmd, { group }, target_server, permission::v2::PermissionUpdateType::delete_value);
}

command_result ConnectedClient::handleCommandServerGroupAutoAddPerm(ts::Command& cmd) {
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(25);

    auto update_type = cmd["sgtype"].as<permission::PermissionValue>();

    auto ref_server = this->server;
    auto group_manager = ref_server ? ref_server->group_manager() : serverInstance->group_manager();

    std::vector<std::shared_ptr<groups::Group>> target_groups{};
    auto available_groups = group_manager->server_groups()->available_groups(groups::GroupCalculateMode::GLOBAL);
    target_groups.reserve(available_groups.size());

    for(const auto& group : available_groups) {
        if(group->update_type() != update_type) {
            continue;
        }

        target_groups.push_back(group);
    }

    if(target_groups.empty()) {
        return ts::command_result{error::database_empty_result};
    }

    return this->executeGroupPermissionEdit(cmd, target_groups, nullptr, permission::v2::PermissionUpdateType::set_value);
}

command_result ConnectedClient::handleCommandServerGroupAutoDelPerm(ts::Command& cmd) {
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(25);

    auto update_type = cmd["sgtype"].as<permission::PermissionValue>();

    auto ref_server = this->server;
    auto group_manager = ref_server ? ref_server->group_manager() : serverInstance->group_manager();

    std::vector<std::shared_ptr<groups::Group>> target_groups{};
    auto available_groups = group_manager->server_groups()->available_groups(groups::GroupCalculateMode::GLOBAL);
    target_groups.reserve(available_groups.size());

    for(const auto& group : available_groups) {
        if(group->update_type() != update_type) {
            continue;
        }

        target_groups.push_back(group);
    }

    if(target_groups.empty()) {
        return ts::command_result{error::database_empty_result};
    }

    return this->executeGroupPermissionEdit(cmd, target_groups, nullptr, permission::v2::PermissionUpdateType::delete_value);
}

command_result ConnectedClient::handleCommandServerGroupsByClientId(Command &cmd) {
    CMD_RESET_IDLE;

    ClientDbId cldbid = cmd["cldbid"];
    if(!serverInstance->databaseHelper()->validClientDatabaseId(this->server, cldbid)) {
        return command_result{error::client_invalid_id};
    }

    auto group_manager = this->server ? this->server->group_manager() : serverInstance->group_manager();
    auto assigned_group_ids = group_manager->assignments().server_groups_of_client(groups::GroupAssignmentCalculateMode::GLOBAL, cldbid);
    if(assigned_group_ids.empty()) {
        return ts::command_result{error::database_empty_result};
    }

    ts::command_builder notify{this->notify_response_command("notifyservergroupsbyclientid"), 64, assigned_group_ids.size()};
    size_t index{0};
    for(const auto& group_id : assigned_group_ids) {
        auto group = group_manager->server_groups()->find_group(groups::GroupCalculateMode::GLOBAL, group_id);
        if(!group) {
            continue;
        }

        auto bulk = notify.bulk(index++);
        bulk.put_unchecked("name", group->display_name());
        bulk.put_unchecked("sgid", group->group_id());
        bulk.put_unchecked("cldbid", cldbid);
    }

    this->sendCommand(notify);

    return command_result{error::ok};
}

















