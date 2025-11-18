//
// Created by WolverinDEV on 11/04/2020.
//
#include "./snapshot.h"
#include "./server.h"
#include "./channel.h"
#include "./permission.h"
#include "./client.h"
#include "./groups.h"
#include "./music.h"
#include "./snapshot_data.h"
#include "../VirtualServerManager.h"
#include "../InstanceHandler.h"
#include <sql/insert.h>
#include <log/LogUtils.h>
#include <src/groups/Group.h>

using namespace ts;
using namespace ts::server;

constexpr static ServerId kSnapshotServerId{0xFFFF};
VirtualServerManager::SnapshotDeployResult VirtualServerManager::deploy_snapshot(std::string &error, std::shared_ptr<VirtualServer>& server, const command_parser &command) {
    if(!server) {
        auto instance_count = this->serverInstances().size();
        if(config::server::max_virtual_server != -1 && instance_count > config::server::max_virtual_server) {
            return SnapshotDeployResult::REACHED_CONFIG_SERVER_LIMIT;
        }

        if(instance_count >= 65534) {
            return SnapshotDeployResult::REACHED_SOFTWARE_SERVER_LIMIT;
        }
    }

    bool success{true};
    auto target_server_id = server ? server->getServerId() : this->next_available_server_id(success);
    if(!success) {
        return SnapshotDeployResult::REACHED_SERVER_ID_LIMIT;
    }

    std::string server_host{server ? server->properties()[property::VIRTUALSERVER_HOST].value() : config::binding::DefaultVoiceHost};
    uint16_t server_port{server ? server->properties()[property::VIRTUALSERVER_PORT].as_unchecked<uint16_t>() : this->next_available_port(server_host)};

    this->delete_server_in_db(kSnapshotServerId, false);
    auto result = sql::command{this->handle->getSql(), "INSERT INTO `servers` (`serverId`, `host`, `port`) VALUES (:sid, :host, :port)"}
            .value(":sid", kSnapshotServerId)
            .value(":host", server_host)
            .value(":port", server_port)
            .execute();

    if(!result) {
        error = "failed to register the server (" + result.fmtStr() + ")";
        return SnapshotDeployResult::CUSTOM_ERROR;
    }

    if(!this->try_deploy_snapshot(error, kSnapshotServerId, target_server_id, command)) {
        return SnapshotDeployResult::CUSTOM_ERROR;
    }

    if(!server) {
        this->delete_server_in_db(target_server_id, false);
        this->change_server_id_in_db(kSnapshotServerId, target_server_id);
    } else {
        this->deleteServer(server);
        this->change_server_id_in_db(kSnapshotServerId, target_server_id);
    }

    server = std::make_shared<VirtualServer>(target_server_id, this->handle->getSql());
    server->self = server;
    if(!server->initialize(true)) {
        //FIXME error handling
    }
    server->properties()[property::VIRTUALSERVER_HOST] = server_host;
    server->properties()[property::VIRTUALSERVER_PORT] = server_port;

    server->properties()[property::VIRTUALSERVER_ASK_FOR_PRIVILEGEKEY] = false;
    {
        threads::MutexLock l(this->instanceLock);
        this->instances.push_back(server);
    }

    if(!server->start(error)) {
        logWarning(server->getServerId(), "Failed to auto start server after snapshot deployment: {}", error);
        error = "";
    }
    return SnapshotDeployResult::SUCCESS;
}

