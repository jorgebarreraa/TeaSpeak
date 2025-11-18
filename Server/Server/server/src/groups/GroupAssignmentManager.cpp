//
// Created by WolverinDEV on 03/03/2020.
//
#include <log/LogUtils.h>
#include "./GroupAssignmentManager.h"
#include "./GroupManager.h"
#include "BasicChannel.h"

using namespace ts::server::groups;

namespace ts::server::groups {
    struct InternalChannelGroupAssignment {
        InternalChannelGroupAssignment(ChannelId channel_id, GroupId group_id, bool t) : channel_id{channel_id}, group_id{group_id}, temporary_assignment{t} { }
        InternalChannelGroupAssignment(const InternalChannelGroupAssignment& other) = default;
        InternalChannelGroupAssignment(InternalChannelGroupAssignment&&) = default;
        InternalChannelGroupAssignment&operator=(const InternalChannelGroupAssignment&) = default;

        ChannelId channel_id;
        GroupId group_id;
        bool temporary_assignment;
    };

    struct InternalServerGroupAssignment {
        explicit InternalServerGroupAssignment(GroupId group_id, bool temporary) : group_id{group_id}, temporary_assignment{temporary} { }
        InternalServerGroupAssignment(const InternalServerGroupAssignment& other) = default;
        InternalServerGroupAssignment(InternalServerGroupAssignment&&) = default;
        InternalServerGroupAssignment&operator=(const InternalServerGroupAssignment&) = default;

        GroupId group_id;
        bool temporary_assignment;
    };
}

GroupAssignmentManager::GroupAssignmentManager(GroupManager* handle) : manager_{handle}, client_cache_lock{std::make_shared<std::mutex>()} { }
GroupAssignmentManager::~GroupAssignmentManager() = default;

bool GroupAssignmentManager::initialize(std::string &error) {
    return true;
}

sql::SqlManager* GroupAssignmentManager::sql_manager() {
    return this->manager_->sql_manager();
}

ts::ServerId GroupAssignmentManager::server_id() {
    return this->manager_->server_id();
}

bool GroupAssignmentManager::load_data(std::string &error) {
    if constexpr(kCacheAllClients) {
        std::lock_guard cache_lock{*this->client_cache_lock};
        std::shared_ptr<ClientCache> current_entry{nullptr};

        auto res = sql::command(this->sql_manager(), "SELECT `groupId`, `cldbid`, `channelId`, `until` FROM `assignedGroups` WHERE `serverId` = :sid ORDER BY `cldbid`", variable{":sid", this->server_id()})
                .query([&](int length, std::string* value, std::string* column) {
                    ChannelId channel_id{0};
                    GroupId group_id{0};
                    ClientDbId client_dbid{0};

                    for(int index = 0; index < length; index++){
                        try {
                            if(column[index] == "groupId"){
                                group_id = stoll(value[index]);
                            } else if(column[index] == "until"){
                            } else if(column[index] == "channelId"){
                                channel_id = stoll(value[index]);
                            } else if(column[index] == "cldbid"){
                                client_dbid = stoll(value[index]);
                            }
                        } catch(std::exception& ex) {
                            logError(this->server_id(), "Failed to load group assignment from database. Column {} contains an invalid value: {}", column[index], value[index]);
                            return 0;
                        }
                    }

                    if(current_entry)
                        if(current_entry->client_database_id != client_dbid)
                            this->client_cache.push_back(std::move(current_entry));

                    if(!current_entry) {
                        current_entry = std::make_shared<ClientCache>();
                        current_entry->client_database_id = client_dbid;
                    }

                    if(channel_id) {
                        current_entry->channel_group_assignments.push_back(std::make_unique<InternalChannelGroupAssignment>(channel_id, group_id, false));
                    } else {
                        current_entry->server_group_assignments.push_back(std::make_unique<InternalServerGroupAssignment>(group_id, false));
                    }
                    return 0;
                });

        if(!res) {
            error = "failed to query database (" + res.fmtStr() + ")";
            return false;
        }

        if(current_entry) {
            this->client_cache.push_back(std::move(current_entry));
        }
    }
    return true;
}

