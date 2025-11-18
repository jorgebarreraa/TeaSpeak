#pragma once

#include <teaspeak/MusicPlayer.h>
#include <misc/net.h>
#include <cstdint>
#include <src/music/PlayablePlaylist.h>
#include <misc/task_executor.h>
#include <misc/sassert.h>
#include "music/Song.h"
#include "../channel/ClientChannelView.h"
#include "DataClient.h"
#include "query/command3.h"

#define CLIENT_STR_LOG_PREFIX_(this) (this->getLoggingPrefix())
#define CLIENT_STR_LOG_PREFIX CLIENT_STR_LOG_PREFIX_(this)

#define CMD_REQ_SERVER   \
do { \
    if(!this->server) { \
        return command_result{error::server_invalid_id}; \
    } \
} while(0)

/* TODO: Play lock the server here with read? So the client dosn't get kicked within that moment */
#define CMD_REF_SERVER(variable_name)   \
std::shared_ptr<VirtualServer> variable_name = this->getServer(); \
if(!variable_name) return command_result{error::server_invalid_id};

#define CMD_REQ_CHANNEL  \
if(!this->currentChannel) return command_result{error::channel_invalid_id};

#define CMD_RESET_IDLE \
do { \
    this->resetIdleTime(); \
} while(false)

#define CMD_REQ_PARM(parm) \
if(!cmd[0].has(parm)) return command_result{error::parameter_not_found};

//the message here is show to the manager!
#define CMD_CHK_AND_INC_FLOOD_POINTS(num) \
do {\
    this->increaseFloodPoints(num); \
    if(this->shouldFloodBlock()) return command_result{error::ban_flooding}; \
} while(0)

#define CMD_CHK_PARM_COUNT(count) \
if(cmd.bulkCount() != count) return command_result{error::parameter_invalid_count};

namespace ts {
    namespace connection {
        class VoiceClientConnection;
    }

    namespace server {
        class VirtualServer;
        class MusicClient;
        class WebClient;
        class MusicClient;

        namespace groups {
            class Group;
            class ServerGroup;
            class ChannelGroup;
        }

        struct ConnectionInfoData {
            std::chrono::time_point<std::chrono::system_clock> timestamp;
            std::map<std::string, std::string> properties;
        };

        class ConnectedClient : public DataClient {
                friend class VirtualServer;
                friend class VoiceClient;
                friend class MusicClient;
                friend class WebClient;
                friend class WebControlServer;
                friend class music::MusicBotManager;
                friend class QueryServer;
                friend class DataClient;
                friend class SpeakingClient;
                friend class connection::VoiceClientConnection;
                friend class VirtualServerManager;
            public:
                explicit ConnectedClient(sql::SqlManager*, const std::shared_ptr<VirtualServer>& server);
                ~ConnectedClient() override;

                ConnectionState connectionState(){ return this->state; }
                std::string getLoggingPeerIp() { return config::server::disable_ip_saving || (this->server && this->server->disable_ip_saving()) ? "X.X.X.X" : this->getPeerIp(); }
                std::string getPeerIp(){ return net::to_string(this->remote_address, false); }
                uint16_t getPeerPort(){ return net::port(this->remote_address); }
                std::string getHardwareId(){ return properties()[property::CLIENT_HARDWARE_ID]; }

                [[nodiscard]] inline std::string getLoggingPrefix() {
                    return std::string{"["} + this->getLoggingPeerIp() + ":" + std::to_string(this->getPeerPort()) + "/" + this->getDisplayName() + " | " + std::to_string(this->getClientId()) + "]";
                }

                //General connection stuff
                bool isAddressV4() { return this->remote_address.ss_family == AF_INET; }
                const sockaddr_in* getAddressV4(){ return (sockaddr_in*) &this->remote_address; }
                bool isAddressV6() { return this->remote_address.ss_family == AF_INET6; }
                const sockaddr_in6* getAddressV6(){ return (sockaddr_in6*) &this->remote_address; }

                /* Note: Order is not guaranteed here! */
                virtual void sendCommand(const ts::Command& command, bool low = false) = 0;
                virtual void sendCommand(const ts::command_builder& command, bool low = false) = 0;

                //General manager stuff
                //FIXME cache the client id for speedup
                virtual uint16_t getClientId() { return this->properties()[property::CLIENT_ID]; }
                virtual void setClientId(uint16_t clId) { properties()[property::CLIENT_ID] = clId; }

                inline std::shared_ptr<BasicChannel> getChannel(){ return this->currentChannel; }
                inline ChannelId getChannelId(){ auto channel = this->currentChannel; return channel ? channel->channelId() : 0; }

