#include <iostream>
#include <algorithm>
#include <bitset>
#include <memory>
#include <Definitions.h>
#include <misc/sassert.h>
#include <misc/memtracker.h>
#include <log/LogUtils.h>
#include <ThreadPool/Timer.h>

#include "src/VirtualServer.h"
#include "voice/VoiceClient.h"
#include "../InstanceHandler.h"
#include "../PermissionCalculator.h"
#include "../groups/GroupManager.h"
#include <event.h>

using namespace std;
using namespace std::chrono;
using namespace ts;
using namespace ts::server;

extern ts::server::InstanceHandler* serverInstance;

ConnectedClient::ConnectedClient(sql::SqlManager* db, const std::shared_ptr<VirtualServer>&server) : DataClient(db, server) {
    memtrack::allocated<ConnectedClient>(this);
    memset(&this->remote_address, 0, sizeof(this->remote_address));

    connectionStatistics = make_shared<stats::ConnectionStatistics>(server ? server->getServerStatistics() : nullptr);
    channel_tree = make_shared<ClientChannelView>(this);
}

ConnectedClient::~ConnectedClient() {
    memtrack::freed<ConnectedClient>(this);
}

void ConnectedClient::initialize_weak_reference(const std::shared_ptr<ConnectedClient> &self) {
    assert(this == &*self);
    this->_this = self;

    auto weak_self = std::weak_ptr{self};
    this->task_update_needed_permissions = multi_shot_task{serverInstance->general_task_executor(), "update permissions for " + this->getLoggingPeerIp(), [weak_self]{
        auto self = weak_self.lock();
        if(self) {
            auto permissions_changed = self->update_client_needed_permissions();
            logTrace(self->getServerId(), "{} Updated client permissions. Permissions changed: {}", CLIENT_STR_LOG_PREFIX_(self), permissions_changed);
            if(permissions_changed) {
                self->sendNeededPermissions(true);
            }
        }
    }};

    this->task_update_channel_client_properties = multi_shot_task{serverInstance->general_task_executor(), "update channel properties for " + this->getLoggingPeerIp(), [weak_self]{
        auto self = weak_self.lock();
        if(self) {
            self->updateChannelClientProperties(true, true);
        }
    }};

    this->task_update_displayed_groups = multi_shot_task{serverInstance->general_task_executor(), "update displayed groups for " + this->getLoggingPeerIp(), [weak_self]{
        auto self = weak_self.lock();
        if(self) {
            bool changed{false};
            self->update_displayed_client_groups(changed, changed);
        }
    }};
}

bool ConnectedClient::loadDataForCurrentServer() {
    auto result = DataClient::loadDataForCurrentServer();
    if(!result) {
        return false;
    }

    serverInstance->databaseHelper()->updateClientIpAddress(this->getServerId(), this->getClientDatabaseId(), this->getLoggingPeerIp());
    return true;
}

std::shared_ptr<ConnectionInfoData> ConnectedClient::request_connection_info(const std::shared_ptr<ConnectedClient> &receiver, bool& send_temp) {
    auto& info = this->connection_info;

    lock_guard info_lock(info.lock);
    if(info.data){
        if(chrono::system_clock::now() - info.data_age < chrono::seconds(1))
            return info.data;

        if(chrono::system_clock::now() - info.data_age > chrono::seconds(5)) //Data timeout
            info.data = nullptr;
    }

    if(receiver) {
        info.receiver.erase(std::remove_if(info.receiver.begin(), info.receiver.end(), [&](const weak_ptr<ConnectedClient>& weak) {
            auto locked = weak.lock();
            if(locked == receiver) {
                send_temp = true;
                return true;
            }
            return !locked;
        }), info.receiver.end());
        info.receiver.push_back(receiver);
    }

    if(chrono::system_clock::now() - info.last_requested >= chrono::seconds(1)) {
        info.last_requested = chrono::system_clock::now();

        Command cmd("notifyconnectioninforequest");

        string invoker;
        for(const auto& weak_request : info.receiver) {
            auto request = weak_request.lock();
            if(!request) continue;
            invoker += (invoker.empty() ? "" : ",") + to_string(request->getClientId());
        }

        cmd["invokerids"] = invoker;
        this->sendCommand(cmd);
    }

    return info.data;
}