void GroupAssignmentManager::unload_data() {
    std::lock_guard cache_lock{*this->client_cache_lock};
    this->client_cache.clear();
}

void GroupAssignmentManager::enable_cache_for_client(GroupAssignmentCalculateMode mode, ClientDbId cldbid) {
    if constexpr(!kCacheAllClients) {
        bool cache_exists{false};
        {
            std::lock_guard cache_lock{*this->client_cache_lock};
            for(auto& client : this->client_cache)
                if(client->client_database_id == cldbid) {
                    client->use_count++;
                    cache_exists = true;
                    break;
                }
        }

        if(!cache_exists) {
            auto cache = std::make_shared<ClientCache>();
            cache->client_database_id = cldbid;
            cache->use_count++;

            auto res = sql::command(this->sql_manager(), "SELECT `groupId`, `channelId`, `until` FROM `assignedGroups` WHERE `serverId` = :sid AND `cldbid` = :cldbid", variable{":sid", this->server_id()}, variable{":cldbid", cldbid})
                    .query([&](int length, std::string* value, std::string* column) {
                        ChannelId channel_id{0};
                        GroupId group_id{0};

                        for(int index = 0; index < length; index++){
                            try {
                                if(column[index] == "groupId"){
                                    group_id = stoll(value[index]);
                                } else if(column[index] == "until"){
                                } else if(column[index] == "channelId"){
                                    channel_id = stoll(value[index]);
                                }
                            } catch(std::exception& ex) {
                                logError(this->server_id(), "Failed to load group assignment from database for client {}. Column {} contains an invalid value: {}", cldbid, column[index], value[index]);
                                return 0;
                            }
                        }
                        if(!group_id)
                            return 0;

                        if(channel_id) {
                            cache->channel_group_assignments.push_back(std::make_unique<InternalChannelGroupAssignment>(channel_id, group_id, false));
                        } else {
                            cache->server_group_assignments.push_back(std::make_unique<InternalServerGroupAssignment>(group_id, false));
                        }
                        return 0;
                    });

            std::lock_guard cache_lock{*this->client_cache_lock};
#if 0 /* lets have some performance over double entries :D */
            for(auto& client : this->client_cache)
                if(client->client_database_id == cldbid) {
                    /* somebody already inserted that client while we've loaded him */
                    cache_exists = true;
                    break;
                }
            if(!cache_exists)
#endif
            this->client_cache.push_back(std::move(cache));
        }
    }

    if(mode == GroupAssignmentCalculateMode::GLOBAL) {
        if(auto parent = this->manager_->parent_manager(); parent) {
            parent->assignments().enable_cache_for_client(mode, cldbid);
        }
    }
}

void GroupAssignmentManager::disable_cache_for_client(GroupAssignmentCalculateMode mode, ClientDbId cldbid) {
    if constexpr(!kCacheAllClients) {
        std::lock_guard cache_lock{*this->client_cache_lock};
        this->client_cache.erase(std::remove_if(this->client_cache.begin(), this->client_cache.end(), [cldbid](const std::shared_ptr<ClientCache>& client) {
            return client->client_database_id == cldbid;
        }), this->client_cache.end());
    }

    if(mode == GroupAssignmentCalculateMode::GLOBAL)
        if(auto parent = this->manager_->parent_manager(); parent)
            parent->assignments().disable_cache_for_client(mode, cldbid);
}