                /* If called without locking the client channel tree **must** be write locked */
                void subscribeChannel(const std::deque<std::shared_ptr<BasicChannel>>& target, bool /* lock server and client channel tree */, bool /* enforce */);
                /* If called without locking the client channel tree **must** be write locked */
                void unsubscribeChannel(const std::deque<std::shared_ptr<BasicChannel>>& target, bool /* lock server and client channel tree */);

                bool isClientVisible(const std::shared_ptr<ConnectedClient>&, bool /* lock channel lock */);
                inline std::deque<std::weak_ptr<ConnectedClient>> getVisibleClients(bool lock_channel) {
                    std::shared_lock lock(this->channel_tree_mutex, std::defer_lock);
                    if(lock_channel) {
                        lock.lock();
                    }
                    assert(mutex_shared_locked(this->channel_tree_mutex));
                    return this->visibleClients;
                }

                /** Notifies general stuff **/
                virtual bool notifyError(const command_result&, const std::string& retCode = "");
                virtual void writeCommandResult(ts::command_builder&, const command_result&, const std::string& errorCodeKey = "id");

                /** Notifies (after request) */
                bool sendNeededPermissions(bool /* force an update */); /* invoke this because it dosn't spam the client */
                virtual bool notifyClientNeededPermissions();
                virtual bool notifyGroupPermList(const std::shared_ptr<groups::Group>&, bool);
                virtual bool notifyClientPermList(ClientDbId, const std::shared_ptr<permission::v2::PermissionManager>&, bool);
                virtual bool notifyConnectionInfo(const std::shared_ptr<ConnectedClient> &target, const std::shared_ptr<ConnectionInfoData> &info);
                virtual bool notifyChannelSubscribed(const std::deque<std::shared_ptr<BasicChannel>> &);
                virtual bool notifyChannelUnsubscribed(const std::deque<std::shared_ptr<BasicChannel>> &);

                virtual bool notifyServerGroupList(std::optional<ts::command_builder>& /* generated notify */, bool /* as notify */);
                virtual bool notifyChannelGroupList(std::optional<ts::command_builder>& /* generated notify */, bool /* as notify */);

                /** Notifies (without request) */
                //Group server
                virtual bool notifyServerUpdated(std::shared_ptr<ConnectedClient>);
                //Group manager
                virtual bool notifyClientPoke(std::shared_ptr<ConnectedClient> invoker, std::string msg);
                virtual bool notifyClientUpdated(
                        const std::shared_ptr<ConnectedClient> &,
                        const std::deque<const property::PropertyDescription*> &,
                        bool lock_channel_tree
                ); /* invalid client id causes error: invalid clientID */

                virtual bool notifyPluginCmd(std::string name, std::string msg, std::shared_ptr<ConnectedClient>);
                //Group manager chat
                virtual bool notifyClientChatComposing(const std::shared_ptr<ConnectedClient> &);
                virtual bool notifyClientChatClosed(const std::shared_ptr<ConnectedClient> &);
                virtual bool notifyTextMessage(ChatMessageMode mode, const std::shared_ptr<ConnectedClient> &sender, uint64_t targetId, ChannelId channel_id, const std::chrono::system_clock::time_point& /* timestamp */, const std::string &textMessage);
                inline void sendChannelMessage(const std::shared_ptr<ConnectedClient>& sender, const std::string& textMessage){
                    this->notifyTextMessage(ChatMessageMode::TEXTMODE_CHANNEL, sender, this->currentChannel ? this->currentChannel->channelId() : 0, 0, std::chrono::system_clock::now(), textMessage);
                }

                /**
                 * Notify the client that a client has received a new server group.
                 * If the target client isn't visible no notify will be send.
                 *
                 * Note:
                 * 1. This method will lock the channel tree in shared mode!
                 * 2. For TS3 clients if the client isn't in view the client will disconnect.
                 */
                virtual bool notifyServerGroupClientAdd(std::optional<ts::command_builder>& /* generated notify */, const std::shared_ptr<ConnectedClient> &/* invoker */, const std::shared_ptr<ConnectedClient> &/* target client */, const GroupId& /* group id */);

                /**
                 * Notify the client that a client has removed a server group.
                 * If the target client isn't visible no notify will be send.
                 *
                 * Note:
                 * 1. This method will lock the channel tree in shared mode!
                 * 2. For TS3 clients if the client isn't in view the client will disconnect.
                 */
                virtual bool notifyServerGroupClientRemove(std::optional<ts::command_builder>& /* generated notify */, const std::shared_ptr<ConnectedClient> &/* invoker */, const std::shared_ptr<ConnectedClient> &/* target client */, const GroupId& /* group id */);