void ConnectedClient::updateChannelClientProperties(bool lock_channel_tree, bool notify_self) {
    /* The server and the current channel might change while executing this method! */
    auto server_ref = this->server;
    auto channel = this->currentChannel;

    auto permissions = this->calculate_permissions({
            permission::i_client_talk_power,
            permission::b_client_ignore_antiflood,
            permission::i_channel_view_power,
            permission::b_channel_ignore_view_power,
    }, channel ? channel->channelId() : 0);

    permission::v2::PermissionFlaggedValue
        permission_talk_power{0, false},
        permission_ignore_antiflood{0, false},
        permission_channel_view_power{0, false},
        permission_channel_ignore_view_power{0, false};

    for(const auto& perm : permissions) {
        if(perm.first == permission::i_client_talk_power) {
            permission_talk_power = perm.second;
        } else if(perm.first == permission::b_client_ignore_antiflood) {
            permission_ignore_antiflood = perm.second;
        } else if(perm.first == permission::i_channel_view_power) {
            permission_channel_view_power = perm.second;
        } else if(perm.first == permission::b_channel_ignore_view_power) {
            permission_channel_ignore_view_power = perm.second;
        } else {
            sassert(false);
        }
    }

    std::deque<property::ClientProperties> updated_client_properties;
    {
        auto old_talk_power = this->properties()[property::CLIENT_TALK_POWER].as_or<int64_t>(0);
        auto new_talk_power = permission_talk_power.has_value ? permission_talk_power.value : 0;

        debugMessage(this->getServerId(), "{} Recalculated talk power. New value: {} Old value: {}", CLIENT_STR_LOG_PREFIX, new_talk_power, old_talk_power);
        if(old_talk_power != new_talk_power) {
            this->properties()[property::CLIENT_TALK_POWER] = new_talk_power;
            updated_client_properties.emplace_back(property::CLIENT_TALK_POWER);

            auto retract_request = this->properties()[property::CLIENT_IS_TALKER].as_or<bool>(false);
            if(!retract_request && channel) {
                retract_request = channel->talk_power_granted(permission_talk_power);
            }

            if(retract_request) {
                if(this->properties()[property::CLIENT_IS_TALKER].update_value(0)) {
                    updated_client_properties.emplace_back(property::CLIENT_IS_TALKER);
                }

                if(this->properties()[property::CLIENT_TALK_REQUEST].update_value(0)) {
                    updated_client_properties.emplace_back(property::CLIENT_TALK_REQUEST);
                }

                if(this->properties()[property::CLIENT_TALK_REQUEST_MSG].update_value("")) {
                    updated_client_properties.emplace_back(property::CLIENT_TALK_REQUEST_MSG);
                }
            }
        }
    }

    {
        IconId current_icon_id = this->properties()[property::CLIENT_ICON_ID].as_or<IconId>(0);
        IconId new_icon_id{ 0};

        auto local_permissions = this->clientPermissions;
        if(local_permissions) {
            permission::v2::PermissionFlaggedValue value{0, false};
            auto permission_flags = local_permissions->permission_flags(permission::i_icon_id);
            if(permission_flags.channel_specific &&  this->currentChannel) {
                auto val = local_permissions->channel_permission(permission::i_icon_id, this->currentChannel->channelId());
                value = { val.values.value, val.flags.value_set };
            }

            if(!value.has_value) {
                value = local_permissions->permission_value_flagged(permission::i_icon_id);
            }

            if(value.has_value) {
                new_icon_id = value.value;
            }
        }


        if(this->properties()[property::CLIENT_ICON_ID].update_value(new_icon_id)) {
            logTrace(this->getServerId(), "{} Updating client icon from {} to {}", CLIENT_STR_LOG_PREFIX, current_icon_id, new_icon_id);
            updated_client_properties.emplace_back(property::CLIENT_ICON_ID);
        }
    }

    {
        auto local_permissions = this->clientPermissions;
        auto permission_speaker = local_permissions ?
                                  local_permissions->channel_permission(permission::b_client_is_priority_speaker, channel ? channel->channelId() : 0) :
                                  permission::v2::empty_channel_permission;

        auto speaker_granted = permission::v2::permission_granted(1, { permission_speaker.values.value, permission_speaker.flags.value_set });
        if(properties()[property::CLIENT_IS_PRIORITY_SPEAKER].update_value(speaker_granted)){
            updated_client_properties.emplace_back(property::CLIENT_IS_PRIORITY_SPEAKER);
        }
    }

    block_flood = !permission::v2::permission_granted(1, permission_ignore_antiflood);
    if(server_ref) {
        server_ref->notifyClientPropertyUpdates(this->ref(), updated_client_properties, notify_self);
    }

    this->updateTalkRights(permission_talk_power);
    if((this->channels_view_power != permission_channel_view_power || this->channels_ignore_view != permission_channel_ignore_view_power) && notify_self && channel && server_ref) {
        this->channels_view_power = permission_channel_view_power;
        this->channels_ignore_view = permission_channel_ignore_view_power;

        shared_lock server_channel_lock(server_ref->channel_tree_mutex, defer_lock);
        unique_lock client_channel_lock(this->channel_tree_mutex, defer_lock);

        if(lock_channel_tree) {
            /* first read lock server channel tree */
            server_channel_lock.lock();
            client_channel_lock.lock();
        }

        /* might have been changed since we locked the tree */
        if(channel) {
            deque<ChannelId> deleted;
            for(const auto& update_entry : this->channel_tree->update_channel_path(server_ref->channelTree->tree_head(), server_ref->channelTree->find_linked_entry(channel->channelId()))) {
                if(update_entry.first) {
                    this->notifyChannelShow(update_entry.second->channel(), update_entry.second->previous_channel);
                } else {
                    deleted.push_back(update_entry.second->channelId());
                }
            }
            if(!deleted.empty()) {
                this->notifyChannelHide(deleted, false); /* we've locked the tree before */
            }
        }
    }
}

void ConnectedClient::updateTalkRights(permission::v2::PermissionFlaggedValue talk_power) {
    bool flag = false;
    flag |= this->properties()[property::CLIENT_IS_TALKER].as_or<bool>(false);

    auto current_channel = this->currentChannel;
    if(!flag && current_channel) {
        flag = current_channel->talk_power_granted(talk_power);
    }
    this->allowedToTalk = flag;
}

void ConnectedClient::resetIdleTime() {
    this->idleTimestamp = std::chrono::system_clock::now();
}

void ConnectedClient::increaseFloodPoints(uint16_t num) {
    this->floodPoints += num;
}

bool ConnectedClient::shouldFloodBlock() {
    if(!this->server) return false;
    if(!this->block_flood) return false;
    return this->floodPoints >
            this->server->properties()[property::VIRTUALSERVER_ANTIFLOOD_POINTS_NEEDED_COMMAND_BLOCK].as_or<uint16_t>(150);
}

void ConnectedClient::subscribeChannel(const std::deque<std::shared_ptr<BasicChannel>>& targets, bool lock_channel, bool enforce) {
    std::deque<std::shared_ptr<BasicChannel>> subscribed_channels;

    auto ref_server = this->server;
    if(!ref_server) {
        return;
    }

    auto general_granted = enforce || permission::v2::permission_granted(1, this->calculate_permission(permission::b_channel_ignore_subscribe_power, 0));
    {
        std::shared_lock server_channel_lock{ref_server->channel_tree_mutex, defer_lock};
        std::unique_lock client_channel_lock{this->channel_tree_mutex, defer_lock};
        if(lock_channel) {
            server_channel_lock.lock();
            client_channel_lock.lock();
        }

        for (const auto& targetChannel : targets) {
            auto local_channel = this->channel_tree->find_channel(targetChannel);
            if(!local_channel) {
                /* The target channel isn't visible. */
                continue;
            }

            if(local_channel->subscribed) {
                /* We've already subscribed to that channel. */
                continue;
            }

            if(!general_granted && targetChannel != this->currentChannel) {
                auto required_subscribe_power = targetChannel->permissions()->permission_value_flagged(permission::i_channel_needed_subscribe_power);
                required_subscribe_power.clear_flag_on_zero();

                ClientPermissionCalculator permissionCalculator{this, targetChannel};
                if(!permissionCalculator.permission_granted(permission::i_channel_subscribe_power, required_subscribe_power)) {
                    if(!permissionCalculator.permission_granted(permission::b_channel_ignore_subscribe_power, 1)) {
                        /* The target client hasn't permissions to view the channel nor ignore the subscribe power */
                        continue;
                    }
                }
            }

            local_channel->subscribed = true;
            subscribed_channels.push_back(targetChannel);
        }

        std::deque<shared_ptr<ConnectedClient>> visible_clients{};
        for(const auto& target_channel : subscribed_channels) {
            /* getClientsByChannel() does not acquire the server channel tree mutex */
            auto channel_clients = ref_server->getClientsByChannel(target_channel);

            auto target_view_power = this->calculate_permission(permission::i_client_serverquery_view_power, target_channel->channelId());
            for(const auto& client : channel_clients) {
                if(client->getType() == ClientType::CLIENT_QUERY) {
                    if(!target_view_power.has_power()) {
                        continue;
                    }

                    if(!permission::v2::permission_granted(client->calculate_permission(permission::i_client_needed_serverquery_view_power, target_channel->channelId()), target_view_power)) {
                        continue;
                    }
                }

                visible_clients.push_back(client);
            }
        }

        if(!visible_clients.empty()) {
            this->notifyClientEnterView(visible_clients, ViewReasonSystem);
        }

        if (!subscribed_channels.empty()) {
            this->notifyChannelSubscribed(subscribed_channels);
        }
    }
}