std::vector<ts::GroupId> GroupAssignmentManager::server_groups_of_client(ts::server::groups::GroupAssignmentCalculateMode mode,
                                                                         ts::ClientDbId cldbid) {
    std::vector<ts::GroupId> result{};
    bool cache_found{false};
    {
        std::lock_guard cache_lock{*this->client_cache_lock};
        for(auto& entry : this->client_cache) {
            if(entry->client_database_id != cldbid) continue;

            result.reserve(entry->server_group_assignments.size());
            for(auto& assignment : entry->server_group_assignments)
                result.push_back(assignment->group_id);

            cache_found = true;
            break;
        }
    }

    if(!cache_found && !kCacheAllClients) {
        debugMessage(this->server_id(), "Query client groups for client {} on server {}.", cldbid, this->server_id());

        result.reserve(64);
        auto res = sql::command(this->sql_manager(), "SELECT `groupId`, `until` FROM `assignedGroups` WHERE `serverId` = :sid AND `cldbid` = :cldbid AND `channelId` = 0", variable{":sid", this->server_id()}, variable{":cldbid", cldbid})
                .query([&](int length, std::string* value, std::string* column) {
            GroupId group_id{0};
            try {
                for(int index = 0; index < length; index++) {
                    if(column[index] == "groupId") {
                        group_id = std::stoull(value[index]);
                        break;
                    }
                }
            } catch(std::exception& ex) {
                logWarning(this->server_id(), "Invalid data found in group assignment table. Failed to parse group id.");
                return 0;
            }
            if(!group_id) return 0;

            result.push_back(group_id);
            return 0;
        });
        LOG_SQL_CMD(res);
    }

    if(mode == GroupAssignmentCalculateMode::GLOBAL)
        if(auto parent = this->manager_->parent_manager(); parent) {
            auto parent_groups = parent->assignments().server_groups_of_client(mode, cldbid);
            result.reserve(result.size() + parent_groups.size());
            result.insert(result.begin(), parent_groups.begin(), parent_groups.end());
        }

    return result;
}

std::vector<ChannelGroupAssignment> GroupAssignmentManager::exact_channel_groups_of_client(GroupAssignmentCalculateMode mode, ClientDbId cldbid) {
    std::vector<ChannelGroupAssignment> result{};
    bool cache_found{false};
    {
        std::lock_guard cache_lock{*this->client_cache_lock};
        for(auto& entry : this->client_cache) {
            if(entry->client_database_id != cldbid) continue;

            result.reserve(entry->channel_group_assignments.size());
            for(const auto& assignment : entry->channel_group_assignments) {
                result.push_back(ChannelGroupAssignment{
                    .client_database_id = cldbid,
                    .channel_id = assignment->channel_id,
                    .group_id = assignment->group_id,
                });
            }
            cache_found = true;
            break;
        }
    }

    if(!cache_found && !kCacheAllClients) {
        debugMessage(this->server_id(), "Query client groups for client {} on server {}.", cldbid, this->server_id());

        result.reserve(64);
        auto res = sql::command(this->sql_manager(), "SELECT `groupId`, `channelId`, `until` FROM `assignedGroups` WHERE `serverId` = :sid AND `cldbid` = :cldbid AND `channelId` != 0", variable{":sid", this->server_id()}, variable{":cldbid", cldbid})
                .query([&](int length, std::string* value, std::string* column) {
                    GroupId group_id{0};
                    ChannelId channel_id{0};
                    try {
                        for(int index = 0; index < length; index++) {
                            if(column[index] == "groupId") {
                                group_id = std::stoull(value[index]);
                            } else if(column[index] == "channelId") {
                                channel_id = std::stoull(value[index]);
                            }
                        }
                    } catch(std::exception& ex) {
                        logWarning(this->server_id(), "Invalid data found in group assignment table. Failed to parse group or channel id.");
                        return 0;
                    }
                    if(!group_id || !channel_id) return 0;

                    result.push_back(ChannelGroupAssignment{
                            .client_database_id = cldbid,
                            .channel_id = channel_id,
                            .group_id = group_id,
                    });

                    return 0;
                });
        LOG_SQL_CMD(res);
    }

    if(mode == GroupAssignmentCalculateMode::GLOBAL) {
        if(auto parent = this->manager_->parent_manager(); parent) {
            auto parent_groups = parent->assignments().exact_channel_groups_of_client(mode, cldbid);
            result.reserve(result.size() + parent_groups.size());
            result.insert(result.begin(), parent_groups.begin(), parent_groups.end());
        }
    }

    return result;
}

