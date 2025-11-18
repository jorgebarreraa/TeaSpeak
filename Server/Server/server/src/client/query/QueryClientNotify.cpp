#include "Properties.h"
#include "query/Command.h"
#include <algorithm>
#include <src/server/QueryServer.h>
#include <src/VirtualServerManager.h>
#include <src/InstanceHandler.h>
#include "QueryClient.h"
#include <log/LogUtils.h>

using namespace std;
using namespace std::chrono;
using namespace ts;
using namespace ts::server;

extern ts::server::InstanceHandler* serverInstance;

bool QueryClient::eventActive(QueryEventGroup group, QueryEventSpecifier specifier) {
    return (this->eventMask[group] & (1U << specifier)) > 0;
}

void QueryClient::toggleEvent(QueryEventGroup group, QueryEventSpecifier specifier, bool flag) {
    if(flag)
        this->eventMask[group] |= 1U << specifier;
    else
        this->eventMask[group] &= ~(1U << specifier);
}

void QueryClient::resetEventMask() {
    memset(eventMask, 0, sizeof(eventMask) / sizeof(*eventMask) * sizeof(uint16_t));
}

bool QueryClient::notifyClientNeededPermissions() {
    return true; //Query gets no needed permissions
}

#define CHK_EVENT(group, specifier)                                                                 \
do {                                                                                                \
    if(!this->eventActive(QueryEventGroup::group, QueryEventSpecifier::specifier)) return false;    \
} while(0)

bool QueryClient::notifyServerUpdated(shared_ptr<ConnectedClient> ptr) {
    CHK_EVENT(QEVENTGROUP_SERVER, QEVENTSPECIFIER_SERVER_EDIT);
    return ConnectedClient::notifyServerUpdated(ptr);
}

bool QueryClient::notifyClientUpdated(const std::shared_ptr<ConnectedClient> &ptr, const std::deque<const property::PropertyDescription*> &deque, bool lock_channel_tree) {
    CHK_EVENT(QEVENTGROUP_CLIENT_MISC, QEVENTSPECIFIER_CLIENT_MISC_UPDATE);
    return ConnectedClient::notifyClientUpdated(ptr, deque, lock_channel_tree);
}

bool QueryClient::notifyClientPoke(std::shared_ptr<ConnectedClient> invoker, std::string msg) {
    CHK_EVENT(QEVENTGROUP_CLIENT_MISC, QEVENTSPECIFIER_CLIENT_MISC_POKE);
    return ConnectedClient::notifyClientPoke(invoker, msg);
}

bool QueryClient::notifyPluginCmd(std::string name, std::string msg, std::shared_ptr<ConnectedClient> sender) {
    CHK_EVENT(QEVENTGROUP_CLIENT_MISC, QEVENTSPECIFIER_CLIENT_MISC_COMMAND);
    return ConnectedClient::notifyPluginCmd(name, msg, sender);
}

bool QueryClient::notifyClientChatComposing(const shared_ptr<ConnectedClient> &ptr) {
    CHK_EVENT(QEVENTGROUP_CHAT, QEVENTSPECIFIER_CHAT_COMPOSING);
    return ConnectedClient::notifyClientChatComposing(ptr);
}

bool QueryClient::notifyClientChatClosed(const shared_ptr<ConnectedClient> &ptr) {
    CHK_EVENT(QEVENTGROUP_CHAT, QEVENTSPECIFIER_CHAT_CLOSED);
    return ConnectedClient::notifyClientChatClosed(ptr);
}

bool QueryClient::notifyTextMessage(ChatMessageMode mode, const shared_ptr<ConnectedClient> &sender, uint64_t targetId, ChannelId channel_id, const std::chrono::system_clock::time_point& tp, const string &textMessage) {
    if(mode == ChatMessageMode::TEXTMODE_PRIVATE) CHK_EVENT(QEVENTGROUP_CHAT, QEVENTSPECIFIER_CHAT_MESSAGE_PRIVATE);
    else if(mode == ChatMessageMode::TEXTMODE_CHANNEL) CHK_EVENT(QEVENTGROUP_CHAT, QEVENTSPECIFIER_CHAT_MESSAGE_CHANNEL);
    else if(mode == ChatMessageMode::TEXTMODE_SERVER) CHK_EVENT(QEVENTGROUP_CHAT, QEVENTSPECIFIER_CHAT_MESSAGE_SERVER);
    return ConnectedClient::notifyTextMessage(mode, sender, targetId, channel_id, tp, textMessage);
}


