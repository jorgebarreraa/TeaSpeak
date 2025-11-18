//
// Created by wolverindev on 26.01.20.
//

#include <memory>

#include <iostream>
#include <bitset>
#include <algorithm>
#include <openssl/sha.h>
#include "../../build.h"
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
#include "../../absl/btree/map.h"
#include "../../PermissionCalculator.h"
#include <experimental/filesystem>
#include <cstdint>
#include <StringVariable.h>
#include <misc/digest.h>

#include "helpers.h"

#include <log/LogUtils.h>
#include <misc/sassert.h>
#include <misc/rnd.h>
#include <bbcode/bbcodes.h>

namespace fs = std::experimental::filesystem;
using namespace std::chrono;
using namespace std;
using namespace ts;
using namespace ts::server;
using namespace ts::server::token;

#define QUERY_PASSWORD_LENGTH 12

command_result ConnectedClient::handleCommand(Command &cmd) {
    threads::MutexLock l2(this->command_lock);
    auto command = cmd.command();
    if (command == "servergetvariables") return this->handleCommandServerGetVariables(cmd);
    else if (command == "serverrequestconnectioninfo") return this->handleCommandServerRequestConnectionInfo(cmd);
    else if (command == "getconnectioninfo") return this->handleCommandGetConnectionInfo(cmd);
    else if (command == "setconnectioninfo") return this->handleCommandSetConnectionInfo(cmd);
    else if (command == "clientgetvariables") return this->handleCommandClientGetVariables(cmd);
    else if (command == "serveredit") return this->handleCommandServerEdit(cmd);
    else if (command == "clientedit") return this->handleCommandClientEdit(cmd);
    else if (command == "channelgetdescription") return this->handleCommandChannelGetDescription(cmd);
    else if (command == "connectioninfoautoupdate") return this->handleCommandConnectionInfoAutoUpdate(cmd);
    else if (command == "permissionlist") return this->handleCommandPermissionList(cmd);
    else if (command == "propertylist") return this->handleCommandPropertyList(cmd);

        //Server group
    else if (command == "servergrouplist") return this->handleCommandServerGroupList(cmd);
    else if (command == "servergroupadd") return this->handleCommandServerGroupAdd(cmd);
    else if (command == "servergroupcopy") return this->handleCommandServerGroupCopy(cmd);
    else if (command == "servergroupdel") return this->handleCommandServerGroupDel(cmd);
    else if (command == "servergrouprename") return this->handleCommandServerGroupRename(cmd);
    else if (command == "servergroupclientlist") return this->handleCommandServerGroupClientList(cmd);
    else if (command == "servergroupaddclient" || command == "clientaddservergroup") return this->handleCommandServerGroupAddClient(cmd);
    else if (command == "servergroupdelclient" || command == "clientdelservergroup") return this->handleCommandServerGroupDelClient(cmd);
    else if (command == "servergrouppermlist") return this->handleCommandServerGroupPermList(cmd);
    else if (command == "servergroupaddperm") return this->handleCommandServerGroupAddPerm(cmd);
    else if (command == "servergroupdelperm") return this->handleCommandServerGroupDelPerm(cmd);

    else if (command == "setclientchannelgroup") return this->handleCommandSetClientChannelGroup(cmd);

        //Channel basic actions
    else if (command == "channelcreate") return this->handleCommandChannelCreate(cmd);
    else if (command == "channelmove") return this->handleCommandChannelMove(cmd);
    else if (command == "channeledit") return this->handleCommandChannelEdit(cmd);
    else if (command == "channeldelete") return this->handleCommandChannelDelete(cmd);
        //Find a channel and get informations
    else if (command == "channelfind") return this->handleCommandChannelFind(cmd);
    else if (command == "channelinfo") return this->handleCommandChannelInfo(cmd);
        //Channel perm actions
    else if (command == "channelpermlist") return this->handleCommandChannelPermList(cmd);
    else if (command == "channeladdperm") return this->handleCommandChannelAddPerm(cmd);
    else if (command == "channeldelperm") return this->handleCommandChannelDelPerm(cmd);
        //Channel group actions
    else if (command == "channelgroupadd") return this->handleCommandChannelGroupAdd(cmd);
    else if (command == "channelgroupcopy") return this->handleCommandChannelGroupCopy(cmd);
    else if (command == "channelgrouprename") return this->handleCommandChannelGroupRename(cmd);
    else if (command == "channelgroupdel") return this->handleCommandChannelGroupDel(cmd);
    else if (command == "channelgrouplist") return this->handleCommandChannelGroupList(cmd);
    else if (command == "channelgroupclientlist") return this->handleCommandChannelGroupClientList(cmd);
    else if (command == "channelgrouppermlist") return this->handleCommandChannelGroupPermList(cmd);
    else if (command == "channelgroupaddperm") return this->handleCommandChannelGroupAddPerm(cmd);
    else if (command == "channelgroupdelperm") return this->handleCommandChannelGroupDelPerm(cmd);
        //Channel sub/unsubscribe
    else if (command == "channelsubscribe") return this->handleCommandChannelSubscribe(cmd);
    else if (command == "channelsubscribeall") return this->handleCommandChannelSubscribeAll(cmd);
    else if (command == "channelunsubscribe") return this->handleCommandChannelUnsubscribe(cmd);
    else if (command == "channelunsubscribeall") return this->handleCommandChannelUnsubscribeAll(cmd);
        //manager channel permissions
    else if (command == "channelclientpermlist") return this->handleCommandChannelClientPermList(cmd);
    else if (command == "channelclientaddperm") return this->handleCommandChannelClientAddPerm(cmd);
    else if (command == "channelclientdelperm") return this->handleCommandChannelClientDelPerm(cmd);
        //Client actions
    else if (command == "clientupdate") return this->handleCommandClientUpdate(cmd);
    else if (command == "clientmove") return this->handleCommandClientMove(cmd);
    else if (command == "clientgetids") return this->handleCommandClientGetIds(cmd);
    else if (command == "clientkick") return this->handleCommandClientKick(cmd);
    else if (command == "clientpoke") return this->handleCommandClientPoke(cmd);
    else if (command == "sendtextmessage") return this->handleCommandSendTextMessage(cmd);
    else if (command == "clientchatcomposing") return this->handleCommandClientChatComposing(cmd);
    else if (command == "clientchatclosed") return this->handleCommandClientChatClosed(cmd);

    else if (command == "clientfind") return this->handleCommandClientFind(cmd);
    else if (command == "clientinfo") return this->handleCommandClientInfo(cmd);

    else if (command == "clientaddperm") return this->handleCommandClientAddPerm(cmd);
    else if (command == "clientdelperm") return this->handleCommandClientDelPerm(cmd);
    else if (command == "clientpermlist") return this->handleCommandClientPermList(cmd);
        //File transfare
    else if (command == "ftgetfilelist") return this->handleCommandFTGetFileList(cmd);
    else if (command == "ftcreatedir") return this->handleCommandFTCreateDir(cmd);
    else if (command == "ftdeletefile") return this->handleCommandFTDeleteFile(cmd);
    else if (command == "ftinitupload") {
        auto result = this->handleCommandFTInitUpload(cmd);
        if(result.has_error() && this->getType() == ClientType::CLIENT_TEAMSPEAK) {
            ts::command_builder notify{"notifystatusfiletransfer"};
            notify.put_unchecked(0, "clientftfid", cmd["clientftfid"].string());
            notify.put(0, "size", 0);
            this->writeCommandResult(notify, result, "status");
            this->sendCommand(notify);
            result.release_data();

            return command_result{error::ok};
        }
        return result;
    }
    else if (command == "ftinitdownload") {
        auto result = this->handleCommandFTInitDownload(cmd);
        if(result.has_error() && this->getType() == ClientType::CLIENT_TEAMSPEAK) {
            ts::command_builder notify{"notifystatusfiletransfer"};
            notify.put_unchecked(0, "clientftfid", cmd["clientftfid"].string());
            notify.put(0, "size", 0);
            this->writeCommandResult(notify, result, "status");
            this->sendCommand(notify);
            result.release_data();

            return command_result{error::ok};
        }
        return result;
    }
    else if (command == "ftgetfileinfo") return this->handleCommandFTGetFileInfo(cmd);
    else if (command == "ftrenamefile") return this->handleCommandFTRenameFile(cmd);
    else if (command == "ftlist") return this->handleCommandFTList(cmd);
    else if (command == "ftstop") return this->handleCommandFTStop(cmd);
        //Banlist
    else if (command == "banlist") return this->handleCommandBanList(cmd);
    else if (command == "banadd") return this->handleCommandBanAdd(cmd);
    else if (command == "banedit") return this->handleCommandBanEdit(cmd);
    else if (command == "banclient") return this->handleCommandBanClient(cmd);
    else if (command == "bandel") return this->handleCommandBanDel(cmd);
    else if (command == "bandelall") return this->handleCommandBanDelAll(cmd);
    else if (command == "bantriggerlist") return this->handleCommandBanTriggerList(cmd);
        //Tokens
    else if (command == "tokenactionlist") return this->handleCommandTokenActionList(cmd);
    else if (command == "tokenlist" || command == "privilegekeylist") return this->handleCommandTokenList(cmd);
    else if (command == "tokenadd" || command == "privilegekeyadd") return this->handleCommandTokenAdd(cmd);
    else if (command == "tokenedit") return this->handleCommandTokenEdit(cmd);
    else if (command == "tokenuse" || command == "privilegekeyuse") return this->handleCommandTokenUse(cmd);
    else if (command == "tokendelete" || command == "privilegekeydelete") return this->handleCommandTokenDelete(cmd);

        //DB stuff
    else if (command == "clientdblist") return this->handleCommandClientDbList(cmd);
    else if (command == "clientdbinfo") return this->handleCommandClientDbInfo(cmd);
    else if (command == "clientdbedit") return this->handleCommandClientDBEdit(cmd);
    else if (command == "clientdbfind") return this->handleCommandClientDBFind(cmd);
    else if (command == "clientdbdelete") return this->handleCommandClientDBDelete(cmd);
    else if (command == "plugincmd") return this->handleCommandPluginCmd(cmd);

    else if (command == "clientmute") return this->handleCommandClientMute(cmd);
    else if (command == "clientunmute") return this->handleCommandClientUnmute(cmd);

    else if (command == "clientlist") return this->handleCommandClientList(cmd);
    else if (command == "whoami") return this->handleCommandWhoAmI(cmd);
    else if (command == "servergroupsbyclientid") return this->handleCommandServerGroupsByClientId(cmd);

    else if (command == "clientgetdbidfromuid") return this->handleCommandClientGetDBIDfromUID(cmd);
    else if (command == "clientgetnamefromdbid") return this->handleCommandClientGetNameFromDBID(cmd);
    else if (command == "clientgetnamefromuid") return this->handleCommandClientGetNameFromUid(cmd);
    else if (command == "clientgetuidfromclid") return this->handleCommandClientGetUidFromClid(cmd);


    else if (command == "complainadd") return this->handleCommandComplainAdd(cmd);
    else if (command == "complainlist") return this->handleCommandComplainList(cmd);
    else if (command == "complaindel") return this->handleCommandComplainDel(cmd);
    else if (command == "complaindelall") return this->handleCommandComplainDelAll(cmd);

    else if (command == "version") return this->handleCommandVersion(cmd);

    else if (command == "verifyserverpassword") return this->handleCommandVerifyServerPassword(cmd);
    else if (command == "verifychannelpassword") return this->handleCommandVerifyChannelPassword(cmd);

    else if (command == "messagelist") return this->handleCommandMessageList(cmd);
    else if (command == "messageadd") return this->handleCommandMessageAdd(cmd);
    else if (command == "messageget") return this->handleCommandMessageGet(cmd);
    else if (command == "messagedel") return this->handleCommandMessageDel(cmd);
    else if (command == "messageupdateflag") return this->handleCommandMessageUpdateFlag(cmd);

    else if (command == "permget") return this->handleCommandPermGet(cmd);
    else if (command == "permfind") return this->handleCommandPermFind(cmd);
    else if (command == "permidgetbyname") return this->handleCommandPermIdGetByName(cmd);
    else if (command == "permoverview") return this->handleCommandPermOverview(cmd);
    else if (command == "permreset") return this->handleCommandPermReset(cmd);

    else if (command == "clientsetserverquerylogin") return this->handleCommandClientSetServerQueryLogin(cmd);

        //Music stuff
    else if (command == "musicbotcreate") return this->handleCommandMusicBotCreate(cmd);
    else if (command == "musicbotdelete") return this->handleCommandMusicBotDelete(cmd);
    else if (command == "musicbotsetsubscription") return this->handleCommandMusicBotSetSubscription(cmd);
    else if (command == "musicbotplayerinfo") return this->handleCommandMusicBotPlayerInfo(cmd);
    else if (command == "musicbotplayeraction") return this->handleCommandMusicBotPlayerAction(cmd);
    else if (command == "musicbotqueuelist") return this->handleCommandMusicBotQueueList(cmd);
    else if (command == "musicbotqueueadd") return this->handleCommandMusicBotQueueAdd(cmd);
    else if (command == "musicbotqueueremove") return this->handleCommandMusicBotQueueRemove(cmd);
    else if (command == "musicbotqueuereorder") return this->handleCommandMusicBotQueueReorder(cmd);
    else if (command == "musicbotplaylistassign") return this->handleCommandMusicBotPlaylistAssign(cmd);

    else if (command == "help") return this->handleCommandHelp(cmd);

    else if (command == "logview") return this->handleCommandLogView(cmd);
    else if (command == "logquery") return this->handleCommandLogQuery(cmd);
    else if (command == "logadd") return this->handleCommandLogAdd(cmd);

    else if (command == "servergroupautoaddperm") return this->handleCommandServerGroupAutoAddPerm(cmd);
    else if (command == "servergroupautodelperm") return this->handleCommandServerGroupAutoDelPerm(cmd);

    else if (command == "updatemytsid") return this->handleCommandUpdateMyTsId(cmd);
    else if (command == "updatemytsdata") return this->handleCommandUpdateMyTsData(cmd);

    else if (command == "querycreate") return this->handleCommandQueryCreate(cmd);
    else if (command == "querydelete") return this->handleCommandQueryDelete(cmd);
    else if (command == "querylist") return this->handleCommandQueryList(cmd);
    else if (command == "queryrename") return this->handleCommandQueryRename(cmd);
    else if (command == "querychangepassword") return this->handleCommandQueryChangePassword(cmd);

    else if (command == "playlistlist") return this->handleCommandPlaylistList(cmd);
    else if (command == "playlistcreate") return this->handleCommandPlaylistCreate(cmd);
    else if (command == "playlistdelete") return this->handleCommandPlaylistDelete(cmd);
    else if (command == "playlistsetsubscription") return this->handleCommandPlaylistSetSubscription(cmd);

    else if (command == "playlistpermlist") return this->handleCommandPlaylistPermList(cmd);
    else if (command == "playlistaddperm") return this->handleCommandPlaylistAddPerm(cmd);
    else if (command == "playlistdelperm") return this->handleCommandPlaylistDelPerm(cmd);
    else if (command == "playlistclientlist") return this->handleCommandPlaylistClientList(cmd);
    else if (command == "playlistclientpermlist") return this->handleCommandPlaylistClientPermList(cmd);
    else if (command == "playlistclientaddperm") return this->handleCommandPlaylistClientAddPerm(cmd);
    else if (command == "playlistclientdelperm") return this->handleCommandPlaylistClientDelPerm(cmd);
    else if (command == "playlistinfo") return this->handleCommandPlaylistInfo(cmd);
    else if (command == "playlistedit") return this->handleCommandPlaylistEdit(cmd);

    else if (command == "playlistsonglist") return this->handleCommandPlaylistSongList(cmd);
    else if (command == "playlistsongsetcurrent") return this->handleCommandPlaylistSongSetCurrent(cmd);
    else if (command == "playlistsongadd") return this->handleCommandPlaylistSongAdd(cmd);
    else if (command == "playlistsongreorder" || command == "playlistsongmove") return this->handleCommandPlaylistSongReorder(cmd);
    else if (command == "playlistsongremove") return this->handleCommandPlaylistSongRemove(cmd);

    else if (command == "dummy_ipchange") return this->handleCommandDummy_IpChange(cmd);
    else if (command == "conversationhistory") return this->handleCommandConversationHistory(cmd);
    else if (command == "conversationfetch") return this->handleCommandConversationFetch(cmd);
    else if (command == "conversationmessagedelete") return this->handleCommandConversationMessageDelete(cmd);

    else if (command == "listfeaturesupport") return this->handleCommandListFeatureSupport(cmd);

    if (this->getType() == ClientType::CLIENT_QUERY)
        return command_result{error::command_not_found}; //Dont log query invalid commands

    if (this->getType() == ClientType::CLIENT_TEAMSPEAK)
        if (command.empty() || command.find_first_not_of(' ') == -1) {
            if (!permission::v2::permission_granted(1, this->calculate_permission(permission::b_client_allow_invalid_packet, this->getChannelId())))
                ((VoiceClient *) this)->disconnect(VREASON_SERVER_KICK, config::messages::kick_invalid_command, this->server ? this->server->serverAdmin : static_pointer_cast<ConnectedClient>(serverInstance->getInitialServerAdmin()), true);
        }

    logError(this->getServerId(), "Missing command '{}'", command);
    return command_result{error::command_not_found};
};