std::optional<ChannelGroupAssignment> GroupAssignmentManager::exact_channel_group_of_client(GroupAssignmentCalculateMode mode,
                                                                                                    ClientDbId client_database_id, ChannelId channel_id) {
    /* TODO: Improve performance by not querying all groups */
    auto assignments = this->exact_channel_groups_of_client(mode, client_database_id);
    for(const auto& assignment : assignments) {
        if(assignment.channel_id != channel_id) {
            continue;
        }

        return std::make_optional(assignment);
    }

    return std::nullopt;
}

std::optional<ts::GroupId> GroupAssignmentManager::calculate_channel_group_of_client(GroupAssignmentCalculateMode mode,
                                                                                 ClientDbId client_database_id,
                                                                                 std::shared_ptr<BasicChannel> &channel) {
    auto assignments = this->exact_channel_groups_of_client(mode, client_database_id);
    while(channel) {
        for(const auto& assignment : assignments) {
            if(assignment.channel_id != channel->channelId()) {
                continue;
            }

            return std::make_optional(assignment.group_id);
        }

        if(permission::v2::permission_granted(1, channel->permissions()->permission_value_flagged(permission::b_channel_group_inheritance_end))) {
            break;
        }

        channel = channel->parent();
    }

    return std::nullopt;
}

std::deque<ServerGroupAssignment> GroupAssignmentManager::server_group_clients(GroupId group_id, bool full_info) {
    std::deque<ServerGroupAssignment> result{};

    if(kCacheAllClients && !full_info) {
        std::lock_guard cache_lock{*this->client_cache_lock};
        for(auto& client : this->client_cache) {
            auto it = std::find_if(client->server_group_assignments.begin(), client->server_group_assignments.end(), [&](const std::unique_ptr<InternalServerGroupAssignment>& assignment) {
                return assignment->group_id == group_id;
            });
            if(it == client->server_group_assignments.end()) {
                continue;
            }
            result.push_back(ServerGroupAssignment{
                .client_database_id = client->client_database_id,
                .group_id = group_id,
            });
        }
    } else {
        if(full_info) {
            constexpr static auto kSqlCommand{
                "SELECT `cldbid`, clients_server.client_unique_id, clients_server.client_nickname FROM `assignedGroups` "
                  "LEFT JOIN `clients_server` ON `assignedGroups`.`serverId` = `clients_server`.`server_id` AND `assignedGroups`.`cldbid` = `clients_server`.`client_database_id` "
                "WHERE `serverId` = :sid AND `channelId` = 0 AND `groupId` = :gid;"
            };

            std::string sql_command{kSqlCommand};
            auto sql = sql::command{this->sql_manager(), kSqlCommand, variable{":sid", this->server_id()}, variable{":gid", group_id}};
            LOG_SQL_CMD(sql.query([&](int length, std::string* values, std::string* names) {
                ServerGroupAssignment assignment{};

                int index{0};
                try {
                    assert(names[index] == "cldbid");
                    assignment.client_database_id = std::stoull(values[index++]);

                    assert(names[index] == "client_unique_id");
                    assignment.client_unique_id = values[index++];

                    assert(names[index] == "client_nickname");
                    assignment.client_display_name = values[index++];

                    assert(index == length);
                } catch (std::exception& ex) {
                    logError(this->server_id(), "Failed to parse client server group assignment at index {}: {}",
                             index - 1,
                             ex.what()
                    );
                    return;
                }

                result.push_back(std::move(assignment));
            }));
        } else {
            auto res = sql::command(this->sql_manager(), "SELECT `cldbid` FROM `assignedGroups` WHERE `serverId` = :sid AND `channelId` = 0 AND `groupId` = :gid", variable{":sid", this->server_id()}, variable{":gid", group_id})
                    .query([&](int length, std::string* value, std::string* column) {
                        ClientDbId cldbid{0};
                        try {
                            for(int index = 0; index < length; index++) {
                                if(column[index] == "cldbid") {
                                    cldbid = std::stoull(value[index]);
                                    break;
                                }
                            }
                        } catch(std::exception& ex) {
                            logWarning(this->server_id(), "Invalid data found in group assignment table. Failed to parse client database id.");
                            return 0;
                        }
                        if(!cldbid) return 0;

                        result.push_back(ServerGroupAssignment{
                                .client_database_id = cldbid,
                                .group_id = group_id,
                        });
                        return 0;
                    });
            LOG_SQL_CMD(res);
        }
    }

    if(auto parent = this->manager_->parent_manager(); parent) {
        auto parent_clients = parent->assignments().server_group_clients(group_id, full_info);
        result.insert(result.begin(), parent_clients.begin(), parent_clients.end());
    }

    return result;
}

