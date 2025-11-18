#include <bitset>
#include <algorithm>
#include "ConnectedClient.h"
#include "voice/VoiceClient.h"
#include "../InstanceHandler.h"
#include "../server/QueryServer.h"
#include "../manager/PermissionNameMapper.h"
#include "music/MusicClient.h"
#include <misc/sassert.h>
#include <log/LogUtils.h>
#include "./web/WebClient.h"
#include "../groups/GroupManager.h"

using namespace std::chrono;
using namespace std;
using namespace ts;
using namespace ts::server;

# define INVOKER(command, invoker)                                           \
do {                                                                        \
    if(invoker) {                                                           \
        command["invokerid"] = invoker->getClientId();                      \
        command["invokername"] = invoker->getDisplayName();                 \
        command["invokeruid"] = invoker->getUid();                          \
    } else {                                                                \
        command["invokerid"] = 0;                                           \
        command["invokername"] = "undefined";                               \
        command["invokeruid"] = "undefined";                                \
    }                                                                       \
} while(0)

#define INVOKER_NEW(command, invoker)                                       \
do {                                                                        \
    if(invoker) {                                                           \
        command.put_unchecked(0, "invokerid", invoker->getClientId());      \
        command.put_unchecked(0, "invokername", invoker->getDisplayName()); \
        command.put_unchecked(0, "invokeruid", invoker->getUid());          \
    } else {                                                                \
        command.put_unchecked(0, "invokerid", "0");                         \
        command.put_unchecked(0, "invokername", "undefined");               \
        command.put_unchecked(0, "invokeruid", "undefined");                \
    }                                                                       \
} while(0)

template <typename T>
inline void build_group_notify(ts::command_builder& notify, bool is_channel_groups, const std::vector<std::shared_ptr<T>>& available_groups) {
    std::string group_id_key{};
    permission::PermissionType permission_modify{};
    permission::PermissionType permission_add{};
    permission::PermissionType permission_remove{};

    if(is_channel_groups) {
        group_id_key = "cgid";

        permission_modify = permission::i_channel_group_needed_modify_power;
        permission_add = permission::i_channel_group_needed_member_add_power;
        permission_remove = permission::i_channel_group_needed_member_remove_power;
    } else {
        group_id_key = "sgid";

        permission_modify = permission::i_server_group_needed_modify_power;
        permission_add = permission::i_server_group_needed_member_add_power;
        permission_remove = permission::i_server_group_needed_member_remove_power;
    }

    size_t index{0};
    for(const auto& group : available_groups) {
        auto bulk = notify.bulk(index++);
        bulk.put_unchecked(group_id_key, group->group_id());
        bulk.put_unchecked("type", (uint8_t) group->group_type());
        bulk.put_unchecked("name", group->display_name());
        bulk.put_unchecked("sortid", group->sort_id());
        bulk.put_unchecked("savedb", group->is_permanent());
        bulk.put_unchecked("namemode", (uint8_t) group->name_mode());
        bulk.put_unchecked("iconid", (int32_t) group->icon_id());

        auto modify_power = group->permissions()->permission_value_flagged(permission_modify);
        auto add_power = group->permissions()->permission_value_flagged(permission_add);
        auto remove_power = group->permissions()->permission_value_flagged(permission_remove);

        bulk.put_unchecked("n_modifyp", modify_power.has_value ? modify_power.value : 0);
        bulk.put_unchecked("n_member_addp", add_power.has_value ? add_power.value : 0);
        bulk.put_unchecked("n_member_removep", remove_power.has_value ? remove_power.value : 0);
    }
}

bool ConnectedClient::notifyServerGroupList(std::optional<ts::command_builder> &generated_notify, bool as_notify) {
    if(!generated_notify.has_value()) {
        auto server_ref = this->server;
        auto group_manager = server_ref ? server_ref->group_manager() : serverInstance->group_manager();
        auto available_groups = group_manager->server_groups()->available_groups(groups::GroupCalculateMode::GLOBAL);

        build_group_notify(generated_notify.emplace(as_notify ? "notifyservergrouplist" : ""), false, available_groups);
    }

    this->sendCommand(*generated_notify);
    return true;
}

bool ConnectedClient::notifyChannelGroupList(std::optional<ts::command_builder>& generated_notify, bool as_notify) {
    if(!generated_notify.has_value()) {
        auto server_ref = this->server;
        auto group_manager = server_ref ? server_ref->group_manager() : serverInstance->group_manager();
        auto available_groups = group_manager->channel_groups()->available_groups(groups::GroupCalculateMode::GLOBAL);

        build_group_notify(generated_notify.emplace(as_notify ? "notifychannelgrouplist" : ""), true, available_groups);
    }

    this->sendCommand(*generated_notify);
    return true;
}

