#pragma once

#include <deque>
#include <memory>
#include <functional>
#include <ThreadPool/ThreadPool.h>
#include <arpa/inet.h>
#include <BasicChannel.h>
#include <sqlite3.h>
#include <sql/SqlQuery.h>
#include "Group.h"
#include "Properties.h"
#include "query/Command.h"
#include "channel/ServerChannel.h"
#include "manager/BanManager.h"
#include "Definitions.h"
#include "ConnectionStatistics.h"
#include "manager/TokenManager.h"
#include "manager/ComplainManager.h"
#include "DatabaseHelper.h"
#include "manager/LetterManager.h"
#include "Configuration.h"
#include "protocol/ringbuffer.h"
#include "absl/btree/map.h"
#include <misc/task_executor.h>

#include <tomcrypt.h>
#undef byte

#ifdef COMPILE_WEB_CLIENT
    #include "server/WebServer.h"
#endif

template<typename T, typename _Tp>
inline bool operator==(T* elm, const std::shared_ptr<_Tp>& __a) noexcept { return elm == __a.get(); }

template<typename T, typename _Tp>
inline bool operator==(const std::shared_ptr<_Tp>& __a, T* elm) noexcept { return elm == __a.get(); }

template<typename T, typename _Tp>
inline bool operator!=(T* elm, const std::shared_ptr<_Tp>& __a) noexcept { return elm != __a.get(); }

template<typename T, typename _Tp>
inline bool operator!=(const std::shared_ptr<_Tp>& __a, T* elm) noexcept { return elm != __a.get(); }

namespace ts {
    class ServerChannelTree;

    namespace music {
        class MusicBotManager;
    }

    namespace rtc {
        class Server;
    }

    namespace server {
        class ConnectedClient;
        class VoiceClient;
        class QueryClient;
        class WebClient;
        class InternalClient;

        class InstanceHandler;
        class VoiceServer;
        class QueryServer;
        class SpeakingClient;

        class WebControlServer;

        namespace conversation {
            class ConversationManager;
        }

        namespace groups {
            class ServerGroup;
            class ChannelGroup;
            class GroupManager;
        }

        struct ServerState {
            enum value {
                OFFLINE,
                BOOTING,
                ONLINE,
                SUSPENDING,
                DELETING
            };

            inline static std::string string(value state) {
                switch (state) {
                    case ServerState::OFFLINE:
                        return "offline";
                    case ServerState::BOOTING:
                        return "booting";
                    case ServerState::ONLINE:
                        return "online";
                    case ServerState::SUSPENDING:
                        return "suspending";
                    case ServerState::DELETING:
                        return "deleting";
                    default:
                        return "unknown";
                }
            }
        };

        struct ServerSlotUsageReport {
            size_t server_count{0};

            size_t max_clients{0};
            size_t reserved_clients{0};

            size_t clients_teamspeak{0};
            size_t clients_teaspeak{0};
            size_t clients_teaweb{0};
            size_t queries{0};
            size_t music_bots{0};

            size_t online_channels{0};

            [[nodiscard]] inline size_t voice_clients() const {
                return this->clients_teaspeak + this->clients_teamspeak + this->clients_teaweb;
            }

            inline ServerSlotUsageReport& operator+=(const ServerSlotUsageReport& other) {
                this->server_count += other.server_count;
                this->max_clients += other.max_clients;
                this->reserved_clients += other.reserved_clients;
                this->clients_teamspeak += other.clients_teamspeak;
                this->clients_teaspeak += other.clients_teaspeak;
                this->clients_teaweb += other.clients_teaweb;
                this->queries += other.queries;
                this->music_bots += other.music_bots;
                this->online_channels += other.online_channels;
                return *this;
            }
        };

        struct CalculateCache {};

        class VirtualServer {
                friend class WebClient;
                friend class DataClient;
                friend class VoiceClient;
                friend class MusicClient;
                friend class ConnectedClient;
                friend class InternalClient;
                friend class QueryServer;
                friend class QueryClient;
                friend class SpeakingClient;
                friend class music::MusicBotManager;
                friend class InstanceHandler;
                friend class VirtualServerManager;
            public:
                struct NetworkReport {
                    float average_ping{0};
                    float average_loss{0};
                };
                
                VirtualServer(ServerId serverId, sql::SqlManager*);
                ~VirtualServer();

                bool initialize(bool test_properties);

                bool start(std::string& error);
                bool running();
                void preStop(const std::string&);
                void stop(const std::string& reason, bool /* disconnect query */);

                ServerSlotUsageReport onlineStats();
                std::shared_ptr<ConnectedClient> find_client_by_id(ClientId /* client id */);
                std::deque<std::shared_ptr<ConnectedClient>> findClientsByCldbId(ClientDbId cldbId);
                std::deque<std::shared_ptr<ConnectedClient>> findClientsByUid(ClientUid uid);
                std::shared_ptr<ConnectedClient> findClient(std::string name, bool ignoreCase = true);
                bool forEachClient(std::function<void(std::shared_ptr<ConnectedClient>)>);
                //bool forEachClient(std::function<std::shared_ptr<VoiceClient>>, bool executeLaterIfLocked = true);

