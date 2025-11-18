#include <cstring>
#include <protocol/buffers.h>
#include "./PermissionCalculator.h"
#include "client/voice/VoiceClient.h"
#include "client/InternalClient.h"
#include "VirtualServer.h"
#include <misc/timer.h>
#include <log/LogUtils.h>
#include <misc/sassert.h>
#include <src/manager/ActionLogger.h>
#include "InstanceHandler.h"
#include "./groups/GroupManager.h"

using namespace std;
using namespace ts::server;
using namespace ts::protocol;
using namespace ts::buffer;
using namespace ts::permission;
using namespace std::chrono;

bool VirtualServer::registerClient(shared_ptr<ConnectedClient> client) {
    sassert(client);

    {
        std::lock_guard clients_lock{this->clients_mutex};
        if(client->getClientId() > 0) {
            logCritical(this->getServerId(), "Client {} ({}|{}) has been already registered!", client->getDisplayName(), client->getClientId(), client->getUid());
            return false;
        }

        ClientId client_id{0};
        while(this->clients.count(client_id)) {
            client_id++;
        }

        this->clients.emplace(client_id, client);
        client->setClientId(client_id);
    }

    {
        std::lock_guard lock{this->client_nickname_lock};

        auto login_name = client->getDisplayName();
        if(client->getExternalType() == ClientType::CLIENT_TEAMSPEAK) {
            client->properties()[property::CLIENT_LOGIN_NAME] = login_name;
        }

        while(login_name.length() < 3) {
            login_name += ".";
        }

        std::shared_ptr<ConnectedClient> found_client{nullptr};
        auto registered_clients = this->getClients();

        auto client_name{login_name};
        size_t counter{0};

        {
            while(true) {
                for(auto& _client : registered_clients) {
                    if(_client->getDisplayName() == client_name && _client != client) {
                        goto increase_name;
                    }
                }
                goto nickname_valid;

                increase_name:
                client_name = login_name + std::to_string(++counter);
            }
        }

        nickname_valid:
        client->setDisplayName(client_name);
    }

    switch(client->getType()) {
        case ClientType::CLIENT_TEAMSPEAK:
        case ClientType::CLIENT_TEASPEAK:
        case ClientType::CLIENT_WEB:
            this->properties()[property::VIRTUALSERVER_CLIENT_CONNECTIONS].increment_by<uint64_t>(1); //increase manager connections
            this->properties()[property::VIRTUALSERVER_LAST_CLIENT_CONNECT] = duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
            break;

        case ClientType::CLIENT_QUERY:
            this->properties()[property::VIRTUALSERVER_LAST_QUERY_CONNECT] = duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
            this->properties()[property::VIRTUALSERVER_QUERY_CLIENT_CONNECTIONS].increment_by<uint64_t>(1); //increase manager connections
            break;

        case ClientType::CLIENT_MUSIC:
        case ClientType::CLIENT_INTERNAL:
            break;


        case ClientType::MAX:
        default:
            assert(false);
            break;
    }

    return true;
}

bool VirtualServer::unregisterClient(shared_ptr<ConnectedClient> client, std::string reason, std::unique_lock<std::shared_mutex>& chan_tree_lock) {
    /* FIXME: Reenable this for the web client as soon we've fixed the web client disconnect method */
    if(client->getType() == ClientType::CLIENT_TEAMSPEAK || client->getType() == ClientType::CLIENT_TEASPEAK/* || client->getType() == ClientType::CLIENT_WEB */) {
        sassert(client->state == ConnectionState::DISCONNECTED);
    }


    {
        if(!chan_tree_lock.owns_lock()) {
            chan_tree_lock.lock();
        }

        if(client->currentChannel) {
            //We dont have to make him invisible if he hasnt even a channel
            this->client_move(client, nullptr, nullptr, reason, ViewReasonId::VREASON_SERVER_LEFT, false, chan_tree_lock);
        }

        chan_tree_lock.unlock();
    }

    {
        std::lock_guard clients_lock{this->clients_mutex};
        auto client_id = client->getClientId();
        if(client_id == 0) {
            return false; /* not registered */
        }

        if(!this->clients.erase(client_id)) {
            client->setClientId(0);
            logError(this->getServerId(), "Tried to unregister a not registered client {}/{} ({})", client->getDisplayName(), client->getUid(), client_id);
            return false;
        }
        client->setClientId(0);
    }

    auto current_time_seconds = std::chrono::duration_cast<seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    switch(client->getType()) {
        case ClientType::CLIENT_TEAMSPEAK:
        case ClientType::CLIENT_TEASPEAK:
        case ClientType::CLIENT_WEB:
            this->properties()[property::VIRTUALSERVER_LAST_CLIENT_DISCONNECT] = current_time_seconds;
            break;

        case ClientType::CLIENT_QUERY:
            this->properties()[property::VIRTUALSERVER_LAST_QUERY_DISCONNECT] = current_time_seconds;
            break;


        case ClientType::CLIENT_MUSIC:
        case ClientType::CLIENT_INTERNAL:
        case ClientType::MAX:
        default:
            break;
    }

    serverInstance->databaseHelper()->saveClientPermissions(this->ref(), client->getClientDatabaseId(), client->clientPermissions);
    return true;
}