                /**
                 * Notify that a client has received a new channel group.
                 * If the target client isn't visible no notify will be send.
                 *
                 * Note:
                 * 1. This method will lock the channel tree in shared mode!
                 * 2. For TS3 clients if the client isn't in view the client will disconnect.
                 */
                virtual bool notifyClientChannelGroupChanged(
                        std::optional<ts::command_builder>& /* generated notify */,
                        const std::shared_ptr<ConnectedClient>& /* invoker */,
                        const std::shared_ptr<ConnectedClient>& /* target client */,
                        const ChannelId& /* channel */,
                        const ChannelId& /* inherited channel */,
                        const GroupId& /* group id */
                );

                //Group channel
                virtual bool notifyChannelMoved(const std::shared_ptr<BasicChannel> &channel, ChannelId order, const std::shared_ptr<ConnectedClient> &invoker);
                virtual bool notifyChannelDescriptionChanged(std::shared_ptr<BasicChannel> channel);
                virtual bool notifyChannelPasswordChanged(std::shared_ptr<BasicChannel>);
                virtual bool notifyChannelEdited(
                        const std::shared_ptr<BasicChannel>& /* channel */,
                        const std::vector<property::ChannelProperties>& /* properties */,
                        const std::shared_ptr<ConnectedClient>& /* invoker */,
                        bool /* send channel description */
                ); /* clients channel tree must be at least read locked */

                virtual bool notifyChannelHide(const std::deque<ChannelId> &channels, bool lock_channel_tree);
                virtual bool notifyChannelShow(const std::shared_ptr<BasicChannel> &channel, ChannelId orderId); /* client channel tree must be unique locked and server channel tree shared locked */
                virtual bool notifyChannelCreate(const std::shared_ptr<BasicChannel> &channel, ChannelId orderId, const std::shared_ptr<ConnectedClient> &invoker);
                virtual bool notifyChannelDeleted(const std::deque<ChannelId>& /* channel_ids */, const std::shared_ptr<ConnectedClient>& /* invoker */);
                //Client view
                virtual bool notifyClientEnterView(
                        const std::shared_ptr<ConnectedClient> &client,
                        const std::shared_ptr<ConnectedClient> &invoker,
                        const std::string& /* reason */,
                        const std::shared_ptr<BasicChannel> &to,
                        ViewReasonId reasonId,
                        const std::shared_ptr<BasicChannel> &from,
                        bool lock_channel_tree
                );
                virtual bool notifyClientEnterView(const std::deque<std::shared_ptr<ConnectedClient>>& /* clients */, const ViewReasonSystemT& /* mode */); /* channel lock must be write locked */
                virtual bool notifyClientMoved(
                        const std::shared_ptr<ConnectedClient> &client,
                        const std::shared_ptr<BasicChannel> &target_channel,
                        ViewReasonId reason,
                        std::string msg,
                        std::shared_ptr<ConnectedClient> invoker,
                        bool lock_channel_tree
                );
                virtual bool notifyClientLeftView(
                        const std::shared_ptr<ConnectedClient> &client,
                        const std::shared_ptr<BasicChannel> &target_channel,
                        ViewReasonId reasonId,
                        const std::string& reasonMessage,
                        std::shared_ptr<ConnectedClient> invoker,
                        bool lock_channel_tree
                );
                virtual bool notifyClientLeftView(
                        const std::deque<std::shared_ptr<ConnectedClient>>& /* clients */,
                        const std::string& /* reason */,
                        bool /* lock channel view */,
                        const ViewReasonServerLeftT& /* mode */
                );

                virtual bool notifyClientLeftViewKicked(
                        const std::shared_ptr<ConnectedClient> &client,
                        const std::shared_ptr<BasicChannel> &target_channel,
                        const std::string& message,
                        std::shared_ptr<ConnectedClient> invoker,
                        bool lock_channel_tree
                );
                virtual bool notifyClientLeftViewBanned(
                        const std::shared_ptr<ConnectedClient> &client,
                        const std::string& message,
                        std::shared_ptr<ConnectedClient> invoker,
                        size_t length,
                        bool lock_channel_tree
                );

                virtual bool notifyMusicPlayerSongChange(const std::shared_ptr<MusicClient>& bot, const std::shared_ptr<music::SongInfo>& newEntry);
                virtual bool notifyMusicQueueAdd(const std::shared_ptr<MusicClient>& bot, const std::shared_ptr<ts::music::SongInfo>& entry, int index, const std::shared_ptr<ConnectedClient>& invoker);
                virtual bool notifyMusicQueueRemove(const std::shared_ptr<MusicClient>& bot, const std::deque<std::shared_ptr<music::SongInfo>>& entry, const std::shared_ptr<ConnectedClient>& invoker);
                virtual bool notifyMusicQueueOrderChange(const std::shared_ptr<MusicClient>& bot, const std::shared_ptr<ts::music::SongInfo>& entry, int order, const std::shared_ptr<ConnectedClient>& invoker);
                virtual bool notifyMusicPlayerStatusUpdate(const std::shared_ptr<MusicClient>&);