std::deque<std::tuple<ts::GroupId, ts::ChannelId, ts::ClientDbId>> GroupAssignmentManager::channel_group_list(
        GroupId group_id,
        ChannelId channel_id,
        ClientDbId client_database_id
) {
    std::string sql_query{};
    sql_query += "SELECT `groupId`, `cldbid`, `channelId` FROM `assignedGroups` WHERE `serverId` = :sid";
    if(group_id > 0) {
        sql_query += " AND `groupId` = :groupId";
    }
    if(channel_id > 0) {
        sql_query += " AND `channelId` = :cid";
    }
    if(client_database_id > 0) {
        sql_query += " AND `cldbid` = :cldbid";
    }
    sql::command sql{this->sql_manager(), sql_query};
    sql.value(":groupId", group_id);
    sql.value(":cid", channel_id);
    sql.value(":cldbid", client_database_id);

    std::deque<std::tuple<ts::GroupId, ts::ChannelId, ts::ClientDbId>> result{};
    LOG_SQL_CMD(sql.query([&](int length, std::string* values, std::string* names) {
        GroupId group_id;
        ChannelId channel_id;
        ClientDbId client_database_id;

        int index{0};
        try {
            assert(names[index] == "groupId");
            group_id = std::stoull(values[index++]);

            assert(names[index] == "cldbid");
            channel_id = std::stoull(values[index++]);

            assert(names[index] == "channelId");
            client_database_id = std::stoull(values[index++]);

            assert(index == length);
        } catch (std::exception& ex) {
            logError(this->server_id(), "Failed to parse client group assignment at index {}: {}",
                     index - 1,
                     ex.what()
            );
            return;
        }

        result.emplace_back(group_id, channel_id, client_database_id);
    }));

    return result;
}

GroupAssignmentResult GroupAssignmentManager::add_server_group(ClientDbId client, GroupId group, bool temporary) {
    bool cache_registered{false};
    {
        std::lock_guard cache_lock{*this->client_cache_lock};
        for(auto& entry : this->client_cache) {
            if(entry->client_database_id != client) continue;

            auto it = std::find_if(entry->server_group_assignments.begin(), entry->server_group_assignments.end(), [&](const std::unique_ptr<InternalServerGroupAssignment>& assignment) {
                return assignment->group_id == group;
            });

            if(it != entry->server_group_assignments.end()) {
                return GroupAssignmentResult::ADD_ALREADY_MEMBER_OF_GROUP;
            }

            entry->server_group_assignments.push_back(std::make_unique<InternalServerGroupAssignment>(group, temporary));
            cache_registered = true;
            break;
        }

        if(!cache_registered && kCacheAllClients) {
            /* add the client to the cache */
            auto cache = std::make_shared<ClientCache>();
            cache->client_database_id = client;
            cache->server_group_assignments.push_back(std::make_unique<InternalServerGroupAssignment>(group, temporary));
            this->client_cache.push_back(std::move(cache));
        }
    }

    if(!temporary) {
        auto command = sql::command(this->sql_manager(), "INSERT INTO `assignedGroups` (`serverId`, `cldbid`, `groupId`, `channelId`, `until`) VALUES (:sid, :cldbid, :gid, :chid, :until)",
                     variable{":sid", this->server_id()},
                     variable{":cldbid", client},
                     variable{":gid", group},
                     variable{":chid", 0},
                     variable{":until", 0});
        if(cache_registered) {
            command.executeLater().waitAndGetLater(LOG_SQL_CMD, {-1, "failed to insert group assignment into the database"});
        } else {
            auto result = command.execute();
            if(!result) return GroupAssignmentResult::ADD_ALREADY_MEMBER_OF_GROUP; //TODO: Parse error from database?
        }
    }

    return GroupAssignmentResult::SUCCESS;
}