void ConnectedClient::unsubscribeChannel(const std::deque<std::shared_ptr<BasicChannel>>& targets, bool lock_channel) {
    auto ref_server = this->server;
    if(!ref_server) {
        return;
    }

    std::deque<std::shared_ptr<BasicChannel> > unsubscribed_channels;
    {
        std::shared_lock server_channel_lock{ref_server->channel_tree_mutex, defer_lock};
        std::unique_lock client_channel_lock{this->channel_tree_mutex, defer_lock};
        if(lock_channel) {
            server_channel_lock.lock();
            client_channel_lock.lock();
        }

        for (const auto& channel : targets) {
            if(this->currentChannel == channel) {
                /* Do not unsubscribe from our own channel. */
                continue;
            }

            auto local_channel = this->channel_tree->find_channel(channel);
            if(!local_channel || !local_channel->subscribed) {
                continue;
            }

            local_channel->subscribed = false;

            /* getClientsByChannel() does not acquire the server channel tree mutex */
            auto clients = this->server->getClientsByChannel(channel);
            this->visibleClients.erase(std::remove_if(this->visibleClients.begin(), this->visibleClients.end(), [&, clients](const std::weak_ptr<ConnectedClient>& weak) {
                auto visible_client = weak.lock();
                if(!visible_client) {
                    return true;
                }

                return std::find(clients.begin(), clients.end(), visible_client) != clients.end();
            }), this->visibleClients.end());

            unsubscribed_channels.push_back(channel);
        }

        if (!unsubscribed_channels.empty()) {
            this->notifyChannelUnsubscribed(unsubscribed_channels);
        }
    }
}

bool ConnectedClient::isClientVisible(const std::shared_ptr<ts::server::ConnectedClient>& client, bool lock) {
    std::shared_lock tree_lock(this->channel_tree_mutex, std::defer_lock);
    if(lock) {
        tree_lock.lock();
    }

    for(const auto& entry : this->visibleClients) {
        if(entry.lock() == client) {
            return true;
        }
    }

    return false;
}

bool ConnectedClient::notifyClientLeftView(const std::deque<std::shared_ptr<ts::server::ConnectedClient>> &clients, const std::string &reason, bool lock, const ts::ViewReasonServerLeftT &_vrsl) {
    if(clients.empty())
        return true;

    if(lock) {
        /* TODO: add locking of server channel tree in read mode and client tree in write mode! */
        assert(false);
    }

    Command cmd("notifyclientleftview");
    cmd["reasonmsg"] = reason;
    cmd["reasonid"] = ViewReasonId::VREASON_SERVER_LEFT;
    cmd["ctid"] = 0;

    ChannelId current_channel_id = 0;

    size_t index = 0;
    auto it = clients.begin();
    while(it != clients.end()) {
        auto client = *it;
        assert(client->getClientId() > 0);
        assert(client->currentChannel || &*client == this);
        if(!client->currentChannel)
            continue;

        if(current_channel_id != (client->currentChannel ? client->currentChannel->channelId() : 0)) {
            if(current_channel_id != 0)
                break;
            else
                cmd[index]["cfid"] = (current_channel_id = client->currentChannel->channelId());
        }
        cmd[index]["clid"] = client->getClientId();
        it++;
        index++;
    }

    this->visibleClients.erase(std::remove_if(this->visibleClients.begin(), this->visibleClients.end(), [&](const weak_ptr<ConnectedClient>& weak) {
        auto c = weak.lock();
        if(!c) {
            logError(this->getServerId(), "{} Got \"dead\" client in visible client list! This can cause a remote client disconnect within the future!", CLIENT_STR_LOG_PREFIX);
            return true;
        }
        return std::find(clients.begin(), it, c) != it;
    }), this->visibleClients.end());

    this->sendCommand(cmd);
    if(it != clients.end())
        return this->notifyClientLeftView({it, clients.end()}, reason, false, _vrsl);
    return true;
}

bool ConnectedClient::notifyClientLeftView(
        const shared_ptr<ConnectedClient> &client,
        const std::shared_ptr<BasicChannel> &target_channel,
        ViewReasonId reason_id,
        const std::string& reason_message,
        std::shared_ptr<ConnectedClient> invoker,
        bool lock_channel_tree) {
    assert(!lock_channel_tree); /* not supported yet! */
    assert(client == this || (client && client->getClientId() != 0));
    assert(client->currentChannel || &*client == this);

    if(client != this) {
        if(reason_id == VREASON_SERVER_STOPPED || reason_id == VREASON_SERVER_SHUTDOWN) {
            debugMessage(this->getServerId(), "Replacing notify left reason " + to_string(reason_id) + " with " + to_string(VREASON_SERVER_LEFT));
            reason_id = VREASON_SERVER_LEFT;
        }
    }

    /*
    switch (reasonId) {
        case ViewReasonId::VREASON_MOVED:
        case ViewReasonId::VREASON_BAN:
        case ViewReasonId::VREASON_CHANNEL_KICK:
        case ViewReasonId::VREASON_SERVER_KICK:
        case ViewReasonId::VREASON_SERVER_SHUTDOWN:
        case ViewReasonId::VREASON_SERVER_STOPPED:
            if(reasonMessage.empty()) {
                logCritical(this->getServerId(), "{} ConnectedClient::notifyClientLeftView() => missing reason message for reason id {}", CLIENT_STR_LOG_PREFIX, reasonId);
                reasonMessage = "<undefined>";
            }
            break;
        default:
            break;
    }
     */

    switch (reason_id) {
        case ViewReasonId::VREASON_MOVED:
        case ViewReasonId::VREASON_BAN:
        case ViewReasonId::VREASON_CHANNEL_KICK:
        case ViewReasonId::VREASON_SERVER_KICK:
            if(!invoker) {
                logCritical(this->getServerId(), "{} ConnectedClient::notifyClientLeftView() => missing invoker for reason id {}", CLIENT_STR_LOG_PREFIX, reason_id);
                if(this->server)
                    invoker = this->server->serverRoot;
            }
            break;

        case ViewReasonId::VREASON_USER_ACTION:
        case ViewReasonId::VREASON_SYSTEM:
        case ViewReasonId::VREASON_TIMEOUT:
        case ViewReasonId::VREASON_SERVER_STOPPED:
        case ViewReasonId::VREASON_SERVER_LEFT:
        case ViewReasonId::VREASON_CHANNEL_UPDATED:
        case ViewReasonId::VREASON_EDITED:
        case ViewReasonId::VREASON_SERVER_SHUTDOWN:
        default:
            break;
    }

    Command cmd("notifyclientleftview");
    cmd["reasonmsg"] = reason_message;
    cmd["reasonid"] = reason_id;
    cmd["clid"] = client->getClientId();
    cmd["cfid"] = client->currentChannel ? client->currentChannel->channelId() : 0;
    cmd["ctid"] = target_channel ? target_channel->channelId() : 0;

    if (invoker) {
        cmd["invokerid"] = invoker->getClientId();
        cmd["invokername"] = invoker->getDisplayName();
        cmd["invokeruid"] = invoker->getUid();
    }


    /* TODO: Critical warn if the client hasn't been found? */
    this->visibleClients.erase(std::remove_if(this->visibleClients.begin(), this->visibleClients.end(), [&, client](const weak_ptr<ConnectedClient>& weak) {
        auto c = weak.lock();
        if(!c) {
            logError(this->getServerId(), "{} Got \"dead\" client in visible client list! This can cause a remote client disconnect within the future!", CLIENT_STR_LOG_PREFIX);
            return true;
        }
        return c == client;
    }), this->visibleClients.end());
    this->sendCommand(cmd);
    return true;
}