bool QueryClient::notifyServerGroupClientAdd(optional<ts::command_builder> &anOptional,
                                             const shared_ptr<ConnectedClient> &ptr,
                                             const shared_ptr<ConnectedClient> &sharedPtr, const GroupId &id) {
    CHK_EVENT(QEVENTGROUP_CLIENT_GROUPS, QEVENTSPECIFIER_CLIENT_GROUPS_ADD);
    return ConnectedClient::notifyServerGroupClientAdd(anOptional, ptr, sharedPtr, id);
}

bool QueryClient::notifyServerGroupClientRemove(optional<ts::command_builder> &anOptional,
                                                const shared_ptr<ConnectedClient> &ptr,
                                                const shared_ptr<ConnectedClient> &sharedPtr, const GroupId &id) {
    CHK_EVENT(QEVENTGROUP_CLIENT_GROUPS, QEVENTSPECIFIER_CLIENT_GROUPS_REMOVE);
    return ConnectedClient::notifyServerGroupClientRemove(anOptional, ptr, sharedPtr, id);
}

bool QueryClient::notifyClientChannelGroupChanged(optional<ts::command_builder> &anOptional,
                                                  const shared_ptr <ConnectedClient> &ptr,
                                                  const shared_ptr <ConnectedClient> &sharedPtr, const ChannelId &id,
                                                  const ChannelId &channelId, const GroupId &groupId) {
    CHK_EVENT(QEVENTGROUP_CLIENT_GROUPS, QEVENTSPECIFIER_CLIENT_GROUPS_CHANNEL_CHANGED);
    return ConnectedClient::notifyClientChannelGroupChanged(anOptional, ptr, sharedPtr, id, channelId, groupId);
}

bool QueryClient::notifyChannelMoved(const std::shared_ptr<BasicChannel> &channel, ChannelId order, const std::shared_ptr<ConnectedClient> &invoker) {
    CHK_EVENT(QEVENTGROUP_CHANNEL, QEVENTSPECIFIER_CHANNEL_MOVE);
    return ConnectedClient::notifyChannelMoved(channel, order, invoker);
}

bool QueryClient::notifyChannelCreate(const std::shared_ptr<BasicChannel> &channel, ChannelId orderId,
                                      const std::shared_ptr<ConnectedClient> &invoker) {
    CHK_EVENT(QEVENTGROUP_CHANNEL, QEVENTSPECIFIER_CHANNEL_CREATE);
    return ConnectedClient::notifyChannelCreate(channel, orderId, invoker);
}

bool QueryClient::notifyChannelDescriptionChanged(std::shared_ptr<BasicChannel> channel) {
    CHK_EVENT(QEVENTGROUP_CHANNEL, QEVENTSPECIFIER_CHANNEL_DESC_EDIT);
    return ConnectedClient::notifyChannelDescriptionChanged(channel);
}

bool QueryClient::notifyChannelPasswordChanged(std::shared_ptr<BasicChannel> channel) {
    CHK_EVENT(QEVENTGROUP_CHANNEL, QEVENTSPECIFIER_CHANNEL_PASSWORD_EDIT);
    return ConnectedClient::notifyChannelPasswordChanged(channel);
}

bool QueryClient::notifyChannelEdited(const std::shared_ptr<BasicChannel> &ptr, const std::vector<property::ChannelProperties> &vector, const std::shared_ptr<ConnectedClient> &sharedPtr, bool b) {
    CHK_EVENT(QEVENTGROUP_CHANNEL, QEVENTSPECIFIER_CHANNEL_EDIT);
    return ConnectedClient::notifyChannelEdited(ptr, vector, sharedPtr, b);
}

bool QueryClient::notifyChannelDeleted(const std::deque<ChannelId> &deque, const std::shared_ptr<ConnectedClient> &ptr) {
    CHK_EVENT(QEVENTGROUP_CHANNEL, QEVENTSPECIFIER_CHANNEL_DELETED);
    return ConnectedClient::notifyChannelDeleted(deque, ptr);
}