                virtual bool notifyConversationMessageDelete(const ChannelId /* conversation id */, const std::chrono::system_clock::time_point& /* begin timestamp */, const std::chrono::system_clock::time_point& /* begin end */, ClientDbId /* client id */, size_t /* messages */);

                /**
                 * Close the network connection.
                 *
                 * Note:
                 * This method could be called from any thread with any locks in hold.
                 * It's not blocking.
                 *
                 * @param timeout The timestamp when to drop the client if not all data has been send.
                 * @returns `true` if the connection has been closed and `false` if the connection is already closed.
                 */
                virtual bool close_connection(const std::chrono::system_clock::time_point &timeout) = 0;

                /* this method should be callable from everywhere; the method is non blocking! */
                virtual bool disconnect(const std::string& reason) = 0;

                void resetIdleTime();
                void increaseFloodPoints(uint16_t);
                bool shouldFloodBlock();

                virtual bool ignoresFlood() { return !this->block_flood; }
                std::shared_ptr<ConnectionInfoData> request_connection_info(const std::shared_ptr<ConnectedClient> & /* receiver */, bool& /* send temporary (no client response yet) */);

                void updateTalkRights(permission::v2::PermissionFlaggedValue talk_power);

                virtual std::shared_ptr<BanRecord> resolveActiveBan(const std::string& ip_address);

                inline std::shared_ptr<stats::ConnectionStatistics> getConnectionStatistics() {
                    return this->connectionStatistics;
                }

                inline std::shared_ptr<ClientChannelView> channel_view() { return this->channel_tree; }

                [[nodiscard]] inline std::shared_ptr<ConnectedClient> ref() { return this->_this.lock(); }
                [[nodiscard]] inline std::weak_ptr<ConnectedClient> weak_ref() { return this->_this; }

                std::shared_mutex& get_channel_lock() { return this->channel_tree_mutex; }

                /* Attention: Ensure that channel_lock has been locked */
                [[nodiscard]] inline std::vector<GroupId>& current_server_groups() { return this->cached_server_groups; }
                [[nodiscard]] inline GroupId& current_channel_group() { return this->cached_channel_group; }

                /**
                 * Attention: This method should never be called directly (except in some edge cases)!
                 *            Use `task_update_channel_client_properties` instead to schedule an update.
                 */
                virtual void updateChannelClientProperties(bool /* lock channel tree */, bool /* notify our self */);

                /*
                 * permission stuff
                 */
                /**
                 * Attention: This method should never be called directly!
                 *            Use `task_update_needed_permissions` instead to schedule an update.
                 * @returns `true` is a permission updated happened.
                 */
                bool update_client_needed_permissions();

                /**
                 * Note: `server_groups_changed` and `channel_group_changed` could be equal.
                 *        If so it will be true if any changes have been made.
                 * Attention: This method should never be called directly!
                 *            Use `task_update_displayed_groups` instead to schedule an update.
                 */
                void update_displayed_client_groups(bool& server_groups_changed, bool& channel_group_changed);

                std::shared_lock<std::shared_mutex> require_connected_state(bool blocking = false) {
                    //try_to_lock_t
                    std::shared_lock<std::shared_mutex> disconnect_lock{};
                    if(blocking) [[unlikely]]
                        disconnect_lock = std::shared_lock{this->finalDisconnectLock};
                    else
                        disconnect_lock = std::shared_lock{this->finalDisconnectLock, std::try_to_lock};

                    if(!disconnect_lock) [[unlikely]]
                        return disconnect_lock;

                    {
                        std::lock_guard state_lock{this->state_lock};
                        if(this->state != ConnectionState::CONNECTED)
                            return {};
                    }
                    return disconnect_lock;
                }

                inline bool playlist_subscribed(const std::shared_ptr<ts::music::Playlist>& playlist) const {
                    return this->subscribed_playlist_.lock() == playlist;
                }

                permission::PermissionType calculate_and_get_join_state(const std::shared_ptr<BasicChannel>&);
            protected:
                std::weak_ptr<ConnectedClient> _this;
                sockaddr_storage remote_address;

                //General states
                /* TODO: Make this a shared lock and lock the client state via holding the lock when executing commands etc */
                std::mutex state_lock{};
                ConnectionState state{ConnectionState::UNKNWON};