bool ConnectedClient::notifyClientLeftViewKicked(const std::shared_ptr<ConnectedClient> &client,
                                                 const std::shared_ptr<BasicChannel> &target_channel,
                                                 const std::string& message,
                                                 std::shared_ptr<ConnectedClient> invoker,
                                                 bool lock_channel_tree) {
    assert(!lock_channel_tree); /* not supported yet! */
    assert(client && client->getClientId() != 0);
    assert(client->currentChannel || &*client == this);

    /* TODO: Critical warn if the client hasn't been found? */
    this->visibleClients.erase(std::remove_if(this->visibleClients.begin(), this->visibleClients.end(), [&, client](const weak_ptr<ConnectedClient>& weak) {
        auto c = weak.lock();
        if(!c) {
            logError(this->getServerId(), "{} Got \"dead\" client in visible client list! This can cause a remote client disconnect within the future!", CLIENT_STR_LOG_PREFIX);
            return true;
        }
        return c == client;
    }), this->visibleClients.end());

    if(!invoker) {
        logCritical(this->getServerId(), "{} ConnectedClient::notifyClientLeftViewKicked() => missing invoker for reason id {}", CLIENT_STR_LOG_PREFIX, target_channel ? ViewReasonId::VREASON_CHANNEL_KICK : ViewReasonId::VREASON_SERVER_KICK);
        if(this->server)
            invoker = this->server->serverRoot;
    }

    Command cmd("notifyclientleftview");

    cmd["clid"] = client->getClientId();
    cmd["cfid"] = client->currentChannel ? client->currentChannel->channelId() : 0;
    cmd["ctid"] = target_channel ? target_channel->channelId() : 0;
    cmd["reasonid"] = (uint8_t) (target_channel ? ViewReasonId::VREASON_CHANNEL_KICK : ViewReasonId::VREASON_SERVER_KICK);
    cmd["reasonmsg"] = message;

    if (invoker) {
        cmd["invokerid"] = invoker->getClientId();
        cmd["invokername"] = invoker->getDisplayName();
        cmd["invokeruid"] = invoker->getUid();
    }

    this->sendCommand(cmd);
    return true;
}

bool ConnectedClient::notifyClientLeftViewBanned(
        const shared_ptr<ConnectedClient> &client,
        const std::string& message,
        std::shared_ptr<ConnectedClient> invoker,
        size_t length,
        bool lock_channel_tree) {

    assert(!lock_channel_tree); /* not supported yet! */
    assert(client && client->getClientId() != 0);
    assert(client->currentChannel || &*client == this);

    Command cmd("notifyclientleftview");

    cmd["clid"] = client->getClientId();
    cmd["cfid"] = client->currentChannel ? client->currentChannel->channelId() : 0;
    cmd["ctid"] = 0;
    cmd["reasonid"] = ViewReasonId::VREASON_BAN;
    cmd["reasonmsg"] = message;

    if (invoker) {
        cmd["invokerid"] = invoker->getClientId();
        cmd["invokername"] = invoker->getDisplayName();
        cmd["invokeruid"] = invoker->getUid();
    }

    if (length > 0) {
        cmd["bantime"] = length;
    }

    /* TODO: Critical warn if the client hasn't been found? */
    this->visibleClients.erase(std::remove_if(this->visibleClients.begin(), this->visibleClients.end(), [&, client](const weak_ptr<ConnectedClient>& weak) {
        auto c = weak.lock();
        if(!c) {
            logError(this->getServerId(), "{} Got \"dead\" client in visible client list! This can cause a remote client disconnect within the future!", CLIENT_STR_LOG_PREFIX);
            return true;
        }
        return c == client;
    }), this->visibleClients.end());

    this->sendCommand(cmd);
    return true;
}

bool ConnectedClient::sendNeededPermissions(bool enforce) {
    if(!enforce && this->state != ConnectionState::CONNECTED) return false;

    if(!enforce && chrono::system_clock::now() - this->lastNeededNotify < chrono::seconds(5) && this->lastNeededPermissionNotifyChannel == this->currentChannel) { //Dont spam these (hang up ui)
        this->requireNeededPermissionResend = true;
        return true;
    }
    this->lastNeededNotify = chrono::system_clock::now();
    this->lastNeededPermissionNotifyChannel = this->currentChannel;
    this->requireNeededPermissionResend = false;

    return this->notifyClientNeededPermissions();
}

bool ConnectedClient::notifyClientNeededPermissions() {
    Command cmd("notifyclientneededpermissions");
    int index = 0;

    unique_lock cache_lock(this->client_needed_permissions_lock);
    auto permissions = this->client_needed_permissions;
    cache_lock.unlock();

    for(const auto& [ key, value ] : permissions) {
        if(!value.has_value) {
            continue;
        }

        cmd[index]["permid"] = key;
        cmd[index++]["permvalue"] = value.value;
    }

    if(index == 0) {
        cmd[index]["permid"] = permission::i_client_talk_power;
        cmd[index++]["permvalue"] = 0;
    }

    this->sendCommand(cmd);
    return true;
}

bool ConnectedClient::notifyError(const command_result& result, const std::string& retCode) {
    ts::command_builder command{"error"};

    this->writeCommandResult(command, result);
    if(!retCode.empty())
        command.put_unchecked(0, "return_code", retCode);

    this->sendCommand(command);
    return true;
}

