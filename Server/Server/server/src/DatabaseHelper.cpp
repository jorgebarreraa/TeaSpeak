#include <algorithm>
#include <cstring>
#include <log/LogUtils.h>
#include <sql/SqlQuery.h>
#include <src/client/DataClient.h>
#include <misc/std_unique_ptr.h>

using namespace std;
using namespace std::chrono;
using namespace ts;
using namespace ts::server;
using namespace ts::permission;

//#define DISABLE_CACHING

struct ts::server::CachedPermissionManager {
    ServerId server_id{0};
    ClientDbId client_database_id{0};
    std::weak_ptr<permission::v2::PermissionManager> instance{};

    std::shared_ptr<permission::v2::PermissionManager> instance_ref{}; /* reference to the current instance, will be refreshed every time the instance gets accessed */
    std::chrono::time_point<std::chrono::system_clock> last_access{};
};


struct ts::server::StartupCacheEntry {
    ServerId sid{0};

    std::deque<std::unique_ptr<StartupPermissionEntry>> permissions{};
    std::deque<std::unique_ptr<StartupPropertyEntry>> properties{};
};

DatabaseHelper::DatabaseHelper(sql::SqlManager* srv) : sql(srv) {}
DatabaseHelper::~DatabaseHelper() {
    this->cached_permission_managers.clear();
}

void DatabaseHelper::tick() {
    auto cache_timeout = std::chrono::system_clock::now() - std::chrono::minutes{10};
    {
        std::lock_guard cp_lock{this->cached_permission_manager_lock};

        this->cached_permission_managers.erase(std::remove_if(this->cached_permission_managers.begin(), this->cached_permission_managers.end(), [&](const std::unique_ptr<CachedPermissionManager>& manager) {
            if(manager->last_access < cache_timeout)
                manager->instance_ref = nullptr;

            if(manager->instance.expired())
                return true;

            return false;
        }), this->cached_permission_managers.end());
    }
}

constexpr static std::string_view kSqlBase{"SELECT `client_unique_id`, `client_database_id`, `client_nickname`, `client_created`, `client_last_connected`, `client_ip`, `client_total_connections` FROM `clients_server`"};
inline std::deque<std::shared_ptr<ClientDatabaseInfo>> query_database_client_info(sql::SqlManager* sql_manager, ServerId server_id, const std::string& query, const std::vector<variable>& variables) {
    std::deque<std::shared_ptr<ClientDatabaseInfo>> result{};

    sql::command command{sql_manager, query};
    for(const auto& variable : variables)
        command.value(variable);
    auto sql_result = command.query([&](int length, std::string* values, std::string* names) {
        auto entry = std::make_shared<ClientDatabaseInfo>();

        auto index{0};
        try {
            assert(names[index] == "client_unique_id");
            entry->client_unique_id = values[index++];

            assert(names[index] == "client_database_id");
            entry->client_database_id = std::stoull(values[index++]);

            assert(names[index] == "client_nickname");
            entry->client_nickname = values[index++];

            assert(names[index] == "client_created");
            entry->client_created = std::chrono::system_clock::time_point{} + std::chrono::seconds{std::stoull(values[index++])};

            assert(names[index] == "client_last_connected");
            entry->client_last_connected = std::chrono::system_clock::time_point{} + std::chrono::seconds{std::stoull(values[index++])};

            assert(names[index] == "client_ip");
            entry->client_ip = values[index++];

            assert(names[index] == "client_total_connections");
            entry->client_total_connections = std::stoull(values[index++]);

            assert(index == length);
        } catch (std::exception& ex) {
            logError(server_id, "Failed to parse client base properties at index {}: {}. Query: {}",
                     index - 1,
                     ex.what(),
                     query
            );
        }

        result.push_back(std::move(entry));
    });
    if(!sql_result) {
        logError(server_id, "Failed to query client database infos: {}; Query: {}", sql_result.fmtStr(), query);
        return result;
    }

    return result;
}

std::deque<std::shared_ptr<ClientDatabaseInfo>> DatabaseHelper::queryDatabaseInfo(const std::shared_ptr<VirtualServer>& server, const std::deque<ClientDbId>& list) {
    if(list.empty())
        return {};

    std::string valueList{};
    for(const auto& element : list)
        valueList += ", " + std::to_string(element);
    valueList = valueList.substr(2);

    auto serverId = server ? server->getServerId() : 0;
    return query_database_client_info(this->sql, serverId, std::string{kSqlBase} + "WHERE `server_id` = :sid AND `client_database_id` IN (" + valueList + ")", {variable{":sid", serverId}});
}

std::deque<std::shared_ptr<ClientDatabaseInfo>> DatabaseHelper::queryDatabaseInfoByUid(const std::shared_ptr<VirtualServer> &server, std::deque<std::string> list) {
    if(list.empty())
        return {};

    std::string valueList{};
    for(size_t value_index{0}; value_index < list.size(); value_index++)
        valueList += ", :v" + std::to_string(value_index);
    valueList = valueList.substr(2);

    auto serverId = server ? server->getServerId() : 0;

    std::vector<variable> values{};
    values.reserve(list.size() + 1);

    values.emplace_back(":sid", serverId);
    for(size_t value_index{0}; value_index < list.size(); value_index++)
        values.emplace_back(":v" + std::to_string(value_index), list[value_index]);

    return query_database_client_info(this->sql, serverId, std::string{kSqlBase} + "WHERE `server_id` = :sid AND `client_unique_id` IN (" + valueList + ")", values);
}

bool DatabaseHelper::validClientDatabaseId(const std::shared_ptr<VirtualServer>& server, ClientDbId cldbid) { return cldbid > 0; } //TODO here check

void DatabaseHelper::deleteClient(const std::shared_ptr<VirtualServer>& server, ClientDbId cldbid) {
    auto serverId = (ServerId) (server ? server->getServerId() : 0);
    {
        lock_guard lock{cached_permission_manager_lock};

        this->cached_permission_managers.erase(std::remove_if(this->cached_permission_managers.begin(), this->cached_permission_managers.end(), [&](const auto& entry) {
            return entry->server_id == serverId && entry->client_database_id == cldbid;
        }), this->cached_permission_managers.end());
    }

    sql::result state{};

    state = sql::command(this->sql, "DELETE FROM `properties` WHERE `serverId` = :sid AND (`type` = :type1 OR `type` = :type2) AND `id` = :id", variable{":sid", serverId}, variable{":type1", property::PROP_TYPE_CONNECTION}, variable{":type2", property::PROP_TYPE_CLIENT}, variable{":id", cldbid}).execute();
    state = sql::command(this->sql, "DELETE FROM `permissions` WHERE `serverId` = :sid AND `type` = :type AND `id` = :id", variable{":sid", serverId}, variable{":type", permission::SQL_PERM_USER}, variable{":id", cldbid}).execute();
    state = sql::command(this->sql, "DELETE FROM `bannedClients` WHERE `serverId` = :sid AND `invokerDbid` = :id", variable{":sid", serverId}, variable{":id", cldbid}).execute();
    state = sql::command(this->sql, "DELETE FROM `assignedGroups` WHERE `serverId` = :sid AND `cldbid` = :id", variable{":sid", serverId}, variable{":id", cldbid}).execute();

    if(serverId == 0) {
        state = sql::command(this->sql, "DELETE FROM `clients_server` WHERE `client_database_id` = :id", variable{":id", cldbid}).execute();
        state = sql::command(this->sql, "DELETE FROM `clients` WHERE `client_database_id` = :id", variable{":id", cldbid}).execute();
    } else {
        state = sql::command(this->sql, "DELETE FROM `clients_server` WHERE `server_id` = :sid AND `client_database_id` = :id", variable{":sid", serverId}, variable{":id", cldbid}).execute();
    }

    //TODO delete letters
    //TODO delete query
    //TODO delete complains
}