void VirtualServer::registerInternalClient(std::shared_ptr<ConnectedClient> client) {
    client->state = ConnectionState::CONNECTED;
    this->registerClient(client);
}

void VirtualServer::unregisterInternalClient(std::shared_ptr<ConnectedClient> client) {
    client->state = ConnectionState::DISCONNECTED;

    std::unique_lock tree_lock{this->channel_tree_mutex};
    this->unregisterClient(client, "internal disconnect", tree_lock);
}

bool VirtualServer::assignDefaultChannel(const shared_ptr<ConnectedClient>& client, bool join) {
    std::shared_ptr<BasicChannel> channel{};

    std::unique_lock server_channel_lock{this->channel_tree_mutex};
    auto requested_channel_path = client->properties()[property::CLIENT_DEFAULT_CHANNEL].value();
    if(!requested_channel_path.empty()) {
        if (requested_channel_path[0] == '/' && requested_channel_path.find_first_not_of("0123456789", 1) == std::string::npos) {
            ChannelId channel_id{0};
            try {
                channel_id = std::stoull(requested_channel_path.substr(1));
            } catch (std::exception&) {
                logTrace(this->getServerId(), "{} Failed to parse provided channel path as channel id.");
            }

            if(channel_id > 0) {
                channel = this->channelTree->findChannel(channel_id);
            }
        } else {
            channel = this->channelTree->findChannelByPath(requested_channel_path);
        }
    }

    if(channel) {
        /* Client proposes a target channel */
        auto& channel_whitelist = client->join_whitelisted_channel;
        auto whitelist_entry = std::find_if(channel_whitelist.begin(), channel_whitelist.end(), [&](const auto& entry) { return entry.first == channel->channelId(); });

        auto client_channel_password = client->properties()[property::CLIENT_DEFAULT_CHANNEL_PASSWORD].value();
        if(whitelist_entry != channel_whitelist.end()) {
            debugMessage(this->getServerId(), "{} Allowing client to join channel {} because the token he used explicitly allowed it.", client->getLoggingPrefix(), channel->channelId());

            if(whitelist_entry->second != "ignore") {
                if (!channel->verify_password(std::make_optional(client_channel_password), true)) {
                    if (!permission::v2::permission_granted(1, client->calculate_permission(permission::b_channel_join_ignore_password, channel->channelId()))) {
                        channel = nullptr;
                        goto skip_permissions;
                    }
                }
            }
            goto skip_permissions;
        }

        if(!channel->permission_granted(permission::i_channel_needed_join_power, client->calculate_permission(permission::i_channel_join_power, channel->channelId()), false)) {
            debugMessage(this->getServerId(), "{} Tried to join channel {} but hasn't enough join power.", client->getLoggingPrefix(), channel->channelId());
            channel = nullptr;
            goto skip_permissions;
        }

        if (!channel->verify_password(std::make_optional(client->properties()[property::CLIENT_DEFAULT_CHANNEL_PASSWORD].value()), true)) {
            if(!permission::v2::permission_granted(1, client->calculate_permission(permission::b_channel_join_ignore_password, channel->channelId()))) {
                debugMessage(this->getServerId(), "{} Tried to join channel {} but hasn't given the right channel password.", client->getLoggingPrefix(), channel->channelId());
                channel = nullptr;
                goto skip_permissions;
            }
        }

        skip_permissions:;
    }

    /* Clear these parameters. We don't need them any more after we initially payed attention. */
    client->properties()[property::CLIENT_DEFAULT_CHANNEL] = "";
    client->properties()[property::CLIENT_DEFAULT_CHANNEL_PASSWORD] = "";

    if(!channel) {
        /* Client did not propose a channel or the proposed channel got rejected */
        channel = this->channelTree->getDefaultChannel();
        if(!channel) {
            logCritical(this->getServerId(), "Channel tree is missing the default channel.");
            return false;
        }
    }

    debugMessage(this->getServerId(), "{} Using channel {} as default client channel.", client->getLoggingPrefix(), channel->channelId());
    if(join) {
        this->client_move(client, channel, nullptr, "", ViewReasonId::VREASON_USER_ACTION, false, server_channel_lock);
    } else {
        client->currentChannel = channel;
    }

    return true;
}