void ConnectedClient::writeCommandResult(ts::command_builder &cmd_builder, const command_result &result, const std::string& errorCodeKey) {
    result.build_error_response(cmd_builder, errorCodeKey);
}

inline std::shared_ptr<ViewEntry> pop_view_entry(std::deque<std::shared_ptr<ViewEntry>>& pool, ChannelId id) {
    for(auto it = pool.begin(); it != pool.end(); it++) {
        if((*it)->channelId() == id) {
            auto handle = *it;
            pool.erase(it);
            return handle;
        }
    }
    return nullptr;
}

using ChannelIT = std::deque<std::shared_ptr<ViewEntry>>::iterator;
inline void send_channels(ConnectedClient* client, ChannelIT begin, const ChannelIT& end, bool override_orderid){
    if(begin == end)
        return;

    ts::command_builder builder{"channellist", 512, 6};
    size_t index = 0;

    while(begin != end) {
        auto channel = (*begin)->channel();
        if(!channel) {
            begin++;
            continue;
        }

        for (const auto &elm : channel->properties()->list_properties(property::FLAG_CHANNEL_VIEW, client->getType() == CLIENT_TEAMSPEAK ? property::FLAG_NEW : (uint16_t) 0)) {
            if(elm.type() == property::CHANNEL_ORDER)
                builder.put_unchecked(index, elm.type().name, override_orderid ? 0 : (*begin)->previous_channel);
            else
                builder.put_unchecked(index, elm.type().name, elm.value());
        }

        begin++;
        if(++index > 3)
            break;
    }

    client->sendCommand(builder);
    if(begin != end)
        send_channels(client, begin, end, override_orderid);
}

void ConnectedClient::sendChannelList(bool lock_channel_tree) {
    shared_lock server_channel_lock(this->server->channel_tree_mutex, defer_lock);
    unique_lock client_channel_lock(this->channel_tree_mutex, defer_lock);
    if(lock_channel_tree) {
        server_channel_lock.lock();
        client_channel_lock.lock();
    }

    auto channels = this->channel_tree->insert_channels(this->server->channelTree->tree_head(), true, false);

    if(this->currentChannel) {
        bool send_success;
        for(const auto& channel : this->channel_tree->show_channel(this->server->channelTree->find_linked_entry(this->currentChannel->channelId()), send_success))
            channels.push_back(channel);
        if(!send_success)
            logCritical(this->getServerId(), "ConnectedClient::sendChannelList => failed to insert default channel!");
    }

    /*
    this->channels->print();
    auto channels_left = channels;
    for(const auto& channel : channels) {
        if(channel->previous_channel == 0) continue;

        bool erased = false;
        bool own = true;
        for(const auto& entry : channels_left) {
            if(entry->channelId() == channel->channelId())
                own = true;
            if(entry->channelId() == channel->previous_channel) {
                channels_left.erase(find(channels_left.begin(), channels_left.end(), entry));
                erased = true;
                break;
            }
        }
        if(!erased || !own) {
            logCritical(this->getServerId(), "Client {} would get an invalid channel order disconnect! Channel {} is not send before his channel! (Flags: erased := {} | own := {})", CLIENT_STR_LOG_PREFIX_(this), channel->previous_channel, erased, own);
        }
    }
     */


    std::deque<std::shared_ptr<ViewEntry>> entry_channels{pop_view_entry(channels, this->currentChannel->channelId())};
    while(entry_channels.front()) entry_channels.push_front(pop_view_entry(channels, entry_channels.front()->parentId()));
    entry_channels.pop_front();
    if(entry_channels.empty())
        logCritical(this->getServerId(), "ConnectedClient::sendChannelList => invalid (empty) own channel path!");

    send_channels(this, entry_channels.begin(), entry_channels.end(), false);
    this->notifyClientEnterView(this->ref(), nullptr, "", this->currentChannel, ViewReasonId::VREASON_SYSTEM, nullptr, false); //Notify self after path is send
    send_channels(this, channels.begin(), channels.end(), false);
    //this->notifyClientEnterView(_this.lock(), nullptr, "", this->currentChannel, ViewReasonId::VREASON_SYSTEM, nullptr, false); //Notify self after path is send
    this->sendCommand(Command("channellistfinished"));
}

void ConnectedClient::tick_server(const std::chrono::system_clock::time_point &time) {
    ALARM_TIMER(A1, "ConnectedClient::tick", milliseconds(2));
    if(this->state == ConnectionState::CONNECTED) {
        if(this->requireNeededPermissionResend)
            this->sendNeededPermissions(false);
        if(this->lastOnlineTimestamp.time_since_epoch().count() == 0) {
            this->lastOnlineTimestamp = time;
        } else if(time - this->lastOnlineTimestamp > seconds(120)) {
            this->properties()[property::CLIENT_MONTH_ONLINE_TIME].increment_by<uint64_t>(duration_cast<seconds>(time - lastOnlineTimestamp).count());
            this->properties()[property::CLIENT_TOTAL_ONLINE_TIME].increment_by<uint64_t>(duration_cast<seconds>(time - lastOnlineTimestamp).count());
            lastOnlineTimestamp = time;
        }
        if(time - this->lastTransfareTimestamp > seconds(5)) {
            lastTransfareTimestamp = time;
            auto update = this->connectionStatistics->mark_file_bytes();
            if(update.first > 0) {
                this->properties()[property::CLIENT_MONTH_BYTES_DOWNLOADED].increment_by<uint64_t>(update.first);
                this->properties()[property::CLIENT_TOTAL_BYTES_DOWNLOADED].increment_by<uint64_t>(update.first);
            }
            if(update.second > 0) {
                this->properties()[property::CLIENT_MONTH_BYTES_UPLOADED].increment_by<uint64_t>(update.second);
                this->properties()[property::CLIENT_TOTAL_BYTES_UPLOADED].increment_by<uint64_t>(update.second);
            }
        }
    }


    this->connectionStatistics->tick();
}