bool QueryClient::notifyClientEnterView(const std::deque<std::shared_ptr<ConnectedClient>> &deque, const ViewReasonSystemT &t) {
    if(!this->eventActive(QueryEventGroup::QEVENTGROUP_CLIENT_VIEW, QueryEventSpecifier::QEVENTSPECIFIER_CLIENT_VIEW_JOIN)) {
        assert(mutex_locked(this->channel_tree_mutex));
        this->visibleClients.insert(this->visibleClients.end(), deque.begin(), deque.end());
        return true;
    } else
        return ConnectedClient::notifyClientEnterView(deque, t);
}

bool QueryClient::notifyClientEnterView(const std::shared_ptr<ts::server::ConnectedClient> &client, const std::shared_ptr<ts::server::ConnectedClient> &invoker, const std::string &string, const std::shared_ptr<ts::BasicChannel> &to,
                                        ts::ViewReasonId reasonId, const std::shared_ptr<ts::BasicChannel> &from, bool lock) {
    if(!this->eventActive(QueryEventGroup::QEVENTGROUP_CLIENT_VIEW, QueryEventSpecifier::QEVENTSPECIFIER_CLIENT_VIEW_JOIN)) {
        assert(!lock);
        this->visibleClients.push_back(client);
        return true;
    }
    return ConnectedClient::notifyClientEnterView(client, invoker, string, to, reasonId, from, lock);
}

bool QueryClient::notifyClientMoved(const std::shared_ptr<ConnectedClient> &client, const std::shared_ptr<BasicChannel> &target_channel, ViewReasonId reason, std::string msg, std::shared_ptr<ConnectedClient> invoker, bool lock_channel_tree) {
    return ConnectedClient::notifyClientMoved(client, target_channel, reason, msg, invoker, lock_channel_tree);
}

bool QueryClient::notifyClientLeftView(const std::shared_ptr<ConnectedClient> &client, const std::shared_ptr<BasicChannel> &target_channel, ViewReasonId reasonId, const std::string &reasonMessage, std::shared_ptr<ConnectedClient> invoker,
                                       bool lock_channel_tree) {
    if(!this->eventActive(QueryEventGroup::QEVENTGROUP_CLIENT_VIEW, QueryEventSpecifier::QEVENTSPECIFIER_CLIENT_VIEW_LEAVE)) {
        std::unique_lock tree_lock{this->channel_tree_mutex, std::defer_lock};
        if(lock_channel_tree) {
            tree_lock.lock();
        }

        this->visibleClients.erase(std::remove_if(this->visibleClients.begin(), this->visibleClients.end(), [&, client](const weak_ptr<ConnectedClient>& weak) {
            auto c = weak.lock();
            if(!c) {
                logError(this->getServerId(), "{} Got \"dead\" client in visible client list! This can cause a remote client disconnect within the future!", CLIENT_STR_LOG_PREFIX);
                return true;
            }
            return c == client;
        }), this->visibleClients.end());
        return true;
    }
    return ConnectedClient::notifyClientLeftView(client, target_channel, reasonId, reasonMessage, invoker, lock_channel_tree);
}

bool QueryClient::notifyClientLeftView(const std::deque<std::shared_ptr<ConnectedClient>> &clients, const std::string &string, bool lock_channel_tree, const ViewReasonServerLeftT &t) {
    if(!this->eventActive(QueryEventGroup::QEVENTGROUP_CLIENT_VIEW, QueryEventSpecifier::QEVENTSPECIFIER_CLIENT_VIEW_LEAVE)) {
        std::unique_lock tree_lock{this->channel_tree_mutex, std::defer_lock};
        if(lock_channel_tree) {
            tree_lock.lock();
        }

        this->visibleClients.erase(std::remove_if(this->visibleClients.begin(), this->visibleClients.end(), [&](const weak_ptr<ConnectedClient>& weak) {
            auto c = weak.lock();
            if(!c) {
                logError(this->getServerId(), "{} Got \"dead\" client in visible client list! This can cause a remote client disconnect within the future!", CLIENT_STR_LOG_PREFIX);
                return true;
            }
            return std::find(clients.begin(), clients.end(), c) != clients.end();
        }), this->visibleClients.end());
        return true;
    }
    return ConnectedClient::notifyClientLeftView(clients, string, lock_channel_tree, t);
}