inline sql::result load_permissions_v2(
        const ServerId& server_id,
        v2::PermissionManager* manager,
        sql::command& command,
        bool test_channel, /* only used for client permissions (client channel permissions) */
        bool is_channel) {
    return command.query([&](int length, char** values, char** names){
        permission::PermissionType key = permission::PermissionType::undefined;
        permission::PermissionValue value = permNotGranted, granted = permNotGranted;
        bool negated = false, skipped = false;
        ChannelId channel_id{0};
        int index;

        try {
            for(index = 0; index < length; index++) {
                if(strcmp(names[index], "permId") == 0) {
                    key = permission::resolvePermissionData(values[index])->type;
                    if(key == permission::unknown){
                        debugMessage(server_id, "[SQL] Permission entry contains invalid permission type! Type: {} Command: {}", values[index], command.sqlCommand());
                        return 0;
                    }
                    if(key == permission::undefined){
                        debugMessage(server_id, "[SQL] Permission entry contains undefined permission type! Type: {} Command: {}", values[index], command.sqlCommand());
                        return 0;
                    }
                } else if(strcmp(names[index], "channelId") == 0) {
                    channel_id = stoull(values[index]);
                } else if(strcmp(names[index], "value") == 0) {
                    value = stoi(values[index]);
                } else if(strcmp(names[index], "grant") == 0) {
                    granted = stoi(values[index]);
                } else if(strcmp(names[index], "flag_skip") == 0)
                    skipped = strcmp(values[index], "1") == 0;
                else if(strcmp(names[index], "flag_negate") == 0)
                    negated = strcmp(values[index], "1") == 0;
            }
        } catch(std::exception& ex) {
            logError(server_id, "[SQL] Cant load permissions! Failed to parse value! Command: {} Message: {} Key: {} Value: {}", command.sqlCommand(), ex.what(),names[index], values[index]);
            return 0;
        }

        if(key == permission::undefined) {
            debugMessage(server_id, "[SQL] Permission entry misses permission type! Command: {}", command.sqlCommand());
            return 0;
        }

        if(channel_id == 0 || is_channel) {
            manager->load_permission(key, {value, granted}, skipped, negated, value != permNotGranted, granted != permNotGranted);
        } else {
            manager->load_permission(key, {value, granted}, channel_id, skipped, negated, value != permNotGranted, granted != permNotGranted);
        }
        return 0;
    });

    //auto end = system_clock::now();
    //auto time = end - start;
    //logTrace(server_id, "[SQL] load_permissions(\"{}\") took {}ms", command.sqlCommand(), duration_cast<milliseconds>(time).count());
}

constexpr static std::string_view kPermissionUpdateCommand{"UPDATE `permissions` SET `value` = :value, `grant` = :grant, `flag_skip` = :flag_skip, `flag_negate` = :flag_negate WHERE `serverId` = :serverId AND `type` = :type AND `id` = :id AND `permId` = :permId AND `channelId` = :chId"};
constexpr static std::string_view kPermissionInsertCommand{"INSERT INTO `permissions` (`serverId`, `type`, `id`, `channelId`, `permId`, `value`, `grant`, `flag_skip`, `flag_negate`) VALUES (:serverId, :type, :id, :chId, :permId, :value, :grant, :flag_skip, :flag_negate)"};
constexpr static std::string_view kPermissionDeleteCommand{"DELETE FROM `permissions` WHERE `serverId` = :serverId AND `type` = :type AND `id` = :id AND `permId` = :permId AND `channelId` = :chId"};

std::shared_ptr<permission::v2::PermissionManager> DatabaseHelper::find_cached_permission_manager(ServerId server_id,
                                                                                                  ClientDbId client_database_id) {
    for(auto it = this->cached_permission_managers.begin(); it != this->cached_permission_managers.end(); it++) {
        auto& cached_manager = *it;

        if(cached_manager->client_database_id == client_database_id && cached_manager->server_id == server_id) {
            auto manager = cached_manager->instance.lock();
            if(!manager){
                this->cached_permission_managers.erase(it);
                break;
            }

            cached_manager->last_access = system_clock::now();
            cached_manager->instance_ref = manager;
            return manager;
        }
    }

    return nullptr;
}

std::shared_ptr<v2::PermissionManager> DatabaseHelper::loadClientPermissionManager(const ServerId& server_id, ClientDbId cldbid) {
#ifndef DISABLE_CACHING
    {
        std::lock_guard lock{cached_permission_manager_lock};
        auto manager = this->find_cached_permission_manager(server_id, cldbid);
        if(manager) return manager;
    }
#endif

    logTrace(server_id, "[Permission] Loading client permission manager for client {}", cldbid);
    auto permission_manager = std::make_shared<v2::PermissionManager>();
    bool loaded = false;
    if(this->use_startup_cache && server_id > 0) {
        shared_ptr<StartupCacheEntry> entry;
        {
            threads::MutexLock lock(this->startup_lock);
            for(const auto& entries : this->startup_entries) {
                if(entries->sid == server_id) {
                    entry = entries;
                    break;
                }
            }
        }
        if(entry) {
            for(const auto& perm : entry->permissions) {
                if(perm->type == permission::SQL_PERM_USER && perm->id == cldbid) {
                    if(perm->channelId > 0) {
                        permission_manager->load_permission(perm->permission->type, {perm->value, perm->grant}, perm->channelId, perm->flag_skip, perm->flag_negate, perm->value != permNotGranted, perm->grant != permNotGranted);
                    } else {
                        permission_manager->load_permission(perm->permission->type, {perm->value, perm->grant}, perm->flag_skip, perm->flag_negate, perm->value != permNotGranted, perm->grant != permNotGranted);
                    }
                }
            }
            loaded = true;
        }
    }

    if(!loaded) {
        auto command = sql::command(this->sql, "SELECT `permId`, `value`, `channelId`, `grant`, `flag_skip`, `flag_negate` FROM `permissions` WHERE `serverId` = :serverId AND `type` = :type AND `id` = :id",
                                    variable{":serverId", server_id},
                                    variable{":type", permission::SQL_PERM_USER},
                                    variable{":id", cldbid});
        LOG_SQL_CMD(load_permissions_v2(server_id, permission_manager.get(), command, true, false));
    }


#ifndef DISABLE_CACHING
    auto cache_entry = std::make_unique<CachedPermissionManager>();
    cache_entry->server_id = server_id;
    cache_entry->instance = permission_manager;
    cache_entry->instance_ref = permission_manager;
    cache_entry->client_database_id = cldbid;
    cache_entry->last_access = system_clock::now();

    {
        std::lock_guard cache_lock{this->cached_permission_manager_lock};

        /* test if we might not got a second instance */
        auto manager = this->find_cached_permission_manager(server_id, cldbid);
        if(manager) return manager;

        this->cached_permission_managers.push_back(std::move(cache_entry));
    }

#endif
    return permission_manager;
}