void ConnectedClient::sendServerInit() {
    Command command("initserver");

    for(const auto& prop : this->server->properties()->list_properties(property::FLAG_SERVER_VIEW, this->getType() == CLIENT_TEAMSPEAK ? property::FLAG_NEW : (uint16_t) 0)) {
        command[std::string{prop.type().name}] = prop.value();
    }
    command["virtualserver_maxclients"] = 32;

    //Server stuff
    command["client_talk_power"] = this->properties()[property::CLIENT_TALK_POWER].value();
    command["client_needed_serverquery_view_power"] = this->properties()[property::CLIENT_NEEDED_SERVERQUERY_VIEW_POWER].value();
    command["client_myteamspeak_id"] = this->properties()[property::CLIENT_MYTEAMSPEAK_ID].value();
    command["client_integrations"] = this->properties()[property::CLIENT_INTEGRATIONS].value();

    switch(ts::config::server::DefaultServerLicense) {
        case LicenseType::LICENSE_AUTOMATIC_INSTANCE:
            /* We offered this option but it's nonsense... Just use automatic server. */
        case LicenseType::LICENSE_AUTOMATIC_SERVER:
            if(this->server->properties()[property::VIRTUALSERVER_MAXCLIENTS].as_or<size_t>(0) <= 32) {
                command["lt"] = LicenseType::LICENSE_NONE;
            } else if(this->server->properties()[property::VIRTUALSERVER_MAXCLIENTS].as_or<size_t>(0) <= 512) {
                command["lt"] = LicenseType::LICENSE_NPL;
            } else {
                command["lt"] = LicenseType::LICENSE_HOSTING;
            }
            break;

        case LicenseType::LICENSE_NONE:
        case LicenseType::LICENSE_HOSTING:
        case LicenseType::LICENSE_OFFLINE:
        case LicenseType::LICENSE_NPL:
        case LicenseType::LICENSE_UNKNOWN:
        case LicenseType::LICENSE_PLACEHOLDER:
        default:
            command["lt"] = ts::config::server::DefaultServerLicense;
            break;
    }
    command["pv"] = 6; /* protocol version */
    command["acn"] = this->getDisplayName();
    command["aclid"] = this->getClientId();
    this->sendCommand(command);
}

bool ConnectedClient::handleCommandFull(Command& cmd, bool disconnectOnFail) {
    system_clock::time_point start, end;
    start = system_clock::now();
#ifdef PKT_LOG_CMD
    logTrace(this->getServerId() == 0 ? LOG_QUERY : this->getServerId(), "{}[Command][Client -> Server] Processing command: {}", CLIENT_STR_LOG_PREFIX, cmd.build(false));
#endif

    command_result result;
    try {
        result.reset(this->handleCommand(cmd));
    } catch(command_value_cast_failed& ex){
        auto message = ex.key() + " at " + std::to_string(ex.index()) + " could not be casted to " + ex.target_type().name();
        if(disconnectOnFail) {
            this->disconnect(message);
            return false;
        } else {
            result.reset(command_result{error::parameter_convert, message});
        }
    } catch(command_bulk_index_out_of_bounds_exception& ex){
        auto message = "missing bulk for index " + std::to_string(ex.index());
        if(disconnectOnFail) {
            this->disconnect(message);
            return false;
        } else {
            result.reset(command_result{error::parameter_invalid_count, message});
        }
    } catch(command_value_missing_exception& ex){
        auto message = "missing value for " + ex.key() + (ex.bulk_index() == std::string::npos ? "" : " at " + std::to_string(ex.bulk_index()));
        if(disconnectOnFail) {
            this->disconnect(message);
            return false;
        } else {
            result.reset(command_result{error::parameter_missing, message});
        }
    } catch(invalid_argument& ex){
        logWarning(this->getServerId(), "{}[Command] Failed to handle command. Received invalid argument exception: {}", CLIENT_STR_LOG_PREFIX, ex.what());
        if(disconnectOnFail) {
            this->disconnect("Invalid argument (" + string(ex.what()) + ")");
            return false;
        } else {
            result.reset(command_result{error::parameter_convert, ex.what()});
        }
    } catch (exception& ex) {
        logWarning(this->getServerId(), "{}[Command] Failed to handle command. Received exception with message: {}", CLIENT_STR_LOG_PREFIX, ex.what());
        if(disconnectOnFail) {
            this->disconnect("Error while command handling (" + string(ex.what()) + ")!");
            return false;
        } else {
            result.reset(command_result{error::vs_critical});
        }
    } catch (...) {
        this->disconnect("Error while command handling! (unknown)");
        return false;
    }

    bool generateReturnStatus = false;
    if(result.has_error() || this->getType() == ClientType::CLIENT_QUERY){
        generateReturnStatus = true;
    } else if(cmd["return_code"].size() > 0) {
        generateReturnStatus = !cmd["return_code"].string().empty();
    }

    if(generateReturnStatus)
        this->notifyError(result, cmd["return_code"].size() > 0 ? cmd["return_code"].first().as<std::string>() : "");

    if(result.has_error() && this->state == ConnectionState::INIT_HIGH) {
        this->close_connection(system_clock::now()); //Disconnect now
    }

    for (const auto& handler : postCommandHandler)
        handler();

    postCommandHandler.clear();
    end = system_clock::now();
    if(end - start > milliseconds(10)) {
        if(end - start > milliseconds(100))
            logError(this->getServerId(), "Command handling of command {} needs {}ms. This could be an issue!", cmd.command(), duration_cast<milliseconds>(end - start).count());
        else
            logWarning(this->getServerId(), "Command handling of command {} needs {}ms.", cmd.command(), duration_cast<milliseconds>(end - start).count());
    }
    result.release_data();
    return true;
}

std::shared_ptr<BanRecord> ConnectedClient::resolveActiveBan(const std::string& ip_address) {
    if(permission::v2::permission_granted(1, this->calculate_permission(permission::b_client_ignore_bans, 0))) {
        return nullptr;
    }

    //Check if manager banned
    auto banManager = serverInstance->banManager();
    shared_ptr<BanRecord> banEntry = nullptr;
    deque<shared_ptr<BanRecord>> entries;

    if (!banEntry) {
        banEntry = banManager->findBanByName(this->server->getServerId(), this->getDisplayName());
        if(banEntry)
            debugMessage(this->getServerId(), "{} Resolved name ban ({}). Record id {}, server id {}",
                    CLIENT_STR_LOG_PREFIX,
                    this->getDisplayName(),
                    banEntry->banId,
                    banEntry->serverId);
    }
    if (!banEntry) {
        banEntry = banManager->findBanByUid(this->server->getServerId(), this->getUid());
        if(banEntry)
            debugMessage(this->getServerId(), "{} Resolved uuid ban ({}). Record id {}, server id {}",
                    CLIENT_STR_LOG_PREFIX,
                    this->getUid(),
                    banEntry->banId,
                    banEntry->serverId);
    }
    if (!banEntry && !ip_address.empty()) {
        banEntry = banManager->findBanByIp(this->server->getServerId(), ip_address);
        if(banEntry)
            debugMessage(this->getServerId(), "{} Resolved ip ban ({}). Record id {}, server id {}",
                    CLIENT_STR_LOG_PREFIX,
                    ip_address,
                    banEntry->banId,
                    banEntry->serverId);
    }
    auto vclient = dynamic_cast<VoiceClient*>(this);
    if(vclient)
        if (!banEntry) {
            banEntry = banManager->findBanByHwid(this->server->getServerId(), vclient->getHardwareId());
            if(banEntry)
                debugMessage(this->getServerId(), "{} Resolved hwid ban ({}). Record id {}, server id {}",
                        CLIENT_STR_LOG_PREFIX,
                        vclient->getHardwareId(),
                        banEntry->banId,
                        banEntry->serverId);
        }

    return banEntry;
}