GroupAssignmentResult GroupAssignmentManager::remove_server_group(ClientDbId client, GroupId group) {
    bool cache_verified{false};
    {
        std::lock_guard cache_lock{*this->client_cache_lock};
        for(auto& entry : this->client_cache) {
            if(entry->client_database_id != client) continue;

            auto it = std::find_if(entry->server_group_assignments.begin(), entry->server_group_assignments.end(), [&](const std::unique_ptr<InternalServerGroupAssignment>& assignment) {
                return assignment->group_id == group;
            });
            if(it == entry->server_group_assignments.end()) {
                return GroupAssignmentResult::REMOVE_NOT_MEMBER_OF_GROUP;
            }
            entry->server_group_assignments.erase(it);
            cache_verified = true;
            break;
        }

        if(!cache_verified && kCacheAllClients)
            return GroupAssignmentResult::REMOVE_NOT_MEMBER_OF_GROUP;
    }

    {
        auto command = sql::command(this->sql_manager(), "DELETE FROM `assignedGroups` WHERE `serverId` =  :sid AND `cldbid` = :cldbid AND `groupId` = :gid AND `channelId` = :chid",
                     variable{":sid", this->server_id()},
                     variable{":cldbid", client},
                     variable{":gid", group},
                     variable{":chid", 0});
        if(cache_verified)
            command.executeLater().waitAndGetLater(LOG_SQL_CMD, {-1, "failed to remove group assignment from database"});
        else {
            auto result = command.execute();
            if(!result) {
                return GroupAssignmentResult::REMOVE_NOT_MEMBER_OF_GROUP; //TODO: Parse error from database?
            }
        }
    }
    return GroupAssignmentResult::SUCCESS;
}

GroupAssignmentResult GroupAssignmentManager::set_channel_group(ClientDbId client, GroupId group, ChannelId channel_id, bool temporary) {
    bool cache_verified{false};
    {
        std::lock_guard cache_lock{*this->client_cache_lock};
        for(auto& entry : this->client_cache) {
            if(entry->client_database_id != client) {
                continue;
            }

            auto it = std::find_if(entry->channel_group_assignments.begin(), entry->channel_group_assignments.end(), [&](const std::unique_ptr<InternalChannelGroupAssignment>& assignment) {
                return assignment->channel_id == channel_id;
            });
            if(it != entry->channel_group_assignments.end()) {
                if(group) {
                    if((*it)->group_id == group) {
                        return GroupAssignmentResult::SET_ALREADY_MEMBER_OF_GROUP;
                    }

                    (*it)->group_id = group;
                } else {
                    entry->channel_group_assignments.erase(it);
                }
            } else {
                if(group) {
                    entry->channel_group_assignments.emplace_back(std::make_unique<InternalChannelGroupAssignment>(channel_id, group, temporary));
                }
            }
            cache_verified = true;
            break;
        }

        if(!cache_verified && kCacheAllClients) {
            if(group) {
                /* add the client to the cache */
                auto cache = std::make_shared<ClientCache>();
                cache->client_database_id = client;
                cache->channel_group_assignments.emplace_back(std::make_unique<InternalChannelGroupAssignment>(channel_id, group, temporary));
                this->client_cache.push_back(std::move(cache));
            } else {
                return GroupAssignmentResult::SUCCESS;
            }
        }
    }

    if(temporary) {
        return GroupAssignmentResult::SUCCESS;
    }

    sql::command(this->sql_manager(), "DELETE FROM `assignedGroups` WHERE `serverId` =  :sid AND `cldbid` = :cldbid AND `channelId` = :chid",
                 variable{":sid", this->server_id()},
                 variable{":cldbid", client},
                 variable{":chid", channel_id})
            .executeLater().waitAndGetLater(LOG_SQL_CMD, {1, "failed to delete old channel group assignment"});

    if(group) {
        sql::command(this->sql_manager(), "INSERT INTO `assignedGroups` (`serverId`, `cldbid`, `groupId`, `channelId`, `until`) VALUES (:sid, :cldbid, :gid, :chid, :until)",
                     variable{":sid", this->server_id()},
                     variable{":cldbid", client},
                     variable{":gid", group},
                     variable{":chid", channel_id},
                     variable{":until", 0})
                .executeLater().waitAndGetLater(LOG_SQL_CMD, {1, "failed to insert channel group assignment"});
    }
    return GroupAssignmentResult::SUCCESS;
}