void DatabaseHelper::saveClientPermissions(const std::shared_ptr<ts::server::VirtualServer> &server, ts::ClientDbId client_dbid, const std::shared_ptr<ts::permission::v2::PermissionManager> &permissions) {
    const auto updates = permissions->flush_db_updates();
    if(updates.empty())
        return;

    auto server_id = server ? server->getServerId() : 0;
    for(auto& update : updates) {
        std::string query{update.flag_delete ? kPermissionDeleteCommand : (update.flag_db ? kPermissionUpdateCommand : kPermissionInsertCommand)};

        auto permission_data = permission::resolvePermissionData(update.permission);
        auto value = update.update_value == v2::delete_value ? permNotGranted : update.values.value;
        auto grant = update.update_grant == v2::delete_value ? permNotGranted : update.values.grant;
        logTrace(server_id, "[CHANNEL] Updating client permission for client {}: {}. New value: {}. New grant: {}. Query: {}",
                 client_dbid,
                 permission_data->name,
                 update.values.value,
                 update.values.grant,
                 query
        );
        sql::command(this->sql, query,
                     variable{":serverId", server ? server->getServerId() : 0},
                     variable{":id", client_dbid},
                     variable{":chId", update.channel_id},
                     variable{":type", permission::SQL_PERM_USER},

                     variable{":permId", permission_data->name},
                     variable{":value", value},
                     variable{":grant", grant},
                     variable{":flag_skip", update.flag_skip},
                     variable{":flag_negate", update.flag_negate})
                .executeLater().waitAndGetLater(LOG_SQL_CMD, {-1, "future error"});
    }
}


std::shared_ptr<permission::v2::PermissionManager> DatabaseHelper::loadGroupPermissions(const ServerId& server_id, ts::GroupId group_id, uint8_t /* target */) {
    auto result = std::make_shared<v2::PermissionManager>();
    if(this->use_startup_cache && server_id > 0) {
        shared_ptr<StartupCacheEntry> entry;
        {
            threads::MutexLock lock(this->startup_lock);
            for(const auto& entries : this->startup_entries) {
                if(entries->sid == server_id) {
                    entry = entries;
                    break;
                }
            }
        }
        if(entry) {
            for(const auto& perm : entry->permissions) {
                if(perm->type == permission::SQL_PERM_GROUP && perm->id == group_id) {
                    result->load_permission(perm->permission->type, {perm->value, perm->grant}, perm->flag_skip, perm->flag_negate, perm->value != permNotGranted, perm->grant != permNotGranted);
                }
            }
            return result;
        }
    }

    //7931
    auto command = sql::command(this->sql, "SELECT `channelId`, `permId`, `value`, `grant`, `flag_skip`, `flag_negate` FROM `permissions` WHERE `serverId` = :serverId AND `type` = :type AND `id` = :id",
                                variable{":serverId", server_id},
                                variable{":type", permission::SQL_PERM_GROUP},
                                variable{":id", group_id});
    LOG_SQL_CMD(load_permissions_v2(server_id, result.get(), command, false, false));
    return result;
}

void DatabaseHelper::saveGroupPermissions(const ServerId &server_id, ts::GroupId group_id, uint8_t target, const std::shared_ptr<ts::permission::v2::PermissionManager> &permissions) {
    const auto updates = permissions->flush_db_updates();
    if(updates.empty())
        return;

    for(auto& update : updates) {
        std::string query{update.flag_delete ? kPermissionDeleteCommand : (update.flag_db ? kPermissionUpdateCommand : kPermissionInsertCommand)};

        auto permission_data = permission::resolvePermissionData(update.permission);
        auto value = update.update_value == v2::delete_value ? permNotGranted : update.values.value;
        auto grant = update.update_grant == v2::delete_value ? permNotGranted : update.values.grant;
        logTrace(server_id, "Updating group permission for group {}/{}: {}. New value: {}. New grant: {}. Query: {}",
                 target, group_id,
                 permission_data->name,
                 value,
                 grant,
                 query
        );
        sql::command(this->sql, query,
                     variable{":serverId", server_id},
                     variable{":id", group_id},
                     variable{":chId", 0},
                     variable{":type", permission::SQL_PERM_GROUP},

                     variable{":permId", permission_data->name},
                     variable{":value", value},
                     variable{":grant", grant},
                     variable{":flag_skip", update.flag_skip},
                     variable{":flag_negate", update.flag_negate})
                .executeLater().waitAndGetLater(LOG_SQL_CMD, {-1, "future error"});
    }
}

std::shared_ptr<permission::v2::PermissionManager> DatabaseHelper::loadPlaylistPermissions(const std::shared_ptr<ts::server::VirtualServer> &server, ts::PlaylistId playlist_id) {
    shared_ptr<permission::v2::PermissionManager> result;
    if(this->use_startup_cache && server) {
        shared_ptr<StartupCacheEntry> entry;
        {
            threads::MutexLock lock(this->startup_lock);
            for(const auto& entries : this->startup_entries) {
                if(entries->sid == server->getServerId()) {
                    entry = entries;
                    break;
                }
            }
        }
        if(entry) {
            result = std::make_shared<permission::v2::PermissionManager>();

            for(const auto& perm : entry->permissions) {
                if(perm->type == permission::SQL_PERM_PLAYLIST && perm->id == playlist_id) {
                    if(perm->channelId)
                        result->load_permission(perm->permission->type, {perm->value, perm->grant}, perm->channelId, perm->flag_skip, perm->flag_negate, perm->value != permNotGranted, perm->grant != permNotGranted);
                    else
                        result->load_permission(perm->permission->type, {perm->value, perm->grant}, perm->flag_skip, perm->flag_negate, perm->value != permNotGranted, perm->grant != permNotGranted);
                }
            }
        }

        return result;
    }

    result = std::make_shared<permission::v2::PermissionManager>();
    auto command = sql::command(this->sql, "SELECT `channelId`, `permId`, `value`, `grant`, `flag_skip`, `flag_negate` FROM `permissions` WHERE `serverId` = :serverId AND `type` = :type AND `id` = :id",
                                variable{":serverId", server ? server->getServerId() : 0},
                                variable{":type", permission::SQL_PERM_PLAYLIST},
                                variable{":id", playlist_id});
    LOG_SQL_CMD(load_permissions_v2(server ? server->getServerId() : 0, result.get(), command, false, false));
    return result;
}

void DatabaseHelper::savePlaylistPermissions(const std::shared_ptr<VirtualServer> &server, PlaylistId pid, const std::shared_ptr<permission::v2::PermissionManager> &permissions) {
    const auto updates = permissions->flush_db_updates();
    if(updates.empty())
        return;

    auto server_id = server ? server->getServerId() : 0;
    for(auto& update : updates) {
        std::string query{update.flag_delete ? kPermissionDeleteCommand : (update.flag_db ? kPermissionUpdateCommand : kPermissionInsertCommand)};

        auto permission_data = permission::resolvePermissionData(update.permission);
        auto value = update.update_value == v2::delete_value ? permNotGranted : update.values.value;
        auto grant = update.update_grant == v2::delete_value ? permNotGranted : update.values.grant;
        logTrace(server_id, "[PLAYLIST] Updating playlist permission for playlist {}: {}. New value: {}. New grant: {}. Query: {}",
                 pid,
                 permission_data->name,
                 value,
                 grant,
                 query
        );
        sql::command(this->sql, query,
                     variable{":serverId", server ? server->getServerId() : 0},
                     variable{":id", pid},
                     variable{":chId", update.channel_id},
                     variable{":type", permission::SQL_PERM_PLAYLIST},

                     variable{":permId", permission_data->name},
                     variable{":value", value},
                     variable{":grant", grant},
                     variable{":flag_skip", update.flag_skip},
                     variable{":flag_negate", update.flag_negate})
                .executeLater().waitAndGetLater(LOG_SQL_CMD, {-1, "future error"});
    }
}