bool ConnectedClient::update_client_needed_permissions() {
    if(this->getType() == ClientType::CLIENT_QUERY) {
        /* Query clients are not interested in their permissions */
        return true;
    }

    /* The server and/or the channel might change while we're executing this method */
    auto currentChannel = this->currentChannel;

    ClientPermissionCalculator permission_helper{this, currentChannel};
    auto values = permission_helper.calculate_permissions(permission::neededPermissions);
    auto updated = false;

    {
        lock_guard cached_lock(this->client_needed_permissions_lock);

        auto old_cached_permissions{this->client_needed_permissions};
        this->client_needed_permissions = values;
        std::sort(this->client_needed_permissions.begin(), this->client_needed_permissions.end(), [](const auto& a, const auto& b) { return a.first < b.first; });

        if(this->client_needed_permissions.size() != old_cached_permissions.size())
            updated = true;
        else {
            for(auto oit = old_cached_permissions.begin(), nit = this->client_needed_permissions.begin(); oit != old_cached_permissions.end(); oit++, nit++) {
                if(oit->first != nit->first || oit->second != nit->second) {
                    updated = true;
                    break;
                }
            }
        }
    }

    this->cpmerission_whisper_power = {0, false};
    this->cpmerission_needed_whisper_power = {0, false};
    for(const auto& entry : values) {
        if(entry.first == permission::i_client_whisper_power)
            this->cpmerission_whisper_power = entry.second;
        else if(entry.first == permission::i_client_needed_whisper_power)
            this->cpmerission_needed_whisper_power = entry.second;
    }

    return updated;
}

void ConnectedClient::update_displayed_client_groups(bool& server_groups_changed, bool& channel_group_changed) {
    auto ref_server = this->server;
    auto group_manager = ref_server ? ref_server->group_manager() : serverInstance->group_manager();

    GroupId channel_group_id{0};
    ChannelId channel_inherit_id{0};
    std::string server_group_assignments{};

    {
        auto server_groups = group_manager->assignments().server_groups_of_client(groups::GroupAssignmentCalculateMode::GLOBAL, this->getClientDatabaseId());
        for(const auto& group_id : server_groups) {
            server_group_assignments += ",";
            server_group_assignments += std::to_string(group_id);
        }

        if(!server_group_assignments.empty()) {
            server_group_assignments = server_group_assignments.substr(1);
        } else {
            if(this->getType() == ClientType::CLIENT_QUERY) {
                if(auto default_group{serverInstance->guest_query_group()}; default_group) {
                    server_group_assignments = std::to_string(default_group->group_id());
                }
            } else {
                if(ref_server) {
                    if(auto default_group{ref_server->default_server_group()}; default_group) {
                        server_group_assignments = std::to_string(default_group->group_id());
                    }
                } else {
                    /* This should (in theory never happen). (But it maybe does with InternalClients idk) */
                }
            }
        }

        if(server_group_assignments.empty()) {
            server_group_assignments = "0";
        }

        std::unique_lock view_lock{this->channel_tree_mutex};
        this->cached_server_groups = server_groups;
    }

    {
        std::shared_ptr<BasicChannel> inherited_channel{this->currentChannel};

        auto channel_group = group_manager->assignments().calculate_channel_group_of_client(groups::GroupAssignmentCalculateMode::GLOBAL, this->getClientDatabaseId(), inherited_channel);
        if(channel_group.has_value()) {
            assert(inherited_channel);
            channel_group_id = *channel_group;
            channel_inherit_id = inherited_channel->channelId();
        } else if(ref_server) {
            channel_group_id = ref_server->properties()[property::VIRTUALSERVER_DEFAULT_CHANNEL_GROUP].as_or<GroupId>(0);
            channel_inherit_id = 0;
        } else {
            channel_group_id = 0;
            channel_inherit_id = 0;
        }

        this->cached_channel_group = channel_group_id;
    }

    server_groups_changed = false;
    channel_group_changed = false;

    std::deque<property::ClientProperties> updated_properties{};
    if(this->properties()[property::CLIENT_SERVERGROUPS].update_value(server_group_assignments)) {
        updated_properties.push_back(property::CLIENT_SERVERGROUPS);
        server_groups_changed = true;
    }

    if(this->properties()[property::CLIENT_CHANNEL_GROUP_ID].update_value(channel_group_id)) {
        updated_properties.push_back(property::CLIENT_CHANNEL_GROUP_ID);
        channel_group_changed = true;
    }

    if(this->properties()[property::CLIENT_CHANNEL_GROUP_INHERITED_CHANNEL_ID].update_value(channel_inherit_id)) {
        updated_properties.push_back(property::CLIENT_CHANNEL_GROUP_INHERITED_CHANNEL_ID);
        channel_group_changed = true;
    }

    if(!updated_properties.empty() && ref_server) {
        ref_server->notifyClientPropertyUpdates(this->ref(), updated_properties);
    }
}

void ConnectedClient::sendTSPermEditorWarning() {
    if(config::voice::warn_on_permission_editor) {
        if(system_clock::now() - this->command_times.servergrouplist > milliseconds(1000)) return;
        if(system_clock::now() - this->command_times.channelgrouplist > milliseconds(1000)) return;

        this->command_times.last_notify = system_clock::now();
        this->notifyClientPoke(this->server->serverRoot, config::messages::teamspeak_permission_editor);
    }
}

#define RESULT(perm_) \
do { \
    ventry->join_state_id = this->join_state_id; \
    ventry->join_permission_error = (perm_); \
    return perm_; \
} while(0)