bool ConnectedClient::notifyGroupPermList(const std::shared_ptr<groups::Group>& group, bool as_sid) {
    auto is_channel_group = !!dynamic_pointer_cast<groups::ChannelGroup>(group);
    ts::command_builder result{this->notify_response_command(is_channel_group ? "notifychannelgrouppermlist" : "notifyservergrouppermlist")};

    if (!is_channel_group) {
        result.put_unchecked(0, "sgid", group->group_id());
    } else {
        result.put_unchecked(0, "cgid", group->group_id());
    }

    int index = 0;

    auto permissions = group->permissions()->permissions();
    auto permission_mapper = serverInstance->getPermissionMapper();
    auto client_type = this->getType();

    result.reserve_bulks(permissions.size() * 2);
    for (const auto &permission_data : permissions) {
        auto& permission = get<1>(permission_data);
        if(!permission.flags.value_set)
            continue;

        if(as_sid) {
            result.put_unchecked(index, "permsid", permission_mapper->permission_name(client_type, get<0>(permission_data)));
        } else {
            result.put_unchecked(index, "permid", (uint16_t) get<0>(permission_data));
        }

        result.put_unchecked(index, "permvalue", permission.values.value);
        result.put_unchecked(index, "permnegated", permission.flags.negate);
        result.put_unchecked(index, "permskip", permission.flags.skip);
        index++;
    }
    for (const auto &permission_data : permissions) {
        auto& permission = get<1>(permission_data);
        if(!permission.flags.grant_set)
            continue;


        if(as_sid) {
            result.put_unchecked(index, "permsid", permission_mapper->permission_name_grant(client_type, get<0>(permission_data)));
        } else {
            result.put_unchecked(index, "permid", (uint16_t) (get<0>(permission_data) | PERM_ID_GRANT));
        }

        result.put_unchecked(index, "permvalue", permission.values.grant);
        result.put_unchecked(index, "permnegated", 0);
        result.put_unchecked(index, "permskip", 0);
        index++;
    }

    if (index == 0)
        return false;

    this->sendCommand(result); //Need hack
    return true;
}

bool ConnectedClient::notifyClientPermList(ClientDbId cldbid, const std::shared_ptr<permission::v2::PermissionManager>& mgr, bool perm_sids) {
    Command res(this->getExternalType() == CLIENT_TEAMSPEAK ? "notifyclientpermlist" : "");

    auto permissions = mgr->permissions();
    if(permissions.empty())
        return false;

    int index = 0;
    res[index]["cldbid"] = cldbid;

    auto permission_mapper = serverInstance->getPermissionMapper();
    auto client_type = this->getType();
    for (const auto &permission_data : permissions) {
        auto& permission = std::get<1>(permission_data);
        if(permission.flags.value_set) {
            if (perm_sids)
                res[index]["permsid"] = permission_mapper->permission_name(client_type, get<0>(permission_data));
            else
                res[index]["permid"] = get<0>(permission_data);
            res[index]["permvalue"] = permission.values.value;

            res[index]["permnegated"] = permission.flags.negate;
            res[index]["permskip"] = permission.flags.skip;
            index++;
        }


        if(permission.flags.grant_set) {
            if (perm_sids)
                res[index]["permsid"] = permission_mapper->permission_name_grant(client_type, get<0>(permission_data));
            else
                res[index]["permid"] = (get<0>(permission_data) | PERM_ID_GRANT);
            res[index]["permvalue"] = permission.values.grant;
            res[index]["permnegated"] = 0;
            res[index]["permskip"] = 0;
            index++;
        }
    }

    this->sendCommand(res);
    return true;
}

bool ConnectedClient::notifyTextMessage(ChatMessageMode mode, const shared_ptr<ConnectedClient> &invoker, uint64_t targetId, ChannelId channel_id, const std::chrono::system_clock::time_point& timestamp, const string &textMessage) {
    //notifytextmessage targetmode=1 msg=asdasd target=2 invokerid=1 invokername=WolverinDEV invokeruid=xxjnc14LmvTk+Lyrm8OOeo4tOqw=
    Command cmd("notifytextmessage");
    INVOKER(cmd, invoker);
    cmd["targetmode"] = mode;
    cmd["target"] = targetId;
    cmd["msg"] = textMessage;
    cmd["timestamp"] = floor<milliseconds>(timestamp.time_since_epoch()).count();
    if(this->getType() != ClientType::CLIENT_TEAMSPEAK)
        cmd["cid"] = channel_id;
    this->sendCommand(cmd);
    return true;
}

bool ConnectedClient::notifyServerGroupClientAdd(
        std::optional<ts::command_builder>& notify,
        const std::shared_ptr<ConnectedClient> &invoker,
        const std::shared_ptr<ConnectedClient> &target_client,
        const GroupId& group_id) {

    /* Deny any client moves 'till we've send the notify */
    std::shared_lock<std::shared_mutex> channel_tree_lock{};
    if(this->server) {
        channel_tree_lock = std::shared_lock{this->server->channel_tree_mutex};
    }

    if(!this->isClientVisible(target_client, true)) {
        return false;
    }

    if(!notify.has_value()) {
        notify.emplace("notifyservergroupclientadded");
        INVOKER_NEW((*notify), invoker);

        notify->put_unchecked(0, "sgid", group_id);
        notify->put_unchecked(0, "clid", target_client->getClientId());
        notify->put_unchecked(0, "name", target_client->getDisplayName());
        notify->put_unchecked(0, "cluid", target_client->getUid());
    }

    this->sendCommand(*notify);
    return true;
}