std::shared_ptr<permission::v2::PermissionManager> DatabaseHelper::loadChannelPermissions(const std::shared_ptr<VirtualServer>& server, ts::ChannelId channel) {
    auto result = std::make_shared<v2::PermissionManager>();
    if(this->use_startup_cache && server) {
        shared_ptr<StartupCacheEntry> entry;
        {
            threads::MutexLock lock(this->startup_lock);
            for(const auto& entries : this->startup_entries) {
                if(entries->sid == server->getServerId()) {
                    entry = entries;
                    break;
                }
            }
        }
        if(entry) {
            for(const auto& perm : entry->permissions) {
                if(perm->type == permission::SQL_PERM_CHANNEL && perm->channelId == channel) {
                    result->load_permission(perm->permission->type, {perm->value, perm->grant}, perm->flag_skip, perm->flag_negate, perm->value != permNotGranted, perm->grant != permNotGranted);
                }
            }
            return result;
        }
    }

    auto command = sql::command(sql, "SELECT `permId`, `value`, `grant`, `flag_skip`, `flag_negate` FROM `permissions` WHERE `serverId` = :serverId AND `channelId` = :chid AND `type` = :type AND `id` = :id",
                                variable{":serverId", server ? server->getServerId() : 0},
                                variable{":chid", channel},
                                variable{":id", 0},
                                variable{":type", permission::SQL_PERM_CHANNEL});
    LOG_SQL_CMD(load_permissions_v2(server ? server->getServerId() : 0, result.get(), command, false, true));
    return result;
}

void DatabaseHelper::saveChannelPermissions(const std::shared_ptr<ts::server::VirtualServer> &server, ts::ChannelId channel_id, const std::shared_ptr<ts::permission::v2::PermissionManager> &permissions) {
    const auto updates = permissions->flush_db_updates();
    if(updates.empty())
        return;

    auto server_id = server ? server->getServerId() : 0;
    for(auto& update : updates) {
        std::string query{update.flag_delete ? kPermissionDeleteCommand : (update.flag_db ? kPermissionUpdateCommand : kPermissionInsertCommand)};

        auto value = update.update_value == v2::delete_value ? permNotGranted : update.values.value;
        auto grant = update.update_grant == v2::delete_value ? permNotGranted : update.values.grant;
        auto permission_data = permission::resolvePermissionData(update.permission);
        logTrace(server_id, "[CHANNEL] Updating channel permission for channel {}: {}. New value: {}. New grant: {}. Query: {}",
                 channel_id,
                 permission_data->name,
                 value,
                 grant,
                 query
        );
        sql::command(this->sql, query,
                     variable{":serverId", server ? server->getServerId() : 0},
                     variable{":id", 0},
                     variable{":chId", channel_id},
                     variable{":type", permission::SQL_PERM_CHANNEL},

                     variable{":permId", permission_data->name},
                     variable{":value", value},
                     variable{":grant", grant},
                     variable{":flag_skip", update.flag_skip},
                     variable{":flag_negate", update.flag_negate})
                .executeLater().waitAndGetLater(LOG_SQL_CMD, {-1, "future error"});
    }
}

std::shared_ptr<PropertyManager> DatabaseHelper::default_properties_client(std::shared_ptr<PropertyManager> properties, ClientType type){
    if(!properties) {
        properties = std::make_shared<PropertyManager>();
    }

    properties->register_property_type<property::ClientProperties>();

    if(type == ClientType::CLIENT_MUSIC || type == ClientType::CLIENT_QUERY){
        (*properties)[property::CLIENT_INPUT_HARDWARE] = true;
        (*properties)[property::CLIENT_OUTPUT_HARDWARE] = true;
    }

    return properties;
}

bool DatabaseHelper::assignDatabaseId(sql::SqlManager *sql, ServerId serverId, std::shared_ptr<DataClient> cl) {
    cl->loadDataForCurrentServer();
    if(cl->getClientDatabaseId() == 0) {
        /* client does not exists, create a new one */

        sql::result sql_result{};

        std::string insert_or_ignore{sql->getType() == sql::TYPE_SQLITE ? "INSERT OR IGNORE" : "INSERT IGNORE"};

        auto currentTimeSeconds = duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
        sql_result = sql::command{sql, insert_or_ignore + " INTO `clients` (`client_unique_id`, `client_created`) VALUES (:uniqueId, :now)",
            variable{":uniqueId", cl->getUid()},
            variable{":now", currentTimeSeconds}
        }.execute();

        if(!sql_result) {
            logCritical(LOG_INSTANCE, "Failed to execute client insert command for {}: {}", cl->getUid(), sql_result.fmtStr());
            return false;
        }

        sql_result = sql::command{sql, "INSERT INTO `clients_server` (`server_id`, `client_unique_id`, `client_database_id`, `client_created`) SELECT :serverId, :uniqueId, `client_database_id`, :now FROM `clients` WHERE `client_unique_id` = :uniqueId;",
            variable{":serverId", serverId},
            variable{":uniqueId", cl->getUid()},
            variable{":now", currentTimeSeconds}
        }.execute();

        if(!sql_result) {
            logCritical(LOG_INSTANCE, "Failed to execute client server insert command for {}: {}", cl->getUid(), sql_result.fmtStr());
            return false;
        }

        if(!cl->loadDataForCurrentServer())
            return false;

        debugMessage(serverId, "Successfully registered client {} for server {} with database id {}.", cl->getUid(), serverId, cl->getClientDatabaseId());
    }
    return true;
}

inline sql::result load_properties(ServerId sid, deque<unique_ptr<FastPropertyEntry>>& properties, sql::command& command) {
    auto start = system_clock::now();

    auto result = command.query([&](int length, string* values, string* names) {
        string key, value;
        property::PropertyType type = property::PROP_TYPE_UNKNOWN;

        for(int index = 0; index < length; index++) {
            try {
                if(names[index] == "key") key = values[index];
                else if(names[index] == "value") value = values[index];
                else if(names[index] == "type") type = (property::PropertyType) stoll(values[index]);
            } catch(const std::exception& ex) {
                logError(sid, "Failed to load parse property \"{}\". key: {}, value: {}, message: {}", key,names[index],values[index],ex.what());
                return 0;
            }
        }

        const auto &info = property::find(type, key);
        if(info.name == "undefined") {
            logError(sid, "Found unknown property in database! ({})", key);
            return 0;
        }

        /*
        auto prop = properties->operator[](info);
        prop = value;
        prop.setModified(true);
        prop.setDbReference(true);
        */

        auto data = std::make_unique<FastPropertyEntry>();
        data->type = &info;
        data->value = value;
        properties.push_back(move(data));
        return 0;
    });

    auto end = system_clock::now();
    auto time = end - start;
    logTrace(sid, "[SQL] load_properties(\"{}\") needs {}ms", command.sqlCommand(), duration_cast<milliseconds>(time).count());
    return result;
}