bool VirtualServerManager::try_deploy_snapshot(std::string &error, ts::ServerId target_server_id, ts::ServerId logging_server_id, const ts::command_parser &command) {
    snapshots::snapshot_data snapshot_data{};

    if(!snapshots::parse_snapshot(snapshot_data, error, logging_server_id, command))
        return false;

    snapshots::snapshot_mappings mappings{};

    /* register clients */
    {
        sql::InsertQuery insert_general_query{"clients",
                 sql::Column<std::string>("client_unique_id"),
                 sql::Column<int64_t>("client_created")
        };

        auto current_time_seconds = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        for(const auto& client : snapshot_data.parsed_clients) {
            insert_general_query.add_entry(
                    client.unique_id,
                    std::chrono::floor<std::chrono::seconds>(client.timestamp_created.time_since_epoch()).count()
            );
        }

        auto result = insert_general_query.execute(this->handle->getSql(), true);
        for(const auto& fail : result.failed_entries)
            logWarning(logging_server_id, "Failed to insert client {} into the general client database: {} (This should not happen!)", snapshot_data.parsed_clients[std::get<0>(fail)].database_id, std::get<1>(fail).fmtStr());


        sql::model insert_command{this->handle->getSql(),
                                 "INSERT INTO `clients_server` (`server_id`, `client_unique_id`, `client_database_id`, `client_nickname`, `client_created`, `original_client_id`) SELECT :serverId, :uniqueId, `client_database_id`, :nickname, :timestamp, :org_id FROM `clients` WHERE `client_unique_id` = :uniqueId;"};

        insert_command.value(":serverId", target_server_id);

        for(const auto& client : snapshot_data.parsed_clients) {
            auto sqlResult = insert_command.command().values(
                variable{":uniqueId", client.unique_id},
                variable{":timestamp", std::chrono::floor<std::chrono::seconds>(client.timestamp_created.time_since_epoch()).count()},
                variable{":org_id", client.database_id},
                variable{":nickname", client.nickname}
            ).execute();

            if(!sqlResult)
                logWarning(logging_server_id, "Failed to insert client {} ({}) into the database: {}", client.unique_id, client.nickname, sqlResult.fmtStr());
        }

        for(const auto& client : snapshot_data.music_bots) {
            auto sqlResult = insert_command.command().values(
                    variable{":uniqueId", client.unique_id},
                    variable{":timestamp", current_time_seconds},
                    variable{":org_id", client.database_id}
            ).execute();

            if(!sqlResult)
                logWarning(logging_server_id, "Failed to insert music bot {} into the database: {}", client.unique_id, sqlResult.fmtStr());
        }

        auto map_query_result = sql::command{this->handle->getSql(), "SELECT `original_client_id`,`client_database_id` FROM `clients_server` WHERE `server_id` = :serverId;"}
                .value(":serverId", target_server_id)
                .query([&](int length, std::string* values, std::string* names) {
                    ClientDbId original_id, new_id;
                    try {
                        original_id = std::stoull(values[0]);
                        new_id = std::stoull(values[1]);
                    } catch (std::exception& ex) {
                        logWarning(logging_server_id, "Failed to parse client database entry mapping for group id {} (New ID: {})", values[1], values[0]);
                        return;
                    }
                    mappings.client_id[original_id] = new_id;
                });
        if(!map_query_result) {
            error = "failed to query client database id mappings (" + map_query_result.fmtStr() + ")";
            return false;
        }

        /* the descriptions */
        {
            sql::InsertQuery insert_properties_query{"properties",
                                                  sql::Column<ServerId>("serverId"),
                                                  sql::Column<property::PropertyType>("type"),
                                                  sql::Column<ClientDbId>("id"),
                                                  sql::Column<std::string>("key"),
                                                  sql::Column<std::string>("value")
            };


            std::string description_property{property::describe(property::CLIENT_DESCRIPTION).name};
            for(const auto& client : snapshot_data.parsed_clients) {
                if(client.description.empty()) {
                    continue;
                }

                auto new_id = mappings.client_id.find(client.database_id);
                if(new_id == mappings.client_id.end()) {
                    continue;
                }

                insert_properties_query.add_entry(
                        target_server_id,
                        property::PROP_TYPE_CLIENT,
                        new_id->second,
                        description_property,
                        client.description
                );
            }

            auto presult = insert_properties_query.execute(this->handle->getSql(), true);
            if(!presult.has_succeeded()) {
                logWarning(logging_server_id, "Failed to insert some client properties into the database ({} failed)", presult.failed_entries.size());
            }
        }
    }

    /* channels */
    {
        /* Assign each channel a new id */
        ChannelId current_id{1};
        for(auto& channel : snapshot_data.parsed_channels) {
            const auto new_id = current_id++;
            mappings.channel_id[channel.properties[property::CHANNEL_ID]] = new_id;
            channel.properties[property::CHANNEL_ID] = new_id;
        }

        /* Update channel parents and order id */
        for(auto& channel : snapshot_data.parsed_channels) {
            {
                auto pid = channel.properties[property::CHANNEL_PID].as_unchecked<ChannelId>();
                if(pid > 0) {
                    auto new_id = mappings.channel_id.find(pid);
                    if(new_id == mappings.channel_id.end()) {
                        error = "failed to remap channel parent id for channel \"" + channel.properties[property::CHANNEL_NAME].value() + "\" (snapshot/channel tree broken?)";
                        return false;
                    }
                    channel.properties[property::CHANNEL_PID] = new_id->second;
                }
            }

            {
                auto oid = channel.properties[property::CHANNEL_ORDER].as_unchecked<ChannelId>();
                if(oid > 0) {
                    auto new_id = mappings.channel_id.find(oid);
                    if(new_id == mappings.channel_id.end()) {
                        error = "failed to remap channel order id for channel \"" + channel.properties[property::CHANNEL_NAME].value() + "\" (snapshot/channel tree broken?)";
                        return false;
                    }
                    channel.properties[property::CHANNEL_ORDER] = new_id->second;
                }
            }
        }


        sql::InsertQuery insert_query{"channels",
            sql::Column<ServerId>("serverId"),
            sql::Column<ChannelId>("channelId"),
            sql::Column<ChannelId>("parentId")
        };

        sql::InsertQuery insert_property_query{"properties",
            sql::Column<ServerId>("serverId"),
            sql::Column<property::PropertyType>("type"),
            sql::Column<uint64_t>("id"),
            sql::Column<std::string>("key"),
            sql::Column<std::string>("value")
        };

        for(auto& channel : snapshot_data.parsed_channels) {
            auto channel_id = channel.properties[property::CHANNEL_ID].as_unchecked<ChannelId>();
            insert_query.add_entry(
                    target_server_id,
                    channel_id,
                    channel.properties[property::CHANNEL_PID].as_unchecked<ChannelId>()
            );

            for(const auto& property : channel.properties.list_properties(property::FLAG_SAVE)) {
                if(!property.isModified()) continue;

                insert_property_query.add_entry(
                    target_server_id,
                    property::PROP_TYPE_CHANNEL,
                    channel_id,
                    std::string{property.type().name},
                    property.value()
                );
            }
        }

        {
            auto result = insert_query.execute(this->handle->getSql(), true);
            for(const auto& fail : result.failed_entries)
                logWarning(logging_server_id, "Failed to insert channel {} into the server database: {}", snapshot_data.parsed_channels[std::get<0>(fail)].properties[property::CHANNEL_NAME].value(), std::get<1>(fail).fmtStr());
        }

        {
            auto result = insert_property_query.execute(this->handle->getSql(), true);
            if(!result.failed_entries.empty())
                logWarning(logging_server_id, "Failed to insert some channel properties into the database. Failed property count: {}", result.failed_entries.size());
        }
    }

    /* channel permissions */
    {
        sql::InsertQuery insert_query{"permissions",
            sql::Column<ServerId>("serverId"),
            sql::Column<permission::PermissionSqlType>("type"),
            sql::Column<uint64_t>("id"),
            sql::Column<ChannelId>("channelId"),
            sql::Column<std::string>("permId"),

            sql::Column<permission::PermissionValue>("value"),
            sql::Column<permission::PermissionValue>("grant"),
            sql::Column<bool>("flag_skip"),
            sql::Column<bool>("flag_negate")
        };

        for(auto& entry : snapshot_data.channel_permissions) {
            {
                auto new_id = mappings.channel_id.find(entry.id1);
                if(new_id == mappings.channel_id.end()) {
                    logWarning(logging_server_id, "Missing channel id mapping for channel permission entry (channel id: {}). Skipping permission insert.", entry.id1);
                    continue;
                }
                entry.id1 = new_id->second;
            }

            for(const auto& permission : entry.permissions) {
                insert_query.add_entry(
                        target_server_id,
                        permission::SQL_PERM_CHANNEL,
                        0,
                        entry.id1,
                        permission.type->name,
                        permission.value.has_value ? permission.value.value : permNotGranted,
                        permission.granted.has_value ? permission.granted.value : permNotGranted,
                        permission.flag_skip,
                        permission.flag_negate
                );
            }
        }

        auto result = insert_query.execute(this->handle->getSql(), true);
        if(!result.failed_entries.empty())
            logWarning(logging_server_id, "Failed to insert all channel permissions into the database. Failed permission count: {}", result.failed_entries.size());
    }

    /* server groups */
    {
        sql::model insert_model{this->handle->getSql(), "INSERT INTO `groups` (`serverId`, `target`, `type`, `displayName`, `original_group_id`) VALUES (:serverId, :target, :type, :name, :id)"};
        insert_model.value(":serverId", target_server_id).value(":target", GroupTarget::GROUPTARGET_SERVER).value(":type", groups::GroupType::GROUP_TYPE_NORMAL);

        for(auto& group : snapshot_data.parsed_server_groups) {
            auto result = insert_model.command().value(":name", group.name).value(":id", group.group_id).execute();
            if(!result)
                logWarning(logging_server_id, "Failed to insert server group \"{}\" into the database: {}", group.name, result.fmtStr());
        }

        sql::command{this->handle->getSql(), "SELECT `original_group_id`,`groupId` FROM `groups` WHERE `serverId` = :serverId AND `target` = :target AND `type` = :type"}
                .value(":serverId", target_server_id).value(":target", GroupTarget::GROUPTARGET_SERVER).value(":type", groups::GroupType::GROUP_TYPE_NORMAL)
                .query([&](int length, std::string* values, std::string* names) {
                    GroupId original_id, new_id;
                    try {
                        original_id = std::stoull(values[0]);
                        new_id = std::stoull(values[1]);
                    } catch (std::exception& ex) {
                        logWarning(logging_server_id, "Failed to parse server group mapping for group id {} (New ID: {})", values[1], values[0]);
                        return;
                    }
                    mappings.server_group_id[original_id] = new_id;
                });
    }

    /* server group relations */
    {
        sql::InsertQuery insert_query{"assignedGroups",
                                      sql::Column<ServerId>("serverId"),
                                      sql::Column<ClientDbId>("cldbid"),
                                      sql::Column<GroupId>("groupId"),
                                      sql::Column<ChannelId>("channelId")
        };

        for(auto& relation : snapshot_data.parsed_server_group_relations) {
            for(auto& entry : relation.second) {
                ClientId client_id{};
                {
                    auto new_id = mappings.client_id.find(entry.client_id);
                    if(new_id == mappings.client_id.end()) {
                        logWarning(logging_server_id, "Missing client id mapping for channel group relation permission entry (client id: {}). Skipping relation insert.", entry.client_id);
                        continue;
                    }
                    client_id = new_id->second;
                }

                GroupId group_id;
                {
                    auto new_id = mappings.server_group_id.find(entry.group_id);
                    if(new_id == mappings.server_group_id.end()) {
                        logWarning(logging_server_id, "Missing server group mapping of group {} for client {}. Skipping relation insert.", entry.group_id, entry.client_id);
                        continue;
                    }
                    group_id = new_id->second;
                }

                insert_query.add_entry(
                        target_server_id,
                        client_id,
                        group_id,
                        0
                );
            }
        }

        auto result = insert_query.execute(this->handle->getSql(), true);
        if(!result.failed_entries.empty())
            logWarning(logging_server_id, "Failed to insert all server group relations into the database. Failed insert count: {}", result.failed_entries.size());
    }

    /* server group permissions */
    {
        sql::InsertQuery insert_query{"permissions",
                                      sql::Column<ServerId>("serverId"),
                                      sql::Column<permission::PermissionSqlType>("type"),
                                      sql::Column<uint64_t>("id"),
                                      sql::Column<ChannelId>("channelId"),
                                      sql::Column<std::string>("permId"),

                                      sql::Column<permission::PermissionValue>("value"),
                                      sql::Column<permission::PermissionValue>("grant"),
                                      sql::Column<bool>("flag_skip"),
                                      sql::Column<bool>("flag_negate"),
        };

        for(auto& group : snapshot_data.parsed_server_groups) {
            GroupId group_id;
            {
                auto new_id = mappings.server_group_id.find(group.group_id);
                if(new_id == mappings.server_group_id.end()) {
                    logWarning(logging_server_id, "Missing server group mapping of group {}. Skipping permission insert.", group.group_id);
                    continue;
                }
                group_id = new_id->second;
            }

            for(const auto& permission : group.permissions) {
                insert_query.add_entry(
                        target_server_id,
                        permission::SQL_PERM_GROUP,
                        group_id,
                        0,
                        permission.type->name,
                        permission.value.has_value ? permission.value.value : permNotGranted,
                        permission.granted.has_value ? permission.granted.value : permNotGranted,
                        permission.flag_skip,
                        permission.flag_negate
                );
            }
        }

        auto result = insert_query.execute(this->handle->getSql(), true);
        if(!result.failed_entries.empty())
            logWarning(logging_server_id, "Failed to insert some channel group permissions into the database. Failed permission count: {}", result.failed_entries.size());
    }

    /* channel groups */
    {
        sql::model insert_model{this->handle->getSql(), "INSERT INTO `groups` (`serverId`, `target`, `type`, `displayName`, `original_group_id`) VALUES (:serverId, :target, :type, :name, :id)"};
        insert_model.value(":serverId", target_server_id).value(":target", GroupTarget::GROUPTARGET_CHANNEL).value(":type", groups::GroupType::GROUP_TYPE_NORMAL);

        for(auto& group : snapshot_data.parsed_channel_groups) {
            auto result = insert_model.command().value(":name", group.name).value(":id", group.group_id).execute();
            if(!result)
                logWarning(logging_server_id, "Failed to insert channel group \"{}\" into the database: {}", group.name, result.fmtStr());
        }

        sql::command{this->handle->getSql(), "SELECT `original_group_id`,`groupId` FROM `groups` WHERE `serverId` = :serverId AND `target` = :target AND `type` = :type"}
                .value(":serverId", target_server_id).value(":target", GroupTarget::GROUPTARGET_CHANNEL).value(":type", groups::GroupType::GROUP_TYPE_NORMAL)
                .query([&](int length, std::string* values, std::string* names) {
                    GroupId original_id, new_id;
                    try {
                        original_id = std::stoull(values[0]);
                        new_id = std::stoull(values[1]);
                    } catch (std::exception& ex) {
                        logWarning(logging_server_id, "Failed to parse channel group mapping for group id {} (New ID: {})", values[1], values[0]);
                        return;
                    }
                    mappings.channel_group_id[original_id] = new_id;
                });
    }

    /* channel group relations */
    {
        sql::InsertQuery insert_query{"assignedGroups",
              sql::Column<ServerId>("serverId"),
              sql::Column<ClientDbId>("cldbid"),
              sql::Column<GroupId>("groupId"),
              sql::Column<ChannelId>("channelId")
        };

        for(auto& relation : snapshot_data.parsed_channel_group_relations) {
            ChannelId channel_id;
            {
                auto new_id = mappings.channel_id.find(relation.first);
                if(new_id == mappings.channel_id.end()) {
                    logWarning(logging_server_id, "Missing channel id mapping for channel group relation entry (channel id: {}). Skipping relation insert.", relation.first);
                    continue;
                }
                channel_id = new_id->second;
            }

            for(auto& entry : relation.second) {
                ClientId client_id;
                {
                    auto new_id = mappings.client_id.find(entry.client_id);
                    if(new_id == mappings.client_id.end()) {
                        logWarning(logging_server_id, "Missing client id mapping for channel group relation permission entry (client id: {}, channel id: {}). Skipping relation insert.", entry.client_id, relation.first);
                        continue;
                    }
                    client_id = new_id->second;
                }

                GroupId group_id;
                {
                    auto new_id = mappings.channel_group_id.find(entry.group_id);
                    if(new_id == mappings.channel_group_id.end()) {
                        logWarning(logging_server_id, "Missing channel group mapping of group {} for client {} for channel {}. Skipping relation insert.", entry.group_id, entry.client_id, channel_id);
                        continue;
                    }
                    group_id = new_id->second;
                }

                insert_query.add_entry(
                        target_server_id,
                        client_id,
                        group_id,
                        channel_id
                );
            }
        }

        auto result = insert_query.execute(this->handle->getSql(), true);
        if(!result.failed_entries.empty())
            logWarning(logging_server_id, "Failed to insert all channel group relations into the database. Failed insert count: {}", result.failed_entries.size());
    }

    /* channel group permissions */
    {
        sql::InsertQuery insert_query{"permissions",
                                      sql::Column<ServerId>("serverId"),
                                      sql::Column<permission::PermissionSqlType>("type"),
                                      sql::Column<uint64_t>("id"),
                                      sql::Column<ChannelId>("channelId"),
                                      sql::Column<std::string>("permId"),

                                      sql::Column<permission::PermissionValue>("value"),
                                      sql::Column<permission::PermissionValue>("grant"),
                                      sql::Column<bool>("flag_skip"),
                                      sql::Column<bool>("flag_negate"),
        };

        for(auto& group : snapshot_data.parsed_channel_groups) {
            GroupId group_id;
            {
                auto new_id = mappings.channel_group_id.find(group.group_id);
                if(new_id == mappings.channel_group_id.end()) {
                    logWarning(logging_server_id, "Missing channel group mapping of group {}. Skipping permission insert.", group.group_id);
                    continue;
                }
                group_id = new_id->second;
            }

            for(const auto& permission : group.permissions) {
                insert_query.add_entry(
                        target_server_id,
                        permission::SQL_PERM_GROUP,
                        group_id,
                        0,
                        permission.type->name,
                        permission.value.has_value ? permission.value.value : permNotGranted,
                        permission.granted.has_value ? permission.granted.value : permNotGranted,
                        permission.flag_skip,
                        permission.flag_negate
                );
            }
        }

        auto result = insert_query.execute(this->handle->getSql(), true);
        if(!result.failed_entries.empty())
            logWarning(logging_server_id, "Failed to insert some channel group permissions into the database. Failed permission count: {}", result.failed_entries.size());
    }

    /* register server & properties */
    {

        sql::InsertQuery insert_property_query{"properties",
                                               sql::Column<ServerId>("serverId"),
                                               sql::Column<property::PropertyType>("type"),
                                               sql::Column<uint64_t>("id"),
                                               sql::Column<std::string>("key"),
                                               sql::Column<std::string>("value")
        };

        for(const auto& property : snapshot_data.parsed_server.properties.list_properties(property::FLAG_SAVE)) {
            if(!property.isModified()) continue;

            auto value = property.value();
            switch(property.type().property_index) {
                case property::VIRTUALSERVER_DEFAULT_SERVER_GROUP: {
                    auto new_id = mappings.server_group_id.find(property.as_unchecked<GroupId>());
                    if(new_id == mappings.server_group_id.end()) {
                        logWarning(logging_server_id, "Missing server group mapping for group {}. {} will reference an invalid group.", property.value(), property.type().name);
                        break;
                    }

                    value = std::to_string(new_id->second);
                    break;
                }

                case property::VIRTUALSERVER_DEFAULT_CHANNEL_ADMIN_GROUP:
                case property::VIRTUALSERVER_DEFAULT_CHANNEL_GROUP: {
                    auto new_id = mappings.channel_group_id.find(property.as_unchecked<GroupId>());
                    if(new_id == mappings.channel_group_id.end()) {
                        logWarning(logging_server_id, "Missing channel group mapping for group {}. {} will reference an invalid group.", property.value(), property.type().name);
                        break;
                    }

                    value = std::to_string(new_id->second);
                    break;
                }

                default:
                    break;
            }

            insert_property_query.add_entry(
                    target_server_id,
                    property::PROP_TYPE_SERVER,
                    0,
                    std::string{property.type().name},
                    value
            );
        }

        {
            auto result = insert_property_query.execute(this->handle->getSql(), true);
            if(!result.failed_entries.empty())
                logWarning(logging_server_id, "Failed to insert all server properties into the database. Failed property count: {}", result.failed_entries.size());
        }
    }

    /* client permissions */
    {
        sql::InsertQuery insert_query{"permissions",
              sql::Column<ServerId>("serverId"),
              sql::Column<permission::PermissionSqlType>("type"),
              sql::Column<uint64_t>("id"),
              sql::Column<ChannelId>("channelId"),
              sql::Column<std::string>("permId"),

              sql::Column<permission::PermissionValue>("value"),
              sql::Column<permission::PermissionValue>("grant"),
              sql::Column<bool>("flag_skip"),
              sql::Column<bool>("flag_negate"),
        };

        for(auto& entry : snapshot_data.client_permissions) {
            {
                auto new_id = mappings.client_id.find(entry.id1);
                if(new_id == mappings.client_id.end()) {
                    logWarning(logging_server_id, "Missing client id mapping for client permission entry (client id: {}). Skipping permission insert.", entry.id1);
                    continue;
                }
                entry.id1 = new_id->second;
            }

            for(const auto& permission : entry.permissions) {
                insert_query.add_entry(
                        target_server_id,
                        permission::SQL_PERM_USER,
                        entry.id1,
                        0,
                        permission.type->name,
                        permission.value.has_value ? permission.value.value : permNotGranted,
                        permission.granted.has_value ? permission.granted.value : permNotGranted,
                        permission.flag_skip,
                        permission.flag_negate
                );
            }
        }

        auto result = insert_query.execute(this->handle->getSql(), true);
        if(!result.failed_entries.empty())
            logWarning(logging_server_id, "Failed to insert all client permissions into the database. Failed permission count: {}", result.failed_entries.size());
    }

    /* client channel permissions */
    {
        sql::InsertQuery insert_query{"permissions",
            sql::Column<ServerId>("serverId"),
            sql::Column<permission::PermissionSqlType>("type"),
            sql::Column<uint64_t>("id"),
            sql::Column<ChannelId>("channelId"),
            sql::Column<std::string>("permId"),

            sql::Column<permission::PermissionValue>("value"),
            sql::Column<permission::PermissionValue>("grant"),
            sql::Column<bool>("flag_skip"),
            sql::Column<bool>("flag_negate"),
        };

        for(auto& entry : snapshot_data.client_channel_permissions) {
            {
                auto new_id = mappings.channel_id.find(entry.id1);
                if(new_id == mappings.channel_id.end()) {
                    logWarning(logging_server_id, "Missing channel id mapping for client channel permission entry (client id: {}, channel id: {}). Skipping permission insert.", entry.id2, entry.id1);
                    continue;
                }
                entry.id1 = new_id->second;
            }

            {
                auto new_id = mappings.client_id.find(entry.id2);
                if(new_id == mappings.client_id.end()) {
                    logWarning(logging_server_id, "Missing client id mapping for client channel permission entry (client id: {}, channel id: {}). Skipping permission insert.", entry.id2, entry.id1);
                    continue;
                }
                entry.id2 = new_id->second;
            }

            for(const auto& permission : entry.permissions) {
                insert_query.add_entry(
                    target_server_id,
                    permission::SQL_PERM_USER,
                    entry.id2,
                    (ChannelId) entry.id1,
                    permission.type->name,
                    permission.value.has_value ? permission.value.value : permNotGranted,
                    permission.granted.has_value ? permission.granted.value : permNotGranted,
                    permission.flag_skip,
                    permission.flag_negate
                );
            }
        }

        auto result = insert_query.execute(this->handle->getSql(), true);
        if(!result.failed_entries.empty())
            logWarning(logging_server_id, "Failed to insert all client channel permissions into the database. Failed permission count: {}", result.failed_entries.size());
    }

    /* music bots */
    {
        sql::InsertQuery insert_query{"musicbots",
                                      sql::Column<ServerId>("serverId"),
                                      sql::Column<ClientDbId>("botId"),
                                      sql::Column<std::string>("uniqueId"),
                                      sql::Column<ClientDbId>("owner"),
        };

        for(const auto& client : snapshot_data.music_bots) {
            ClientDbId bot_id;
            {
                auto new_id = mappings.client_id.find(client.database_id);
                if(new_id == mappings.client_id.end()) {
                    logWarning(logging_server_id, "Missing music bot database id mapping for bot {}. Skipping bot register.", client.unique_id);
                    continue;
                }

                bot_id = new_id->second;
            }

            ClientDbId bot_owner_id;
            {
                auto new_id = mappings.client_id.find(client.bot_owner_id);
                if(new_id == mappings.client_id.end()) {
                    logWarning(logging_server_id, "Missing music bots owner database id mapping for bot {}. Resetting it to zero.", client.unique_id);
                    bot_owner_id = 0;
                } else {
                    bot_owner_id = new_id->second;
                }
            }

            insert_query.add_entry(
                target_server_id,
                bot_id,
                client.unique_id,
                bot_owner_id
            );
        }

        {
            auto result = insert_query.execute(this->handle->getSql(), true);
            if(!result.failed_entries.empty())
                logWarning(logging_server_id, "Failed to insert some music bots into the database. Failed bot count: {}", result.failed_entries.size());
        }
    }

    /* music bot attributes */
    {

        sql::InsertQuery insert_property_query{"properties",
               sql::Column<ServerId>("serverId"),
               sql::Column<property::PropertyType>("type"),
               sql::Column<uint64_t>("id"),
               sql::Column<std::string>("key"),
               sql::Column<std::string>("value")
        };

        for(auto& bot : snapshot_data.music_bots) {
            auto new_id = mappings.client_id.find(bot.database_id);
            if(new_id == mappings.client_id.end()) {
                logWarning(logging_server_id, "Missing client id mapping for music bot (Unique ID: {}). Skipping permission insert.", bot.unique_id);
                continue;
            }

            for(const auto& property : bot.properties.list_properties(property::FLAG_SAVE | property::FLAG_SAVE_MUSIC)) {
                if(!property.isModified()) continue;

                auto value = property.value();
                if(property.type() == property::CLIENT_LAST_CHANNEL) {
                    auto channel_id = property.as_unchecked<ChannelId>();

                    auto new_channel_id = mappings.channel_id.find(channel_id);
                    if(new_channel_id == mappings.channel_id.end()) {
                        logWarning(logging_server_id, "Missing channel id mapping for channel {} for music bot {}.", channel_id, bot.unique_id);
                        continue;
                    }
                    value = std::to_string(new_channel_id->second);
                }
                insert_property_query.add_entry(
                        target_server_id,
                        property::PROP_TYPE_CLIENT,
                        new_id->second,
                        std::string{property.type().name},
                        value
                );
            }
        }

        auto result = insert_property_query.execute(this->handle->getSql(), true);
        if(!result.failed_entries.empty())
            logWarning(logging_server_id, "Failed to insert some music bot  properties into the database. Failed property count: {}", result.failed_entries.size());
    }

    /* playlists */
    {

        sql::InsertQuery insert_playlist_query{"playlists",
                                               sql::Column<ServerId>("serverId"),
                                               sql::Column<PlaylistId>("playlist_id")
        };

        sql::InsertQuery insert_property_query{"properties",
                                               sql::Column<ServerId>("serverId"),
                                               sql::Column<property::PropertyType>("type"),
                                               sql::Column<uint64_t>("id"),
                                               sql::Column<std::string>("key"),
                                               sql::Column<std::string>("value")
        };

        for(auto& playlist : snapshot_data.playlists) {
            insert_playlist_query.add_entry(target_server_id, playlist.playlist_id);

            for(const auto& property : playlist.properties.list_properties(property::FLAG_SAVE | property::FLAG_SAVE_MUSIC)) {
                if(!property.isModified()) continue;

                insert_property_query.add_entry(
                        target_server_id,
                        property::PROP_TYPE_PLAYLIST,
                        playlist.playlist_id,
                        std::string{property.type().name},
                        property.value()
                );
            }
        }

        {
            auto result = insert_playlist_query.execute(this->handle->getSql(), true);
            if(!result.failed_entries.empty())
                logWarning(logging_server_id, "Failed to insert some playlists into the database. Failed playlist count: {}", result.failed_entries.size());
        }

        {
            auto result = insert_property_query.execute(this->handle->getSql(), true);
            if(!result.failed_entries.empty())
                logWarning(logging_server_id, "Failed to insert some playlist properties into the database. Failed playlist count: {}", result.failed_entries.size());
        }
    }

    /* playlist songs */
    {
        sql::InsertQuery insert_song_query{"playlist_songs",
                                           sql::Column<ServerId>("serverId"),
                                           sql::Column<PlaylistId>("playlist_id"),
                                           sql::Column<SongId>("song_id"),
                                           sql::Column<SongId>("order_id"),
                                           sql::Column<ClientDbId>("invoker_dbid"),
                                           sql::Column<std::string>("url"),
                                           sql::Column<std::string>("url_loader"),
                                           sql::Column<bool>("loaded"),
                                           sql::Column<std::string>("metadata")
        };

        for(auto& song : snapshot_data.playlist_songs) {
            {
                auto new_id = mappings.client_id.find(song.invoker_id);
                if(new_id == mappings.client_id.end()) {
                    logWarning(logging_server_id, "Missing client id mapping for playlist song invoker (Invoker ID: {}). Changing it to zero.", song.invoker_id);
                    song.invoker_id = 0;
                } else {
                    song.invoker_id = new_id->second;
                }
            }

            insert_song_query.add_entry(
                    target_server_id,
                    song.playlist_id,
                    song.song_id,
                    song.order_id,
                    song.invoker_id,
                    song.url,
                    song.loader,
                    song.loaded,
                    song.metadata
            );
        }

        {
            auto result = insert_song_query.execute(this->handle->getSql(), true);
            if(!result.failed_entries.empty())
                logWarning(logging_server_id, "Failed to insert some playlist songs into the database. Failed song count: {}", result.failed_entries.size());
        }
    }

    return true;
}