void GroupAssignmentManager::cleanup_temporary_channel_assignment(ClientDbId client_dbid, ChannelId channel) {
    std::lock_guard cache_lock{*this->client_cache_lock};
    for(auto& client : this->client_cache) {
        if(client->client_database_id == client_dbid) {
            auto assignment = std::find_if(client->channel_group_assignments.begin(), client->channel_group_assignments.end(), [&](const std::unique_ptr<InternalChannelGroupAssignment>& assignment) {
                return assignment->channel_id == channel;
            });

            if(assignment == client->channel_group_assignments.end()) {
                break;
            }

            if((*assignment)->temporary_assignment) {
                client->channel_group_assignments.erase(assignment);
            }
            break;
        }
    }
}

bool GroupAssignmentManager::is_server_group_empty(GroupId group_id) {
    bool result{true};
    if(kCacheAllClients) {
        std::lock_guard cache_lock{*this->client_cache_lock};
        for(auto& entry : this->client_cache) {
            for(auto& assignment : entry->server_group_assignments) {
                if(assignment->group_id == group_id) {
                    return false;
                }
            }
        }
    } else {
        auto sql = sql::command{this->sql_manager(), "SELECT COUNT(*) FROM `assignedGroups` WHERE `serverId` = :sid AND `groupId` = :gid", variable{":sid", this->server_id()}, variable{":gid", group_id}};
        LOG_SQL_CMD(sql.query([&](int, std::string* values, std::string*) {
            result = std::stoul(values[0]) == 0;
        }));
    }
    return result;
}

bool GroupAssignmentManager::is_channel_group_empty(GroupId group_id) {
    bool result{true};
    if(kCacheAllClients) {
        std::lock_guard cache_lock{*this->client_cache_lock};
        for(auto& entry : this->client_cache) {
            for(auto& assignment : entry->channel_group_assignments) {
                if(assignment->group_id == group_id) {
                    return false;
                }
            }
        }
    } else {
        auto sql = sql::command{this->sql_manager(), "SELECT COUNT(*) FROM `assignedGroups` WHERE `serverId` = :sid AND `groupId` = :gid", variable{":sid", this->server_id()}, variable{":gid", group_id}};
        LOG_SQL_CMD(sql.query([&](int, std::string* values, std::string*) {
            result = std::stoul(values[0]) == 0;
        }));
    }
    return result;
}

void GroupAssignmentManager::handle_channel_deleted(ChannelId channel_id) {
    auto sql = sql::command{this->sql_manager(), "DELETE FROM `assignedGroups` WHERE `serverId` = :sid AND `channelId` = :cid", variable{":sid", this->server_id()}, variable{":cid", channel_id}};
    sql.executeLater().waitAndGetLater(LOG_SQL_CMD, {-1, "failed to delete assignments for deleted channel"});

    std::lock_guard cache_lock{*this->client_cache_lock};
    for(auto& entry : this->client_cache) {
        entry->channel_group_assignments.erase(std::remove_if(entry->channel_group_assignments.begin(), entry->channel_group_assignments.end(), [&](const std::unique_ptr<InternalChannelGroupAssignment>& assignment) {
            return assignment->channel_id == channel_id;
        }), entry->channel_group_assignments.end());
    }
}