std::shared_ptr<PropertyManager> DatabaseHelper::loadServerProperties(const std::shared_ptr<ts::server::VirtualServer>& server) {
    auto props = std::make_shared<PropertyManager>();

    props->register_property_type<property::VirtualServerProperties>();
    (*props)[property::VIRTUALSERVER_HOST] = config::binding::DefaultVoiceHost;
    (*props)[property::VIRTUALSERVER_WEB_HOST] = config::binding::DefaultWebHost;

    bool loaded = false;
    if(use_startup_cache && server) {
        shared_ptr<StartupCacheEntry> entry;
        {
            threads::MutexLock lock(this->startup_lock);
            for(const auto& entries : this->startup_entries) {
                if(entries->sid == server->getServerId()) {
                    entry = entries;
                    break;
                }
            }
        }
        if(entry) {
            for(const auto& prop : entry->properties) {
                if(prop->type == property::PROP_TYPE_SERVER && prop->id == 0) {
                    auto p = (*props)[prop->info];
                    p = prop->value;
                    p.setModified(true);
                    p.setDbReference(true);
                }
            }
            loaded = true;
        }
    }
    if(!loaded) {
        auto command = sql::command(this->sql, "SELECT `key`, `value`, `type` FROM properties WHERE `serverId` = :serverId AND `type` = :type", variable{":serverId", server ? server->getServerId() : 0}, variable{":type", property::PropertyType::PROP_TYPE_SERVER});

        deque<unique_ptr<FastPropertyEntry>> property_list;
        LOG_SQL_CMD(load_properties(server ? server->getServerId() : 0, property_list, command));
        for(const auto& entry : property_list) {
            auto prop = props->operator[](entry->type);
            prop = entry->value;
            prop.setModified(true);
            prop.setDbReference(true);
        }
    }

    weak_ptr<VirtualServer> weak = server;
    ServerId serverId = server ? server->getServerId() : 0;
    props->registerNotifyHandler([&, serverId, weak](Property& prop){
        if((prop.type().flags & property::FLAG_SAVE) == 0) {
            prop.setModified(false);
            return;
        }
        auto weak_server = weak.lock();
        if(!weak_server && serverId != 0) {
            return;
        }

        string sql;
        if(prop.hasDbReference()) {
            sql = "UPDATE `properties` SET `value` = :value WHERE `serverId` = :sid AND `type` = :type AND `id` = :id AND `key` = :key";
        } else {
            prop.setDbReference(true);
            sql = "INSERT INTO `properties` (`serverId`, `type`, `id`, `key`, `value`) VALUES (:sid, :type, :id, :key, :value)";
        }

        logTrace(serverId, "Updating server property: " + std::string{prop.type().name} + ". New value: " + prop.value() + ". Query: " + sql);
        sql::command(this->sql, sql,
                     variable{":sid", serverId},
                     variable{":type", property::PropertyType::PROP_TYPE_SERVER},
                     variable{":id", 0},
                     variable{":key", prop.type().name},
                     variable{":value", prop.value()}
        ).executeLater().waitAndGetLater(LOG_SQL_CMD, sql::result{1, "future failed"});
    });
    return props;
}

std::shared_ptr<PropertyManager> DatabaseHelper::loadPlaylistProperties(const std::shared_ptr<ts::server::VirtualServer>& server, PlaylistId id) {
    auto props = std::make_shared<PropertyManager>();

    props->register_property_type<property::PlaylistProperties>();
    (*props)[property::PLAYLIST_ID] = id;

    bool loaded = false;
    if(use_startup_cache && server) {
        shared_ptr<StartupCacheEntry> entry;
        {
            threads::MutexLock lock(this->startup_lock);
            for(const auto& entries : this->startup_entries) {
                if(entries->sid == server->getServerId()) {
                    entry = entries;
                    break;
                }
            }
        }
        if(entry) {
            for(const auto& prop : entry->properties) {
                if(prop->type == property::PROP_TYPE_PLAYLIST && prop->id == id) {
                    auto p = (*props)[prop->info];
                    p = prop->value;
                    p.setModified(true);
                    p.setDbReference(true);
                }
            }
            loaded = true;
        }
    }
    if(!loaded) {
        auto command = sql::command(this->sql, "SELECT `key`, `value`, `type` FROM properties WHERE `serverId` = :serverId AND `type` = :type AND `id` = :id", variable{":serverId", server ? server->getServerId() : 0}, variable{":type", property::PropertyType::PROP_TYPE_PLAYLIST}, variable{":id", id});

        deque<unique_ptr<FastPropertyEntry>> property_list;
        LOG_SQL_CMD(load_properties(server ? server->getServerId() : 0, property_list, command));
        for(const auto& entry : property_list) {
            auto prop = props->operator[](entry->type);
            prop = entry->value;
            prop.setModified(true);
            prop.setDbReference(true);
        }
    }

    weak_ptr<VirtualServer> weak = server;
    ServerId serverId = server ? server->getServerId() : 0;
    props->registerNotifyHandler([&, serverId, weak, id](Property& prop){
        if((prop.type().flags & property::FLAG_SAVE) == 0) {
            prop.setModified(false);
            return;
        }
        auto weak_server = weak.lock();
        if(!weak_server && serverId != 0) return;

        string sql;
        if(prop.hasDbReference()) {
            sql = "UPDATE `properties` SET `value` = :value WHERE `serverId` = :sid AND `type` = :type AND `id` = :id AND `key` = :key";
        } else {
            prop.setDbReference(true);
            sql = "INSERT INTO `properties` (`serverId`, `type`, `id`, `key`, `value`) VALUES (:sid, :type, :id, :key, :value)";
        }

        logTrace(serverId, "Updating playlist property for {}. Key: {} Value: {}", id, prop.type().name, prop.value());
        sql::command(this->sql, sql,
                     variable{":sid", serverId},
                     variable{":type", property::PropertyType::PROP_TYPE_PLAYLIST},
                     variable{":id", id},
                     variable{":key", prop.type().name},
                     variable{":value", prop.value()}
        ).executeLater().waitAndGetLater(LOG_SQL_CMD, sql::result{1, "future failed"});
    });
    return props;
}