bool ConnectedClient::notifyServerGroupClientRemove(
        std::optional<ts::command_builder>& notify,
        const std::shared_ptr<ConnectedClient> &invoker,
        const std::shared_ptr<ConnectedClient> &target_client,
        const GroupId& group_id) {

    /* Deny any client moves 'till we've send the notify */
    std::shared_lock<std::shared_mutex> channel_tree_lock{};
    if(this->server) {
        channel_tree_lock = std::shared_lock{this->server->channel_tree_mutex};
    }

    if(!this->isClientVisible(target_client, true)) {
        return false;
    }

    if(!notify.has_value()) {
        notify.emplace("notifyservergroupclientdeleted");
        INVOKER_NEW((*notify), invoker);

        notify->put_unchecked(0, "sgid", group_id);
        notify->put_unchecked(0, "clid", target_client->getClientId());
        notify->put_unchecked(0, "name", target_client->getDisplayName());
        notify->put_unchecked(0, "cluid", target_client->getUid());
    }

    this->sendCommand(*notify);
    return true;
}

bool ConnectedClient::notifyClientChannelGroupChanged(std::optional<ts::command_builder> &notify,
                                                      const std::shared_ptr<ConnectedClient> &invoker,
                                                      const std::shared_ptr<ConnectedClient> &target_client,
                                                      const ChannelId &channel_id,
                                                      const ChannelId &inherited_channel_id,
                                                      const GroupId &group_id) {
    /* Deny any client moves 'till we've send the notify */
    std::shared_lock<std::shared_mutex> channel_tree_lock{};
    if(this->server) {
        channel_tree_lock = std::shared_lock{this->server->channel_tree_mutex};
    }

    /* No need to check if the channel is visible since if this is the case the client would not be visible as well. */
    if(!this->isClientVisible(target_client, true)) {
        return false;
    }

    if(!notify.has_value()) {
        notify.emplace("notifyclientchannelgroupchanged");
        INVOKER_NEW((*notify), invoker);

        notify->put_unchecked(0, "cgid", group_id);
        notify->put_unchecked(0, "clid", target_client->getClientId());
        notify->put_unchecked(0, "name", target_client->getDisplayName());
        notify->put_unchecked(0, "cid", channel_id);
        notify->put_unchecked(0, "cgi", inherited_channel_id == 0 ? channel_id : inherited_channel_id);
    }

    this->sendCommand(*notify);
    return true;
}