                std::vector<std::shared_ptr<ConnectedClient>> getClients();
                std::deque<std::shared_ptr<ConnectedClient>> getClientsByChannel(std::shared_ptr<BasicChannel>);
                std::deque<std::shared_ptr<ConnectedClient>> getClientsByChannelRoot(const std::shared_ptr<BasicChannel> &, bool lock_channel_tree);
                [[nodiscard]] size_t countChannelRootClients(const std::shared_ptr<BasicChannel> &, size_t /* limit */, bool /* lock the channel tree */);
                [[nodiscard]] bool isChannelRootEmpty(const std::shared_ptr<BasicChannel> &, bool lock_channel_tree);

                template <typename ClType>
                std::vector<std::shared_ptr<ClType>> getClientsByChannel(const std::shared_ptr<BasicChannel>& ch) {
                    std::vector<std::shared_ptr<ClType>> result;
                    for(const auto& cl : this->getClientsByChannel(ch))
                        if(std::dynamic_pointer_cast<ClType>(cl))
                            result.push_back(std::dynamic_pointer_cast<ClType>(cl));
                    return result;
                }

                ecc_key* serverKey(){ return _serverKey; }
                std::string publicServerKey();

                inline PropertyWrapper properties() { return PropertyWrapper{this->_properties}; }
                inline const PropertyWrapper properties() const { return PropertyWrapper{this->_properties}; }

                inline sql::SqlManager * getSql(){ return this->sql; }
                sql::AsyncSqlPool* getSqlPool(){ return this->sql->pool; }

                inline ServerId getServerId(){ return this->serverId; }
                inline ServerChannelTree* getChannelTree(){ return this->channelTree; }
                inline rtc::Server& rtc_server() { return *this->rtc_server_; }

                [[nodiscard]] inline auto& getTokenManager()  {
                    return *this->tokenManager;
                }

                [[nodiscard]] inline auto group_manager() { return this->groups_manager_; }
                [[nodiscard]] std::shared_ptr<groups::ServerGroup> default_server_group();
                [[nodiscard]] std::shared_ptr<groups::ChannelGroup> default_channel_group();

                bool notifyServerEdited(std::shared_ptr<ConnectedClient>, std::deque<std::string> keys);
                bool notifyClientPropertyUpdates(std::shared_ptr<ConnectedClient>, const std::deque<const property::PropertyDescription*>& keys, bool selfNotify = true); /* execute only with at least channel tree read lock! */
                inline bool notifyClientPropertyUpdates(const std::shared_ptr<ConnectedClient>& client, const std::deque<property::ClientProperties>& keys, bool selfNotify = true) {
                    if(keys.empty()) {
                        return false;
                    }

                    std::deque<const property::PropertyDescription*> _keys{};
                    for(const auto& key : keys) {
                        _keys.push_back(&property::describe(key));
                    }

                    return this->notifyClientPropertyUpdates(client, _keys, selfNotify);
                };

                void broadcastMessage(std::shared_ptr<ConnectedClient>, std::string message);

#ifndef __deprecated
    #define __deprecated __attribute__((deprecated))
#endif
                __deprecated void registerInternalClient(std::shared_ptr<ConnectedClient>);
                __deprecated void unregisterInternalClient(std::shared_ptr<ConnectedClient>);

                std::shared_ptr<ConnectedClient> getServerRoot(){ return this->serverRoot; }

                std::string getDisplayName(){ return properties()[property::VIRTUALSERVER_NAME]; }

                std::shared_ptr<stats::ConnectionStatistics> getServerStatistics(){ return server_statistics_; }

                std::shared_ptr<VoiceServer> getVoiceServer(){ return this->udpVoiceServer; }
                WebControlServer* getWebServer(){ return this->webControlServer; }

                /* calculate permissions for an client in this server */
                permission::v2::PermissionFlaggedValue calculate_permission(
                        permission::PermissionType,
                        ClientDbId,
                        ClientType type,
                        ChannelId channel,
                        bool granted = false
                );

                std::vector<std::pair<permission::PermissionType, permission::v2::PermissionFlaggedValue>> calculate_permissions(
                        const std::deque<permission::PermissionType>&,
                        ClientDbId,
                        ClientType type,
                        ChannelId channel,
                        bool granted = false
                );

                [[nodiscard]] bool verifyServerPassword(std::string /* password */, bool /* hashed */);

                void testBanStateChange(const std::shared_ptr<ConnectedClient>& invoker);

                [[nodiscard]] NetworkReport generate_network_report();

                bool resetPermissions(std::string&);
                void ensureValidDefaultGroups();

                ServerState::value getState() { return this->state; }