std::shared_ptr<PropertyManager> DatabaseHelper::loadChannelProperties(const shared_ptr<VirtualServer>& server, ChannelId channel) {
    ServerId serverId = server ? server->getServerId() : 0U;
    auto props = std::make_shared<PropertyManager>();

    props->register_property_type<property::ChannelProperties>();
    if(server) {
        props->operator[](property::CHANNEL_TOPIC) = server->properties()[property::VIRTUALSERVER_DEFAULT_CHANNEL_TOPIC].value();
        props->operator[](property::CHANNEL_DESCRIPTION) = server->properties()[property::VIRTUALSERVER_DEFAULT_CHANNEL_DESCRIPTION].value();
    }

    bool loaded = false;
    if(use_startup_cache && server) {
        shared_ptr<StartupCacheEntry> entry;
        {
            threads::MutexLock lock(this->startup_lock);
            for(const auto& entries : this->startup_entries) {
                if(entries->sid == server->getServerId()) {
                    entry = entries;
                    break;
                }
            }
        }
        if(entry) {
            for(const auto& prop : entry->properties) {
                if(prop->type == property::PROP_TYPE_CHANNEL && prop->id == channel) {
                    auto p = (*props)[prop->info];
                    p = prop->value;
                    p.setModified(true);
                    p.setDbReference(true);
                }
            }
            loaded = true;
        }
    }
    if(!loaded) {
        auto command = sql::command(this->sql, "SELECT `key`, `value`, `type` FROM properties WHERE `serverId` = :serverId AND `type` = :type AND `id` = :id", variable{":serverId", serverId}, variable{":type", property::PropertyType::PROP_TYPE_CHANNEL}, variable{":id", channel});

        deque<unique_ptr<FastPropertyEntry>> property_list;
        LOG_SQL_CMD(load_properties(serverId, property_list, command));
        for(const auto& entry : property_list) {
            auto prop = props->operator[](entry->type);
            prop = entry->value;
            prop.setModified(true);
            prop.setDbReference(true);
        }
    }

    weak_ptr<VirtualServer> weak = server;
    props->registerNotifyHandler([&, weak, serverId, channel](Property& prop){
        auto weak_server = weak.lock();
        if(!weak_server && serverId != 0)
            return;
        if((prop.type().flags & property::FLAG_SAVE) == 0)
            return;
        if(!prop.isModified())
            return;

        std::string query;
        if(prop.type() == property::CHANNEL_PID){
            query = "UPDATE `channels` SET `parentId` = :value WHERE `serverId` = :serverId AND `channelId` = :id";
        } else if(!prop.hasDbReference()){
            query = "INSERT INTO `properties` (`serverId`, `type`, `id`, `key`, `value`) VALUES (:serverId, :type, :id, :key, :value)";
        } else {
            query = "UPDATE `properties` SET `value` = :value WHERE `serverId` = :serverId AND `id` = :id AND `key` = :key AND `type` = :type";
        }
        logTrace(serverId, "[CHANNEL] Updating channel property for channel {}: {}. New value: '{}'. Query: {}", channel, prop.type().name, prop.value(), query);

        sql::command(this->sql, query,
                     variable{":serverId", serverId},
                     variable{":id", channel},
                     variable{":type", property::PropertyType::PROP_TYPE_CHANNEL},
                     variable{":key", prop.type().name},
                     variable{":value", prop.value()}
        ).executeLater().waitAndGetLater(LOG_SQL_CMD, {-1, "future error"});
        prop.setModified(false);
        prop.setDbReference(true);
    });

    return props;
}

std::shared_ptr<PropertyManager> DatabaseHelper::loadClientProperties(const std::shared_ptr<VirtualServer>& server, ClientDbId cldbid, ClientType type) {
    auto props = DatabaseHelper::default_properties_client(nullptr, type);
    if(server) {
        props->operator[](property::CLIENT_DESCRIPTION) = server->properties()[property::VIRTUALSERVER_DEFAULT_CLIENT_DESCRIPTION].value();
    }

    bool loaded = false;
    if(use_startup_cache && server) {
        shared_ptr<StartupCacheEntry> entry;
        {
            threads::MutexLock lock(this->startup_lock);
            for(const auto& entries : this->startup_entries) {
                if(entries->sid == server->getServerId()) {
                    entry = entries;
                    break;
                }
            }
        }
        if(entry) {
            for(const auto& prop : entry->properties) {
                if(prop->id == cldbid && (prop->type == property::PROP_TYPE_CLIENT)) {
                    auto p = (*props)[prop->info];
                    p = prop->value;
                    p.setModified(true);
                    p.setDbReference(true);
                }
            }
            loaded = true;
        }
    }

    if(!loaded) {
        auto command = sql::command(this->sql, "SELECT `key`, `value`, `type` FROM properties WHERE `serverId` = :serverId AND `type` = :type AND `id` = :id", variable{":serverId", server ? server->getServerId() : 0}, variable{":type", property::PropertyType::PROP_TYPE_CLIENT}, variable{":id", cldbid});

        deque<unique_ptr<FastPropertyEntry>> property_list;
        LOG_SQL_CMD(load_properties(server ? server->getServerId() : 0, property_list, command));
        for(const auto& entry : property_list) {
            auto prop = props->operator[](entry->type);
            prop = entry->value;
            prop.setModified(true);
            prop.setDbReference(true);
        }
    }


    weak_ptr<VirtualServer> weak_server = server;
    auto server_id = server ? server->getServerId() : 0;
    props->registerNotifyHandler([&, weak_server, server_id, cldbid, type](Property& prop){ //General save
        auto server = weak_server.lock();
        if(!server && server_id != 0) {
            logError(server_id, "Tried to update client permissions of a expired server!");
            return;
        }

        if(!prop.isModified()) return;
        if((prop.type().flags & property::FLAG_SAVE) == 0 && (type != ClientType::CLIENT_MUSIC || (prop.type().flags & property::FLAG_SAVE_MUSIC) == 0)) {
            logTrace(server ? server->getServerId() : 0, "[Property] Not saving property '" + std::string{prop.type().name} + "', changed for " + to_string(cldbid) + " (New value: " + prop.value() + ")");
            return;
        }

        auto handle = prop.get_handle();
        if(!handle || !handle->isSaveEnabled()) {
            return;
        }
        if(!prop.hasDbReference() && (prop.default_value() == prop.value())) return; //No changes to default value
        prop.setModified(false);

        std::string sqlCommand;
        if(prop.hasDbReference())
            sqlCommand = "UPDATE `properties` SET `value` = :value WHERE `serverId` = :serverId AND `type` = :type AND `id` = :id AND `key` = :key";
        else {
            prop.setDbReference(true);
            sqlCommand = "INSERT INTO `properties` (`serverId`, `type`, `id`, `key`, `value`) VALUES (:serverId, :type, :id, :key, :value)";
        }
        logTrace(server ? server->getServerId() : 0, "[Property] Changed property in db key: " + std::string{prop.type().name} + " value: " + prop.value());
        sql::command(this->sql, sqlCommand,
                variable{":serverId", server ? server->getServerId() : 0},
                variable{":type", prop.type().type_property},
                variable{":id", cldbid},
                variable{":key", prop.type().name},
                variable{":value", prop.value()}
        ).executeLater().waitAndGetLater(LOG_SQL_CMD, sql::result{1, "future failed"});
    });

    props->registerNotifyHandler([&, weak_server, server_id, cldbid](Property& prop){
        auto server = weak_server.lock();
        if(!server && server_id != 0) {
            logError(server_id, "Tried to update client permissions of a expired server!");
            return;
        }

        std::string column;
        if(prop.type().type_property == property::PROP_TYPE_CLIENT) {
            switch (prop.type().property_index) {
                case property::CLIENT_NICKNAME:
                    column = "client_nickname";
                    break;

                case property::CLIENT_LASTCONNECTED:
                    column = "client_last_connected";
                    break;

                case property::CLIENT_TOTALCONNECTIONS:
                    column = "client_total_connections";
                    break;

                case property::CLIENT_MONTH_BYTES_UPLOADED:
                    column = "client_month_upload";
                    break;

                case property::CLIENT_TOTAL_BYTES_UPLOADED:
                    column = "client_total_upload";
                    break;

                case property::CLIENT_MONTH_BYTES_DOWNLOADED:
                    column = "client_month_download";
                    break;

                case property::CLIENT_TOTAL_BYTES_DOWNLOADED:
                    column = "client_total_download";
                    break;

                default:
                    return;
            }
        }

        debugMessage(server ? server->getServerId() : 0, "[Property] Changing client property '{}' for {} (New value: {}, Column: {})",
            prop.type().name,
            cldbid,
            prop.value(),
            column
        );
        sql::command(this->sql, "UPDATE `clients_server` SET `" + column + "` = :value WHERE `server_id` = :serverId AND `client_database_id` = :cldbid",
                variable{":serverId", server ? server->getServerId() : 0},
                variable{":cldbid", cldbid},
                variable{":value", prop.value()}
        ).executeLater().waitAndGetLater(LOG_SQL_CMD, {1, "future failed"});
    });

    return props;
}