bool ConnectedClient::notifyConnectionInfo(const shared_ptr<ConnectedClient> &target, const shared_ptr<ConnectionInfoData> &info) {
    command_builder notify{"notifyconnectioninfo"};
    auto bulk = notify.bulk(0);
    bulk.put_unchecked("clid", target->getClientId());

    auto not_set = this->getType() == CLIENT_TEAMSPEAK ? 0 : -1;
    /* we deliver data to the web client as well, because its a bit dump :D */
    if(target->getClientId() != this->getClientId()) {
        auto file_stats = target->connectionStatistics->file_stats();

        /* default values which normally sets the client */
        bulk.put_unchecked(property::CONNECTION_BANDWIDTH_RECEIVED_LAST_MINUTE_CONTROL, not_set);
        bulk.put_unchecked(property::CONNECTION_BANDWIDTH_RECEIVED_LAST_MINUTE_KEEPALIVE, not_set);
        bulk.put_unchecked(property::CONNECTION_BANDWIDTH_RECEIVED_LAST_MINUTE_SPEECH, not_set);
        bulk.put_unchecked(property::CONNECTION_BANDWIDTH_RECEIVED_LAST_SECOND_CONTROL, not_set);
        bulk.put_unchecked(property::CONNECTION_BANDWIDTH_RECEIVED_LAST_SECOND_KEEPALIVE, not_set);
        bulk.put_unchecked(property::CONNECTION_BANDWIDTH_RECEIVED_LAST_SECOND_SPEECH, not_set);

        bulk.put_unchecked(property::CONNECTION_BANDWIDTH_SENT_LAST_MINUTE_CONTROL, not_set);
        bulk.put_unchecked(property::CONNECTION_BANDWIDTH_SENT_LAST_MINUTE_KEEPALIVE, not_set);
        bulk.put_unchecked(property::CONNECTION_BANDWIDTH_SENT_LAST_MINUTE_SPEECH, not_set);
        bulk.put_unchecked(property::CONNECTION_BANDWIDTH_SENT_LAST_SECOND_CONTROL, not_set);
        bulk.put_unchecked(property::CONNECTION_BANDWIDTH_SENT_LAST_SECOND_KEEPALIVE, not_set);
        bulk.put_unchecked(property::CONNECTION_BANDWIDTH_SENT_LAST_SECOND_SPEECH, not_set);

        /* its flipped here because the report is out of the clients view */
        bulk.put_unchecked(property::CONNECTION_BYTES_RECEIVED_CONTROL, not_set);
        bulk.put_unchecked(property::CONNECTION_BYTES_RECEIVED_KEEPALIVE, not_set);
        bulk.put_unchecked(property::CONNECTION_BYTES_RECEIVED_SPEECH, not_set);
        bulk.put_unchecked(property::CONNECTION_BYTES_SENT_CONTROL, not_set);
        bulk.put_unchecked(property::CONNECTION_BYTES_SENT_KEEPALIVE, not_set);
        bulk.put_unchecked(property::CONNECTION_BYTES_SENT_SPEECH, not_set);

        /* its flipped here because the report is out of the clients view */
        bulk.put_unchecked(property::CONNECTION_PACKETS_RECEIVED_CONTROL, not_set);
        bulk.put_unchecked(property::CONNECTION_PACKETS_RECEIVED_KEEPALIVE, not_set);
        bulk.put_unchecked(property::CONNECTION_PACKETS_RECEIVED_SPEECH, not_set);
        bulk.put_unchecked(property::CONNECTION_PACKETS_SENT_CONTROL, not_set);
        bulk.put_unchecked(property::CONNECTION_PACKETS_SENT_KEEPALIVE, not_set);
        bulk.put_unchecked(property::CONNECTION_PACKETS_SENT_SPEECH, not_set);

        bulk.put_unchecked(property::CONNECTION_SERVER2CLIENT_PACKETLOSS_CONTROL, not_set);
        bulk.put_unchecked(property::CONNECTION_SERVER2CLIENT_PACKETLOSS_KEEPALIVE, not_set);
        bulk.put_unchecked(property::CONNECTION_SERVER2CLIENT_PACKETLOSS_SPEECH, not_set);
        bulk.put_unchecked(property::CONNECTION_SERVER2CLIENT_PACKETLOSS_TOTAL, not_set);

        bulk.put_unchecked(property::CONNECTION_CLIENT2SERVER_PACKETLOSS_SPEECH, not_set);
        bulk.put_unchecked(property::CONNECTION_CLIENT2SERVER_PACKETLOSS_KEEPALIVE, not_set);
        bulk.put_unchecked(property::CONNECTION_CLIENT2SERVER_PACKETLOSS_CONTROL, not_set);
        bulk.put_unchecked(property::CONNECTION_CLIENT2SERVER_PACKETLOSS_TOTAL, not_set);

        bulk.put_unchecked(property::CONNECTION_PING, 0);
        bulk.put_unchecked(property::CONNECTION_PING_DEVIATION, 0);

        bulk.put_unchecked(property::CONNECTION_CONNECTED_TIME, 0);
        bulk.put_unchecked(property::CONNECTION_IDLE_TIME, 0);

        /* its flipped here because the report is out of the clients view */
        bulk.put_unchecked(property::CONNECTION_FILETRANSFER_BANDWIDTH_SENT, file_stats.bytes_received);
        bulk.put_unchecked(property::CONNECTION_FILETRANSFER_BANDWIDTH_RECEIVED, file_stats.bytes_sent);
    }

    if(info) {
        for(const auto& [key, value] : info->properties) {
            bulk.put(key, value);
        }
    } else {
        //Fill in what we can, else we trust the client
        if(target->getType() == ClientType::CLIENT_TEASPEAK || target->getType() == ClientType::CLIENT_TEAMSPEAK || target->getType() == ClientType::CLIENT_WEB) {
            auto& stats = target->connectionStatistics->total_stats();
            /* its flipped here because the report is out of the clients view */
            bulk.put(property::CONNECTION_BYTES_RECEIVED_CONTROL, stats.connection_bytes_received[stats::ConnectionStatistics::category::COMMAND]);
            bulk.put(property::CONNECTION_BYTES_RECEIVED_KEEPALIVE, stats.connection_bytes_received[stats::ConnectionStatistics::category::KEEP_ALIVE]);
            bulk.put(property::CONNECTION_BYTES_RECEIVED_SPEECH, stats.connection_bytes_received[stats::ConnectionStatistics::category::VOICE]);
            bulk.put(property::CONNECTION_BYTES_SENT_CONTROL, stats.connection_bytes_sent[stats::ConnectionStatistics::category::COMMAND]);
            bulk.put(property::CONNECTION_BYTES_SENT_KEEPALIVE, stats.connection_bytes_sent[stats::ConnectionStatistics::category::KEEP_ALIVE]);
            bulk.put(property::CONNECTION_BYTES_SENT_SPEECH, stats.connection_bytes_sent[stats::ConnectionStatistics::category::VOICE]);

            /* its flipped here because the report is out of the clients view */
            bulk.put(property::CONNECTION_PACKETS_RECEIVED_CONTROL, stats.connection_packets_received[stats::ConnectionStatistics::category::COMMAND]);
            bulk.put(property::CONNECTION_PACKETS_RECEIVED_KEEPALIVE, stats.connection_packets_received[stats::ConnectionStatistics::category::KEEP_ALIVE]);
            bulk.put(property::CONNECTION_PACKETS_RECEIVED_SPEECH, stats.connection_packets_received[stats::ConnectionStatistics::category::VOICE]);
            bulk.put(property::CONNECTION_PACKETS_SENT_CONTROL, stats.connection_packets_sent[stats::ConnectionStatistics::category::COMMAND]);
            bulk.put(property::CONNECTION_PACKETS_SENT_KEEPALIVE, stats.connection_packets_sent[stats::ConnectionStatistics::category::KEEP_ALIVE]);
            bulk.put(property::CONNECTION_PACKETS_SENT_SPEECH, stats.connection_packets_sent[stats::ConnectionStatistics::category::VOICE]);
        }
    }

    if(auto vc = dynamic_pointer_cast<VoiceClient>(target); vc) {
        bulk.put(property::CONNECTION_PING, floor<milliseconds>(vc->current_ping()).count());
        bulk.put(property::CONNECTION_PING_DEVIATION, vc->current_ping_deviation());
    }
#ifdef COMPILE_WEB_CLIENT
    else if(dynamic_pointer_cast<WebClient>(target)) {
        bulk.put(property::CONNECTION_PING, floor<milliseconds>(dynamic_pointer_cast<WebClient>(target)->client_ping()).count());
    }
#endif

    if(auto vc = dynamic_pointer_cast<VoiceClient>(target); vc){
        auto& calculator = vc->connection->packet_statistics();
        auto report = calculator.loss_report();
        bulk.put(property::CONNECTION_CLIENT2SERVER_PACKETLOSS_SPEECH, std::to_string(report.voice_loss()));
        bulk.put(property::CONNECTION_CLIENT2SERVER_PACKETLOSS_KEEPALIVE, std::to_string(report.keep_alive_loss()));
        bulk.put(property::CONNECTION_CLIENT2SERVER_PACKETLOSS_CONTROL, std::to_string(report.control_loss()));
        bulk.put(property::CONNECTION_CLIENT2SERVER_PACKETLOSS_TOTAL, std::to_string(report.total_loss()));
    }

    if(target->getClientId() == this->getClientId() || permission::v2::permission_granted(1, this->calculate_permission(permission::b_client_remoteaddress_view, this->getChannelId()))) {
        bulk.put(property::CONNECTION_CLIENT_IP, target->getLoggingPeerIp());
        bulk.put(property::CONNECTION_CLIENT_PORT, target->getPeerPort());
    }

    bulk.put(property::CONNECTION_CONNECTED_TIME, chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - target->connectTimestamp).count());
    bulk.put(property::CONNECTION_IDLE_TIME, chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - target->idleTimestamp).count());
    this->sendCommand(notify);
    return true;
}

