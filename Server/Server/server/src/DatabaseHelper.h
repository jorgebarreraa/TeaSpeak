#pragma once

#include <Definitions.h>
#include <map>
#include <vector>
#include <chrono>
#include <PermissionManager.h>
#include <Properties.h>
#include <cstdint>

namespace ts::server {
    class VirtualServer;
    class DataClient;

    struct ClientDatabaseInfo {
        ServerId server_id;

        ClientDbId client_database_id;
        std::string client_unique_id;
        std::string client_nickname;
        std::string client_ip;

        std::chrono::time_point<std::chrono::system_clock> client_created;
        std::chrono::time_point<std::chrono::system_clock> client_last_connected;

        uint32_t client_total_connections;
    };

    struct StartupPermissionEntry {
        permission::PermissionSqlType type = permission::SQL_PERM_CHANNEL;
        uint64_t id = 0;
        ChannelId channelId = 0;
        std::shared_ptr<permission::PermissionTypeEntry> permission = permission::PermissionTypeEntry::unknown;
        permission::PermissionValue value = 0;
        permission::PermissionValue grant = 0;

        bool flag_skip = false;
        bool flag_negate = false;
    };

    struct StartupPropertyEntry {
        property::PropertyType type = property::PropertyType::PROP_TYPE_UNKNOWN;
        uint64_t id{0};
        const property::PropertyDescription* info{&property::undefined_property_description};
        std::string value;
    };

    struct DatabaseClient {
        ClientDbId client_database_id;
        std::string client_unique_id;

        std::string client_nickname;
        std::string client_ip;

        std::string client_created; /* seconds since epoch */
        std::string client_last_connected; /* seconds since epoch */
        std::string client_total_connections;

        std::string client_login_name;
        std::string client_description; /* optional and only given sometimes */
    };

    struct FastPropertyEntry {
        const property::PropertyDescription* type;
        std::string value;
    };

    struct CachedPermissionManager;
    struct StartupCacheEntry;
    class DatabaseHelper {
        public:
            static std::shared_ptr<PropertyManager> default_properties_client(std::shared_ptr<PropertyManager> /* properties */, ClientType /* type */);
            static bool assignDatabaseId(sql::SqlManager *, ServerId serverId, std::shared_ptr<DataClient>);

            explicit DatabaseHelper(sql::SqlManager*);
            ~DatabaseHelper();

            void loadStartupCache();
            size_t cacheBinarySize();
            void clearStartupCache(ServerId sid = 0);

            void handleServerDelete(ServerId /* server id */);

            void listDatabaseClients(
                    ServerId /* server id */,
                    const std::optional<int64_t>& offset,
                    const std::optional<int64_t>& limit,
                    void(* /* callback */)(void* /* user argument */, const DatabaseClient& /* client */),
                    void* /* user argument */);

            void deleteClient(const std::shared_ptr<VirtualServer>&,ClientDbId);
            bool validClientDatabaseId(const std::shared_ptr<VirtualServer>&, ClientDbId);
            std::deque<std::shared_ptr<ClientDatabaseInfo>> queryDatabaseInfo(const std::shared_ptr<VirtualServer>&, const std::deque<ClientDbId>&);
            std::deque<std::shared_ptr<ClientDatabaseInfo>> queryDatabaseInfoByUid(const std::shared_ptr<VirtualServer> &, std::deque<std::string>);

            std::shared_ptr<permission::v2::PermissionManager> loadClientPermissionManager(const ServerId&, ClientDbId);
            void saveClientPermissions(const std::shared_ptr<VirtualServer>&, ClientDbId , const std::shared_ptr<permission::v2::PermissionManager>& /* permission manager */);

            std::shared_ptr<permission::v2::PermissionManager> loadChannelPermissions(const std::shared_ptr<VirtualServer>&, ChannelId);
            void saveChannelPermissions(const std::shared_ptr<VirtualServer>&, ChannelId, const std::shared_ptr<permission::v2::PermissionManager>& /* permission manager */);

            std::shared_ptr<permission::v2::PermissionManager> loadGroupPermissions(const ServerId& server_id, GroupId, uint8_t /* target */);
            void saveGroupPermissions(const ServerId&, GroupId, uint8_t /* target */, const std::shared_ptr<permission::v2::PermissionManager>& /* permission manager */);

            std::shared_ptr<permission::v2::PermissionManager> loadPlaylistPermissions(const std::shared_ptr<VirtualServer>&, PlaylistId /* playlist id */);
            void savePlaylistPermissions(const std::shared_ptr<VirtualServer>&, PlaylistId, const std::shared_ptr<permission::v2::PermissionManager>& /* permission manager */);

            std::shared_ptr<PropertyManager> loadServerProperties(const std::shared_ptr<VirtualServer>&); //Read and write
            std::shared_ptr<PropertyManager> loadPlaylistProperties(const std::shared_ptr<VirtualServer>&, PlaylistId); //Read and write
            std::shared_ptr<PropertyManager> loadChannelProperties(const std::shared_ptr<VirtualServer>&, ChannelId); //Read and write
            std::shared_ptr<PropertyManager> loadClientProperties(const std::shared_ptr<VirtualServer>&, ClientDbId, ClientType);
            void updateClientIpAddress(const ServerId& /* server id */, ClientDbId /* target database id */, const std::string& /* ip address */);

            void deleteGroupArtifacts(ServerId, GroupId);
            bool deleteChannelPermissions(const std::shared_ptr<VirtualServer>&, ChannelId);
            bool deletePlaylist(const std::shared_ptr<VirtualServer>&, PlaylistId /* playlist id */);
            std::deque<std::unique_ptr<FastPropertyEntry>> query_properties(ServerId /* server */, property::PropertyType /* type */, uint64_t /* id */); /* required for server snapshots */

            void tick();
        private:
            void loadStartupPermissionCache();
            void loadStartupPropertyCache();

            bool use_startup_cache = false;
            threads::Mutex startup_lock;
            std::deque<std::shared_ptr<StartupCacheEntry>> startup_entries;

            sql::SqlManager* sql = nullptr;

            threads::Mutex cached_permission_manager_lock;
            std::deque<std::unique_ptr<CachedPermissionManager>> cached_permission_managers;

            /* Attention: cached_permission_manager_lock should be locked! */
            [[nodiscard]] inline std::shared_ptr<permission::v2::PermissionManager> find_cached_permission_manager(ServerId /* server id */, ClientDbId /* client id */);
    };
}