void DatabaseHelper::updateClientIpAddress(const ServerId &server_id, ClientDbId client_database_id, const std::string &ip_address) {
    sql::command(this->sql, "UPDATE `clients_server` SET `client_ip` = :value WHERE `server_id` = :serverId AND `client_database_id` = :cldbid",
                 variable{":serverId", server_id},
                 variable{":cldbid", client_database_id},
                 variable{":value", ip_address}
    ).executeLater().waitAndGetLater(LOG_SQL_CMD, {1, "future failed"});
}

void DatabaseHelper::loadStartupCache() {
    this->loadStartupPermissionCache();
    this->loadStartupPropertyCache();

    this->use_startup_cache = true;
}

size_t DatabaseHelper::cacheBinarySize() {
    size_t result = 0;
    result += sizeof(this->startup_entries);
    for(const auto& entry : this->startup_entries) {
        result += sizeof(entry);
        result += sizeof(*entry.get());
        for(const auto& e : entry->permissions) {
            result += sizeof(e);
            result += sizeof(e.get());
        }
        for(const auto& e : entry->properties) {
            result += sizeof(e);
            result += sizeof(e.get());
            result += e->value.length();
        }
    }
    return result;
}

void DatabaseHelper::clearStartupCache(ts::ServerId sid) {
    if(sid == 0) {
        threads::MutexLock lock(this->startup_lock);
        this->startup_entries.clear();
        this->use_startup_cache = false;
    } else {
        threads::MutexLock lock(this->startup_lock);
        /*
        this->startup_entries.erase(std::remove_if(this->startup_entries.begin(), this->startup_entries.end(), [&](const shared_ptr<StartupCacheEntry>& entry) {
            return entry->sid == sid;
        }), this->startup_entries.end());
         */
    }
}

void DatabaseHelper::handleServerDelete(ServerId server_id) {
    {
        std::lock_guard pm_lock{this->cached_permission_manager_lock};
        this->cached_permission_managers.erase(std::remove_if(this->cached_permission_managers.begin(), this->cached_permission_managers.end(), [&](const auto& entry) {
            return entry->server_id == server_id;
        }), this->cached_permission_managers.end());
    }
}

//SELECT `serverId`, `type`, `id`, `key`, `value` FROM properties ORDER BY `serverId`
//SELECT `serverId`, `type`, `id`, `channelId`, `permId`, `value`, `grant`, `flag_skip`, `flag_negate` FROM permissions ORDER BY `serverId`
struct StartupPermissionArgument {
    std::shared_ptr<StartupCacheEntry> current_server;
};

void DatabaseHelper::loadStartupPermissionCache() {
    StartupPermissionArgument arg;
    sql::command(this->sql, "SELECT `serverId`, `type`, `id`, `channelId`, `permId`, `value`, `grant`, `flag_skip`, `flag_negate` FROM permissions ORDER BY `serverId`").query([&](StartupPermissionArgument* arg, int length, char** values, char** names) {
        auto key = permission::PermissionTypeEntry::unknown;
        permission::PermissionValue value = permNotGranted, granted = permNotGranted;
        permission::PermissionSqlType type = SQL_PERM_GROUP;
        bool negated = false, skipped = false;
        ChannelId channel = 0;
        uint64_t id = 0;
        ServerId serverId = 0;

        int index;
        try {
            for(index = 0; index < length; index++) {
                if(strcmp(names[index], "permId") == 0) {
                    key = permission::resolvePermissionData(values[index]);
                    if(key->type == permission::unknown || key->type == permission::undefined) {
                        debugMessage(0, "[SQL] Permission entry contains invalid permission type! Type: {}", values[index]);
                        return 0;
                    }
                } else if(strcmp(names[index], "channelId") == 0) {
                    channel = stoull(values[index]);
                } else if(strcmp(names[index], "id") == 0) {
                    id = stoull(values[index]);
                } else if(strcmp(names[index], "value") == 0) {
                    value = stoi(values[index]);
                } else if(strcmp(names[index], "grant") == 0) {
                    granted = stoi(values[index]);
                } else if(strcmp(names[index], "flag_skip") == 0)
                    skipped = strcmp(values[index], "1") == 0;
                else if(strcmp(names[index], "flag_negate") == 0)
                    negated = strcmp(values[index], "1") == 0;
                else if(strcmp(names[index], "serverId") == 0)
                    serverId = stoll(values[index]);
                else if(strcmp(names[index], "type") == 0)
                    type = static_cast<PermissionSqlType>(stoll(values[index]));
            }
        } catch(std::exception& ex) {
            logError(0, "[SQL] Cant load permissions! Failed to parse value! Message: {}. Key: {}, Value: \"{}\"", ex.what(),names[index],values[index]);
            return 0;
        }

        if(key == permission::PermissionTypeEntry::unknown) {
            debugMessage(0, "[SQL] Permission entry misses permission type!");
            return 0;
        }
        if(serverId == 0) return 0;

        if(!arg->current_server || arg->current_server->sid != serverId) {
            arg->current_server = nullptr;
            {
                threads::MutexLock lock(this->startup_lock);
                for(const auto& entry : this->startup_entries) {
                    if(entry->sid == serverId) {
                        arg->current_server = entry;
                        break;
                    }
                }
                if(!arg->current_server) {
                    arg->current_server = make_shared<StartupCacheEntry>();
                    arg->current_server->sid = serverId;
                    this->startup_entries.push_back(arg->current_server);
                }
            }
        }

        auto entry = make_unique<StartupPermissionEntry>();
        entry->permission = key;
        entry->type = type;
        entry->value = value;
        entry->grant = granted;
        entry->flag_negate = negated;
        entry->flag_skip = skipped;
        entry->id = id;
        entry->channelId = channel;
        arg->current_server->permissions.push_back(std::move(entry));
        return 0;
    }, &arg);
}