                bool allowedToTalk{false};

                std::shared_mutex finalDisconnectLock; /* locked before state lock! */

                std::vector<GroupId> cached_server_groups{}; /* variable locked with channel_lock */
                GroupId cached_channel_group = 0; /* variable locked with channel_lock */

                std::deque<std::weak_ptr<ConnectedClient>> visibleClients{}; /* variable locked with channel_lock */
                std::deque<std::weak_ptr<ConnectedClient>> mutedClients{}; /* variable locked with channel_lock */
                std::deque<std::weak_ptr<ConnectedClient>> open_private_conversations{}; /* variable locked with channel_lock */

                std::chrono::system_clock::time_point lastNeededNotify;
                std::shared_ptr<BasicChannel> lastNeededPermissionNotifyChannel = nullptr;
                bool requireNeededPermissionResend = false;

                std::chrono::system_clock::time_point connectTimestamp;
                std::chrono::system_clock::time_point lastOnlineTimestamp;
                std::chrono::system_clock::time_point lastTransfareTimestamp;
                std::chrono::system_clock::time_point idleTimestamp;

                struct {
                    std::mutex lock;
                    std::shared_ptr<ConnectionInfoData> data;
                    std::chrono::system_clock::time_point data_age;

                    std::deque<std::weak_ptr<ConnectedClient>> receiver;
                    std::chrono::system_clock::time_point last_requested;
                } connection_info;

                struct {
                    std::chrono::system_clock::time_point servergrouplist;
                    std::chrono::system_clock::time_point channelgrouplist;

                    std::chrono::system_clock::time_point last_notify;
                    std::chrono::milliseconds notify_timeout = std::chrono::seconds(60);
                } command_times;

                std::shared_ptr<stats::ConnectionStatistics> connectionStatistics = nullptr;

                bool block_flood = true;
                FloodPoints floodPoints = 0;

                std::shared_ptr<ClientChannelView> channel_tree{};
                std::shared_mutex channel_tree_mutex{};

                /* The permission overview which the client itself has (for basic client actions ) */
                std::mutex client_needed_permissions_lock;
                std::vector<std::pair<permission::PermissionType, permission::v2::PermissionFlaggedValue>> client_needed_permissions;

                permission::v2::PermissionFlaggedValue channels_view_power{0, false};
                permission::v2::PermissionFlaggedValue channels_ignore_view{0, false};
                permission::v2::PermissionFlaggedValue cpmerission_whisper_power{0, false};
                permission::v2::PermissionFlaggedValue cpmerission_needed_whisper_power{0, false};

                virtual void initialize_weak_reference(const std::shared_ptr<ConnectedClient>& /* self reference */);

                bool subscribeToAll{false};
                uint16_t join_state_id{1}; /* default channel value is 0 and by default we need to calculate at least once, so we use 1 */
                /* (ChannelId, ChannelPasswordHash!) (If empty no password/permissions, if "ignore" ignore permissions granted) */
                std::vector<std::pair<ChannelId, std::string>> join_whitelisted_channel{}; /* Access only when the command mutex is acquired */

                std::weak_ptr<MusicClient> selectedBot;
                std::weak_ptr<MusicClient> subscribed_bot;
                std::weak_ptr<ts::music::Playlist> subscribed_playlist_{};

                multi_shot_task task_update_needed_permissions{};
                multi_shot_task task_update_channel_client_properties{};
                multi_shot_task task_update_displayed_groups{};

                bool loadDataForCurrentServer() override;

                virtual void tick_server(const std::chrono::system_clock::time_point &time);
                //Locked by everything who has something todo with command handling
                threads::Mutex command_lock; /* Note: This mutex must be recursive! */
                std::vector<std::function<void()>> postCommandHandler;
                virtual bool handleCommandFull(Command&, bool disconnectOnFail = false);
                virtual command_result handleCommand(Command&);

                command_result handleCommandServerGetVariables(Command&);
                command_result handleCommandServerEdit(Command&);

                command_result handleCommandGetConnectionInfo(Command&);
                command_result handleCommandSetConnectionInfo(Command&);
                command_result handleCommandServerRequestConnectionInfo(Command&);
                command_result handleCommandConnectionInfoAutoUpdate(Command&);
                command_result handleCommandPermissionList(Command&);
                command_result handleCommandPropertyList(Command&);

                command_result handleCommandServerGroupList(Command&);

                command_result handleCommandClientGetIds(Command&);
                command_result handleCommandClientUpdate(Command&);
                command_result handleCommandClientEdit(Command&);
                command_result handleCommandClientEdit(Command&, const std::shared_ptr<ConnectedClient>& /* target */);
                command_result handleCommandClientMove(Command&);
                command_result handleCommandClientGetVariables(Command&);
                command_result handleCommandClientKick(Command&);
                command_result handleCommandClientPoke(Command&);