void VirtualServer::testBanStateChange(const std::shared_ptr<ConnectedClient>& invoker) {
    this->forEachClient([&](shared_ptr<ConnectedClient> client) {
        auto ban = client->resolveActiveBan(client->getPeerIp());
        if(ban) {
            logMessage(this->getServerId(), "Client {} was online, but had an ban whcih effect him has been registered. Disconnecting client.", CLIENT_STR_LOG_PREFIX_(client));
            auto entryTime = ban->until.time_since_epoch().count() > 0 ? (uint64_t) chrono::ceil<seconds>(ban->until - system_clock::now()).count() : 0UL;
            this->notify_client_ban(client, invoker, ban->reason, entryTime);
            client->close_connection(system_clock::now() + seconds(1));
        }
    });
}

void VirtualServer::notify_client_ban(const shared_ptr<ConnectedClient> &target, const std::shared_ptr<ts::server::ConnectedClient> &invoker, const std::string &reason, size_t time) {
    /* the target is not allowed to execute anything; Must before channel tree lock because the target may waits for us to finish the channel stuff */
    lock_guard command_lock(target->command_lock);
    unique_lock server_channel_lock(this->channel_tree_mutex); /* we're "moving" a client! */

    if(target->currentChannel) {
        for(const auto& client : this->getClients()) {
            if(!client || client == target)
                continue;

            unique_lock client_channel_lock(client->channel_tree_mutex);
            if(client->isClientVisible(target, false))
                client->notifyClientLeftViewBanned(target, reason, invoker, time, false);
        }

        auto s_channel = dynamic_pointer_cast<ServerChannel>(target->currentChannel);
        s_channel->unregister_client(target);
    }

    /* now disconnect the target itself */
    unique_lock client_channel_lock(target->channel_tree_mutex);
    target->notifyClientLeftViewBanned(target, reason, invoker, time, false);
    target->currentChannel = nullptr;
}

void VirtualServer::notify_client_kick(
        const std::shared_ptr<ts::server::ConnectedClient> &target,
        const std::shared_ptr<ts::server::ConnectedClient> &invoker,
        const std::string &reason,
        const std::shared_ptr<ts::BasicChannel> &target_channel) {

    if(target_channel) {
        /* use the move! */
        unique_lock server_channel_lock(this->channel_tree_mutex, defer_lock);
        this->client_move(target, target_channel, invoker, reason, ViewReasonId::VREASON_CHANNEL_KICK, true, server_channel_lock);
    } else {
        /* the target is not allowed to execute anything; Must before channel tree lock because the target may waits for us to finish the channel stuff */
        lock_guard command_lock(target->command_lock);
        unique_lock server_channel_lock(this->channel_tree_mutex); /* we're "moving" a client! */

        if(target->currentChannel) {
            for(const auto& client : this->getClients()) {
                if(!client || client == target)
                    continue;

                unique_lock client_channel_lock(client->channel_tree_mutex);
                if(client->isClientVisible(target, false))
                    client->notifyClientLeftViewKicked(target, nullptr, reason, invoker, false);
            }

            auto s_channel = dynamic_pointer_cast<ServerChannel>(target->currentChannel);
            s_channel->unregister_client(target);
            if(auto client{dynamic_pointer_cast<SpeakingClient>(target)}; client) {
                this->rtc_server().assign_channel(client->rtc_client_id, 0);
            }
        }

        /* now disconnect the target itself */
        unique_lock client_channel_lock(target->channel_tree_mutex);
        target->notifyClientLeftViewKicked(target, nullptr, reason, invoker, false);
        target->currentChannel = nullptr;
    }
}