permission::PermissionType ConnectedClient::calculate_and_get_join_state(const std::shared_ptr<BasicChannel>& channel) {
    std::shared_ptr<ViewEntry> ventry;
    {
        shared_lock view_lock(this->channel_tree_mutex);
        ventry = this->channel_view()->find_channel(channel);
        if(!ventry) {
            return permission::i_channel_view_power;
        }
    }
    if(ventry->join_state_id == this->join_state_id) {
        return ventry->join_permission_error;
    }

    ClientPermissionCalculator target_permissions{this, channel};

    switch(channel->channelType()) {
        case ChannelType::permanent:
            if(!target_permissions.permission_granted(permission::b_channel_join_permanent, 1)) {
                RESULT(permission::b_channel_join_permanent);
            }
            break;
        case ChannelType::semipermanent:
            if(!target_permissions.permission_granted(permission::b_channel_join_semi_permanent, 1)) {
                RESULT(permission::b_channel_join_semi_permanent);
            }
            break;
        case ChannelType::temporary:
            if(!target_permissions.permission_granted(permission::b_channel_join_temporary, 1)) {
                RESULT(permission::b_channel_join_temporary);
            }
            break;
    }

    auto required_join_power = channel->permissions()->permission_value_flagged(permission::i_channel_needed_join_power);
    required_join_power.clear_flag_on_zero();
    if(!target_permissions.permission_granted(permission::i_channel_join_power, required_join_power)) {
        if(!target_permissions.permission_granted(permission::b_channel_ignore_join_power, 1)) {
            RESULT(permission::i_channel_join_power);
        }
    }

    if(permission::v2::permission_granted(1, this->calculate_permission(permission::b_client_is_sticky, this->getChannelId()))) {
        if(!target_permissions.permission_granted(permission::b_client_ignore_sticky, 1)) {
            RESULT(permission::b_client_is_sticky);
        }
    }
    RESULT(permission::ok);
}

void ConnectedClient::useToken(token::TokenId token_id) {
    using groups::GroupCalculateMode;
    using groups::GroupAssignmentResult;

    auto server_ref = this->server;
    if(!server_ref) {
        return;
    }

    std::deque<token::TokenAction> actions{};
    if(!server_ref->getTokenManager().query_token_actions(token_id, actions)) {
        return;
    }

    if(actions.empty()) {
        return;
    }

    bool tree_registered = !!this->currentChannel;

    bool server_groups_changed{false}, channel_group_changed{false};
    std::deque<std::shared_ptr<groups::ServerGroup>> added_server_groups{};
    std::deque<std::shared_ptr<groups::ServerGroup>> removed_server_groups{};

    for(const auto& action : actions) {
        switch(action.type) {
            case token::ActionType::AddServerGroup:
            case token::ActionType::RemoveServerGroup: {
                auto group = server->group_manager()->server_groups()->find_group(GroupCalculateMode::GLOBAL, action.id1);
                if(!group) {
                    debugMessage(this->getServerId(), "{} Skipping token action add/remove server group for group {} because the group does not exists anymore.", CLIENT_STR_LOG_PREFIX, action.id1);
                    break;
                }

                if(action.type == token::ActionType::AddServerGroup) {
                    auto result = this->server->group_manager()->assignments().add_server_group(this->getClientDatabaseId(), group->group_id(), !group->is_permanent());
                    switch(result) {
                        case GroupAssignmentResult::SUCCESS:
                            debugMessage(this->getServerId(), "{} Executing token action add server group for group {}.", CLIENT_STR_LOG_PREFIX, action.id1);
                            added_server_groups.push_back(group);
                            server_groups_changed = true;
                            break;

                        case GroupAssignmentResult::ADD_ALREADY_MEMBER_OF_GROUP:
                            debugMessage(this->getServerId(), "{} Skipping token action add server group for group {} because client is already member of that group.", CLIENT_STR_LOG_PREFIX, action.id1);
                            break;

                        case GroupAssignmentResult::REMOVE_NOT_MEMBER_OF_GROUP:
                        case GroupAssignmentResult::SET_ALREADY_MEMBER_OF_GROUP:
                        default:
                            assert(false);
                            break;
                    }
                } else {
                    auto result = this->server->group_manager()->assignments().remove_server_group(this->getClientDatabaseId(), group->group_id());
                    switch(result) {
                        case GroupAssignmentResult::SUCCESS:
                            debugMessage(this->getServerId(), "{} Executing token action remove server group for group {}.", CLIENT_STR_LOG_PREFIX, action.id1);
                            removed_server_groups.push_back(group);
                            server_groups_changed = true;
                            break;

                        case GroupAssignmentResult::REMOVE_NOT_MEMBER_OF_GROUP:
                            debugMessage(this->getServerId(), "{} Skipping token action remove server group for group {} because client is not a member of that group.", CLIENT_STR_LOG_PREFIX, action.id1);
                            break;

                        case GroupAssignmentResult::ADD_ALREADY_MEMBER_OF_GROUP:
                        case GroupAssignmentResult::SET_ALREADY_MEMBER_OF_GROUP:
                        default:
                            assert(false);
                            break;
                    }
                }

                break;
            }

            case token::ActionType::SetChannelGroup: {
                auto group = server->group_manager()->channel_groups()->find_group(GroupCalculateMode::GLOBAL, action.id1);
                if(!group) {
                    debugMessage(this->getServerId(), "{} Skipping token action set channel group for group {} at channel {} because the group does not exists anymore.", CLIENT_STR_LOG_PREFIX, action.id1, action.id2);
                    break;
                }

                auto channel = this->server->channelTree->findChannel(action.id2);
                if (!channel) {
                    debugMessage(this->getServerId(), "{} Skipping token action set channel group for group {} at channel {} because the channel does not exists anymore.", CLIENT_STR_LOG_PREFIX, action.id1, action.id2);
                    break;
                }

                channel_group_changed = true;
                this->server->group_manager()->assignments().set_channel_group(this->getClientDatabaseId(), group->group_id(), channel->channelId(), !group->is_permanent());
                break;
            }

            case token::ActionType::AllowChannelJoin: {
                auto speaking_client = dynamic_cast<SpeakingClient*>(this);
                if(speaking_client) {
                    speaking_client->join_whitelisted_channel.emplace_back(action.id2, action.text);
                }
                break;
            }

            case token::ActionType::ActionSqlFailed:
            case token::ActionType::ActionDeleted:
            case token::ActionType::ActionIgnore:
            default:
                break;
        }
    }

    if(this->state > ConnectionState::INIT_HIGH) {
        this->task_update_channel_client_properties.enqueue();
        this->task_update_needed_permissions.enqueue();
    }

    if(tree_registered && (server_groups_changed || channel_group_changed)) {
        this->task_update_displayed_groups.enqueue();
    }

    if(tree_registered && server_groups_changed) {
        for(const auto& group : added_server_groups) {
            std::optional<ts::command_builder> notify{};
            for(const auto &viewer : this->server->getClients()) {
                viewer->notifyServerGroupClientAdd(notify, this->server->serverRoot, this->ref(), group->group_id());
            }
        }
        for(const auto& group : added_server_groups) {
            std::optional<ts::command_builder> notify{};
            for(const auto &viewer : this->server->getClients()) {
                viewer->notifyServerGroupClientRemove(notify, this->server->serverRoot, this->ref(), group->group_id());
            }
        }
    }
}