                command_result handleCommandChannelSubscribe(Command&);
                command_result handleCommandChannelSubscribeAll(Command&);
                command_result handleCommandChannelUnsubscribe(Command&);
                command_result handleCommandChannelUnsubscribeAll(Command&);
                command_result handleCommandChannelCreate(Command&);
                command_result handleCommandChannelDelete(Command&);
                command_result handleCommandChannelEdit(Command&);
                command_result handleCommandChannelGetDescription(Command&);
                command_result handleCommandChannelMove(Command&);
                command_result handleCommandChannelPermList(Command&);
                command_result handleCommandChannelAddPerm(Command&);
                command_result handleCommandChannelDelPerm(Command&);

                //Server group manager management
                command_result handleCommandServerGroupAdd(Command&);
                command_result handleCommandServerGroupCopy(Command&);
                command_result handleCommandServerGroupRename(Command&);
                command_result handleCommandServerGroupDel(Command&);
                command_result handleCommandServerGroupClientList(Command&);
                command_result handleCommandServerGroupAddClient(Command&);
                command_result handleCommandServerGroupDelClient(Command&);
                command_result handleCommandServerGroupPermList(Command&);
                command_result handleCommandServerGroupAddPerm(Command&);
                command_result handleCommandServerGroupDelPerm(Command&);

                command_result handleCommandServerGroupAutoAddPerm(Command&);
                command_result handleCommandServerGroupAutoDelPerm(Command&);

                command_result handleCommandClientAddPerm(Command&);  
                command_result handleCommandClientDelPerm(Command&);  
                command_result handleCommandClientPermList(Command&);  

                command_result handleCommandChannelClientAddPerm(Command&);  
                command_result handleCommandChannelClientDelPerm(Command&);  
                command_result handleCommandChannelClientPermList(Command&);  

                command_result handleCommandChannelGroupAdd(Command&);
                command_result handleCommandChannelGroupCopy(Command&);
                command_result handleCommandChannelGroupRename(Command&);
                command_result handleCommandChannelGroupDel(Command&);
                command_result handleCommandChannelGroupList(Command&);
                command_result handleCommandChannelGroupClientList(Command&);
                command_result handleCommandChannelGroupPermList(Command&);
                command_result handleCommandChannelGroupAddPerm(Command&);
                command_result handleCommandChannelGroupDelPerm(Command&);
                command_result handleCommandSetClientChannelGroup(Command&);

                command_result handleCommandSendTextMessage(Command&);
                command_result handleCommandClientChatComposing(Command&);
                command_result handleCommandClientChatClosed(Command&);

                //File transfare commands
                command_result handleCommandFTGetFileList(Command&);  
                command_result handleCommandFTCreateDir(Command&);  
                command_result handleCommandFTDeleteFile(Command&);  
                command_result handleCommandFTInitUpload(Command&);  
                command_result handleCommandFTInitDownload(Command&); 
                command_result handleCommandFTGetFileInfo(Command&);
                command_result handleCommandFTRenameFile(Command&);
                command_result handleCommandFTList(Command&);
                command_result handleCommandFTStop(Command&);

                command_result handleCommandBanList(Command&);
                command_result handleCommandBanAdd(Command&);
                command_result handleCommandBanEdit(Command&);
                command_result handleCommandBanClient(Command&);
                command_result handleCommandBanDel(Command&);
                command_result handleCommandBanDelAll(Command&);
                command_result handleCommandBanTriggerList(Command&);

                command_result handleCommandTokenList(Command&);
                command_result handleCommandTokenActionList(Command&);
                command_result handleCommandTokenAdd(Command&);
                command_result handleCommandTokenEdit(Command&);
                command_result handleCommandTokenUse(Command&);
                command_result handleCommandTokenDelete(Command&);

                command_result handleCommandClientDbList(Command&);
                command_result handleCommandClientDBEdit(Command&);
                command_result handleCommandClientDbInfo(Command&);
                command_result handleCommandClientDBDelete(Command&);
                command_result handleCommandClientDBFind(Command&);

                command_result handleCommandPluginCmd(Command&);

                command_result handleCommandClientMute(Command&);
                command_result handleCommandClientUnmute(Command&);

                command_result handleCommandComplainAdd(Command&);
                command_result handleCommandComplainList(Command&);
                command_result handleCommandComplainDel(Command&);
                command_result handleCommandComplainDelAll(Command&);