/*
 * 1. flag channel as deleted (lock channel tree so no moves)
 * 2. Gather all clients within the channel (lock their execute lock)
 * 3. Unlock channel tree and lock client locks
 * 4. lock channel tree again and move the clients (No new clients should be joined because channel is flagged as deleted!)
 *
 * Note: channel cant be a ref because the channel itself gets deleted!
 */
void VirtualServer::delete_channel(shared_ptr<ts::ServerChannel> channel, const shared_ptr<ConnectedClient> &invoker, const std::string& kick_message, unique_lock<std::shared_mutex> &tree_lock, bool temp_delete) {
    if(!tree_lock.owns_lock()) {
        tree_lock.lock();
    }

    if(channel->deleted) {
        return;
    }

    deque<std::shared_ptr<ConnectedClient>> clients;
    {
        for(const auto& sub_channel : this->channelTree->channels(channel)) {
            auto s_channel = dynamic_pointer_cast<ServerChannel>(sub_channel);
            assert(s_channel);

            auto chan_clients = this->getClientsByChannel(sub_channel);
            clients.insert(clients.end(), chan_clients.begin(), chan_clients.end());
            s_channel->deleted = true;
        }
        auto chan_clients = this->getClientsByChannel(channel);
        clients.insert(clients.end(), chan_clients.begin(), chan_clients.end());
        channel->deleted = true;
    }
    auto default_channel = this->channelTree->getDefaultChannel();

    deque<unique_lock<threads::Mutex>> command_locks;
    for(const auto& client : clients) {
        command_locks.push_back(move(unique_lock(client->command_lock)));
    }

    for(const auto& client : clients) {
        this->client_move(client, default_channel, invoker, kick_message, ViewReasonId::VREASON_CHANNEL_KICK, true, tree_lock);
    }

    if(!tree_lock.owns_lock()) {
        /* This case should never happen. client_move should never unlock the tree lock! */
        tree_lock.lock();
    }
    command_locks.clear();

    auto deleted_channels = this->channelTree->delete_channel_root(channel);
    log::ChannelDeleteReason delete_reason{temp_delete ? log::ChannelDeleteReason::EMPTY : log::ChannelDeleteReason::USER_ACTION};
    for(const auto& deleted_channel : deleted_channels) {
        serverInstance->action_logger()->channel_logger.log_channel_delete(this->serverId, invoker, deleted_channel->channelId(), channel == deleted_channel ? delete_reason : log::ChannelDeleteReason::PARENT_DELETED);
    }

    this->forEachClient([&](const shared_ptr<ConnectedClient>& client) {
        unique_lock client_channel_lock(client->channel_tree_mutex);
        client->notifyChannelDeleted(client->channel_tree->delete_channel_root(channel), invoker);
    });

    {
        std::vector<ChannelId> deleted_channel_ids{};
        deleted_channel_ids.reserve(deleted_channels.size());
        for(const auto& deleted_channel : deleted_channels) {
            deleted_channel_ids.push_back(deleted_channel->channelId());
        }

        auto ref_self = this->ref();
        task_id task_id{};
        serverInstance->general_task_executor()->schedule(task_id, "database cleanup after channel delete", [ref_self, deleted_channel_ids]{
            for(const auto& deleted_channel_id : deleted_channel_ids) {
                ref_self->tokenManager->handle_channel_deleted(deleted_channel_id);
            }

            for(const auto& deleted_channel_id : deleted_channel_ids) {
                ref_self->group_manager()->assignments().handle_channel_deleted(deleted_channel_id);
            }
        });
    }
}

/*
 * This method had previously owned the clients command lock but that's not really needed.
 * Everything which is related to the server channel tree or the client channel tree should be locked with
 * the appropriate mutexes.
 */