command_result ConnectedClient::handleCommandGetConnectionInfo(Command &cmd) {
    CMD_REQ_SERVER;
    CMD_CHK_AND_INC_FLOOD_POINTS(5);

    ConnectedLockedClient client{this->server->find_client_by_id(cmd["clid"].as<ClientId>())};
    if (!client) return command_result{error::client_invalid_id};

    bool send_temp{false};
    auto info = client->request_connection_info(this->ref(), send_temp);
    if (info || send_temp) {
        this->notifyConnectionInfo(client.client, info);
    } else if(this->getType() != ClientType::CLIENT_TEAMSPEAK)
        return command_result{error::no_cached_connection_info};

    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandSetConnectionInfo(Command &cmd) {
    auto info = std::make_shared<ConnectionInfoData>();
    info->timestamp = chrono::system_clock::now();
    for (const auto &key : cmd[0].keys()) {
        info->properties.insert({key, cmd[key].string()});
    }

    /*
            CONNECTION_FILETRANSFER_BANDWIDTH_SENT,                     //how many bytes per second are currently being sent by file transfers
            CONNECTION_FILETRANSFER_BANDWIDTH_RECEIVED,                 //how many bytes per second are currently being received by file transfers
            CONNECTION_FILETRANSFER_BYTES_RECEIVED_TOTAL,               //how many bytes we received in total through file transfers
            CONNECTION_FILETRANSFER_BYTES_SENT_TOTAL,                   //how many bytes we sent in total through file transfers
     */

    deque<shared_ptr<ConnectedClient>> receivers;
    {
        lock_guard info_lock(this->connection_info.lock);
        for(const auto& weak_receiver : this->connection_info.receiver) {
            auto receiver = weak_receiver.lock();
            if(!receiver) continue;

            receivers.push_back(receiver);
        }
        this->connection_info.receiver.clear();
        this->connection_info.data = info;
        this->connection_info.data_age = system_clock::now();
    }
    for(const auto& receiver : receivers)
        receiver->notifyConnectionInfo(this->ref(), info);
    return command_result{error::ok};
}

//connectioninfoautoupdate connection_server2client_packetloss_speech=0.0000 connection_server2client_packetloss_keepalive=0.0010 connection_server2client_packetloss_control=0.0000 connection_server2client_packetloss_total=0.0009
command_result ConnectedClient::handleCommandConnectionInfoAutoUpdate(Command &cmd) {
    /* FIXME: Reimplement this!
    this->properties()[property::CONNECTION_SERVER2CLIENT_PACKETLOSS_KEEPALIVE] = cmd["connection_server2client_packetloss_keepalive"].as<std::string>();
    this->properties()[property::CONNECTION_SERVER2CLIENT_PACKETLOSS_CONTROL] = cmd["connection_server2client_packetloss_control"].as<std::string>();
    this->properties()[property::CONNECTION_SERVER2CLIENT_PACKETLOSS_SPEECH] = cmd["connection_server2client_packetloss_speech"].as<std::string>();
    this->properties()[property::CONNECTION_SERVER2CLIENT_PACKETLOSS_TOTAL] = cmd["connection_server2client_packetloss_total"].as<std::string>();
    */
    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandPermissionList(Command &cmd) {
    CMD_CHK_AND_INC_FLOOD_POINTS(5);

    static std::string permission_list_string[ClientType::MAX];
    static std::mutex permission_list_string_lock;

    static auto build_permission_list = [](const std::string& command, const ClientType& type) {
        Command list_builder(command);
        int index = 0;
        for (auto group : permission::availableGroups)
            list_builder[index++]["group_id_end"] = group;

        auto avPerms = permission::availablePermissions;
        std::sort(avPerms.begin(), avPerms.end(), [](const std::shared_ptr<permission::PermissionTypeEntry> &a, const std::shared_ptr<permission::PermissionTypeEntry> &b) {
            return a->type < b->type;
        });

        auto mapper = serverInstance->getPermissionMapper();
        for (const auto& permission : avPerms) {
            if (!permission->clientSupported) continue;

            auto &blk = list_builder[index++];

            blk["permname"] = permission->name;
            blk["permname"] = mapper->permission_name(type, permission->type);
            blk["permdesc"] = permission->description;
            blk["permid"] = permission->type;
        }
        return list_builder;
    };

    auto type = this->getType();
    if(type == CLIENT_TEASPEAK || type == CLIENT_TEAMSPEAK || type == CLIENT_QUERY) {
        Command response{this->notify_response_command("notifypermissionlist")};
        {
            lock_guard lock(permission_list_string_lock);
            if(permission_list_string[type].empty())
                permission_list_string[type] = build_permission_list("", type).build();
            response[0][""] = permission_list_string[type];
        }
        this->sendCommand(response);
    } else {
        this->sendCommand(build_permission_list(this->notify_response_command("notifypermissionlist"), type));
    }
    return command_result{error::ok};
}


#define M(ptype)                \
do {                \
    for(const auto& prop : property::list<ptype>()) { \
        if((prop->flags & property::FLAG_INTERNAL) > 0) continue; \
        response[index]["name"] = prop->name; \
        response[index]["flags"] = prop->flags; \
        response[index]["type"] = property::PropertyType_Names[prop->type_property]; \
        response[index]["value"] = prop->default_value; \
        response[index]["value_type"] = property::ValueType_Names[(int) prop->type_value]; \
        index++; \
    } \
} while(0)

command_result ConnectedClient::handleCommandPropertyList(ts::Command& cmd) {
    Command response(this->notify_response_command("notifypropertylist"));

    {
        string pattern;
        for (auto flag_name : property::flag_names) {
            if(flag_name) {
                pattern = flag_name + string("|") + pattern;
            }
        }
        pattern = pattern.substr(0, pattern.length() - 1);
        response["flag_set"] = pattern;
    }

    int index = 0;
    if(cmd.hasParm("all") || cmd.hasParm("server"))
        M(property::VirtualServerProperties);
    if(cmd.hasParm("all") || cmd.hasParm("channel"))
        M(property::ChannelProperties);
    if(cmd.hasParm("all") || cmd.hasParm("client"))
        M(property::ClientProperties);
    if(cmd.hasParm("all") || cmd.hasParm("instance"))
        M(property::InstanceProperties);
    if(cmd.hasParm("all") || cmd.hasParm("connection"))
        M(property::ConnectionProperties);
    if(cmd.hasParm("all") || cmd.hasParm("playlist"))
        M(property::PlaylistProperties);

    this->sendCommand(response);
    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandSetClientChannelGroup(Command &cmd) {
    CMD_REQ_SERVER;
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(25);

    auto target_channel_group_id = cmd["cgid"].as<GroupId>();
    std::shared_ptr<groups::ChannelGroup> target_channel_group{}, default_channel_group{};

    default_channel_group = this->server->default_channel_group();
    if(target_channel_group_id == 0) {
        target_channel_group = default_channel_group;
        if(!target_channel_group) {
            return command_result{error::vs_critical};
        }
    } else {
        target_channel_group = this->server->group_manager()->channel_groups()->find_group(groups::GroupCalculateMode::GLOBAL, cmd["cgid"].as<GroupId>());
        if(!target_channel_group) {
            return command_result{error::group_invalid_id};
        }
    }

    std::shared_lock server_channel_lock{this->server->channel_tree_mutex}; /* ensure we dont get moved or somebody could move us */
    auto channel_id = cmd["cid"].as<ChannelId>();
    auto channel = this->server->channelTree->findChannel(channel_id);
    if (!channel) {
        return command_result{error::channel_invalid_id};
    }

    auto target_cldbid = cmd["cldbid"].as<ClientDbId>();
    {
        auto channel_group_member_add_power = this->calculate_permission(permission::i_channel_group_member_add_power, channel_id);
        if(!target_channel_group->permission_granted(permission::i_channel_group_needed_member_add_power, channel_group_member_add_power, true)) {
            if(target_cldbid != this->getClientDatabaseId()) {
                return command_result{permission::i_channel_group_member_add_power};
            }

            auto channel_group_self_add_power = this->calculate_permission(permission::i_channel_group_self_add_power, channel_id);
            if(!target_channel_group->permission_granted(permission::i_channel_group_needed_member_add_power, channel_group_self_add_power, true)) {
                return command_result{permission::i_channel_group_self_add_power};
            }
        }


        auto client_permission_modify_power = this->calculate_permission(permission::i_client_permission_modify_power, channel_id);
        auto client_needed_permission_modify_power = this->server->calculate_permission(
                permission::i_client_needed_permission_modify_power, target_cldbid, ClientType::CLIENT_TEAMSPEAK, channel_id
        ).zero_if_unset();


        if(client_needed_permission_modify_power.has_value) {
            if(!permission::v2::permission_granted(client_needed_permission_modify_power, client_permission_modify_power)) {
                return command_result{permission::i_client_permission_modify_power};
            }
        }
    }

    auto old_group_id = this->server->group_manager()->assignments().exact_channel_group_of_client(groups::GroupAssignmentCalculateMode::GLOBAL, target_cldbid, channel->channelId());
    std::shared_ptr<groups::ChannelGroup> old_group{};

    if(old_group_id.has_value()) {
        old_group = this->server->group_manager()->channel_groups()->find_group(groups::GroupCalculateMode::GLOBAL, old_group_id->group_id);
        if(old_group) {
            auto channel_group_member_remove_power = this->calculate_permission(permission::i_channel_group_member_remove_power, channel_id);
            if(!old_group->permission_granted(permission::i_channel_group_needed_member_remove_power, channel_group_member_remove_power, true)) {
                if(target_cldbid != this->getClientDatabaseId()) {
                    return command_result{permission::i_channel_group_member_remove_power};
                }

                auto channel_group_self_remove_power = this->calculate_permission(permission::i_channel_group_self_remove_power, channel_id);
                if(!old_group->permission_granted(permission::i_channel_group_needed_member_remove_power, channel_group_self_remove_power, true)) {
                    return command_result{permission::i_channel_group_self_remove_power};
                }
            }
        }
    }

    this->server->group_manager()->assignments().set_channel_group(target_cldbid, target_channel_group->group_id(), channel->channelId(), !target_channel_group->is_permanent());

    std::shared_ptr<ConnectedClient> connected_client{};
    for (const auto &targetClient : this->server->findClientsByCldbId(target_cldbid)) {
        connected_client = targetClient;

        /* Permissions might changed because of group inheritance */
        targetClient->join_state_id++;

        bool channel_group_changed, server_group_changed;
        targetClient->update_displayed_client_groups(server_group_changed, channel_group_changed);

        auto client_channel = targetClient->getChannel();
        if(!client_channel) {
            continue;
        }

        if(channel_group_changed) {
            targetClient->task_update_needed_permissions.enqueue();
            targetClient->task_update_displayed_groups.enqueue();

            std::optional<ts::command_builder> notify{};
            for (const auto &viewer : this->server->getClients()) {
                viewer->notifyClientChannelGroupChanged(
                        notify,
                        this->ref(), targetClient,
                        client_channel->channelId(),
                        targetClient->properties()[property::CLIENT_CHANNEL_GROUP_INHERITED_CHANNEL_ID],
                        targetClient->properties()[property::CLIENT_CHANNEL_GROUP_ID]);
            }
        }
    }

    if(old_group) {
        serverInstance->action_logger()->group_assignment_logger.log_group_assignment_remove(this->getServerId(),
                                                                                             this->ref(), log::GroupTarget::CHANNEL,
                                                                                             old_group->group_id(), old_group->display_name(),
                                                                                             target_cldbid, connected_client ? connected_client->getDisplayName() : ""
        );
    }

    if(target_channel_group != default_channel_group) {
        serverInstance->action_logger()->group_assignment_logger.log_group_assignment_add(this->getServerId(),
                                                                                          this->ref(), log::GroupTarget::CHANNEL,
                                                                                          target_channel_group->group_id(), target_channel_group->display_name(),
                                                                                          target_cldbid, connected_client ? connected_client->getDisplayName() : ""
        );
    }

    return command_result{error::ok};
}

//sendtextmessage targetmode=1 <1 = direct | 2 = channel | 3 = server> msg=asd target=1 <clid>
command_result ConnectedClient::handleCommandSendTextMessage(Command &cmd) {
    CMD_REQ_SERVER;
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(5);

    auto timestamp = system_clock::now();
    if (cmd["targetmode"].as<ChatMessageMode>() == ChatMessageMode::TEXTMODE_PRIVATE) {
        ConnectedLockedClient target{this->server->find_client_by_id(cmd["target"].as<ClientId>())};
        if (!target) return command_result{error::client_invalid_id};

        bool chat_open{false};
        {

            std::unique_lock self_channel_lock{this->channel_tree_mutex};
            this->open_private_conversations.erase(std::remove_if(this->open_private_conversations.begin(), this->open_private_conversations.end(), [](const weak_ptr<ConnectedClient>& weak) {
                return !weak.lock();
            }), this->open_private_conversations.end());

            for(const auto& entry : this->open_private_conversations) {
                if(entry.lock() == target) {
                    chat_open = true;
                    break;
                }
            }
        }

        if(!chat_open) {
            if(target.client == this) {
                ACTION_REQUIRES_PERMISSION(permission::b_client_even_textmessage_send, 1, this->getChannelId());
            }

            if(!permission::v2::permission_granted(target->calculate_permission(permission::i_client_needed_private_textmessage_power, target->getClientId()), this->calculate_permission(permission::i_client_private_textmessage_power, this->getClientId()))) {
                return command_result{permission::i_client_private_textmessage_power};
            }


            {
                std::unique_lock target_channel_lock{target->get_channel_lock()};
                target->open_private_conversations.push_back(_this);
            }

            {
                std::unique_lock self_channel_lock{this->channel_tree_mutex};
                this->open_private_conversations.push_back(target.client);
            }
        }

        if(this->handleTextMessage(ChatMessageMode::TEXTMODE_PRIVATE, cmd["msg"], target.client)) {
            return command_result{error::ok};
        }

        target->notifyTextMessage(ChatMessageMode::TEXTMODE_PRIVATE, this->ref(), target->getClientId(), 0, timestamp, cmd["msg"].string());
        this->notifyTextMessage(ChatMessageMode::TEXTMODE_PRIVATE, this->ref(), target->getClientId(), 0, timestamp, cmd["msg"].string());
    } else if (cmd["targetmode"] == ChatMessageMode::TEXTMODE_CHANNEL) {
        if(cmd[0].has("cid")) {
            cmd["target"] = cmd["cid"].string();
        }

        if(!cmd[0].has("target")) {
            cmd["target"] = 0;
        }

        RESOLVE_CHANNEL_R(cmd["target"], false);
        auto channel = l_channel ? dynamic_pointer_cast<BasicChannel>(l_channel->entry) : nullptr;
        if(!channel) {
            CMD_REQ_CHANNEL;
            channel = this->currentChannel;
            channel_id = this->currentChannel->channelId();
        }

        if(!permission::v2::permission_granted(1, this->calculate_permission(permission::b_client_channel_textmessage_send, channel_id))) \
            return command_result{permission::b_client_channel_textmessage_send};

        if(channel == this->currentChannel) {
            channel_tree_read_lock.unlock(); //Method may creates a music bot which modifies the channel tree
            if(this->handleTextMessage(ChatMessageMode::TEXTMODE_CHANNEL, cmd["msg"], nullptr)) {
                return command_result{error::ok};
            }
            channel_tree_read_lock.lock();
        }

        auto conversation_mode = channel->properties()[property::CHANNEL_CONVERSATION_MODE].as_unchecked<ChannelConversationMode>();
        if(conversation_mode == ChannelConversationMode::CHANNELCONVERSATIONMODE_NONE) {
            return command_result{error::conversation_not_exists};
        } else if(channel != this->currentChannel) {
            if(conversation_mode == ChannelConversationMode::CHANNELCONVERSATIONMODE_PRIVATE) {
                return command_result{error::conversation_is_private};
            }

            if(auto fail_perm{this->calculate_and_get_join_state(channel)}; fail_perm != permission::ok) {
                return command_result{fail_perm}; /* You're not allowed to send messages :) */
            }
        }

        this->server->send_text_message(channel, this->ref(), cmd["msg"].string());
    } else if (cmd["targetmode"] == ChatMessageMode::TEXTMODE_SERVER) {
        ACTION_REQUIRES_GLOBAL_PERMISSION(permission::b_client_server_textmessage_send, 1);

        if(this->handleTextMessage(ChatMessageMode::TEXTMODE_SERVER, cmd["msg"], nullptr)) return command_result{error::ok};
        for(const auto& client : this->server->getClients()) {
            if (client->connectionState() != ConnectionState::CONNECTED)
                continue;

            auto type = client->getType();
            if (type == ClientType::CLIENT_INTERNAL || type == ClientType::CLIENT_MUSIC)
                continue;

            client->notifyTextMessage(ChatMessageMode::TEXTMODE_SERVER, this->ref(), this->getClientId(), 0, timestamp, cmd["msg"].string());
        }

        {
            auto conversations = this->server->conversation_manager();
            auto conversation = conversations->get_or_create(0);
            conversation->register_message(this->getClientDatabaseId(), this->getUid(), this->getDisplayName(), timestamp, cmd["msg"].string());
        }
    } else return command_result{error::parameter_invalid, "invalid target mode"};

    return command_result{error::ok};
}

//notifybanlist banid=3 ip name uid=zbex8X3bFRTIKLI7mzeyJGZsh64= lastnickname=Wolf\sC++\sXXXX created=1510357269 duration=3600 invokername=WolverinDEV invokercldbid=5 invokeruid=xxjnc14LmvTk+Lyrm8OOeo4tOqw= reason=Prefix\sFake\s\p\sName enforcements=3
command_result ConnectedClient::handleCommandBanList(Command &cmd) {
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(25);

    ServerId sid = this->getServerId();
    if (cmd[0].has("sid")) {
        sid = cmd["sid"];
    }

    if (sid == 0) {
        ACTION_REQUIRES_GLOBAL_PERMISSION(permission::b_client_ban_list_global, 1);
    } else {
        auto server = serverInstance->getVoiceServerManager()->findServerById(sid);
        if (!server) return command_result{error::parameter_invalid};

        if (!permission::v2::permission_granted(1, server->calculate_permission(permission::b_client_ban_list, this->getClientDatabaseId(), this->getType(), 0))) {
            return command_result{permission::b_client_ban_list};
        }
    }

    //When empty: return command_result{error::database_empty_result};
    auto banList = serverInstance->banManager()->listBans(sid);
    if (banList.empty()) return command_result{error::database_empty_result};

    auto allow_ip = permission::v2::permission_granted(1, this->calculate_permission(permission::b_client_remoteaddress_view, 0));
    Command notify(this->getExternalType() == CLIENT_TEAMSPEAK ? "notifybanlist" : "");
    int index = 0;
    for (const auto &elm : banList) {
        notify[index]["sid"] = elm->serverId;
        notify[index]["banid"] = elm->banId;
        if(allow_ip)
            notify[index]["ip"] = elm->ip;
        else
            notify[index]["ip"] = "hidden";
        notify[index]["name"] = elm->name;
        notify[index]["uid"] = elm->uid;
        notify[index]["hwid"] = elm->hwid;
        notify[index]["lastnickname"] = elm->name; //Maybe update?

        notify[index]["created"] = chrono::duration_cast<chrono::seconds>(elm->created.time_since_epoch()).count();
        if (elm->until.time_since_epoch().count() != 0)
            notify[index]["duration"] = chrono::duration_cast<chrono::seconds>(elm->until - elm->created).count();
        else
            notify[index]["duration"] = 0;

        notify[index]["reason"] = elm->reason;
        notify[index]["enforcements"] = elm->triggered;

        notify[index]["invokername"] = elm->invokerName;
        notify[index]["invokercldbid"] = elm->invokerDbId;
        notify[index]["invokeruid"] = elm->invokerUid;
        index++;
    }
    this->sendCommand(notify);
    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandBanAdd(Command &cmd) {
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(25);

    string ip = cmd[0].has("ip") ? cmd["ip"].string() : "";
    string name = cmd[0].has("name") ? cmd["name"].string() : "";
    string uid = cmd[0].has("uid") ? cmd["uid"].string() : "";
    string hwid = cmd[0].has("hwid") ? cmd["hwid"].string() : "";
    string banreason = cmd[0].has("banreason") ? cmd["banreason"].string() : "No reason set";
    auto time = cmd[0].has("time") ? cmd["time"].as<int64_t>() : 0L;

    ServerId sid = this->getServerId();
    if (cmd[0].has("sid"))
        sid = cmd["sid"];

    if (sid == 0) {
        ACTION_REQUIRES_GLOBAL_PERMISSION(permission::b_client_ban_create_global, 1);
    } else {
        auto server = serverInstance->getVoiceServerManager()->findServerById(sid);
        if (!server) return command_result{error::parameter_invalid};
        if (!permission::v2::permission_granted(1, server->calculate_permission(permission::b_client_ban_create, this->getClientDatabaseId(), this->getType(), 0)))
            return command_result{permission::b_client_ban_create};
    }

    auto max_ban_time = this->calculate_permission(permission::i_client_ban_max_bantime, this->getClientDatabaseId());
    if(!max_ban_time.has_value)
        return command_result{permission::i_client_ban_max_bantime};
    if (!max_ban_time.has_infinite_power()) {
        if (max_ban_time.value < time)
            return command_result{permission::i_client_ban_max_bantime};
    }

    chrono::time_point<chrono::system_clock> until = time > 0 ? chrono::system_clock::now() + chrono::seconds(time) : chrono::time_point<chrono::system_clock>();

    auto existing = serverInstance->banManager()->findBanExact(sid, banreason, uid, ip, name, hwid);
    bool banned = false;
    if(existing) {
        if(existing->invokerDbId == this->getClientDatabaseId()) {
            if(existing->until == until) {
                return command_result{error::database_duplicate_entry};
            } else {
                existing->until = until;
                serverInstance->banManager()->updateBan(existing);
                banned = true;
            }
        } else if(!banned) {
            serverInstance->banManager()->unban(existing);
        }
    }
    if(!banned) {
        serverInstance->banManager()->registerBan(sid, this->getClientDatabaseId(), banreason, uid, ip, name, hwid, until);
    }

    for(auto server : (this->server ? std::deque<shared_ptr<VirtualServer>>{this->server} : serverInstance->getVoiceServerManager()->serverInstances()))
        server->testBanStateChange(this->ref());
    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandBanEdit(Command &cmd) {
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(25);

    ServerId sid = this->getServerId();
    if (cmd[0].has("sid"))
        sid = cmd["sid"];

    auto ban = serverInstance->banManager()->findBanById(sid, cmd["banid"].as<uint64_t>());
    if (!ban) return command_result{error::database_empty_result};

    if (sid == 0) {
        ACTION_REQUIRES_GLOBAL_PERMISSION(permission::b_client_ban_edit_global, 1);
    } else {
        auto server = serverInstance->getVoiceServerManager()->findServerById(sid);
        if (!server) return command_result{error::parameter_invalid};

        if (!permission::v2::permission_granted(1, server->calculate_permission(permission::b_client_ban_edit, this->getClientDatabaseId(), this->getType(), 0)))
            return command_result{permission::b_client_ban_edit};
    }

    /* ip name uid reason time hwid */
    bool changed = false;
    if (cmd[0].has("ip")) {
        ban->ip = cmd["ip"].as<string>();
        changed |= true;
    }

    if (cmd[0].has("name")) {
        ban->name = cmd["name"].as<string>();
        changed |= true;
    }

    if (cmd[0].has("uid")) {
        ban->uid = cmd["uid"].as<string>();
        changed |= true;
    }

    if (cmd[0].has("reason")) {
        ban->reason = cmd["reason"].as<string>();
        changed |= true;
    }
    if (cmd[0].has("banreason")) {
        ban->reason = cmd["banreason"].as<string>();
        changed |= true;
    }

    if (cmd[0].has("time")) {
        ban->until = ban->created + seconds(cmd["time"].as<size_t>());
        changed |= true;
    }

    if (cmd[0].has("hwid")) {
        ban->hwid = cmd["hwid"].as<string>();
        changed |= true;
    }

    if (changed)
        serverInstance->banManager()->updateBan(ban);
    else return command_result{error::parameter_invalid};

    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandBanClient(Command &cmd) {
    CMD_REQ_SERVER;
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(25);

    std::string target_unique_id{};
    ClientDbId target_database_id{0};

    string reason = cmd[0].has("banreason") ? cmd["banreason"].string() : "";
    auto time = cmd[0].has("time") ? cmd["time"].as<uint64_t>() : 0UL;
    chrono::time_point<chrono::system_clock> until = time > 0 ? chrono::system_clock::now() + chrono::seconds(time) : chrono::time_point<chrono::system_clock>();

    const auto no_nickname = cmd.hasParm("no-nickname");
    const auto no_hwid = cmd.hasParm("no-hardware-id");
    const auto no_ip = cmd.hasParm("no-ip");

    std::deque<std::shared_ptr<ConnectedClient>> target_clients;
    if (cmd[0].has("uid")) {
        target_clients = this->server->findClientsByUid(target_unique_id = cmd["uid"].string());
    } else if(cmd[0].has("cldbid")) {
        target_clients = this->server->findClientsByCldbId(target_database_id = cmd["cldbid"].as<ClientDbId>());
    } else {
        target_clients = {this->server->find_client_by_id(cmd["clid"].as<ClientId>())};
        if(!target_clients[0]) {
            return command_result{error::client_invalid_id, "Could not find target client"};
        }
    }

    for(const auto& client : target_clients)
        if(client->getType() == ClientType::CLIENT_MUSIC)
            return command_result{error::client_invalid_id, "You cant ban a music bot!"};

    if(!target_clients.empty()) {
        if(target_unique_id.empty())
            target_unique_id = target_clients.back()->getUid();

        if(!target_database_id)
            target_database_id = target_clients.back()->getClientDatabaseId();
    }
    if(!permission::v2::permission_granted(this->server->calculate_permission(permission::i_client_needed_ban_power, target_database_id, ClientType::CLIENT_TEAMSPEAK, 0).zero_if_unset(), this->calculate_permission(permission::i_client_ban_power, 0)))
        return command_result{permission::i_client_ban_power};

    if (permission::v2::permission_granted(1, this->server->calculate_permission(permission::b_client_ignore_bans, target_database_id, ClientType::CLIENT_TEAMSPEAK, 0)))
        return command_result{permission::b_client_ignore_bans};

    deque<BanId> ban_ids;
    if(!target_unique_id.empty()) {
        auto _id = serverInstance->banManager()->registerBan(this->getServer()->getServerId(), this->getClientDatabaseId(), reason, target_unique_id, "", "", "", until);
        ban_ids.push_back(_id);
    }

    auto b_ban_name = permission::v2::permission_granted(1, this->calculate_permission(permission::b_client_ban_name, 0));
    auto b_ban_ip = permission::v2::permission_granted(1, this->calculate_permission(permission::b_client_ban_ip, 0));
    auto b_ban_hwid = permission::v2::permission_granted(1, this->calculate_permission(permission::b_client_ban_hwid, 0));

    auto max_ban_time = this->calculate_permission(permission::i_client_ban_max_bantime, 0);
    if(!max_ban_time.has_value) return command_result{permission::i_client_ban_max_bantime};
    if (!max_ban_time.has_infinite_power()) {
        if (max_ban_time.value < time)
            return command_result{permission::i_client_ban_max_bantime};
    }

    for (const auto &client : target_clients) {
        if (client->getType() != CLIENT_TEAMSPEAK && client->getType() != CLIENT_QUERY) continue; //Remember if you add new type you have to change stuff here

        this->server->notify_client_ban(client, this->ref(), reason, time);
        client->close_connection(system_clock::now() + seconds(2));

        string entry_name, entry_ip, entry_hardware_id;
        if(b_ban_name && !no_nickname) {
            entry_name = client->getDisplayName();
        }
        if(b_ban_ip && !no_ip && !config::server::disable_ip_saving) {
            entry_ip = client->getPeerIp();
        }
        if(b_ban_hwid && !no_hwid) {
            entry_hardware_id = client->getHardwareId();
        }
        auto exact = serverInstance->banManager()->findBanExact(this->getServer()->getServerId(), reason, "", entry_ip, entry_name, entry_hardware_id);
        if(exact) {
            exact->until = until;
            exact->invokerDbId = this->getClientDatabaseId();
            serverInstance->banManager()->updateBan(exact);
            ban_ids.push_back(exact->banId);
        } else {
            auto id = serverInstance->banManager()->registerBan(this->getServer()->getServerId(), this->getClientDatabaseId(), reason, "", entry_ip, entry_name, entry_hardware_id, until);
            ban_ids.push_back(id);
        }
    }
    this->server->testBanStateChange(this->ref());

    if (this->getType() == CLIENT_QUERY) {
        Command notify("");
        int index = 0;
        for(const auto& ban_id : ban_ids)
            notify[index++]["banid"] = ban_id;
        this->sendCommand(notify);
    }

    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandBanDel(Command &cmd) {
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(5);

    ServerId sid = this->getServerId();
    if (cmd[0].has("sid"))
        sid = cmd["sid"];

    auto ban = serverInstance->banManager()->findBanById(sid, cmd["banid"].as<uint64_t>());
    if (!ban) return command_result{error::database_empty_result};

    if (sid == 0) {
        const auto permission = ban->invokerDbId == this->getClientDatabaseId() ? permission::b_client_ban_delete_own_global : permission::b_client_ban_delete_global;
        ACTION_REQUIRES_GLOBAL_PERMISSION(permission, 1);
    } else {
        auto server = serverInstance->getVoiceServerManager()->findServerById(sid);
        if (!server) return command_result{error::parameter_invalid};

        auto perm = ban->invokerDbId == this->getClientDatabaseId() ? permission::b_client_ban_delete_own : permission::b_client_ban_delete;
        if (!permission::v2::permission_granted(1, server->calculate_permission(perm, this->getClientDatabaseId(), this->getType(), 0)))
            return command_result{perm};
    }
    serverInstance->banManager()->unban(ban);
    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandBanDelAll(Command &cmd) {
    CMD_REQ_SERVER;
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(25);
    ACTION_REQUIRES_GLOBAL_PERMISSION(permission::b_client_ban_delete, 1);

    serverInstance->banManager()->deleteAllBans(server->getServerId());
    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandBanTriggerList(ts::Command &cmd) {
    CMD_REQ_SERVER;
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(25);
    ACTION_REQUIRES_GLOBAL_PERMISSION(permission::b_client_ban_trigger_list, 1);

    CMD_REQ_PARM("banid");

    auto record = serverInstance->banManager()->findBanById(this->getServerId(), cmd["banid"]);
    if(!record) return command_result{error::parameter_invalid, "Invalid ban id"};

    Command notify(this->getExternalType() == CLIENT_TEAMSPEAK ? "notifybantriggerlist" : "");
    notify["banid"] = record->banId;
    notify["serverid"] = record->serverId;

    auto allow_ip = permission::v2::permission_granted(1, this->calculate_permission(permission::b_client_remoteaddress_view, 0));
    int index = 0;
    for (auto &entry : serverInstance->banManager()->trigger_list(record, this->getServerId(), cmd[0].has("offset") ? cmd["offset"].as<int64_t>() : 0, cmd[0].has("limit") ? cmd["limit"].as<int64_t>() : -1)) {
        notify[index]["client_unique_identifier"] = entry->unique_id;
        notify[index]["client_hardware_identifier"] = entry->hardware_id;
        notify[index]["client_nickname"] = entry->name;
        if(allow_ip)
            notify[index]["connection_client_ip"] = entry->ip;
        else
            notify[index]["connection_client_ip"] = "hidden";
        notify[index]["timestamp"] = duration_cast<milliseconds>(entry->timestamp.time_since_epoch()).count();
        index++;
    }
    if (index == 0) return command_result{error::database_empty_result};

    this->sendCommand(notify);
    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandTokenList(Command &cmd) {
    CMD_REQ_SERVER;
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(5);

    auto& token_manager = this->server->getTokenManager();

    size_t offset = cmd[0].has("offset") ? cmd["offset"].as<size_t>() : 0;
    size_t limit = cmd[0].has("limit") ? cmd["limit"].as<int>() : 0;
    if(limit > 2000 || limit < 1) {
        limit = 1000;
    }

    size_t total_tokens{0};
    std::deque<std::shared_ptr<Token>> tokens{};

    auto own_tokens_only = cmd.hasParm("own-only") || !permission::v2::permission_granted(1, this->calculate_permission(permission::b_virtualserver_token_list_all, 0));
    if(own_tokens_only) {
        tokens = token_manager.list_tokens(total_tokens, { offset }, { limit }, { this->getClientDatabaseId() });
    } else {
        tokens = token_manager.list_tokens(total_tokens, { offset }, { limit }, std::nullopt);
    }

    if(tokens.empty()) {
        return ts::command_result{error::database_empty_result};
    }

    auto new_command = cmd.hasParm("new");
    auto own_database_id = this->getClientDatabaseId();

    ts::command_builder notify{this->notify_response_command("notifytokenlist")};
    size_t notify_index{0};
    for(size_t index{0}; index < tokens.size(); index++) {
        auto& token = tokens[index];
        if(own_tokens_only && token->issuer_database_id != own_database_id) {
            continue;
        }

        auto bulk = notify.bulk(notify_index++);
        bulk.put_unchecked("token", token->token);
        bulk.put_unchecked("token_id", token->id);
        bulk.put_unchecked("token_created", std::chrono::duration_cast<std::chrono::seconds>(token->timestamp_created.time_since_epoch()).count());
        bulk.put_unchecked("token_expired", std::chrono::duration_cast<std::chrono::seconds>(token->timestamp_expired.time_since_epoch()).count());
        bulk.put_unchecked("token_use_count", token->use_count);
        bulk.put_unchecked("token_max_uses", token->max_uses);
        bulk.put_unchecked("token_issuer_database_id", token->issuer_database_id);
        bulk.put_unchecked("token_description", token->description);

        if(!new_command) {
            bulk.put_unchecked("token_type", 0);
            bulk.put_unchecked("token_id1", 0);
            bulk.put_unchecked("token_id2", 0);
        }
    }

    if(notify_index == 0) {
        return ts::command_result{error::database_empty_result};
    }

    notify.put_unchecked(0, "token_count", total_tokens);
    this->sendCommand(notify);
    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandTokenActionList(Command &cmd) {
    CMD_REQ_SERVER;
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(5);

    auto& token_manager = this->server->getTokenManager();
    std::shared_ptr<token::Token> token_info{};
    if(cmd[0].has("token_id")) {
        token_info = this->server->getTokenManager().load_token_by_id(cmd["token_id"].as<token::TokenId>(), true);
    } else if(cmd[0].has("token")) {
        token_info = this->server->getTokenManager().load_token(cmd["token"], true);
    }
    if(!token_info) {
        return ts::command_result{error::token_invalid_id};
    }

    if(token_info->issuer_database_id != this->getClientDatabaseId()) {
        ACTION_REQUIRES_GLOBAL_PERMISSION(permission::b_virtualserver_token_list_all, 1);
    }

    std::deque<TokenAction> actions{};
    if(!token_manager.query_token_actions(token_info->id, actions)) {
        return ts::command_result{error::vs_critical};
    }

    ts::command_builder notify{this->notify_response_command("notifytokenactions")};
    for(size_t index{0}; index < actions.size(); index++) {
        auto bulk = notify.bulk(index);
        auto& action = actions[index];

        bulk.put_unchecked("action_type", (uint8_t) action.type);
        bulk.put_unchecked("action_id", action.id);
        bulk.put_unchecked("action_id1", action.id1);
        bulk.put_unchecked("action_id2", action.id2);
        bulk.put_unchecked("action_text", action.text);
    }
    this->sendCommand(notify);
    return command_result{error::ok};
}

/**
 *
 * @param result Should already contain cmd.bulkCount() results
 * @param cmd
 * @param token_description
 * @param max_uses
 * @param timestamp_expired
 * @param actions
 * @return
 */
ts::command_result parseTokenParameters(
        ts::command_result_bulk& result,
        Command &cmd,
        std::optional<std::string>& token_description,
        std::optional<size_t>& max_uses,
        std::optional<std::chrono::system_clock::time_point>& timestamp_expired,
        std::vector<token::TokenAction>& actions
) {
    actions.reserve(cmd.bulkCount());

    /* legacy (should not be provided on token edit) */
    if(cmd[0].has("tokentype")) {
        auto ttype = cmd["tokentype"].as<uint32_t>();
        auto gId = cmd["tokenid1"].as<GroupId>();
        auto cId = cmd["tokenid2"].as<ChannelId>();

        if(ttype == 0) {
            auto& action = actions.emplace_back();
            action.type = token::ActionType::AddServerGroup;
            action.id1 = gId;
        } else if(ttype == 1) {
            auto& action = actions.emplace_back();
            action.type = token::ActionType::SetChannelGroup;
            action.id1 = gId;
            action.id2 = cId;
        } else {
            result.set_result(0, ts::command_result{error::parameter_invalid, "tokentype"});
        }
    } else {
        for(size_t index{0}; index < cmd.bulkCount(); index++) {
            auto& action = actions.emplace_back();
            action.id = cmd[index].has("action_id") ? cmd[index]["action_id"] : 0;

            if(!cmd[index].has("action_type")) {
                if(action.id > 0) {
                    /* we want to delete the action */
                    action.type = token::ActionType::ActionDeleted;
                    continue;
                }

                /* Missing the parameter action type */
                action.type = token::ActionType::ActionIgnore;
                if(index == 0) {
                    /* no actions provided */
                    break;
                } else {
                    result.set_result(index, ts::command_result{error::parameter_missing, "action_type"});
                    continue;
                }
            }

            action.type = cmd[index]["action_type"];
            switch(action.type) {
                case token::ActionType::AddServerGroup:
                case token::ActionType::RemoveServerGroup:
                case token::ActionType::SetChannelGroup:
                case token::ActionType::AllowChannelJoin:
                    break;

                case token::ActionType::ActionSqlFailed:
                case token::ActionType::ActionDeleted:
                case token::ActionType::ActionIgnore:
                default:
                    result.set_result(index, ts::command_result{error::parameter_invalid, "action_type"});
                    continue;
            }

            if(cmd[index].has("action_id1")) {
                action.id1 = cmd[index]["action_id1"];
            }

            if(cmd[index].has("action_id2")) {
                action.id2 = cmd[index]["action_id2"];
            }

            if(cmd[index].has("action_text")) {
                action.text = cmd[index]["action_text"].string();
            }
        }
    }

    if(cmd[0].has("token_description")) {
        token_description = { cmd["token_description"].string() };
    } else if(cmd[0].has("tokendescription")) {
        token_description = { cmd["tokendescription"].string() };
    }

    if(token_description.has_value() && token_description->length() > 255) {
        return ts::command_result{error::parameter_invalid, "token_description"};
    }

    if(cmd[0].has("token_max_uses")) {
        max_uses = { cmd["token_max_uses"].as<size_t>() };
    }

    if(cmd[0].has("token_expired")) {
        auto seconds = cmd["token_expired"].as<uint64_t>();
        timestamp_expired = { std::chrono::system_clock::time_point{} + std::chrono::seconds{seconds} };
    }

    return ts::command_result{error::ok};
}

/**
 * Check if the client has permissions to do such actions.
 * If not the toke will be set to ignored and a proper error will be emplaced
 */
void check_token_action_permissions(const std::shared_ptr<ConnectedClient>& client, ts::command_result_bulk& result, std::vector<token::TokenAction>& actions) {
    for(size_t index{0}; index < actions.size(); index++) {
        auto& action = actions[index];

        switch(action.type) {
            case ActionType::AddServerGroup: {
                auto target_group = client->getServer()->group_manager()->server_groups()->find_group(groups::GroupCalculateMode::GLOBAL, action.id1);
                if(!target_group || target_group->group_type() == groups::GroupType::GROUP_TYPE_TEMPLATE) {
                    action.type = ActionType::ActionIgnore;
                    result.set_result(index, ts::command_result{error::group_invalid_id});
                    break;
                }

                if(!target_group->permission_granted(permission::i_server_group_needed_member_add_power, client->calculate_permission(permission::i_server_group_member_add_power, 0), true)) {
                    action.type = ActionType::ActionIgnore;
                    result.set_result(index, ts::command_result{permission::i_server_group_member_add_power});
                    break;
                }

                break;
            }

            case ActionType::RemoveServerGroup: {
                auto target_group = client->getServer()->group_manager()->channel_groups()->find_group(groups::GroupCalculateMode::GLOBAL, action.id1);
                if(!target_group || target_group->group_type() == groups::GroupType::GROUP_TYPE_TEMPLATE) {
                    action.type = ActionType::ActionIgnore;
                    result.set_result(index, ts::command_result{error::group_invalid_id});
                    break;
                }

                if(!target_group->permission_granted(permission::i_server_group_needed_member_remove_power, client->calculate_permission(permission::i_server_group_member_remove_power, 0), true)) {
                    action.type = ActionType::ActionIgnore;
                    result.set_result(index, ts::command_result{permission::i_server_group_member_remove_power});
                    break;
                }

                break;
            }

            case ActionType::SetChannelGroup: {
                auto target_group = client->getServer()->group_manager()->server_groups()->find_group(groups::GroupCalculateMode::GLOBAL, action.id1);
                if(!target_group || target_group->group_type() == groups::GroupType::GROUP_TYPE_TEMPLATE) {
                    action.type = ActionType::ActionIgnore;
                    result.set_result(index, ts::command_result{error::group_invalid_id});
                    break;
                }

                auto granted_permission = client->calculate_permission(permission::i_channel_group_member_add_power, action.id2);
                if(!target_group->permission_granted(permission::i_channel_group_needed_member_add_power, granted_permission, true)) {
                    action.type = ActionType::ActionIgnore;
                    result.set_result(index, ts::command_result{permission::i_channel_group_member_add_power});
                    break;
                }

                {
                    std::shared_lock tree_lock{client->getServer()->get_channel_tree_lock()};
                    auto target_channel = client->getServer()->getChannelTree()->findChannel(action.id2);
                    tree_lock.unlock();

                    if(!target_channel) {
                        action.type = ActionType::ActionIgnore;
                        result.set_result(index, ts::command_result{error::channel_invalid_id});
                        break;
                    }
                }

                break;
            }

            case ActionType::AllowChannelJoin: {
                std::shared_lock tree_lock{client->getServer()->get_channel_tree_lock()};
                auto target_channel = client->getServer()->getChannelTree()->findChannel(action.id2);

                if(!target_channel) {
                    action.type = ActionType::ActionIgnore;
                    result.set_result(index, ts::command_result{error::channel_invalid_id});
                    break;
                }

                auto join_permission = client->calculate_and_get_join_state(target_channel);
                if(join_permission != permission::ok) {
                    action.type = ActionType::ActionIgnore;
                    result.set_result(index, ts::command_result{join_permission});
                    break;
                }

                if(client->getType() == ClientType::CLIENT_QUERY && !action.text.empty()) {
                    action.text = digest::sha1(action.text);
                }

                if(permission::v2::permission_granted(1, client->calculate_permission(permission::b_channel_join_ignore_password, target_channel->channelId()))) {
                    action.text = "ignore";
                } else if(!target_channel->verify_password(action.text, true)) {
                    action.type = ActionType::ActionIgnore;
                    result.set_result(index, ts::command_result{error::channel_invalid_password});
                    break;
                }
                break;
            }

            case ActionType::ActionSqlFailed:
            case ActionType::ActionIgnore:
            case ActionType::ActionDeleted:
            default:
                continue;
        }
    }
}

command_result ConnectedClient::handleCommandTokenAdd(Command &cmd) {
    CMD_REQ_SERVER;
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(5);

    auto client_tokens = this->server->tokenManager->client_token_count(this->getClientDatabaseId());
    auto token_limit = this->calculate_permission(permission::i_virtualserver_token_limit, 0);
    if(token_limit.has_value) {
        if(!permission::v2::permission_granted(client_tokens + 1, token_limit)) {
            return ts::command_result{permission::i_virtualserver_token_limit};
        }
    }

    std::vector<token::TokenAction> actions{};
    actions.reserve(cmd.bulkCount());

    std::optional<std::string> token_description{};
    std::optional<size_t> max_uses{};
    std::optional<std::chrono::system_clock::time_point> timestamp_expired{};

    ts::command_result_bulk result{};
    result.emplace_result_n(cmd.bulkCount(), error::ok);

    auto parse_result = parseTokenParameters(
            result,
            cmd,
            token_description,
            max_uses,
            timestamp_expired,
            actions
    );

    if(parse_result.has_error()) {
        return parse_result;
    }
    parse_result.release_data();

    check_token_action_permissions(this->ref(), result, actions);
    auto token = this->server->getTokenManager().create_token(
            this->getClientDatabaseId(),
            token_description.value_or(""),
            max_uses.value_or(0),
            timestamp_expired.value_or(std::chrono::system_clock::time_point{})
    );

    if(!token) {
        return ts::command_result{error::vs_critical};
    }

    ts::command_builder notify{this->notify_response_command("notifytokenadd")};
    if(!actions.empty()) {
        this->server->getTokenManager().add_token_actions(token->id, actions);

        for(size_t index{0}; index < actions.size(); index++) {
            auto& action = actions[index];
            if(action.type == token::ActionType::ActionIgnore || action.type == token::ActionType::ActionDeleted) {
                continue;
            } else if(action.type == token::ActionType::ActionSqlFailed) {
                result.set_result(index, ts::command_result{error::database_constraint});
            } else {
                notify.put_unchecked(index, "action_id", action.id);
            }
        }
    }

    notify.put_unchecked(0, "token", token->token);
    notify.put_unchecked(0, "token_id", token->id);
    this->sendCommand(notify);

    return command_result{std::move(result)};
}

command_result ConnectedClient::handleCommandTokenEdit(Command &cmd) {
    CMD_REQ_SERVER;
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(15);

    std::shared_ptr<token::Token> token{};
    if(cmd[0].has("token_id")) {
        token = this->server->getTokenManager().load_token_by_id(cmd["token_id"].as<token::TokenId>(), true);
    } else if(cmd[0].has("token")) {
        token = this->server->getTokenManager().load_token(cmd["token"], true);
    }

    if(!token) {
        return ts::command_result{error::token_invalid_id};
    }

    if(token->issuer_database_id != this->getClientDatabaseId()) {
        ACTION_REQUIRES_GLOBAL_PERMISSION(permission::b_virtualserver_token_edit_all, 1);
    }

    std::vector<token::TokenAction> actions{};
    actions.reserve(cmd.bulkCount());

    std::optional<std::string> token_description{};
    std::optional<size_t> max_uses{};
    std::optional<std::chrono::system_clock::time_point> timestamp_expired{};

    ts::command_result_bulk result{};
    result.emplace_result_n(cmd.bulkCount(), error::ok);

    auto parse_result = parseTokenParameters(
            result,
            cmd,
            token_description,
            max_uses,
            timestamp_expired,
            actions
    );

    if(parse_result.has_error()) {
        return parse_result;
    }
    parse_result.release_data();

    /* filter actions for duplicates etc  */
    {
        std::deque<token::TokenAction> current_actions{};
        if(!this->server->getTokenManager().query_token_actions(token->id, current_actions)) {
            return ts::command_result{error::vs_critical};
        }


        for(const auto& action : actions) {
            if (action.type == ActionType::ActionDeleted) {
                auto index = std::find_if(current_actions.begin(), current_actions.end(), [&](const auto& caction) { return caction.id == action.id; });
                if(index != current_actions.end()) {
                    current_actions.erase(index);
                }
            }
        }

        size_t action_index{0};
        for(auto& action : actions) {
            switch(action.type) {
                case ActionType::AddServerGroup:
                case ActionType::RemoveServerGroup:
                case ActionType::SetChannelGroup:
                case ActionType::AllowChannelJoin: {
                    auto index = std::find_if(current_actions.begin(), current_actions.end(), [&](const TokenAction& caction) {
                        if(caction.type != action.type) {
                            return false;
                        }

                        if(caction.text != action.text) {
                            return false;
                        }

                        if(caction.id1 != action.id1) {
                            return false;
                        }

                        return caction.id2 == action.id2;
                    });

                    if(index != current_actions.end()) {
                        action.type = ActionType::ActionIgnore;
                        result.set_result(action_index, ts::command_result{error::database_no_modifications});
                        break;
                    }

                    current_actions.push_back(action);
                    break;
                }

                case ActionType::ActionSqlFailed:
                case ActionType::ActionIgnore:
                case ActionType::ActionDeleted:
                default:
                    break;
            }

            action_index++;
        }
    }

    check_token_action_permissions(this->ref(), result, actions);

    if(token_description.has_value() || max_uses.has_value() || timestamp_expired.has_value()) {
        token->description = token_description.value_or(token->description);
        token->max_uses = max_uses.value_or(token->max_uses);
        token->timestamp_expired = timestamp_expired.value_or(token->timestamp_expired);

        this->server->getTokenManager().save_token(token);
    }

    if(!actions.empty()) {
        size_t new_actions{0};

        std::vector<token::TokenActionId> actions_removed{};
        actions_removed.reserve(actions.size());
        for(const auto& action : actions) {
            if(action.type == ActionType::ActionDeleted) {
                actions_removed.push_back(action.id);
            } else if(action.type == ActionType::ActionIgnore) {
                /* just ignore the action */
            } else {
                new_actions++;
            }
        }

        if(!actions_removed.empty()) {
            this->server->getTokenManager().delete_token_actions(token->id, actions_removed);
        }

        if(new_actions > 0) {
            this->server->getTokenManager().add_token_actions(token->id, actions);

            ts::command_builder notify{this->notify_response_command("notifytokenedit")};
            for(size_t index{0}; index < actions.size(); index++) {
                auto& action = actions[index];
                if(action.type == token::ActionType::ActionIgnore || action.type == token::ActionType::ActionDeleted) {
                    continue;
                } else if(action.type == token::ActionType::ActionSqlFailed) {
                    result.set_result(0, ts::command_result{error::database_constraint});
                } else {
                    notify.put_unchecked(index, "action_id", action.id);
                }
            }
            this->sendCommand(notify);
        }
    }

    return command_result{std::move(result)};
}

command_result ConnectedClient::handleCommandTokenUse(Command &cmd) {
    CMD_REQ_SERVER;
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(5);
    //ACTION_REQUIRES_GLOBAL_PERMISSION(permission::b_virtualserver_token_use, 1);

    auto& token_manager = this->server->getTokenManager();

    auto token = token_manager.load_token(cmd["token"], true);
    if(!token) {
        return ts::command_result{error::token_invalid_id};
    }

    if(token->is_expired()) {
        return ts::command_result{error::token_expired};
    }

    if(token->use_limit_reached()) {
        return ts::command_result{error::token_use_limit_exceeded};
    }

    this->server->properties()[property::VIRTUALSERVER_ASK_FOR_PRIVILEGEKEY] = false; //TODO test if its the default token
    token_manager.log_token_use(token->id);
    this->useToken(token->id);

    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandTokenDelete(Command &cmd) {
    CMD_REQ_SERVER;
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(5);

    std::shared_ptr<token::Token> token{};
    if(cmd[0].has("token_id")) {
        token = this->server->getTokenManager().load_token_by_id(cmd["token_id"].as<token::TokenId>(), true);
    } else if(cmd[0].has("token")) {
        token = this->server->getTokenManager().load_token(cmd["token"], true);
    }

    if(!token) {
        return ts::command_result{error::token_invalid_id};
    }

    if(token->issuer_database_id != this->getClientDatabaseId()) {
        ACTION_REQUIRES_GLOBAL_PERMISSION(permission::b_virtualserver_token_delete_all, 1);
    }

    this->server->getTokenManager().delete_token(token->id);

    if(token->token ==
            this->server->properties()[property::VIRTUALSERVER_AUTOGENERATED_PRIVILEGEKEY].as_unchecked<string>()) {
        this->server->properties()[property::VIRTUALSERVER_AUTOGENERATED_PRIVILEGEKEY] = "";
        this->server->properties()[property::VIRTUALSERVER_ASK_FOR_PRIVILEGEKEY] = false;
        logMessage(this->getServerId(), "{} Deleting the default server token. Don't ask anymore for this a token!", CLIENT_STR_LOG_PREFIX);
    }

    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandPluginCmd(Command &cmd) {
    CMD_REQ_SERVER;
    CMD_CHK_AND_INC_FLOOD_POINTS(5);

    auto mode = cmd["targetmode"].as<PluginTargetMode>();

    if (mode == PluginTargetMode::PLUGINCMD_CURRENT_CHANNEL) {
        CMD_REQ_CHANNEL;
        for (auto &cl : this->server->getClientsByChannel(this->currentChannel))
            cl->notifyPluginCmd(cmd["name"], cmd["data"], this->ref());
    } else if (mode == PluginTargetMode::PLUGINCMD_SUBSCRIBED_CLIENTS) {
        for (auto &cl : this->server->getClients())
            if (cl->isClientVisible(this->ref(), true))
                cl->notifyPluginCmd(cmd["name"], cmd["data"], this->ref());
    } else if (mode == PluginTargetMode::PLUGINCMD_SERVER) {
        for (auto &cl : this->server->getClients())
            cl->notifyPluginCmd(cmd["name"], cmd["data"], this->ref());
    } else if (mode == PluginTargetMode::PLUGINCMD_CLIENT) {
        for (int index = 0; index < cmd.bulkCount(); index++) {
            auto target = cmd[index]["target"].as<ClientId>();
            ConnectedLockedClient cl{this->server->find_client_by_id(target)};
            if (!cl) return command_result{error::client_invalid_id};
            cl->notifyPluginCmd(cmd["name"], cmd["data"], this->ref());
        }
    }

        /*
        else if(mode == PluginTargetMode::PLUGINCMD_SEND_COMMAND) {
            auto target = this->ref();
            if(cmd[0].has("target"))
                target = this->server->findClient(cmd["target"].as<ClientId>());
            if(!target) return command_result{error::client_invalid_id, "invalid target id"};

            target->sendCommand(Command(cmd["command"].string()), cmd[0].has("low") ? cmd["low"] : false);
        }
        */

    else return command_result{error::not_implemented};
    return command_result{error::ok};
}


command_result ConnectedClient::handleCommandWhoAmI(Command &cmd) {
    CMD_RESET_IDLE;

    Command result("");

    if (this->server) {
        result["virtualserver_status"] = ServerState::string(this->getServer()->state);
        result["virtualserver_id"] = this->server->getServerId();
        result["virtualserver_unique_identifier"] = this->server->properties()[property::VIRTUALSERVER_UNIQUE_IDENTIFIER].as_unchecked<string>();
        result["virtualserver_port"] = 0;
        if (this->server->udpVoiceServer) {
            result["virtualserver_port"] = this->server->properties()[property::VIRTUALSERVER_PORT].as_or<uint16_t>(0);
        }
    } else {
        result["virtualserver_status"] = "template";
        result["virtualserver_id"] = "0";
        result["virtualserver_unique_identifier"] = ""; //TODO generate uid
        result["virtualserver_port"] = "0";
    }

    result["client_id"] = this->getClientId();
    result["client_channel_id"] = this->currentChannel ? this->currentChannel->channelId() : 0;
    result["client_nickname"] = this->getDisplayName();
    result["client_database_id"] = this->getClientDatabaseId();
    result["client_login_name"] = this->properties()[property::CLIENT_LOGIN_NAME].as_unchecked<string>();
    result["client_unique_identifier"] = this->getUid();

    {
        auto query = dynamic_cast<QueryClient*>(this);
        if(query) {
            auto account = query->getQueryAccount();
            result["client_origin_server_id"] = account ? account->bound_server : 0;
        } else
            result["client_origin_server_id"] = 0;
    }

    this->sendCommand(result);
    return command_result{error::ok};
}

struct DBFindArgs {
    int index = 0;
    bool full = false;
    bool ip = false;
    Command cmd{""};
};

command_result ConnectedClient::handleCommandVersion(Command &) {
    CMD_RESET_IDLE;

    Command res("");
    res["version"] = build::version()->string(false);
    res["build_count"] = build::buildCount();
    res["build"] = duration_cast<seconds>(build::version()->timestamp.time_since_epoch()).count();
#ifdef WIN32
    res["platform"] = "Windows";
#else
    res["platform"] = "Linux";
#endif
    this->sendCommand(res);
    return command_result{error::ok};
}

//cid=%d password=%s
command_result ConnectedClient::handleCommandVerifyChannelPassword(Command &cmd) {
    CMD_REQ_SERVER;
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(5);

    std::shared_ptr<BasicChannel> channel = (this->server ? this->server->channelTree : serverInstance->getChannelTree().get())->findChannel(cmd["cid"].as<ChannelId>());
    if (!channel) return command_result{error::channel_invalid_id, "Cant resolve channel"};

    if (!channel->verify_password(cmd["password"].optional_string(), this->getType() != ClientType::CLIENT_QUERY)) return command_result{error::server_invalid_password};
    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandVerifyServerPassword(Command &cmd) {
    CMD_REQ_SERVER;
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(5);

    std::string password = cmd["password"];
    if (!this->server->verifyServerPassword(password, false)) return command_result{error::server_invalid_password};
    return command_result{error::ok};
}

//msgid=2 cluid=IkBXingb46\/z1Q3hhMvJEweb3lw= subject=The\sSubject timestamp=1512224138 flag_read=0
//notifymessagelist msgid=2 cluid=IkBXingb46\/z1Q3hhMvJEweb3lw= subject=The\sSubject timestamp=1512224138 flag_read=0
command_result ConnectedClient::handleCommandMessageList(Command &cmd) {
    CMD_REQ_SERVER;
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(5);

    auto msgList = this->server->letters->avariableLetters(this->getUid());
    if (msgList.empty()) return command_result{error::database_empty_result, "no letters available"};

    Command notify(this->getExternalType() == CLIENT_TEAMSPEAK ? "notifymessagelist" : "");

    int index = 0;
    for (const auto &elm : msgList) {
        notify[index]["msgid"] = elm->id;
        notify[index]["cluid"] = elm->sender;
        notify[index]["subject"] = elm->subject;
        notify[index]["timestamp"] = duration_cast<seconds>(elm->created.time_since_epoch()).count();
        notify[index]["flag_read"] = elm->read;
        index++;
    }

    this->sendCommand(notify);
    return command_result{error::ok};
}

//messageadd cluid=ePHuXhcai9nk\/4Fd\/xkxrokvnNk= subject=Test message=Message
command_result ConnectedClient::handleCommandMessageAdd(Command &cmd) {
    CMD_REQ_SERVER;
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(25);
    ACTION_REQUIRES_GLOBAL_PERMISSION(permission::b_client_offline_textmessage_send, 1);

    this->server->letters->createLetter(this->getUid(), cmd["cluid"], cmd["subject"], cmd["message"]);
    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandMessageGet(Command &cmd) {
    CMD_REQ_SERVER;
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(10);

    auto letter = this->server->letters->getFullLetter(cmd["msgid"]);

    //msgid=2 cluid=IkBXingb46\/z1Q3hhMvJEweb3lw= subject=The\sSubject message=The\sbody timestamp=1512224138
    Command notify(this->getExternalType() == CLIENT_TEAMSPEAK ? "notifymessage" : "");
    notify["msgid"] = cmd["msgid"];
    notify["cluid"] = letter->sender;
    notify["subject"] = letter->subject;
    notify["message"] = letter->message;
    notify["timestamp"] = duration_cast<seconds>(letter->created.time_since_epoch()).count();
    this->sendCommand(notify);

    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandMessageUpdateFlag(Command &cmd) {
    CMD_REQ_SERVER;
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(5);

    this->server->letters->updateReadFlag(cmd["msgid"], cmd["flag"]);

    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandMessageDel(Command &cmd) {
    CMD_REQ_SERVER;
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(5);

    this->server->letters->deleteLetter(cmd["msgid"]);

    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandPermGet(Command &cmd) {
    CMD_RESET_IDLE;
    ACTION_REQUIRES_GLOBAL_PERMISSION(permission::b_client_permissionoverview_own, 1);

    Command res("");

    deque<permission::PermissionType> requrested;
    auto permission_mapper = serverInstance->getPermissionMapper();
    auto type = this->getType();
    for (int index = 0; index < cmd.bulkCount(); index++) {
        permission::PermissionType permType = permission::unknown;
        if (cmd[index].has("permid"))
            permType = cmd[index]["permid"].as<permission::PermissionType>();
        else if (cmd[index].has("permsid"))
            permType = permission::resolvePermissionData(cmd[index]["permsid"].as<string>())->type; //TODO: Map the other way around!
        if (permission::resolvePermissionData(permType)->type == permission::PermissionType::unknown) return command_result{error::parameter_invalid, "could not resolve permission"};

        requrested.push_back(permType);
    }

    int index = 0;
    for(const auto& entry : this->calculate_permissions(requrested, this->getChannelId())) {
        if(!entry.second.has_value) continue;

        res[index]["permsid"] = permission_mapper->permission_name(type, entry.first);;
        res[index]["permid"] = entry.first;
        res[index++]["permvalue"] = entry.second.value;
    }
    if(index == 0)
        return command_result{error::database_empty_result};
    this->sendCommand(res);

    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandPermIdGetByName(Command &cmd) {
    auto found = permission::resolvePermissionData(cmd["permsid"].as<string>()); //TODO: Map the other way around
    Command res("");
    res["permid"] = found->type;
    this->sendCommand(res);
    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandPermFind(Command &cmd) {
    struct PermissionEntry {
        permission::PermissionType  permission_type;
        permission::PermissionValue permission_value;
        permission::PermissionSqlType type;

        GroupId group_id;
        ChannelId channel_id;
        ClientDbId client_id;

        bool negate;
        bool skip;
    };

    CMD_REQ_SERVER;
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(5);
    ACTION_REQUIRES_GLOBAL_PERMISSION(permission::b_virtualserver_permission_find, 1);

    std::vector<std::tuple<std::string, permission::PermissionType, bool>> requested_permissions{};
    requested_permissions.reserve(cmd.bulkCount());

    std::shared_ptr<permission::PermissionTypeEntry> permission;
    for(size_t index = 0; index < cmd.bulkCount(); index++) {
        bool granted = false;
        if (cmd[index].has("permid")) {
            permission = permission::resolvePermissionData((permission::PermissionType) (cmd[index]["permid"].as<permission::PermissionType>() & (~PERM_ID_GRANT)));
            granted = (cmd[index]["permid"].as<permission::PermissionType>() & PERM_ID_GRANT) > 0;

            if(permission->type == permission::PermissionType::unknown)
                return command_result{error::parameter_invalid, "could not resolve permission (id=" + cmd[index]["permid"].string() + ")"};
        } else if (cmd[index].has("permsid")) {
            permission = permission::resolvePermissionData(cmd[index]["permsid"].as<string>()); //TODO: Map the other way around
            granted = permission->grant_name == cmd[index]["permsid"].as<string>();

            if(permission->type == permission::PermissionType::unknown)
                return command_result{error::parameter_invalid, "could not resolve permission (id=" + cmd[index]["permid"].string() + ")"};
        } else {
            continue;
        }

        requested_permissions.emplace_back(permission->name, permission->type, granted);
    }

    if(requested_permissions.empty())
        return command_result{error::database_empty_result};

    std::map<std::string, std::tuple<permission::PermissionType, uint8_t>> db_lookup_mapping{};
    std::string query_string{};
    for(const auto& [name, id, as_granted] : requested_permissions) {
        auto& mapping = db_lookup_mapping[name];
        if(std::get<0>(mapping) == 0) {
            std::get<0>(mapping) = id;
            query_string += std::string{query_string.empty() ? "" : " OR "} + "`permId` = '" + name + "'";
        }
        std::get<1>(mapping) |= (1U << as_granted);
    }

    deque<unique_ptr<PermissionEntry>> entries;
    //`serverId` INT NOT NULL, `type` INT, `id` INT, `channelId` INT, `permId` VARCHAR(" UNKNOWN_KEY_LENGTH "), `value` INT, `grant` INT
    sql::command(this->sql, "SELECT `permId`, `type`, `id`, `channelId`, `value`, `grant`, `flag_skip`, `flag_negate` FROM `permissions` WHERE `serverId` = :sid AND (" + query_string + ") AND `type` != :playlist",
                 variable{":sid", this->server->getServerId()},
                 variable{":playlist", permission::SQL_PERM_PLAYLIST}
    ).query([&](int length, string* values, string* columns) {
        permission::PermissionSqlType type{permission::SQL_PERM_GROUP};
        uint64_t id{0};
        ChannelId channel_id{0};
        permission::PermissionValue value{0}, granted_value{0};
        string permission_name{};
        bool negate = false, skip = false;

#if 0
        for (int index = 0; index < length; index++) {
            try {
                if(columns[index] == "type")
                    type = static_cast<permission::PermissionSqlType>(stoll(values[index]));
                else if(columns[index] == "permId")
                    permission_name = values[index];
                else if(columns[index] == "id")
                    id = static_cast<uint64_t>(stoll(values[index]));
                else if(columns[index] == "channelId")
                    channel_id = static_cast<ChannelId>(stoll(values[index]));
                else if(columns[index] == "value")
                    value = static_cast<permission::PermissionValue>(stoll(values[index]));
                else if(columns[index] == "grant")
                    granted_value = static_cast<permission::PermissionValue>(stoll(values[index]));
                else if(columns[index] == "flag_negate")
                    negate = !values[index].empty() && stol(values[index]) == 1;
                else if(columns[index] == "flag_skip")
                    skip = !values[index].empty() && stol(values[index]) == 1;
            } catch(std::exception& ex) {
                debugMessage(this->getServerId(), "[{}] 'permfind' iterates over invalid permission entry. Key: {}, Value: {}, Error: {}", CLIENT_STR_LOG_PREFIX, columns[index], values[index], ex.what());
                return 0;
            }
        }
#else
        assert(length == 8);
        try {
            type = static_cast<permission::PermissionSqlType>(stoll(values[1]));
            permission_name = values[0];
            id = static_cast<uint64_t>(stoll(values[2]));
            channel_id = static_cast<ChannelId>(stoll(values[3]));
            value = static_cast<permission::PermissionValue>(stoll(values[4]));
            granted_value = static_cast<permission::PermissionValue>(stoll(values[5]));
            negate = values[7] == "1";
            skip = values[6] == "1";
        } catch(std::exception& ex) {
            return 0;
        }
#endif

        auto request = db_lookup_mapping.find(permission_name);
        if(request == db_lookup_mapping.end()) return 0; /* shall not happen */

        auto flags = std::get<1>(request->second);
        /* value */
        if((flags & 0x1U) > 0 && value > 0) {
            auto result = make_unique<PermissionEntry>();
            result->permission_type = std::get<0>(request->second);
            result->permission_value = value;
            result->type = type;
            result->channel_id = channel_id;
            result->negate = negate;
            result->skip = skip;
            if (type == permission::SQL_PERM_GROUP) {
                result->group_id = id;
            } else if(type == permission::SQL_PERM_USER) {
                result->client_id = id;
            }

            entries.push_back(std::move(result));
        }

        /* granted */
        if((flags & 0x2U) > 0 && granted_value > 0) {
            auto result = make_unique<PermissionEntry>();
            result->permission_type = (permission::PermissionType) (std::get<0>(request->second) | PERM_ID_GRANT);
            result->permission_value = granted_value;
            result->type = type;
            result->channel_id = channel_id;
            result->negate = negate;
            result->skip = skip;

            if (type == permission::SQL_PERM_GROUP) {
                result->group_id = id;
            } else if(type == permission::SQL_PERM_USER) {
                result->client_id = id;
            }

            entries.push_back(std::move(result));
        }
        return 0;
    });

    if(entries.empty()) {
        return command_result{error::database_empty_result};
    }

    struct CommandPerm {
        permission::PermissionType p;
        permission::PermissionValue v;
        int64_t id1;
        int64_t id2;
        uint8_t t;
    };

    std::vector<CommandPerm> perms;
    perms.resize(entries.size());

    size_t index{0};

    /* 1 := Server | 2 := Channel */
    btree::map<GroupId, int> group_types{};
    for(const auto& s_group : this->server->group_manager()->server_groups()->available_groups(groups::GroupCalculateMode::GLOBAL)) {
        group_types[s_group->group_id()] = 1;
    }
    for(const auto& c_group : this->server->group_manager()->channel_groups()->available_groups(groups::GroupCalculateMode::GLOBAL)) {
        group_types[c_group->group_id()] = 2;
    }

    for(const auto& entry : entries) {
        auto& perm = perms[index++];

#if 0 /* TS3 switched the oder and YatQa as well, to keep compatibility we do it as well */
        if(entry->type == permission::SQL_PERM_USER) {
            if(entry->channel_id > 0) {
                perm.id1 = entry->client_id;
                perm.id2 = entry->channel_id;
                perm.t = 4; /* client channel */
            } else {
                perm.id1 = 0;
                perm.id2 = entry->client_id;
                perm.t = 1; /* client server */
            }
        } else if(entry->type == permission::SQL_PERM_CHANNEL) {
            perm.id1 = 0;
            perm.id2 = entry->channel_id;
            perm.t = 2; /* channel permission */
        } else if(entry->type == permission::SQL_PERM_GROUP) {
            if(entry->channel_id > 0) {
                perm.id1 = entry->group_id;
                perm.id2 = 0;
                perm.t = 3; /* channel group */
            } else {
                perm.id1 = entry->group_id;
                perm.id2 = 0;
                perm.t = 0; /* server group */
            }
        }
#else
        if(entry->type == permission::SQL_PERM_USER) {
            if(entry->channel_id > 0) {
                perm.id1 = entry->channel_id;
                perm.id2 = entry->client_id;
                perm.t = 4; /* client channel */
            } else {
                perm.id1 = entry->client_id;
                perm.id2 = 0;
                perm.t = 1; /* client server */
            }
        } else if(entry->type == permission::SQL_PERM_CHANNEL) {
            perm.id1 = entry->channel_id;
            perm.id2 = 0;
            perm.t = 2; /* channel permission */
        } else if(entry->type == permission::SQL_PERM_GROUP) {
            auto group_type = group_types[entry->group_id];
            switch(group_type) {
                case 1:
                    perm.id1 = entry->group_id;
                    perm.id2 = 0;
                    perm.t = 0; /* server group */
                    break;

                case 2:
                    perm.id1 = 0;
                    perm.id2 = entry->group_id;
                    perm.t = 3; /* channel group */
                    break;

                case 0:
                default:
                    /* unknown group */
                    break;
            }
        }
#endif
        perm.p = entry->permission_type;
        perm.v = entry->permission_value;
    }
    perms.erase(perms.begin() + index, perms.end());


    sort(perms.begin(), perms.end(), [](const CommandPerm& a, const CommandPerm& b) {
        if(a.t < b.t) return true;
        else if(b.t < a.t) return false;

        if(a.id1 < b.id1) return true;
        else if(b.id1 < a.id1) return false;

        if(a.id2 < b.id2) return true;
        else if(b.id2 < a.id2) return false;

        if(a.p < b.p) return true;
        else if(b.p < a.p) return false;

        return &a > &b;
    });

    command_builder result{this->notify_response_command("notifypermfind"), 64, perms.size()};
    index = 0;

    for(const auto& e : perms) {
        auto bulk = result.bulk(index++);
        bulk.put("t", e.t);
        bulk.put("p", (uint16_t) e.p);
        bulk.put("v", e.v);
        bulk.put("id1", e.id1);
        bulk.put("id2", e.id2);
    }
    this->sendCommand(result);

    return command_result{error::ok};
}

/*
 * - Alle rechte der aktuellen server gruppen vom client
 * - Alle client rechte | channel cleint rechte
 * - Alle rechte des channels
 */
command_result ConnectedClient::handleCommandPermOverview(Command &cmd) {
    CMD_REQ_SERVER;
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(5);

    auto client_dbid = cmd["cldbid"].as<ClientDbId>();
    if(!serverInstance->databaseHelper()->validClientDatabaseId(this->getServer(), client_dbid)) {
        return command_result{error::client_invalid_id};
    }

    if(client_dbid == this->getClientDatabaseId()) {
        ACTION_REQUIRES_GLOBAL_PERMISSION(permission::b_client_permissionoverview_own, 1);
    } else {
        ACTION_REQUIRES_GLOBAL_PERMISSION(permission::b_client_permissionoverview_view, 1);
    }

    string channel_query, perm_query;

    auto channel = this->server ? this->server->channelTree->findChannel(cmd["cid"]) : serverInstance->getChannelTree()->findChannel(cmd["cid"]);
    if(!channel) {
        return command_result{error::channel_invalid_id};
    }

    auto permission_manager = serverInstance->databaseHelper()->loadClientPermissionManager(this->getServerId(), client_dbid);

    Command result(this->getExternalType() == ClientType::CLIENT_TEAMSPEAK ? "notifypermoverview" : "");
    size_t index = 0;
    result["cldbid"] = client_dbid;
    result["cid"] = channel->channelId();
    if(cmd["return_code"].size() > 0) {
        result["return_code"] = cmd["return_code"].string();
    }

    for(const auto& server_group : this->assignedServerGroups()) {
        auto permission_manager = server_group->permissions();
        for(const auto& permission_data : permission_manager->permissions()) {
            auto& permission = std::get<1>(permission_data);
            if(permission.flags.value_set) {
                result[index]["t"] = 0; /* server group */
                result[index]["id1"] = server_group->group_id();
                result[index]["id2"] = 0;

                result[index]["p"] = std::get<0>(permission_data);
                result[index]["v"] = permission.values.value;
                result[index]["n"] = permission.flags.negate;
                result[index]["s"] = permission.flags.skip;
                index++;
            }
            if(permission.flags.grant_set) {
                result[index]["t"] = 0; /* server group */
                result[index]["id1"] = server_group->group_id();
                result[index]["id2"] = 0;

                result[index]["p"] = (std::get<0>(permission_data) | PERM_ID_GRANT);
                result[index]["v"] = permission.values.grant;
                result[index]["n"] = false;
                result[index]["s"] = false;
                index++;
            }
        }
    }

    {
        for(const auto& permission_data : permission_manager->permissions()) {
            auto& permission = std::get<1>(permission_data);
            if(permission.flags.value_set) {
                result[index]["t"] = 1; /* client */
                result[index]["id1"] = client_dbid;
                result[index]["id2"] = 0;

                result[index]["p"] = std::get<0>(permission_data);
                result[index]["v"] = permission.values.value;
                result[index]["n"] = permission.flags.negate;
                result[index]["s"] = permission.flags.skip;
                index++;
            }
            if(permission.flags.grant_set) {
                result[index]["t"] = 1; /* client */
                result[index]["id1"] = client_dbid;
                result[index]["id2"] = 0;

                result[index]["p"] = (std::get<0>(permission_data) | PERM_ID_GRANT);
                result[index]["v"] = permission.values.grant;
                result[index]["n"] = false;
                result[index]["s"] = false;
                index++;
            }
        }
    }

    {
        auto permission_manager =  channel->permissions();
        for(const auto& permission_data : permission_manager->permissions()) {
            auto& permission = std::get<1>(permission_data);
            if(permission.flags.value_set) {
                result[index]["t"] = 2; /* server channel */
                result[index]["id1"] = channel->channelId();
                result[index]["id2"] = 0;

                result[index]["p"] = std::get<0>(permission_data);
                result[index]["v"] = permission.values.value;
                result[index]["n"] = permission.flags.negate;
                result[index]["s"] = permission.flags.skip;
                index++;
            }
            if(permission.flags.grant_set) {
                result[index]["t"] = 2; /* server channel */
                result[index]["id1"] = channel->channelId();
                result[index]["id2"] = 0;

                result[index]["p"] = (std::get<0>(permission_data) | PERM_ID_GRANT);
                result[index]["v"] = permission.values.grant;
                result[index]["n"] = false;
                result[index]["s"] = false;
                index++;
            }
        }
    }

    auto interited_channel{channel};
    auto channel_group = this->assignedChannelGroup(interited_channel);
    if(channel_group) {
        assert(interited_channel);

        auto permission_manager = channel_group->permissions();
        for(const auto& permission_data : permission_manager->permissions()) {
            auto& permission = std::get<1>(permission_data);
            if(permission.flags.value_set) {
                result[index]["t"] = 3; /* channel group */
                result[index]["id1"] = interited_channel->channelId();
                result[index]["id2"] = channel_group->group_id();

                result[index]["p"] = std::get<0>(permission_data);
                result[index]["v"] = permission.values.value;
                result[index]["n"] = permission.flags.negate;
                result[index]["s"] = permission.flags.skip;
                index++;
            }
            if(permission.flags.grant_set) {
                result[index]["t"] = 3; /* channel group */
                result[index]["id1"] = interited_channel->channelId();
                result[index]["id2"] = channel_group->group_id();

                result[index]["p"] = (std::get<0>(permission_data) | PERM_ID_GRANT);
                result[index]["v"] = permission.values.grant;
                result[index]["n"] = false;
                result[index]["s"] = false;
                index++;
            }
        }
    }

    {
        for(const auto& permission_data : permission_manager->channel_permissions()) {
            auto& permission = std::get<2>(permission_data);
            if(permission.flags.value_set) {
                result[index]["t"] = 4; /* client channel */
                result[index]["id1"] = std::get<1>(permission_data);
                result[index]["id2"] = client_dbid;

                result[index]["p"] = std::get<0>(permission_data);
                result[index]["v"] = permission.values.value;
                result[index]["n"] = permission.flags.negate;
                result[index]["s"] = permission.flags.skip;
                index++;
            }
            if(permission.flags.grant_set) {
                result[index]["t"] = 1; /* client */
                result[index]["id1"] = std::get<1>(permission_data);
                result[index]["id2"] = client_dbid;

                result[index]["p"] = (std::get<0>(permission_data) | PERM_ID_GRANT);
                result[index]["v"] = permission.values.grant;
                result[index]["n"] = false;
                result[index]["s"] = false;
                index++;
            }
        }
    }

    if (index == 0) return command_result{error::database_empty_result};
    this->sendCommand(result);

    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandComplainAdd(Command &cmd) {
    CMD_REQ_SERVER;
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(25);

    ClientDbId target = cmd["tcldbid"];
    std::string msg = cmd["message"];

    auto cl = this->server->findClientsByCldbId(target);
    if (cl.empty()) return command_result{error::client_invalid_id};
    ACTION_REQUIRES_GLOBAL_PERMISSION(permission::i_client_complain_power, cl[0]->calculate_permission(permission::i_client_needed_complain_power, 0));

    /*
    if(!serverInstance->databaseHelper()->validClientDatabaseId(target))
        return command_result{error::client_invalid_id, "invalid database id"};
    */

    for (const auto &elm : this->server->complains->findComplainsFromTarget(target))
        if (elm->invoker == this->getClientDatabaseId())
            return command_result{error::database_duplicate_entry, "you already send a complain"};

    if (!this->server->complains->createComplain(target, this->getClientDatabaseId(), msg)) return command_result{error::vs_critical, "could not create complains"};
    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandComplainList(Command &cmd) {
    CMD_REQ_SERVER;
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(25);
    ACTION_REQUIRES_GLOBAL_PERMISSION(permission::b_client_complain_list, 1);

    ClientDbId id = cmd[0].has("tcldbid") ? cmd["tcldbid"].as<ClientDbId>() : 0;
    auto list = id == 0 ? this->server->complains->complains() : this->server->complains->findComplainsFromTarget(id);
    if (list.empty()) return command_result{error::database_empty_result};

    deque<ClientDbId> nameQuery;
    for (const auto &elm : list) {
        if (std::find(nameQuery.begin(), nameQuery.end(), elm->invoker) == nameQuery.end())
            nameQuery.push_back(elm->invoker);
        if (std::find(nameQuery.begin(), nameQuery.end(), elm->target) == nameQuery.end())
            nameQuery.push_back(elm->target);
    }

    auto dbInfo = serverInstance->databaseHelper()->queryDatabaseInfo(this->server, nameQuery);

    Command result(this->getExternalType() == CLIENT_TEAMSPEAK ? "notifycomplainlist" : "");
    int index = 0;
    for (const auto &elm : list) {
        result[index]["tcldbid"] = elm->target;
        result[index]["tname"] = "unknown";

        result[index]["fcldbid"] = elm->invoker;
        result[index]["fname"] = "unknown";

        result[index]["message"] = elm->reason;
        result[index]["timestamp"] = chrono::duration_cast<chrono::seconds>(elm->created.time_since_epoch()).count();

        for (const auto &e : dbInfo) {
            if (e->client_database_id == elm->target)
                result[index]["tname"] = e->client_nickname;
            if (e->client_database_id == elm->invoker)
                result[index]["fname"] = e->client_nickname;
        }
        index++;
    }

    this->sendCommand(result);
    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandComplainDel(Command &cmd) {
    CMD_REQ_SERVER;
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(5);

    ClientDbId tid = cmd["tcldbid"];
    ClientDbId fid = cmd["fcldbid"];

    shared_ptr<ComplainEntry> entry;
    for (const auto &elm : this->server->complains->findComplainsFromTarget(tid))
        if (elm->invoker == fid) {
            entry = elm;
            break;
        }
    if (!entry) return command_result{error::database_empty_result};

    if (entry->invoker == this->getClientDatabaseId())
        ACTION_REQUIRES_GLOBAL_PERMISSION(permission::b_client_complain_delete_own, 1);
    else
        ACTION_REQUIRES_GLOBAL_PERMISSION(permission::b_client_complain_delete, 1);

    this->server->complains->deleteComplain(entry);

    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandComplainDelAll(Command &cmd) {
    CMD_REQ_SERVER;
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(25);
    ACTION_REQUIRES_GLOBAL_PERMISSION(permission::b_client_complain_delete, 1);

    ClientDbId tid = cmd["tcldbid"];
    if (!this->server->complains->deleteComplainsFromTarget(tid)) return command_result{error::database_empty_result};
    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandHelp(Command& cmd) {
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(5);
    ACTION_REQUIRES_GLOBAL_PERMISSION(permission::b_serverinstance_help_view, 1);

    string command = cmd[0].has("command") ? cmd["command"].as<string>() : "";
    if(command.empty())
        for(const auto& key : cmd[0].keys()) {
            command = key;
            break;
        }
    if(command.empty())
        command = "help";
    std::transform(command.begin(), command.end(), command.begin(), ::tolower);

    auto file = fs::u8path("commanddocs/" + command + ".txt");
    if(!fs::exists(file)) return command_result{error::file_not_found};

    string line;
    ifstream stream(file);
    if(!stream) return command_result{error::file_io_error};
    while(getline(stream, line))
        this->sendCommand(Command{line});

    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandPermReset(ts::Command& cmd) {
    CMD_REQ_SERVER;
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(50);
    ACTION_REQUIRES_GLOBAL_PERMISSION(permission::b_virtualserver_permission_reset, 1);

    string token;
    if(!this->server->resetPermissions(token))
        return command_result{error::vs_critical};

    Command result("");
    result["token"] = token;
    this->sendCommand(result);

    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandLogView(ts::Command& cmd) {
    CMD_CHK_AND_INC_FLOOD_POINTS(50);

    ServerId target_server = cmd[0].has("instance") && cmd["instance"].as<bool>() ? (ServerId) 0 : this->getServerId();
    if(target_server == 0)
        ACTION_REQUIRES_INSTANCE_PERMISSION(permission::b_serverinstance_log_view, 1);
    else
        ACTION_REQUIRES_GLOBAL_PERMISSION(permission::b_virtualserver_log_view, 1);

    auto lagacy = this->getType() == CLIENT_TEAMSPEAK || cmd.hasParm("lagacy") || cmd.hasParm("legacy");
#if 0
    string log_path;
    string server_identifier;
    if(target_server > 0)
        server_identifier = to_string(target_server);
    else
        server_identifier = "[A-Z]{0,7}";

    for(const auto& sink : logger::logger(target_server)->sinks()) {
        if(dynamic_pointer_cast<spdlog::sinks::rotating_file_sink_mt>(sink)) {
            log_path = dynamic_pointer_cast<spdlog::sinks::rotating_file_sink_mt>(sink)->filename();
            break;
        } else if(dynamic_pointer_cast<spdlog::sinks::rotating_file_sink_st>(sink)) {
            log_path = dynamic_pointer_cast<spdlog::sinks::rotating_file_sink_st>(sink)->filename();
            break;
        }
    }
    if(log_path.empty())
        return command_result{error::file_not_found, "Cant find log file (May log disabled?)"};

    { //Replace " within the log path
        size_t index = 0;
        while((index = log_path.find('"', index)) != string::npos) {
            log_path.replace(index, 1, "\\\"");
            index += 2;
        }
    }
    string command = "cat \"" + log_path + "\"";
    command += " | grep -E ";
    command += R"("\] \[.*\]( ){0,6}?)" + server_identifier + " \\|\"";

    size_t beginpos = cmd[0].has("begin_pos") ? cmd["begin_pos"].as<size_t>() : 0ULL; //TODO test it?
    size_t file_index = 0;
    size_t max_lines = cmd[0].has("lines") ? cmd["lines"].as<size_t>() : 100ULL; //TODO bounds?
    deque<pair<uintptr_t, string>> lines;
    {
        debugMessage(target_server, "Logview command: \"{}\"", command);

        array<char, 1024> buffer{};
        string line_buffer;

        std::shared_ptr<FILE> pipe(popen(command.c_str(), "r"), pclose);
        if (!pipe) return command_result{error::file_io_error, "Could not execute command"};
        while (!feof(pipe.get())) {
            auto read = fread(buffer.data(), 1, buffer.size(), pipe.get());
            if(read > 0) {
                if(beginpos == 0 || file_index < beginpos) {
                    if(beginpos != 0 && file_index + read > beginpos) { //We're done we just want to get the size later
                        line_buffer += string(buffer.data(), beginpos - file_index);

                        lines.emplace_back(file_index, line_buffer);
                        if(lines.size() > max_lines) lines.pop_front();
                        //debugMessage(LOG_GENERAL, "Final line {}", line_buffer);
                        line_buffer = "";
                    } else {
                        line_buffer += string(buffer.data(), read);

                        size_t index;
                        size_t length;
                        size_t cut_offset = 0;
                        while((index = line_buffer.find("\n")) != string::npos || (index = line_buffer.find("\r")) != string::npos) {
                            length = 0;
                            if(index > 0) {
                                if(line_buffer[index - 1] == '\r' || line_buffer[index - 1] == '\n') {
                                    length = 2;
                                    index--;
                                }
                            }
                            if(length == 0) {
                                if(index + 1 < line_buffer.length()) {
                                    if(line_buffer[index + 1] == '\r' || line_buffer[index + 1] == '\n') {
                                        length = 2;
                                    }
                                }
                            }
                            if(length == 0) length = 1;

                            //debugMessage(LOG_GENERAL, "Got line {}",  line_buffer.substr(0, index));
                            lines.emplace_back(file_index + cut_offset, line_buffer.substr(0, index));
                            if(lines.size() > max_lines) lines.pop_front();

                            cut_offset += index + length;
                            line_buffer = line_buffer.substr(index + length);
                        }
                    }
                }
                file_index += read;
            } else if(read < 0) return command_result{error::file_io_error, "fread(...) returned " + to_string(read) + " (" + to_string(errno) + ")"};
        }

        if(!line_buffer.empty()) {
            lines.emplace_back(file_index - line_buffer.length(), line_buffer);
            if(lines.size() > max_lines) lines.pop_front();
        }
    }
    //last_pos=1558 file_size=1764 l
    if(lines.empty()) return command_result{error::database_empty_result};
    Command result(this->getExternalType() == ClientType::CLIENT_TEAMSPEAK ? "notifyserverlog" : "");
    result["last_pos"] = lines.front().first;
    result["file_size"] = file_index;

    if(!(cmd.hasParm("reverse") && cmd["revers"].as<bool>()))
        std::reverse(lines.begin(), lines.end());

    int index = 0;
    for(const auto& index_line : lines) {
        auto line = index_line.second;
        //2018-07-15 21:01:46.488639
        //TeamSpeak format:
        //YYYY-MM-DD hh:mm:ss.millis|{:<8}|{:<14}|{:<3}|....
        //2018-07-15 21:01:47.066367|INFO    |VirtualServer |1  |listening on 0.0.0.0:9989, [::]:9989

        //TeaSpeak:
        //[2018-07-15 23:21:47] [ERROR] Timer sql_test tick needs more than 9437 microseconds. Max allowed was 5000 microseconds.
        if(lagacy) {
            string ts = line.substr(1, 19) + ".000000|";

            {
                string type = "unknown";

                auto idx = line.find_first_of('[', 2);
                if(idx != string::npos) {
                    type = line.substr(idx + 1, line.find(']', idx + 1) - idx - 1);
                }

                ts += type + " |    | |" + line.substr(line.find('|') + 1);
            }

            if(ts.length() > 1024)
                ts = ts.substr(0, 1024) + "...";
            result[index++]["l"] = ts;
        } else {
            if(line.length() > 1024)
                line = line.substr(0, 1024) + "...";
            result[index++]["l"] = line;
        }
    }
    this->sendCommand(result);
#else
    constexpr static std::array<std::string_view, 5> log_output{
        "located at your TeaSpeak installation folder. All logs could be found there.",
        "If you need to lookup the TeaSpeak - Server logs, please visit the 'logs/' folder,",
        "",
        "In order to lookup the server actions use 'logquery'.",
        "The command 'logview' is not supported anymore."
    };

    command_builder result{this->getExternalType() == ClientType::CLIENT_TEAMSPEAK ? "notifyserverlog" : ""};
    result.put_unchecked(0, "last_pos", 0);
    result.put_unchecked(0, "file_size", 0);

    size_t index{0};
    if(lagacy) {
        for(const auto& message : log_output) {
           std::string line{"2020-06-27 00:00.000" + std::to_string(index) + "|CRITICAL|Server Instance | |"};
           line += message;
           result.put_unchecked(index++, "l", line);
        }
    } else {
        for(const auto& message : log_output) {
            std::string line{"[2020-06-27 00:00:0" + std::to_string(index) + "][ERROR] "};
            line += message;
            result.put_unchecked(index++, "l", line);
        }
    }
    this->sendCommand(result);
#endif

    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandLogQuery(ts::Command &cmd) {
    CMD_CHK_AND_INC_FLOOD_POINTS(50);

    uint64_t target_server = (cmd[0].has("instance") && cmd["instance"].as<bool>()) || cmd.hasParm("instance") ? (ServerId) 0 : this->getServerId();
    if(target_server == 0) {
        ACTION_REQUIRES_INSTANCE_PERMISSION(permission::b_serverinstance_log_view, 1);
    } else {
        ACTION_REQUIRES_GLOBAL_PERMISSION(permission::b_virtualserver_log_view, 1);
    }

    std::chrono::system_clock::time_point timestamp_begin{}, timestamp_end{};

    if(cmd[0].has("begin"))
        timestamp_begin += std::chrono::milliseconds{cmd["begin"].as<uint64_t>()};

    if(cmd[0].has("end"))
        timestamp_end += std::chrono::milliseconds{cmd["end"].as<uint64_t>()};

    if(timestamp_begin <= timestamp_end && timestamp_begin.time_since_epoch().count() != 0)
        return command_result{error::parameter_constraint_violation, "begin > end"};

    size_t limit{100};
    if(cmd[0].has("limit"))
        limit = std::min((size_t) 2000, cmd["limit"].as<size_t>());

    std::vector<log::LoggerGroup> groups{};
    if(cmd[0].has("groups")) {
        auto groups_string = cmd["groups"].string();
        size_t offset{0}, findex;
        do {
            findex = groups_string.find(',', offset);

            auto group_name = groups_string.substr(offset, findex - offset);
            offset = findex + 1;


            size_t index{0};
            for(; index < (size_t) log::LoggerGroup::MAX; index++) {
                auto group = static_cast<log::LoggerGroup>(index);
                if(log::kLoggerGroupName[(int) group] == group_name) {
                    if(std::find(groups.begin(), groups.end(), group) != groups.end())
                        return command_result{error::parameter_invalid, "groups"};

                    groups.push_back(group);
                }
            }

            if(index == (size_t) log::LoggerGroup::MAX)
                return command_result{error::parameter_invalid, "groups"};
        } while(offset != 0);
    }

    auto result = serverInstance->action_logger()->query(groups, target_server, timestamp_begin, timestamp_end, limit);
    if(result.empty())
        return command_result{error::database_empty_result};

    command_builder notify{this->notify_response_command("notifylogquery")};
    size_t index{0};
    size_t threshold = this->getType() == ClientType::CLIENT_QUERY ? (size_t) -1 : 4096; /* limit each command to 4096 bytes */
    for(log::LogEntryInfo& entry : result) {
        notify.set_bulk(index, std::move(entry.info));
        if(index == 0)
            notify.put_unchecked(0, "sid", this->getServerId());

        if(notify.current_size() > threshold) {
            this->sendCommand(notify);
            notify.reset();
            index = 0;
        }
        index++;
    }
    if(index> 0)
        this->sendCommand(notify);

    if(this->getType() != ClientType::CLIENT_QUERY)
        this->sendCommand(command_builder{"notifylogqueryfinished"});

    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandLogAdd(ts::Command& cmd) {
    CMD_CHK_AND_INC_FLOOD_POINTS(50);

    uint64_t target_server = cmd[0].has("instance") && cmd["instance"].as<bool>() ? (ServerId) 0 : this->getServerId();
    if(target_server == 0) {
        ACTION_REQUIRES_INSTANCE_PERMISSION(permission::b_serverinstance_log_add, 1);
    } else {
        ACTION_REQUIRES_GLOBAL_PERMISSION(permission::b_virtualserver_log_add, 1);
    }

    serverInstance->action_logger()->custom_logger.add_log_message(target_server, this->ref(), cmd["msg"]);
    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandUpdateMyTsId(ts::Command &) {
    if(config::voice::suppress_myts_warnings) return command_result{error::ok};
    return command_result{error::not_implemented};
}

command_result ConnectedClient::handleCommandUpdateMyTsData(ts::Command &) {
    if(config::voice::suppress_myts_warnings) return command_result{error::ok};
    return command_result{error::not_implemented};
}


command_result ConnectedClient::handleCommandQueryList(ts::Command &cmd) {
    OptionalServerId server_id = EmptyServerId;
    if(this->getExternalType() == ClientType::CLIENT_TEAMSPEAK)
        server_id = this->getServerId();

    if(cmd[0].has("server_id"))
        server_id = cmd["server_id"];
    if(cmd[0].has("sid"))
        server_id = cmd["sid"];

    auto server = server_id == EmptyServerId ? nullptr : serverInstance->getVoiceServerManager()->findServerById(server_id);
    if(!server && server_id != EmptyServerId && server_id != 0)
        return command_result{error::server_invalid_id};

    ClientPermissionCalculator client_permissions{server, this->getClientDatabaseId(), this->getType(), 0};
    auto global_list = permission::v2::permission_granted(1, client_permissions.calculate_permission(permission::b_client_query_list));
    auto own_list = global_list || permission::v2::permission_granted(1, client_permissions.calculate_permission(permission::b_client_query_list_own));

    if(!own_list && !global_list)
        return command_result{permission::b_client_query_list};

    auto accounts = serverInstance->getQueryServer()->list_query_accounts(server_id);
    if(!global_list) {
        accounts.erase(remove_if(accounts.begin(), accounts.end(), [&](const std::shared_ptr<QueryAccount>& account) {
            return account->unique_id != this->getUid();
        }), accounts.end());
    }

    if(accounts.empty())
        return command_result{error::database_empty_result};

    Command result(this->getExternalType() == ClientType::CLIENT_TEAMSPEAK ? "notifyquerylist" : "");
    result["server_id"] = server_id;
    result["flag_own"] = own_list;
    result["flag_all"] = global_list;

    size_t index = 0;
    for(const auto& account : accounts) {
        result[index]["client_unique_identifier"] = account->unique_id;
        result[index]["client_login_name"] = account->username;
        result[index]["client_bound_server"] = account->bound_server;
        index++;
    }

    this->sendCommand(result);

    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandQueryCreate(ts::Command &cmd) {
    OptionalServerId server_id = this->getServerId();
    if(cmd[0].has("server_id"))
        server_id = cmd["server_id"];

    if(cmd[0].has("sid"))
        server_id = cmd["sid"];

    auto server = server_id == EmptyServerId ? nullptr : serverInstance->getVoiceServerManager()->findServerById(server_id);
    if(!server && server_id != EmptyServerId && server_id != 0)
        return command_result{error::server_invalid_id};

    auto username = cmd["client_login_name"].as<string>();
    auto password = cmd[0].has("client_login_password") ? cmd["client_login_password"].as<string>() : "";

    auto account = serverInstance->getQueryServer()->find_query_account_by_name(username);
    if(account) return command_result{error::query_already_exists};

    ClientPermissionCalculator client_permissions{server, this->getClientDatabaseId(), this->getType(), 0};
    auto uid{this->getUid()};
    if(cmd[0].has("cldbid")){
        if(!serverInstance->databaseHelper()->validClientDatabaseId(server, cmd["cldbid"].as<ClientDbId>()))
            return command_result{error::database_empty_result};


        if(!permission::v2::permission_granted(1, client_permissions.calculate_permission(permission::b_client_query_create))) {
            return command_result{permission::b_client_query_create};
        }

        auto info = serverInstance->databaseHelper()->queryDatabaseInfo(server, {cmd["cldbid"].as<ClientDbId>()});
        if(info.empty())
            return command_result{error::database_empty_result};
        uid = info[0]->client_unique_id;
    } else {
        if(!permission::v2::permission_granted(1, client_permissions.calculate_permission(permission::b_client_query_create_own))) {
            return command_result{permission::b_client_query_create_own};
        }
    }

    if(password.empty())
        password = rnd_string(QUERY_PASSWORD_LENGTH);

    account = serverInstance->getQueryServer()->create_query_account(username, server_id, uid, password);
    if(!account)
        return command_result{error::vs_critical};

    Command result(this->getExternalType() == ClientType::CLIENT_TEAMSPEAK ? "notifyquerycreated" : "");
    result["client_unique_identifier"] = account->unique_id;
    result["client_login_name"] = account->username;
    result["client_login_password"] = password;
    result["client_bound_server"] = account->bound_server;
    this->sendCommand(result);

    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandQueryDelete(ts::Command &cmd) {
    auto username = cmd["client_login_name"].as<string>();
    auto account = serverInstance->getQueryServer()->find_query_account_by_name(username);
    if(!account)
        return command_result{error::query_not_exists};

    auto server = serverInstance->getVoiceServerManager()->findServerById(account->bound_server);
    /* If the server is not existing anymore, we're asking for global permissions
    if(!server && account->bounded_server != 0)
        return command_result{error::server_invalid_id};
    */

    ClientPermissionCalculator client_permissions{server, this->getClientDatabaseId(), this->getType(), 0};
    auto delete_all = permission::v2::permission_granted(1, client_permissions.calculate_permission(permission::b_client_query_delete));
    auto delete_own = delete_all || permission::v2::permission_granted(1, client_permissions.calculate_permission(permission::b_client_query_delete_own));

    if(account->unique_id == this->getUid()) {
        if(!delete_own) {
            return command_result{permission::b_client_query_delete_own};
        }
    } else {
        if(!delete_all) {
            return command_result{permission::b_client_query_delete};
        }
    }

    if(account->unique_id == "serveradmin" && account->username == "serveradmin") {
        return command_result{error::vs_critical};
    }

    serverInstance->getQueryServer()->delete_query_account(account);

    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandQueryRename(ts::Command &cmd) {
    auto username = cmd["client_login_name"].as<string>();
    auto new_username = cmd["client_new_login_name"].as<string>();

    auto account = serverInstance->getQueryServer()->find_query_account_by_name(username);
    if(!account)
        return command_result{error::query_not_exists};

    auto server = serverInstance->getVoiceServerManager()->findServerById(account->bound_server);
    if(!server && account->bound_server != 0)
        return command_result{error::server_invalid_id};


    ClientPermissionCalculator client_permissions{server, this->getClientDatabaseId(), this->getType(), 0};
    auto rename_all = permission::v2::permission_granted(1, client_permissions.calculate_permission(permission::b_client_query_rename));
    auto rename_own = rename_all || permission::v2::permission_granted(1, client_permissions.calculate_permission(permission::b_client_query_rename_own));

    if(account->unique_id == this->getUid()) {
        if(!rename_own) {
            return command_result{permission::b_client_query_rename_own};
        }
    } else {
        if(!rename_all) {
            return command_result{permission::b_client_query_rename};
        }
    }

    auto target_account = serverInstance->getQueryServer()->find_query_account_by_name(new_username);
    if(target_account) return command_result{error::query_already_exists};

    if(account->unique_id == "serveradmin" && account->username == "serveradmin")
        return command_result{error::parameter_invalid};

    if(!serverInstance->getQueryServer()->rename_query_account(account, new_username))
        return command_result{error::vs_critical};

    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandQueryChangePassword(ts::Command &cmd) {
    auto username = cmd["client_login_name"].as<string>();
    auto account = serverInstance->getQueryServer()->find_query_account_by_name(username);
    if(!account)
        return command_result{error::query_not_exists};

    auto server = serverInstance->getVoiceServerManager()->findServerById(account->bound_server);
    if(!server && account->bound_server != 0)
        return command_result{error::server_invalid_id};

    ClientPermissionCalculator client_permissions{server, this->getClientDatabaseId(), this->getType(), 0};
    auto change_all = permission::v2::permission_granted(1, client_permissions.calculate_permission(permission::b_client_query_change_password));
    auto change_own = change_all || permission::v2::permission_granted(1, client_permissions.calculate_permission(permission::b_client_query_change_own_password));

    auto password = cmd[0].has("client_login_password") ? cmd["client_login_password"].as<string>() : "";

    if(password.empty())
        password = rnd_string(QUERY_PASSWORD_LENGTH);

    if(account->unique_id == this->getUid()) {
        if(!change_own)
            return command_result{permission::b_client_query_change_own_password};
    } else {
        if(!change_all)
            return command_result{server ? permission::b_client_query_change_password : permission::b_client_query_change_password_global};
    }

    if(!serverInstance->getQueryServer()->change_query_password(account, password))
        return command_result{error::vs_critical};

    Command result(this->getExternalType() == ClientType::CLIENT_TEAMSPEAK ? "notifyquerypasswordchanges" : "");
    result["client_login_name"] = account->username;
    result["client_login_password"] = password;
    this->sendCommand(result);

    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandDummy_IpChange(ts::Command &cmd) {
    CMD_REF_SERVER(server);
    logMessage(this->getServerId(), "[{}] Address changed from {} to {}", CLIENT_STR_LOG_PREFIX, cmd["old_ip"].string(), cmd["new_ip"].string());

    if(geoloc::provider_vpn && !permission::v2::permission_granted(1, this->calculate_permission(permission::b_client_ignore_vpn, 0))) {
        auto provider = this->isAddressV4() ? geoloc::provider_vpn->resolveInfoV4(this->getPeerIp(), true) : geoloc::provider_vpn->resolveInfoV6(this->getPeerIp(), true);
        if(provider) {
            this->disconnect(strvar::transform(ts::config::messages::kick_vpn, strvar::StringValue{"provider.name", provider->name}, strvar::StringValue{"provider.website", provider->side}));
            return command_result{error::ok};
        }
    }

    string new_country = config::geo::countryFlag;
    if(geoloc::provider) {
        auto loc = this->isAddressV4() ? geoloc::provider->resolveInfoV4(this->getPeerIp(), false) : geoloc::provider->resolveInfoV6(this->getPeerIp(), false);
        if(loc) {
            logMessage(this->getServerId(), "[{}] Received new ip location. IP {} traced to {} ({}).", CLIENT_STR_LOG_PREFIX, this->getLoggingPeerIp(), loc->name, loc->identifier);
            this->properties()[property::CLIENT_COUNTRY] = loc->identifier;
            server->notifyClientPropertyUpdates(this->ref(),  deque<property::ClientProperties>{property::CLIENT_COUNTRY});
            new_country = loc->identifier;
        } else {
            logError(this->getServerId(), "[{}] Failed to resolve ip location for IP {}.", CLIENT_STR_LOG_PREFIX, this->getLoggingPeerIp());
        }
    }

    serverInstance->databaseHelper()->updateClientIpAddress(this->getServerId(), this->getClientDatabaseId(), this->getLoggingPeerIp());
    return command_result{error::ok};
}

//conversationhistory cid=1 [cpw=xxx] [timestamp_begin] [timestamp_end (0 := no end)] [message_count (default 25| max 100)] [-merge]
command_result ConnectedClient::handleCommandConversationHistory(ts::Command &command) {
    CMD_REF_SERVER(ref_server);
    CMD_CHK_AND_INC_FLOOD_POINTS(25);

    if(!command[0].has("cid") || !command[0]["cid"].castable<ChannelId>())
        return command_result{error::conversation_invalid_id};

    auto conversation_id = command[0]["cid"].as<ChannelId>();
    /* test if we have access to the conversation */
    if(conversation_id > 0) {
        /* test if we're able to see the channel */
        {
            shared_lock channel_view_lock(this->channel_tree_mutex);
            auto channel = this->channel_view()->find_channel(conversation_id);
            if(!channel)
                return command_result{error::conversation_invalid_id};

            auto conversation_mode = channel->channel()->properties()[property::CHANNEL_CONVERSATION_MODE].as_unchecked<ChannelConversationMode>();
            switch (conversation_mode) {
                case ChannelConversationMode::CHANNELCONVERSATIONMODE_PRIVATE:
                    return command_result{error::conversation_is_private};

                case ChannelConversationMode::CHANNELCONVERSATIONMODE_NONE:
                    return command_result{error::conversation_not_exists};

                case ChannelConversationMode::CHANNELCONVERSATIONMODE_PUBLIC:
                    break;
            }
        }

        /* test if there is a channel password or join power which denies that we see the conversation */
        {
            shared_lock channel_view_lock(ref_server->channel_tree_mutex);
            auto channel = ref_server->getChannelTree()->findChannel(conversation_id);
            if(!channel) /* should never happen! */
                return command_result{error::conversation_invalid_id};

            if (!channel->verify_password(command["cpw"].optional_string(), this->getType() != ClientType::CLIENT_QUERY)) {
                ACTION_REQUIRES_PERMISSION(permission::b_channel_join_ignore_password, 1, channel->channelId());
            }

            auto error = this->calculate_and_get_join_state(channel);
            if(error) return command_result{error};
        }
    }

    auto conversation_manager = ref_server->conversation_manager();
    auto conversation = conversation_manager->get(conversation_id);
    if(!conversation)
        return command_result{error::database_empty_result};

    system_clock::time_point timestamp_begin = system_clock::now();
    system_clock::time_point timestamp_end;
    size_t message_count = 25;

    if(command[0].has("timestamp_begin"))
        timestamp_begin = system_clock::time_point{} + milliseconds(command[0]["timestamp_begin"].as<uint64_t>());

    if(command[0].has("timestamp_end"))
        timestamp_end = system_clock::time_point{} + milliseconds(command[0]["timestamp_end"].as<uint64_t>());

    if(command[0].has("message_count"))
        message_count = command[0]["message_count"].as<uint64_t>();

    if(timestamp_begin < timestamp_end)
        return command_result{error::parameter_invalid};
    if(message_count > 100)
        message_count = 100;

    auto messages = conversation->message_history(timestamp_begin, message_count + 1, timestamp_end); /* query one more to test for more data */
    if(messages.empty())
        return command_result{error::database_empty_result};
    bool more_data = messages.size() > message_count;
    if(more_data)
        messages.pop_back();

    Command notify(this->notify_response_command("notifyconversationhistory"));
    size_t index = 0;
    size_t length = 0;
    bool merge = command.hasParm("merge");

    for(auto it = messages.rbegin(); it != messages.rend(); it++) {
        if(index == 0) {
            notify[index]["cid"] = conversation_id;
            notify[index]["flag_volatile"] = conversation->volatile_only();
        }

        auto& message = *it;
        notify[index]["timestamp"] = duration_cast<milliseconds>(message->message_timestamp.time_since_epoch()).count();
        notify[index]["sender_database_id"] = message->sender_database_id;
        notify[index]["sender_unique_id"] = message->sender_unique_id;
        notify[index]["sender_name"] = message->sender_name;

        notify[index]["msg"] = message->message;
        length += message->message.size();
        length += message->sender_name.size();
        length += message->sender_unique_id.size();
        if(length > 1024 * 8 || !merge) {
            index = 0;
            this->sendCommand(notify);
            notify = Command{this->notify_response_command("notifyconversationhistory")};
        } else
            index++;
    }
    if(index > 0)
        this->sendCommand(notify);

    if(more_data)
        return command_result{error::conversation_more_data};

    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandConversationFetch(ts::Command &cmd) {
    CMD_REF_SERVER(ref_server);
    CMD_CHK_AND_INC_FLOOD_POINTS(25);


    Command result(this->notify_response_command("notifyconversationindex"));
    size_t result_index = 0;

    auto conversation_manager = ref_server->conversation_manager();
    for(size_t index = 0; index < cmd.bulkCount(); index++) {
        auto& bulk = cmd[index];

        if(!bulk.has("cid") || !bulk["cid"].castable<ChannelId>())
            continue;
        auto conversation_id = bulk["cid"].as<ChannelId>();

        auto& result_bulk = result[result_index++];
        result_bulk["cid"] = conversation_id;

        /* test if we have access to the conversation */
        if(conversation_id > 0) {
            /* test if we're able to see the channel */
            {
                shared_lock channel_view_lock(this->channel_tree_mutex);
                auto channel = this->channel_view()->find_channel(conversation_id);
                if(!channel) {
                    auto error = findError("conversation_invalid_id");
                    result_bulk["error_id"] = error.errorId;
                    result_bulk["error_msg"] = error.message;
                    continue;
                }

                auto conversation_mode = channel->channel()->properties()[property::CHANNEL_CONVERSATION_MODE].as_unchecked<ChannelConversationMode>();
                switch (conversation_mode) {
                    case ChannelConversationMode::CHANNELCONVERSATIONMODE_PRIVATE: {
                        auto error = findError("conversation_is_private");
                        result_bulk["error_id"] = error.errorId;
                        result_bulk["error_msg"] = error.message;
                        continue;
                    }

                    case ChannelConversationMode::CHANNELCONVERSATIONMODE_NONE: {
                        auto error = findError("conversation_not_exists");
                        result_bulk["error_id"] = error.errorId;
                        result_bulk["error_msg"] = error.message;
                        continue;
                    }

                    case ChannelConversationMode::CHANNELCONVERSATIONMODE_PUBLIC:
                        break;
                }
            }

            /* test if there is a channel password or join power which denies that we see the conversation */
            {
                shared_lock channel_view_lock(ref_server->channel_tree_mutex);
                auto channel = ref_server->getChannelTree()->findChannel(conversation_id);
                if(!channel) { /* should never happen! */
                    auto error = findError("conversation_invalid_id");
                    result_bulk["error_id"] = error.errorId;
                    result_bulk["error_msg"] = error.message;
                    continue;
                }

                if (!channel->verify_password( bulk.has("cpw") ? std::make_optional(bulk["cpw"].string()) : std::nullopt, this->getType() != ClientType::CLIENT_QUERY)) {
                    if(!permission::v2::permission_granted(1, this->calculate_permission(permission::b_channel_join_ignore_password, 1, channel->channelId()))) {
                        auto error = findError("channel_invalid_password");
                        result_bulk["error_id"] = error.errorId;
                        result_bulk["error_msg"] = error.message;
                        continue;
                    }
                }

                if(auto error_perm = this->calculate_and_get_join_state(channel); error_perm) {
                    auto error = findError("server_insufficeient_permissions");
                    result_bulk["error_id"] = error.errorId;
                    result_bulk["error_msg"] = error.message;
                    result_bulk["failed_permid"] = (int) error_perm;
                    continue;
                }
            }
        }

        auto conversation = conversation_manager->get(conversation_id);
        if(conversation) {
            result_bulk["timestamp"] = duration_cast<milliseconds>(conversation->last_message().time_since_epoch()).count();
            result_bulk["flag_volatile"] = conversation->volatile_only();
        } else {
            result_bulk["timestamp"] = 0;
            result_bulk["flag_volatile"] = false;
        }
    }
    if(result_index == 0)
        return command_result{error::database_empty_result};
    this->sendCommand(result);


    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandConversationMessageDelete(ts::Command &cmd) {
    CMD_REF_SERVER(ref_server);
    CMD_CHK_AND_INC_FLOOD_POINTS(25);

    auto conversation_manager = ref_server->conversation_manager();
    std::shared_ptr<conversation::Conversation> current_conversation;
    ChannelId current_conversation_id = 0;

    for(size_t index = 0; index < cmd.bulkCount(); index++) {
        auto &bulk = cmd[index];

        if(!bulk.has("cid") || !bulk["cid"].castable<ChannelId>())
            continue;

        /* test if we have access to the conversation */
        if(current_conversation_id != bulk["cid"].as<ChannelId>()) {
            current_conversation_id = bulk["cid"].as<ChannelId>();

            /* test if we're able to see the channel */
            {
                shared_lock channel_view_lock(this->channel_tree_mutex);
                auto channel = this->channel_view()->find_channel(current_conversation_id);
                if(!channel)
                    return command_result{error::conversation_invalid_id};
            }

            /* test if there is a channel password or join power which denies that we see the conversation */
            {
                shared_lock channel_view_lock(ref_server->channel_tree_mutex);
                auto channel = ref_server->getChannelTree()->findChannel(current_conversation_id);
                if(!channel)
                    return command_result{error::conversation_invalid_id};

                if (!channel->verify_password( bulk.has("cpw") ? std::make_optional(bulk["cpw"].string()) : std::nullopt, this->getType() != ClientType::CLIENT_QUERY)) {
                    ACTION_REQUIRES_PERMISSION(permission::b_channel_join_ignore_password, 1, channel->channelId());
                }

                if (!permission::v2::permission_granted(1, this->calculate_permission(permission::b_channel_conversation_message_delete, channel->channelId())))
                    return command_result{permission::b_channel_conversation_message_delete};

                if(auto error_perm = this->calculate_and_get_join_state(channel); error_perm != permission::ok && error_perm != permission::b_client_is_sticky)
                    return command_result{error_perm};
            }
        }

        current_conversation = conversation_manager->get(current_conversation_id);
        if(!current_conversation) continue;

        auto timestamp_begin = system_clock::time_point{} + milliseconds{bulk["timestamp_begin"]};
        auto timestamp_end = system_clock::time_point{} + milliseconds{bulk.has("timestamp_end") ? bulk["timestamp_end"].as<uint64_t>() : 0};
        auto limit = bulk.has("limit") ? bulk["limit"].as<uint64_t>() : 1;
        if(limit > 100)
            limit = 100;

        auto delete_count = current_conversation->delete_messages(timestamp_end, limit, timestamp_begin, bulk["cldbid"]);
        if(delete_count > 0) {
            for(const auto& client : ref_server->getClients()) {
                if(client->connectionState() != ConnectionState::CONNECTED)
                    continue;

                auto type = client->getType();
                if(type == ClientType::CLIENT_INTERNAL || type == ClientType::CLIENT_MUSIC)
                    continue;

                client->notifyConversationMessageDelete(current_conversation_id, timestamp_begin, timestamp_end, bulk["cldbid"], delete_count);
            }
        }
    }

    return command_result{error::ok};
}

enum struct FeatureSupportMode {
    NONE,
    FULL,
    EXPERIMENTAL,
    DEPRECATED
};

#define REGISTER_FEATURE(name, support, version)                \
    notify.put_unchecked(index, "name", name);                  \
    notify.put_unchecked(index, "support", (int) support);      \
    notify.put_unchecked(index, "version", version);            \
    index++

command_result ConnectedClient::handleCommandListFeatureSupport(ts::Command &cmd) {
    ts::command_builder notify{this->notify_response_command("notifyfeaturesupport")};
    int index{0};

    REGISTER_FEATURE("error-bulks", FeatureSupportMode::FULL, 1);
    REGISTER_FEATURE("advanced-channel-chat", FeatureSupportMode::FULL, 1);
    REGISTER_FEATURE("log-query", FeatureSupportMode::FULL, 1);
    REGISTER_FEATURE("whisper-echo", FeatureSupportMode::FULL, 1);
    REGISTER_FEATURE("video", FeatureSupportMode::EXPERIMENTAL, 1);
    REGISTER_FEATURE("sidebar-mode", FeatureSupportMode::FULL, 1);
    REGISTER_FEATURE("token", FeatureSupportMode::FULL, 1);

    this->sendCommand(notify);
    return command_result{error::ok};
}

