                command_result handleCommandClientGetDBIDfromUID(Command&);
                command_result handleCommandClientGetNameFromDBID(Command&);
                command_result handleCommandClientGetNameFromUid(Command&);
                command_result handleCommandClientGetUidFromClid(Command&);

                //Original from query but still reachable for all
                command_result handleCommandClientList(Command&);
                command_result handleCommandWhoAmI(Command&);
                command_result handleCommandServerGroupsByClientId(Command &); //Maybe not query?

                command_result handleCommandClientFind(Command&);
                command_result handleCommandClientInfo(Command&);
                command_result handleCommandVersion(Command&);

                command_result handleCommandVerifyChannelPassword(Command&);
                command_result handleCommandVerifyServerPassword(Command&);

                command_result handleCommandMessageList(Command&);
                command_result handleCommandMessageAdd(Command&);
                command_result handleCommandMessageGet(Command&);
                command_result handleCommandMessageUpdateFlag(Command&);
                command_result handleCommandMessageDel(Command&);

                command_result handleCommandPermGet(Command&);
                command_result handleCommandPermIdGetByName(Command&);
                command_result handleCommandPermFind(Command&);
                command_result handleCommandPermOverview(Command&);

                command_result handleCommandChannelFind(Command&); 
                command_result handleCommandChannelInfo(Command&); 

                command_result handleCommandMusicBotCreate(Command&); 
                command_result handleCommandMusicBotDelete(Command&); 
                command_result handleCommandMusicBotSetSubscription(Command&); 

                command_result handleCommandMusicBotPlayerInfo(Command&); 
                command_result handleCommandMusicBotPlayerAction(Command&); 

                command_result handleCommandMusicBotQueueList(Command&);  
                command_result handleCommandMusicBotQueueAdd(Command&); 
                command_result handleCommandMusicBotQueueRemove(Command&);
                command_result handleCommandMusicBotQueueReorder(Command&); 

                command_result handleCommandMusicBotPlaylistAssign(Command&);

                /* playlist management */
                command_result handleCommandPlaylistList(Command&);
                command_result handleCommandPlaylistCreate(Command&);
                command_result handleCommandPlaylistDelete(Command&);
                command_result handleCommandPlaylistSetSubscription(Command&);

                command_result handleCommandPlaylistPermList(Command&);
                command_result handleCommandPlaylistAddPerm(Command&);
                command_result handleCommandPlaylistDelPerm(Command&);

                command_result handleCommandPlaylistClientList(Command&);
                command_result handleCommandPlaylistClientPermList(Command&);
                command_result handleCommandPlaylistClientAddPerm(Command&);
                command_result handleCommandPlaylistClientDelPerm(Command&);

                /* playlist properties */
                command_result handleCommandPlaylistInfo(Command&);
                command_result handleCommandPlaylistEdit(Command&);

                command_result handleCommandPlaylistSongList(Command&);
                command_result handleCommandPlaylistSongSetCurrent(Command&);
                command_result handleCommandPlaylistSongAdd(Command&);
                command_result handleCommandPlaylistSongReorder(Command&);
                command_result handleCommandPlaylistSongRemove(Command&);

                command_result handleCommandPermReset(Command&); 

                command_result handleCommandHelp(Command&); 

                command_result handleCommandUpdateMyTsId(Command&);
                command_result handleCommandUpdateMyTsData(Command&);
                /// <summary>
                /// With a whisper list set a client can talk to the specified clients and channels bypassing the standard rule that voice is only transmitted to the current channel. Whisper lists can be defined for individual clients.
                /// </summary>
                /// <remarks>
                /// To control which client is allowed to whisper to own client, the Library implements an internal whisper whitelist mechanism. When a client receives a whisper while the whispering client has not yet been added to the whisper allow list, the receiving client gets the <see cref="Connection.WhisperIgnored"/>-Event. Note that whisper voice data is not received until the sending client is added to the receivers whisper allow list.
                /// </remarks>
                /// <param name="targetChannelArray">array of channels to whisper to, set to null to disable</param>
                /// <param name="targetClientArray">array of clients to whisper to, set to null to disable</param>
                //CMD_TODO handleCommandSetWhisperlist

                //CMD_TODO handleCommandServerTempPasswordList
                //CMD_TODO handleCommandServerTempPasswordDel
                //CMD_TODO handleCommandServerTempPasswordAdd -> servertemppasswordadd pw=PWD desc=DES duration=1200 (20 min) tcid=4 tcpw=asdasd (channel password)

                //Legacy, for TeamSpeak 3
                command_result handleCommandClientSetServerQueryLogin(Command&);