bool QueryClient::notifyClientLeftViewKicked(const std::shared_ptr<ConnectedClient> &client, const std::shared_ptr<BasicChannel> &target_channel, const std::string &message, std::shared_ptr<ConnectedClient> invoker, bool lock_channel_tree) {
    if(!this->eventActive(QueryEventGroup::QEVENTGROUP_CLIENT_VIEW, QueryEventSpecifier::QEVENTSPECIFIER_CLIENT_VIEW_LEAVE)) {
        std::unique_lock tree_lock{this->channel_tree_mutex, std::defer_lock};
        if(lock_channel_tree) {
            tree_lock.lock();
        }

        this->visibleClients.erase(std::remove_if(this->visibleClients.begin(), this->visibleClients.end(), [&, client](const weak_ptr<ConnectedClient>& weak) {
            auto c = weak.lock();
            if(!c) {
                logError(this->getServerId(), "{} Got \"dead\" client in visible client list! This can cause a remote client disconnect within the future!", CLIENT_STR_LOG_PREFIX);
                return true;
            }
            return c == client;
        }), this->visibleClients.end());
        return true;
    }
    return ConnectedClient::notifyClientLeftViewKicked(client, target_channel, message, invoker, lock_channel_tree);
}

bool QueryClient::notifyClientLeftViewBanned(const std::shared_ptr<ConnectedClient> &client, const std::string &message, std::shared_ptr<ConnectedClient> invoker, size_t length, bool lock_channel_tree) {
    if(!this->eventActive(QueryEventGroup::QEVENTGROUP_CLIENT_VIEW, QueryEventSpecifier::QEVENTSPECIFIER_CLIENT_VIEW_LEAVE)) {
        std::unique_lock tree_lock{this->channel_tree_mutex, std::defer_lock};
        if(lock_channel_tree) {
            tree_lock.lock();
        }

        this->visibleClients.erase(std::remove_if(this->visibleClients.begin(), this->visibleClients.end(), [&, client](const weak_ptr<ConnectedClient>& weak) {
            auto c = weak.lock();
            if(!c) {
                logError(this->getServerId(), "{} Got \"dead\" client in visible client list! This can cause a remote client disconnect within the future!", CLIENT_STR_LOG_PREFIX);
                return true;
            }
            return c == client;
        }), this->visibleClients.end());
        return true;
    }
    return ConnectedClient::notifyClientLeftViewBanned(client, message, invoker, length, lock_channel_tree);
}

bool QueryClient::notifyMusicQueueAdd(const std::shared_ptr<MusicClient> &bot, const std::shared_ptr<ts::music::SongInfo> &entry, int index, const std::shared_ptr<ConnectedClient> &invoker) {
    CHK_EVENT(QEVENTGROUP_MUSIC, QEVENTSPECIFIER_MUSIC_QUEUE);
    return ConnectedClient::notifyMusicQueueAdd(bot, entry, index, invoker);
}

bool QueryClient::notifyMusicQueueRemove(const std::shared_ptr<MusicClient> &bot, const std::deque<std::shared_ptr<music::SongInfo>> &entry, const std::shared_ptr<ConnectedClient> &invoker) {
    CHK_EVENT(QEVENTGROUP_MUSIC, QEVENTSPECIFIER_MUSIC_QUEUE);
    return ConnectedClient::notifyMusicQueueRemove(bot, entry, invoker);
}

bool QueryClient::notifyMusicQueueOrderChange(const std::shared_ptr<MusicClient> &bot, const std::shared_ptr<ts::music::SongInfo> &entry, int order, const std::shared_ptr<ConnectedClient> &invoker) {
    CHK_EVENT(QEVENTGROUP_MUSIC, QEVENTSPECIFIER_MUSIC_QUEUE);
    return ConnectedClient::notifyMusicQueueOrderChange(bot, entry, order, invoker);
}


bool QueryClient::notifyMusicPlayerSongChange(const std::shared_ptr<MusicClient> &bot, const shared_ptr<ts::music::SongInfo> &newEntry) {
    CHK_EVENT(QEVENTGROUP_MUSIC, QEVENTSPECIFIER_MUSIC_PLAYER);
    return ConnectedClient::notifyMusicPlayerSongChange(bot, newEntry);
}

bool QueryClient::notifyChannelSubscribed(const deque<shared_ptr<BasicChannel>> &) {
    return false;
}

bool QueryClient::notifyChannelUnsubscribed(const deque<shared_ptr<BasicChannel>> &){
    return false;
}