void DatabaseHelper::loadStartupPropertyCache() {
    StartupPermissionArgument arg;
    sql::command(this->sql, "SELECT `serverId`, `type`, `id`, `key`, `value` FROM properties ORDER BY `serverId`").query([&](StartupPermissionArgument* arg, int length, char** values, char** names) {
        std::string key, value;
        property::PropertyType type = property::PROP_TYPE_UNKNOWN;
        ServerId serverId = 0;
        uint64_t id = 0;
        for(int index = 0; index < length; index++) {
            try {
                if(strcmp(names[index], "key") == 0) key = values[index];
                else if(strcmp(names[index], "value") == 0) value = values[index] == nullptr ? "" : values[index];
                else if(strcmp(names[index], "type") == 0) type = (property::PropertyType) stoll(values[index]);
                else if(strcmp(names[index], "serverId") == 0) serverId = stoll(values[index]);
                else if(strcmp(names[index], "id") == 0) id = stoll(values[index]);
            } catch(const std::exception& ex) {
                logError(0, "[SQL] Cant load property! Failed to parse value! Message: {}. Key: {}, Value: \"{}\"", ex.what(),names[index],values[index]);
                return 0;
            }
        }

        const auto& info = property::find(type, key);
        if(info.is_undefined()) {
            logError(serverId, "Invalid property ({} | {})", key, type);
            return 0;
        }
        if(serverId == 0) return 0;

        if(!arg->current_server || arg->current_server->sid != serverId) {
            arg->current_server = nullptr;
            {
                threads::MutexLock lock(this->startup_lock);
                for(const auto& entry : this->startup_entries) {
                    if(entry->sid == serverId) {
                        arg->current_server = entry;
                        break;
                    }
                }
                if(!arg->current_server) {
                    arg->current_server = make_shared<StartupCacheEntry>();
                    arg->current_server->sid = serverId;
                    this->startup_entries.push_back(arg->current_server);
                }
            }
        }

        auto entry = make_unique<StartupPropertyEntry>();
        entry->info = &info;
        entry->value = value;
        entry->id = id;
        entry->type = type;
        arg->current_server->properties.push_back(std::move(entry));
        return 0;
    }, &arg);
}

void DatabaseHelper::deleteGroupArtifacts(ServerId server_id, GroupId group_id) {
    sql::result result{};

    result = sql::command(this->sql, "DELETE FROM `permissions` WHERE `serverId` = :serverId AND `type` = :type AND `id` = :id",
                          variable{":serverId", server_id},
                          variable{":type", permission::SQL_PERM_GROUP},
                          variable{":id", group_id}).execute();
    LOG_SQL_CMD(result);

    result = sql::command(this->sql, "DELETE FROM `tokens` WHERE `serverId` = :serverId AND `targetGroup` = :id",
                          variable{":serverId", server_id},
                          variable{":id", group_id}).execute();
    LOG_SQL_CMD(result);
}

bool DatabaseHelper::deleteChannelPermissions(const std::shared_ptr<ts::server::VirtualServer> &server, ts::ChannelId channel_id) {
    auto command = sql::command(sql, "DELETE FROM `permissions` WHERE `serverId` = :serverId AND `channelId` = :chid",
                                variable{":serverId", server ? server->getServerId() : 0},
                                variable{":chid", channel_id}).execute();
    LOG_SQL_CMD(command);
    return !!command;
}

std::deque<std::unique_ptr<FastPropertyEntry>> DatabaseHelper::query_properties(ts::ServerId server_id, ts::property::PropertyType type, uint64_t id) {
    deque<unique_ptr<FastPropertyEntry>> result;

    auto command = sql::command(this->sql, "SELECT `key`, `value`, `type` FROM properties WHERE `serverId` = :serverId AND `type` = :type AND `id` = :id", variable{":serverId", server_id}, variable{":type", type}, variable{":id", id});
    LOG_SQL_CMD(load_properties(server_id, result, command));

    return result;
}

bool DatabaseHelper::deletePlaylist(const std::shared_ptr<ts::server::VirtualServer> &server, ts::PlaylistId playlist_id) {
    auto server_id = server ? server->getServerId() : (ServerId) 0;

    sql::command(this->sql, "DELETE FROM `playlists` WHERE `serverId` = :server_id AND `playlist_id` = :playlist_id",
                 variable{":server_id", server_id},
                 variable{":playlist_id", playlist_id}
    ).executeLater().waitAndGetLater(LOG_SQL_CMD, {-1, "failed to delete playlist " + to_string(playlist_id) + " from table `playlists`"});

    sql::command(this->sql, "DELETE FROM `permissions` WHERE `serverId` = :serverId AND `type` = :type AND `id` = :id",
                   variable{":serverId", server ? server->getServerId() : 0},
                   variable{":type", permission::SQL_PERM_PLAYLIST},
                   variable{":id", playlist_id}
    ).executeLater().waitAndGetLater(LOG_SQL_CMD, {-1, "failed to delete playlist permissions for playlist " + to_string(playlist_id)});

    sql::command(this->sql, "DELETE FROM `properties` WHERE `serverId` = :serverId AND `type` = :type AND `id` = :id",
                 variable{":serverId", server ? server->getServerId() : 0},
                 variable{":type", property::PROP_TYPE_PLAYLIST},
                 variable{":id", playlist_id}
    ).executeLater().waitAndGetLater(LOG_SQL_CMD, {-1, "failed to delete playlist properties for playlist " + to_string(playlist_id)});

    return true;
}

constexpr static auto kDBListQuery{R"(
SELECT `clients`.*, `properties`.`value` as `client_description` FROM (
    SELECT
       `clients_server`.`client_database_id`,
       `clients_server`.`client_unique_id`,
       `clients_server`.`client_nickname`,
       `clients_server`.`client_ip`,
       `clients_server`.`client_created`,
       `clients_server`.`client_last_connected`,
       `clients_server`.`client_total_connections`,
       `clients`.`client_login_name` FROM `clients_server`
    INNER JOIN `clients` ON `clients`.`client_database_id` = `clients_server`.`client_database_id`
    WHERE `server_id` = :serverId LIMIT :offset, :limit
) AS `clients`
LEFT JOIN `properties` ON `properties`.serverId = :serverId AND `properties`.key = 'client_description' AND `properties`.`id` = `clients`.`client_database_id`
)"};

void DatabaseHelper::listDatabaseClients(
        ServerId server_id,
        const std::optional<int64_t>& offset,
        const std::optional<int64_t>& limit,
        void (* callback)(void *, const DatabaseClient &),
        void *user_argument) {

    DatabaseClient client;
    size_t set_index{0};
    auto sqlResult = sql::command{this->sql, kDBListQuery,
                                  variable{":serverId", server_id},
                                  variable{":offset", offset.has_value() ? *offset : 0},
                                  variable{":limit", limit.has_value() ? *limit : -1}
    }.query([&](int length, std::string* values, std::string* names) {
        set_index++;

        auto index{0};
        try {
            assert(names[index] == "client_database_id");
            client.client_database_id = std::stoull(values[index++]);

            assert(names[index] == "client_unique_id");
            client.client_unique_id = values[index++];

            assert(names[index] == "client_nickname");
            client.client_nickname = values[index++];

            assert(names[index] == "client_ip");
            client.client_ip = values[index++];

            assert(names[index] == "client_created");
            client.client_created = values[index++];

            assert(names[index] == "client_last_connected");
            client.client_last_connected = values[index++];

            assert(names[index] == "client_total_connections");
            client.client_total_connections = values[index++];

            assert(names[index] == "client_login_name");
            client.client_login_name = values[index++];

            assert(names[index] == "client_description");
            client.client_description = values[index++];

            assert(index == length);
        } catch (std::exception& ex) {
            logError(server_id, "Failed to parse client base properties at index {}: {}. Offset: {} Limit: {} Set: {}",
                     index - 1,
                     ex.what(),
                     offset.has_value() ? std::to_string(*offset) : "not given",
                     limit.has_value() ? std::to_string(*limit) : "not given",
                     set_index - 1
            );
            return;
        }

        callback(user_argument, client);
    });
}