bool ConnectedClient::notifyClientMoved(const shared_ptr<ConnectedClient> &client,
                                        const std::shared_ptr<BasicChannel> &target_channel,
                                        ViewReasonId reason,
                                        std::string msg,
                                        std::shared_ptr<ConnectedClient> invoker,
                                        bool lock_channel_tree) {
    assert(!lock_channel_tree);
    assert(client->getClientId() > 0);
    assert(client->currentChannel);
    assert(target_channel);
    sassert(mutex_shared_locked(this->channel_tree_mutex));
    sassert(mutex_shared_locked(client->channel_tree_mutex));
    assert(this->isClientVisible(client, false) || &*client == this);

    Command mv("notifyclientmoved");

    mv["clid"] = client->getClientId();
    mv["cfid"] = client->currentChannel->channelId();
    mv["ctid"] = target_channel->channelId();
    mv["reasonid"] = reason;
    if (invoker)
        INVOKER(mv, invoker);
    if (!msg.empty()) mv["reasonmsg"] = msg;
    else mv["reasonmsg"] = "";

    this->sendCommand(mv);
    return true;
}

bool ConnectedClient::notifyClientUpdated(const std::shared_ptr<ConnectedClient> &client, const deque<const property::PropertyDescription*> &props, bool lock) {
    std::shared_lock channel_lock(this->channel_tree_mutex, defer_lock);
    if(lock) {
        channel_lock.lock();
    }

    sassert(mutex_shared_locked(this->channel_tree_mutex));
    if(!this->isClientVisible(client, false) && client != this)
        return false;

    auto client_id = client->getClientId();
    if(client_id == 0) {
        logError(this->getServerId(), "{} Attempted to send a clientupdate for client id 0. Updated client: {}", CLIENT_STR_LOG_PREFIX, CLIENT_STR_LOG_PREFIX_(client));
        return false;
    }
    Command response("notifyclientupdated");
    response["clid"] = client_id;
    for (const auto &prop : props) {
        if(lastOnlineTimestamp.time_since_epoch().count() > 0 && (*prop == property::CLIENT_TOTAL_ONLINE_TIME || *prop == property::CLIENT_MONTH_ONLINE_TIME))
            response[prop->name] = client->properties()[prop].as_or<int64_t>(0) + duration_cast<seconds>(system_clock::now() - client->lastOnlineTimestamp).count();
        else
            response[prop->name] = client->properties()[prop].value();
    }

    this->sendCommand(response);
    return true;
}

bool ConnectedClient::notifyPluginCmd(std::string name, std::string msg, std::shared_ptr<ConnectedClient> sender) {
    Command notify("notifyplugincmd");
    notify["name"] = name;
    notify["data"] = msg;
    INVOKER(notify, sender);
    this->sendCommand(notify);
    return true;
}

bool ConnectedClient::notifyClientChatComposing(const shared_ptr<ConnectedClient> &client) {
    Command notify("notifyclientchatcomposing");
    notify["clid"] = client->getClientId();
    notify["cluid"] = client->getUid();
    this->sendCommand(notify);
    return true;
}

bool ConnectedClient::notifyClientChatClosed(const shared_ptr<ConnectedClient> &client) {
    Command notify("notifyclientchatclosed");
    notify["clid"] = client->getClientId();
    notify["cluid"] = client->getUid();
    this->sendCommand(notify);
    return true;
}

bool ConnectedClient::notifyChannelMoved(const std::shared_ptr<BasicChannel> &channel, ChannelId order, const std::shared_ptr<ConnectedClient> &invoker) {
    if(!channel || !this->channel_tree->channel_visible(channel)) return false;

    Command notify("notifychannelmoved");
    INVOKER(notify, invoker);
    notify["reasonid"] = ViewReasonId::VREASON_MOVED;
    notify["cid"] = channel->channelId();
    notify["cpid"] = channel->parent() ? channel->parent()->channelId() : 0;
    notify["order"] = order;
    this->sendCommand(notify);
    return true;
}