void GroupAssignmentManager::handle_server_group_deleted(GroupId group_id) {
    auto sql = sql::command{this->sql_manager(), "DELETE FROM `assignedGroups` WHERE `serverId` = :sid AND `groupId` = :gid", variable{":sid", this->server_id()}, variable{":gid", group_id}};
    sql.executeLater().waitAndGetLater(LOG_SQL_CMD, {-1, "failed to delete assignments for deleted server group"});

    std::lock_guard cache_lock{*this->client_cache_lock};
    for(auto& entry : this->client_cache) {
        entry->server_group_assignments.erase(std::remove_if(entry->server_group_assignments.begin(), entry->server_group_assignments.end(), [&](const std::unique_ptr<InternalServerGroupAssignment>& assignment) {
            return assignment->group_id == group_id;
        }), entry->server_group_assignments.end());
    }
}

void GroupAssignmentManager::handle_channel_group_deleted(GroupId group_id) {
    auto sql = sql::command{this->sql_manager(), "DELETE FROM `assignedGroups` WHERE `serverId` = :sid AND `groupId` = :gid", variable{":sid", this->server_id()}, variable{":gid", group_id}};
    sql.executeLater().waitAndGetLater(LOG_SQL_CMD, {-1, "failed to delete assignments for deleted channel group"});

    std::lock_guard cache_lock{*this->client_cache_lock};
    for(auto& entry : this->client_cache) {
        entry->channel_group_assignments.erase(std::remove_if(entry->channel_group_assignments.begin(), entry->channel_group_assignments.end(), [&](const std::unique_ptr<InternalChannelGroupAssignment>& assignment) {
            return assignment->group_id == group_id;
        }), entry->channel_group_assignments.end());
    }
}

void GroupAssignmentManager::reset_all() {
    auto sql = sql::command{this->sql_manager(), "DELETE FROM `assignedGroups` WHERE `serverId` = :sid", variable{":sid", this->server_id()}};
    sql.executeLater().waitAndGetLater(LOG_SQL_CMD, {-1, "failed to delete all assignments"});

    {
        std::lock_guard cache_lock{*this->client_cache_lock};
        this->client_cache.clear();
    }
}

std::shared_ptr<TemporaryAssignmentsLock> GroupAssignmentManager::create_tmp_assignment_lock(ClientDbId cldbid) {
    std::shared_ptr<ClientCache> cache{};

    std::lock_guard cache_lock{*this->client_cache_lock};
    for(const auto& entry : this->client_cache) {
        if(entry->client_database_id == cldbid) {
            cache = entry;
            break;
        }
    }
    if(!cache) {
        cache = std::make_shared<ClientCache>();
        cache->client_database_id = cldbid;
        this->client_cache.push_back(cache);
    }

    auto cache_mutex = this->client_cache_lock;
    std::shared_ptr<char> temp_assignment_lock{new char{}, [cache, cache_mutex](void* buffer) {
        delete (char*) buffer;

        std::lock_guard cache_lock{*cache_mutex};
        cache->server_group_assignments.erase(std::remove_if(cache->server_group_assignments.begin(), cache->server_group_assignments.end(), [](const std::unique_ptr<InternalServerGroupAssignment>& assignment){
            return assignment->temporary_assignment;
        }), cache->server_group_assignments.end());

        cache->channel_group_assignments.erase(std::remove_if(cache->channel_group_assignments.begin(), cache->channel_group_assignments.end(), [](const std::unique_ptr<InternalChannelGroupAssignment>& assignment){
            return assignment->temporary_assignment;
        }), cache->channel_group_assignments.end());
    }};

    cache->temp_assignment_lock = temp_assignment_lock;
    return temp_assignment_lock;
}