                inline std::shared_ptr<VirtualServer> ref() { return this->self.lock(); }
                inline bool disable_ip_saving() { return this->_disable_ip_saving; }
                inline std::chrono::system_clock::time_point start_timestamp() { return this->startTimestamp; };

                /* Note: Use only this method to disconnect the client and notify everybody else that he has been banned! */
                void notify_client_ban(const std::shared_ptr<ConnectedClient>& /* client */, const std::shared_ptr<ConnectedClient>& /* invoker */, const std::string& /* reason */, size_t /* length */);
                void notify_client_kick(
                        const std::shared_ptr<ConnectedClient>& /* client */,
                        const std::shared_ptr<ConnectedClient>& /* invoker */,
                        const std::string& /* reason */,
                        const std::shared_ptr<BasicChannel>& /* target channel */
                );

                void client_move(
                        const std::shared_ptr<ConnectedClient>& /* client */,
                        std::shared_ptr<BasicChannel> /* target channel */,
                        const std::shared_ptr<ConnectedClient>& /* invoker */,
                        const std::string& /* reason */,
                        ViewReasonId /* reason id */,
                        bool /* notify the client */,
                        std::unique_lock<std::shared_mutex>& /* tree lock */
                );

                void delete_channel(
                        std::shared_ptr<ServerChannel> /* target channel */,
                        const std::shared_ptr<ConnectedClient>& /* invoker */,
                        const std::string& /* kick message */,
                        std::unique_lock<std::shared_mutex>& /* tree lock */,
                        bool temporary_auto_delete
                );

                void send_text_message(const std::shared_ptr<BasicChannel>& /* channel */, const std::shared_ptr<ConnectedClient>& /* sender */, const std::string& /* message */);

                inline int voice_encryption_mode() { return this->_voice_encryption_mode; }
                inline std::shared_ptr<conversation::ConversationManager> conversation_manager() { return this->conversation_manager_; }

                inline auto& get_channel_tree_lock() { return this->channel_tree_mutex; }

                void update_channel_from_permissions(const std::shared_ptr<BasicChannel>& /* channel */, const std::shared_ptr<ConnectedClient>& /* issuer */);

                inline void enqueue_notify_channel_group_list() { this->task_notify_channel_group_list.enqueue(); }
                inline void enqueue_notify_server_group_list() {  this->task_notify_server_group_list.enqueue(); }
            protected:
                bool registerClient(std::shared_ptr<ConnectedClient>);
                bool unregisterClient(std::shared_ptr<ConnectedClient>, std::string, std::unique_lock<std::shared_mutex>& channel_tree_lock);
                bool assignDefaultChannel(const std::shared_ptr<ConnectedClient>&, bool join);

            private:
                std::weak_ptr<VirtualServer> self;

                //Locks by tick, start and stop
                std::shared_mutex state_mutex{};
                ServerState::value state{ServerState::OFFLINE};

                task_id tick_task_id{};
                std::chrono::system_clock::time_point lastTick;
                void executeServerTick();

                std::shared_ptr<VoiceServer> udpVoiceServer = nullptr;
                WebControlServer* webControlServer = nullptr;
                token::TokenManager* tokenManager = nullptr;
                ComplainManager* complains = nullptr;
                letter::LetterManager* letters = nullptr;
                std::shared_ptr<music::MusicBotManager> music_manager_;
                std::shared_ptr<stats::ConnectionStatistics> server_statistics_;
                std::shared_ptr<conversation::ConversationManager> conversation_manager_;
                std::unique_ptr<rtc::Server> rtc_server_;

                sql::SqlManager* sql;

                uint16_t serverId = 1;

                std::chrono::system_clock::time_point startTimestamp;
                std::chrono::system_clock::time_point fileStatisticsTimestamp;
                std::chrono::system_clock::time_point conversation_cache_cleanup_timestamp;

                //The client list
                std::mutex clients_mutex{};
                btree::map<ClientId, std::shared_ptr<ConnectedClient>> clients{};

                std::recursive_mutex client_nickname_lock;

                //General server properties
                ecc_key* _serverKey = nullptr;
                std::shared_ptr<PropertyManager> _properties;
                int _voice_encryption_mode = 2; /* */

                ServerChannelTree* channelTree = nullptr;
                std::shared_mutex channel_tree_mutex; /* lock if access channel tree! */

                std::shared_ptr<groups::GroupManager> groups_manager_{};

                std::shared_ptr<ConnectedClient> serverRoot = nullptr;
                std::shared_ptr<ConnectedClient> serverAdmin = nullptr;

                std::mutex join_attempts_lock;
                std::map<std::string, uint16_t > join_attempts;
                std::chrono::system_clock::time_point join_last_decrease;

                std::chrono::milliseconds spoken_time{0};
                std::chrono::system_clock::time_point spoken_time_timestamp;

                multi_shot_task task_notify_channel_group_list{};
                multi_shot_task task_notify_server_group_list{};

                bool _disable_ip_saving = false;
        };
    }
}