bool ConnectedClient::notifyChannelCreate(const std::shared_ptr<BasicChannel> &channel, ChannelId orderId, const std::shared_ptr<ConnectedClient> &invoker) {
    Command notify("notifychannelcreated");
    for (auto &prop : channel->properties()->list_properties(property::FLAG_CHANNEL_VARIABLE | property::FLAG_CHANNEL_VIEW, this->getType() == CLIENT_TEAMSPEAK ? property::FLAG_NEW : (uint16_t) 0)) {
        if(prop.type() == property::CHANNEL_ORDER)
            notify[prop.type().name] = orderId;
        else if(prop.type() == property::CHANNEL_DESCRIPTION)
            continue;
        else
            notify[prop.type().name] = prop.value();
    }
    INVOKER(notify, invoker);
    this->sendCommand(notify);
    return true;
}

bool ConnectedClient::notifyChannelHide(const std::deque<ChannelId> &channel_ids, bool lock_channel_tree) {
    if(channel_ids.empty()) return true;

    if(this->getType() == ClientType::CLIENT_TEAMSPEAK) { //Voice hasnt that event
        shared_lock server_channel_lock(this->server->channel_tree_mutex, defer_lock);
        unique_lock client_channel_lock(this->channel_tree_mutex, defer_lock);
        if(lock_channel_tree) {
            server_channel_lock.lock();
            client_channel_lock.lock();
        } else {
            assert(mutex_locked(this->channel_tree_mutex));
        }

        deque<shared_ptr<ConnectedClient>> clients_to_remove;
        {
            for(const auto& w_client : this->visibleClients) {
                auto client = w_client.lock();
                if(!client) continue;
                if(client->currentChannel) {
                    auto id = client->currentChannel->channelId();
                    for(const auto cid : channel_ids) {
                        if(cid == id) {
                            clients_to_remove.push_back(client);
                            break;
                        }
                    }
                }
            }
        }

        //TODO: May send a unsubscribe and remove the clients like that?
        this->notifyClientLeftView(clients_to_remove, "Channel gone out of view", false, ViewReasonServerLeft);
        return this->notifyChannelDeleted(channel_ids, this->server->serverRoot);
    } else {
        Command notify("notifychannelhide");
        int index = 0;
        for(const auto& channel : channel_ids)
            notify[index++]["cid"] = channel;
        this->sendCommand(notify);
    }
    return true;
}

bool ConnectedClient::notifyChannelShow(const std::shared_ptr<ts::BasicChannel> &channel, ts::ChannelId orderId) {
    if(!channel) {
        return false;
    }

    auto result = false;
    if(this->getType() == ClientType::CLIENT_TEAMSPEAK) {
        /* The TeamSpeak 3 client dosn't know about hidden channels */
        result = this->notifyChannelCreate(channel, orderId, this->server->serverRoot);
    } else {
        Command notify("notifychannelshow");
        for (auto &prop : channel->properties()->list_properties(property::FLAG_CHANNEL_VARIABLE | property::FLAG_CHANNEL_VIEW, (uint16_t) 0)) {
            if(prop.type() == property::CHANNEL_ORDER) {
                notify[prop.type().name] = orderId;
            } else if(prop.type() == property::CHANNEL_DESCRIPTION) {
                continue;
            } else {
                notify[prop.type().name] = prop.value();
            }
        }
        this->sendCommand(notify);
    }

    if(result && this->subscribeToAll) {
        this->subscribeChannel({channel}, false, true);
    }

    return true;
}

bool ConnectedClient::notifyChannelDescriptionChanged(std::shared_ptr<BasicChannel> channel) {
    if(!this->channel_tree->channel_visible(channel)) return false;
    Command notifyDesChanges("notifychanneldescriptionchanged");
    notifyDesChanges["cid"] = channel->channelId();
    this->sendCommand(notifyDesChanges);
    return true;
}

bool ConnectedClient::notifyChannelPasswordChanged(std::shared_ptr<BasicChannel> channel) {
    if(!this->channel_tree->channel_visible(channel)) return false;
    Command notifyDesChanges("notifychannelpasswordchanged");
    notifyDesChanges["cid"] = channel->channelId();
    this->sendCommand(notifyDesChanges);
    return true;
}