void VirtualServer::client_move(
        const shared_ptr<ts::server::ConnectedClient> &target_client,
        shared_ptr<ts::BasicChannel> target_channel,
        const std::shared_ptr<ts::server::ConnectedClient> &invoker,
        const std::string &reason_message,
        ts::ViewReasonId reason_id,
        bool notify_client,
        std::unique_lock<std::shared_mutex> &server_channel_write_lock) {

    TIMING_START(timings);
    if(!server_channel_write_lock.owns_lock()) {
        server_channel_write_lock.lock();
    }

    TIMING_STEP(timings, "chan tree l");
    if(target_client->currentChannel == target_channel) {
        return;
    }

    /* first step: verify thew source and target channel */
    auto s_target_channel = dynamic_pointer_cast<ServerChannel>(target_channel);
    auto s_source_channel = dynamic_pointer_cast<ServerChannel>(target_client->currentChannel);
    assert(!target_client->currentChannel || s_source_channel != nullptr);

    std::deque<property::ClientProperties> updated_client_properties{};
    if(target_channel) {
        assert(s_target_channel);
        if(s_target_channel->deleted) {
            return;
        }
    }

    auto l_target_channel = s_target_channel ? this->channelTree->findLinkedChannel(s_target_channel->channelId()) : nullptr;
    auto l_source_channel = s_source_channel ? this->channelTree->findLinkedChannel(s_source_channel->channelId()) : nullptr;
    TIMING_STEP(timings, "channel res");

    /* second step: show the target channel to the client if its not shown and let him subscribe to the channel */
    if(target_channel && notify_client) {
        std::unique_lock client_channel_lock{target_client->channel_tree_mutex};

        bool success{false};
        /* TODO: Use a bunk here and not a notify for every single */
        for(const auto& channel : target_client->channel_tree->show_channel(l_target_channel, success)) {
            target_client->notifyChannelShow(channel->channel(), channel->previous_channel);
        }

        sassert(success);
        if(!success) {
            return;
        }

        target_client->subscribeChannel({ target_channel }, false, true);
    }
    TIMING_STEP(timings, "target show");

    if(s_source_channel) {
        s_source_channel->unregister_client(target_client);
    }

    if(target_channel) {
        ClientPermissionCalculator target_client_permissions{&*target_client, target_channel};
        auto needed_view_power = target_client_permissions.calculate_permission(permission::i_client_needed_serverquery_view_power);

        /* ct_... is for client channel tree */
        this->forEachClient([&](const std::shared_ptr<ConnectedClient>& client) {
            if (!notify_client && client == target_client) {
                return;
            }

            bool move_target_client_visible{true};
            if(target_client->getType() == ClientType::CLIENT_QUERY) {
                auto query_view_power = client->calculate_permission(permission::i_client_serverquery_view_power, target_channel->channelId());
                move_target_client_visible = permission::v2::permission_granted(needed_view_power, query_view_power);
            }

            std::unique_lock client_channel_lock{client->channel_tree_mutex};
            auto ct_target_channel = move_target_client_visible ? client->channel_tree->find_channel(target_channel) : nullptr;

            if(ct_target_channel) {
                auto ct_source_channel = client->channel_tree->find_channel(s_source_channel);
                if(ct_source_channel) {
                    /* Source and target channel are visible for the client. Just a "normal" move. */
                    if (ct_target_channel->subscribed || client == target_client) {
                        if (client == target_client || client->isClientVisible(target_client, false)) {
                            client->notifyClientMoved(target_client, s_target_channel, reason_id, reason_message, invoker, false);
                        } else {
                            client->notifyClientEnterView(target_client, invoker, reason_message, s_target_channel, reason_id, s_source_channel, false);
                        }
                    } else if(client->isClientVisible(target_client, false)){
                        /* Client has been moved into an unsubscribed channel */
                        client->notifyClientLeftView(target_client, s_target_channel, reason_id, reason_message.empty() ? string("view left") : reason_message, invoker, false);
                    }
                } else if(ct_target_channel->subscribed) {
                    /* Target client entered the view from an invisible channel */
                    client->notifyClientEnterView(target_client, invoker, reason_message, s_target_channel, ViewReasonId::VREASON_USER_ACTION, nullptr, false);
                }
            } else {
                if(client->isClientVisible(target_client, false)) {
                    /* Client has been moved out of view into an invisible channel */
                    if(reason_id == ViewReasonId::VREASON_USER_ACTION) {
                        client->notifyClientLeftView(target_client, nullptr, ViewReasonId::VREASON_SERVER_LEFT, reason_message.empty() ? "joined a hidden channel" : reason_message, invoker, false);
                    } else {
                        client->notifyClientLeftView(target_client, nullptr, ViewReasonId::VREASON_SERVER_LEFT, reason_message.empty() ? "moved to a hidden channel" : reason_message, invoker, false);
                    }
                }
            }
        });

        s_target_channel->register_client(target_client);
        if(auto client{dynamic_pointer_cast<SpeakingClient>(target_client)}; client) {
            this->rtc_server().assign_channel(client->rtc_client_id, s_target_channel->rtc_channel_id);
        }

        if(auto client{dynamic_pointer_cast<VoiceClient>(target_client)}; client) {
            /* Start normal broadcasting, what the client expects */
            this->rtc_server().start_broadcast_audio(client->rtc_client_id, 1);
            client->clear_video_unsupported_message_flag();
        }
    } else {
        /* client left the server */
        if(target_client->currentChannel) {
            for(const auto& client : this->getClients()) {
                if(!client || client == target_client)
                    continue;

                unique_lock client_channel_lock(client->channel_tree_mutex);
                if(client->isClientVisible(target_client, false)) {
                    client->notifyClientLeftView(target_client, nullptr, reason_id, reason_message, invoker, false);
                }
            }

            if(auto client{dynamic_pointer_cast<SpeakingClient>(target_client)}; client) {
                this->rtc_server().assign_channel(client->rtc_client_id, 0);
            }
        }
    }
    TIMING_STEP(timings, "notify view");
    target_client->currentChannel = target_channel;

    /* third step: update stuff for the client (remember: the client cant execute anything at the moment!) */
    unique_lock client_channel_lock{target_client->channel_tree_mutex};
    TIMING_STEP(timings, "lock own tr");

    if (s_source_channel) {
        s_source_channel->properties()[property::CHANNEL_LAST_LEFT] = std::chrono::duration_cast<chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        this->group_manager()->assignments().cleanup_temporary_channel_assignment(target_client->getClientDatabaseId(), s_source_channel->channelId());

        if(target_client->properties()[property::CLIENT_IS_TALKER].update_value("0")) {
            updated_client_properties.push_back(property::CLIENT_IS_TALKER);
        }

        if(target_client->properties()[property::CLIENT_TALK_REQUEST].update_value("0")) {
            updated_client_properties.push_back(property::CLIENT_TALK_REQUEST);
        }

        if(target_client->properties()[property::CLIENT_TALK_REQUEST_MSG].update_value("")) {
            updated_client_properties.push_back(property::CLIENT_TALK_REQUEST_MSG);
        }
        TIMING_STEP(timings, "src chan up");
    }

    if (s_target_channel) {
        target_client->task_update_needed_permissions.enqueue();
        target_client->task_update_displayed_groups.enqueue();
        TIMING_STEP(timings, "perm gr upd");

        if(s_source_channel) {
            deque<ChannelId> deleted;
            for(const auto& channel : target_client->channel_tree->test_channel(l_source_channel, l_target_channel)) {
                deleted.push_back(channel->channelId());
            }

            if(!deleted.empty()) {
                target_client->notifyChannelHide(deleted, false);
            }

            auto i_source_channel = s_source_channel->channelId();
            if(std::find(deleted.begin(), deleted.end(), i_source_channel) == deleted.end()) {
                auto source_channel_sub_power = target_client->calculate_permission(permission::i_channel_subscribe_power, i_source_channel);
                if(!s_source_channel->permission_granted(permission::i_channel_needed_subscribe_power, source_channel_sub_power, false)) {
                    auto source_channel_sub_power_ignore = target_client->calculate_permission(permission::b_channel_ignore_subscribe_power, i_source_channel);
                    if(!permission::v2::permission_granted(1, source_channel_sub_power_ignore)) {
                        logTrace(this->serverId, "Force unsubscribing of client {} for channel {}/{}. (Channel switch and no permissions)",
                                 CLIENT_STR_LOG_PREFIX_(target_client), s_source_channel->name(),
                                 i_source_channel
                        );
                        target_client->unsubscribeChannel({ s_source_channel }, false); //Unsubscribe last channel (hasn't permissions)
                    }
                }
            }
            TIMING_STEP(timings, "src hide ts");
        }
    }
    client_channel_lock.unlock();

    /* both methods lock if they require stuff */
    this->notifyClientPropertyUpdates(target_client, updated_client_properties, s_source_channel ? true : false);
    TIMING_STEP(timings, "notify cpro");
    if(s_target_channel) {
        target_client->updateChannelClientProperties(false, s_source_channel ? true : false);
        TIMING_STEP(timings, "notify_t_pr");
    }
    debugMessage(this->getServerId(), "{} Client move timings: {}", CLIENT_STR_LOG_PREFIX_(target_client), TIMING_FINISH(timings));
}