                command_result handleCommandQueryList(Command&);
                command_result handleCommandQueryRename(Command&);
                command_result handleCommandQueryCreate(Command&);
                command_result handleCommandQueryDelete(Command&);
                command_result handleCommandQueryChangePassword(Command&);

                command_result handleCommandConversationHistory(Command&);
                command_result handleCommandConversationFetch(Command&);
                command_result handleCommandConversationMessageDelete(Command&);

                command_result handleCommandLogView(Command&);
                command_result handleCommandLogQuery(Command&);
                command_result handleCommandLogAdd(Command&);

                command_result handleCommandListFeatureSupport(Command &cmd);

                //handleCommandClientSiteReport() -> return findError(0x00)
                //handleCommandChannelCreatePrivate() -> return findError(0x02)
                //handleCommandCustome_Unknown_Command() -> return findError(0x100)

                command_result handleCommandDummy_IpChange(Command&);
                //handleCommandDummy_NewIp
                //handleCommandDummy_ConnectFailed
                //handleCommandDummy_ConnectionLost

                //Not needed - completely useless
                //CMD_TODO handleCommandCustomInfo
                //CMD_TODO handleCommandCustomSearch
                //CMD_TODO serverquerycmd

                void sendChannelList(bool lock_channel_tree);
                void sendServerInit();
                void sendTSPermEditorWarning();

                bool handleTextMessage(ChatMessageMode, std::string, const std::shared_ptr<ConnectedClient>& /* sender target */);

                /**
                 * Call this method only when command handling is locked (aka the client can't do anything).
                 * All other locks shall be free.
                 *
                 * Note: This will not increase the token use count.
                 *       The callee will have to do so.
                 *
                 */
                void useToken(token::TokenId);

                typedef std::function<void(const std::shared_ptr<ConnectedClient>& /* sender */, const std::string& /* message */)> handle_text_command_fn_t;
                bool handle_text_command(
                        ChatMessageMode,
                        const std::string& /* key */,
                        const std::deque<std::string>& /* arguments */,
                        const handle_text_command_fn_t& /* send function */,
                        const std::shared_ptr<ConnectedClient>& /* sender target */
                );

                /* Function to execute the channel edit. We're not checking for any permissions */
                ts::command_result execute_channel_edit(
                        ChannelId& /* channel id */,
                        const std::map<property::ChannelProperties, std::string>& /* values */,
                        bool /* is channel create */
                );

                inline std::string notify_response_command(const std::string_view& notify) {
                    if(this->getExternalType() == ClientType::CLIENT_TEAMSPEAK)
                        return std::string(notify);
                    return "";
                }

                command_result handleCommandGroupAdd(Command&, GroupTarget);
                command_result handleCommandGroupCopy(Command&, GroupTarget);
                command_result handleCommandGroupRename(Command&, GroupTarget);
                command_result handleCommandGroupDel(Command&, GroupTarget);

                command_result executeGroupPermissionEdit(Command&, const std::vector<std::shared_ptr<groups::Group>>& /* groups */, const std::shared_ptr<VirtualServer>& /* target server */, permission::v2::PermissionUpdateType /* mode */);
        };

        template <typename T>
        struct ConnectedLockedClient {
            ConnectedLockedClient() {}
            explicit ConnectedLockedClient(std::shared_ptr<T> client) : client{std::move(client)} {
                if(this->client)
                    this->connection_lock = this->client->require_connected_state();
            }
            explicit ConnectedLockedClient(ConnectedLockedClient&& client) : client{std::move(client.client)}, connection_lock{std::move(client.connection_lock)} { }

            inline ConnectedLockedClient &operator=(const ConnectedLockedClient& other) {
                this->client = other.client;
                if(other)
                    this->connection_lock = std::shared_lock{*other.connection_lock.mutex()}; /* if the other is true (state locked & client) than we could easily acquire a shared lock */
            }

            inline ConnectedLockedClient &operator=(ConnectedLockedClient&& other) {
                this->client = std::move(other.client);
                this->connection_lock = std::move(other.connection_lock);
            }

            inline bool valid() const { return !!this->client && !!this->connection_lock; }

            inline operator bool() const { return this->valid(); }

            inline bool operator!() const { return !this->valid(); }

            template <typename _T>
            inline bool operator==(const std::shared_ptr<_T>& other) { return this->client.get() == other.get(); }

            T* operator->() { return &*this->client; }

            T &operator*() { return *this->client; }

            std::shared_ptr<T> client;
            std::shared_lock<std::shared_mutex> connection_lock{};
        };
    }
}

template <typename T1, typename T2>
inline bool operator==(const std::shared_ptr<T1>& a, const ts::server::ConnectedLockedClient<T2>& b) {
    return a.get() == b.client.get();
}