bool ConnectedClient::notifyClientEnterView(const std::shared_ptr<ConnectedClient> &client, const std::shared_ptr<ConnectedClient> &invoker, const std::string& reason, const std::shared_ptr<BasicChannel> &to, ViewReasonId reasonId, const std::shared_ptr<BasicChannel> &from, bool lock_channel_tree) {
    sassert(client && client->getClientId() > 0);
    sassert(to);
    sassert(!lock_channel_tree); /* we don't support locking */
    sassert(mutex_locked(this->channel_tree_mutex));
    sassert(!this->isClientVisible(client, false) || &*client == this);

    switch (reasonId) {
        case ViewReasonId::VREASON_MOVED:
        case ViewReasonId::VREASON_BAN:
        case ViewReasonId::VREASON_CHANNEL_KICK:
        case ViewReasonId::VREASON_SERVER_KICK:
            if(!invoker) {
                logCritical(this->getServerId(), "{} ConnectedClient::notifyClientEnterView() => missing invoker for reason id {}", CLIENT_STR_LOG_PREFIX, reasonId);
                if(this->server)
                    ;//invoker = this->server->serverRoot.get();
            }
            break;

        case ViewReasonId::VREASON_SYSTEM:
        case ViewReasonId::VREASON_TIMEOUT:
        case ViewReasonId::VREASON_SERVER_STOPPED:
        case ViewReasonId::VREASON_SERVER_LEFT:
        case ViewReasonId::VREASON_CHANNEL_UPDATED:
        case ViewReasonId::VREASON_EDITED:
        case ViewReasonId::VREASON_SERVER_SHUTDOWN:
        case ViewReasonId::VREASON_USER_ACTION:
        default:
            break;
    }

    ts::command_builder builder{"notifycliententerview", 1024, 1};

    builder.put_unchecked(0, "cfid", from ? from->channelId() : 0);
    builder.put_unchecked(0, "ctid", to ? to->channelId() : 0);
    builder.put_unchecked(0, "reasonid", reasonId);
    INVOKER_NEW(builder, invoker);
    switch (reasonId) {
        case ViewReasonId::VREASON_MOVED:
        case ViewReasonId::VREASON_BAN:
        case ViewReasonId::VREASON_CHANNEL_KICK:
        case ViewReasonId::VREASON_SERVER_KICK:
            builder.put_unchecked(0, "reasonmsg", reason);
            break;

        case ViewReasonId::VREASON_SYSTEM:
        case ViewReasonId::VREASON_TIMEOUT:
        case ViewReasonId::VREASON_SERVER_STOPPED:
        case ViewReasonId::VREASON_SERVER_LEFT:
        case ViewReasonId::VREASON_CHANNEL_UPDATED:
        case ViewReasonId::VREASON_EDITED:
        case ViewReasonId::VREASON_SERVER_SHUTDOWN:
        case ViewReasonId::VREASON_USER_ACTION:
        default:
            break;
    }

    for (const auto &elm : client->properties()->list_properties(property::FLAG_CLIENT_VIEW, this->getType() == CLIENT_TEAMSPEAK ? property::FLAG_NEW : (uint16_t) 0)) {
        builder.put_unchecked(0, elm.type().name, elm.value());
    }

    visibleClients.emplace_back(client);
    this->sendCommand(builder);

    return true;
}

bool ConnectedClient::notifyClientEnterView(const std::deque<std::shared_ptr<ConnectedClient>> &clients, const ts::ViewReasonSystemT &_vrs) {
    if(clients.empty()) {
        return true;
    }

    assert(mutex_locked(this->channel_tree_mutex));

    Command cmd("notifycliententerview");

    cmd["cfid"] = 0;
    cmd["reasonid"] = ViewReasonId::VREASON_SYSTEM;
    ChannelId current_channel = 0;

    size_t index = 0;
    auto it = clients.begin();
    while(it != clients.end()) {
        auto client = *(it++);

        if(this->isClientVisible(client, false)) {
            continue;
        }

        auto channel = client->getChannel();
        sassert(!channel || channel->channelId() != 0);
        if(!channel) /* hmm suspecious */
            continue;

        if(current_channel != channel->channelId()) {
            if(current_channel == 0)
                cmd[index]["ctid"] = (current_channel = channel->channelId());
            else {
                it--; /* we still have to send him */
                break;
            }
        }

        this->visibleClients.push_back(client);
        for (const auto &elm : client->properties()->list_properties(property::FLAG_CLIENT_VIEW, this->getType() == CLIENT_TEAMSPEAK ? property::FLAG_NEW : (uint16_t) 0)) {
            cmd[index][std::string{elm.type().name}] = elm.value();
        }

        index++;
        if(index > 16) /* max 16 clients per packet */
            break;
    }

    if(index > 0)
        this->sendCommand(cmd);

    if(it != clients.end())
        return this->notifyClientEnterView({it, clients.end()}, _vrs);
    return true;
}

bool ConnectedClient::notifyChannelEdited(
        const std::shared_ptr<BasicChannel> &channel,
        const std::vector<property::ChannelProperties> &properties,
        const std::shared_ptr<ConnectedClient> &invoker,
        bool) {
    auto v_channel = this->channel_tree->find_channel(channel->channelId());
    if(!v_channel) return false; //Not visible? Important do not remove!

    bool send_description_change{false};
    size_t property_count{0};

    Command notify("notifychanneledited");
    for(auto prop : properties) {
        const auto& prop_info = property::describe(prop);

        if(prop == property::CHANNEL_ORDER) {
            notify[prop_info.name] = v_channel->previous_channel;
            property_count++;
        } else if(prop == property::CHANNEL_DESCRIPTION) {
            send_description_change = true;
        } else {
            notify[prop_info.name] = channel->properties()[prop].value();
            property_count++;
        }
    }

    if(property_count > 0) {
        notify["cid"] = channel->channelId();
        notify["reasonid"] = ViewReasonId::VREASON_EDITED;

        INVOKER(notify, invoker);
        this->sendCommand(notify);
    }

    if(send_description_change) {
        Command notify_dchange{"notifychanneldescriptionchanged"};
        notify_dchange["cid"] = channel->channelId();
        this->sendCommand(notify_dchange);
    }

    return true;
}

bool ConnectedClient::notifyChannelDeleted(const deque<ChannelId>& channel_ids, const std::shared_ptr<ConnectedClient>& invoker) {
    if(channel_ids.empty())
        return true;

    Command notify("notifychanneldeleted");

    int index = 0;
    for (const auto& channel_id : channel_ids)
        notify[index++]["cid"] = channel_id;

    INVOKER(notify, invoker);
    notify["reasonid"] = ViewReasonId::VREASON_EDITED;

    this->sendCommand(notify);
    return true;
}

bool ConnectedClient::notifyServerUpdated(std::shared_ptr<ConnectedClient> invoker) {
    Command response("notifyserverupdated");

    for (const auto& elm : this->server->properties()->list_properties(property::FLAG_SERVER_VARIABLE, this->getType() == CLIENT_TEAMSPEAK ? property::FLAG_NEW : (uint16_t) 0)) {
        if(elm.type() == property::VIRTUALSERVER_MIN_WINPHONE_VERSION)
            continue;

        //if(elm->type() == property::VIRTUALSERVER_RESERVED_SLOTS)
        response[elm.type().name] = elm.value();
    }

    if(getType() == CLIENT_QUERY)
        INVOKER(response, invoker);
    this->sendCommand(response);
    return true;
}

bool ConnectedClient::notifyClientPoke(std::shared_ptr<ConnectedClient> invoker, std::string msg) {
    Command cmd("notifyclientpoke");
    INVOKER(cmd, invoker);
    cmd["msg"] = msg;
    this->sendCommand(cmd);
    return true;
}

bool ConnectedClient::notifyChannelSubscribed(const deque<shared_ptr<BasicChannel>> &channels) {
    Command notify("notifychannelsubscribed");
    int index = 0;
    for (const auto&  ch : channels) {
        notify[index]["es"] = this->server->getClientsByChannel(ch).empty() ? ch->empty_seconds() : 0;
        notify[index++]["cid"] = ch->channelId();
    }
    this->sendCommand(notify);
    return true;
}

bool ConnectedClient::notifyChannelUnsubscribed(const deque<shared_ptr<BasicChannel>> &channels) {
    Command notify("notifychannelunsubscribed");
    int index = 0;
    for (const auto&  ch : channels) {
        notify[index]["es"] = this->server->getClientsByChannel(ch).empty() ? ch->empty_seconds() : 0;
        notify[index++]["cid"] = ch->channelId();
    }
    this->sendCommand(notify);
    return true;
}

bool ConnectedClient::notifyMusicQueueAdd(const shared_ptr<MusicClient>& bot, const shared_ptr<ts::music::SongInfo>& entry, int index, const std::shared_ptr<ConnectedClient>& invoker) {
    Command notify("notifymusicqueueadd");
    notify["bot_id"] = bot->getClientDatabaseId();
    notify["song_id"] = entry->getSongId();
    notify["url"] = entry->getUrl();
    notify["index"] = index;
    INVOKER(notify, invoker);
    this->sendCommand(notify);
    return true;
}

bool ConnectedClient::notifyMusicQueueRemove(const std::shared_ptr<MusicClient> &bot, const std::deque<std::shared_ptr<music::SongInfo>> &entry, const std::shared_ptr<ConnectedClient>& invoker) {
    Command notify("notifymusicqueueremove");
    notify["bot_id"] = bot->getClientDatabaseId();
    int index = 0;
    for(const auto& song : entry)
        notify[index++]["song_id"] = song->getSongId();
    INVOKER(notify, invoker);
    this->sendCommand(notify);
    return true;
}

bool ConnectedClient::notifyMusicQueueOrderChange(const std::shared_ptr<MusicClient> &bot, const std::shared_ptr<ts::music::SongInfo> &entry, int order, const std::shared_ptr<ConnectedClient>& invoker) {
    Command notify("notifymusicqueueorderchange");
    notify["bot_id"] = bot->getClientDatabaseId();
    notify["song_id"] = entry->getSongId();
    notify["index"] = order;
    INVOKER(notify, invoker);
    this->sendCommand(notify);
    return true;
}

bool ConnectedClient::notifyMusicPlayerStatusUpdate(const std::shared_ptr<ts::server::MusicClient> &bot) {
    Command notify("notifymusicstatusupdate");
    notify["bot_id"] = bot->getClientDatabaseId();

    auto player = bot->current_player();
    if(player) {
        notify["player_buffered_index"] = player->bufferedUntil().count();
        notify["player_replay_index"] = player->currentIndex().count();
    } else {
        notify["player_buffered_index"] = 0;
        notify["player_replay_index"] = 0;
    }
    this->sendCommand(notify);
    return true;
}

extern void apply_song(Command& command, const std::shared_ptr<ts::music::SongInfo>& element, int index = 0);
bool ConnectedClient::notifyMusicPlayerSongChange(const std::shared_ptr<MusicClient> &bot, const shared_ptr<music::SongInfo> &newEntry) {
    Command notify("notifymusicplayersongchange");
    notify["bot_id"] = bot->getClientDatabaseId();

    if(newEntry) {
        apply_song(notify, newEntry);
    } else {
        notify["song_id"] = 0;
    }
    this->sendCommand(notify);
    return true;
}

bool ConnectedClient::notifyConversationMessageDelete(const ts::ChannelId channel_id, const std::chrono::system_clock::time_point& begin, const std::chrono::system_clock::time_point& end, ts::ClientDbId client_id, size_t size) {
    Command notify("notifyconversationmessagedelete");

    notify["cid"] = channel_id;
    notify["timestamp_begin"] = floor<milliseconds>(begin.time_since_epoch()).count();
    notify["timestamp_end"] = floor<milliseconds>(end.time_since_epoch()).count();
    notify["cldbid"] = client_id;
    notify["limit"] = size;

    this->sendCommand(notify);
    return true;
}