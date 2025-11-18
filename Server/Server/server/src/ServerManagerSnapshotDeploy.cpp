#include <algorithm>
#include <log/LogUtils.h>
#include <misc/std_unique_ptr.h>
#include <Properties.h>
#include "VirtualServerManager.h"
#include "src/server/VoiceServer.h"
#include "InstanceHandler.h"
#include "./groups/GroupManager.h"

//TODO: When using the new command builder make sure you're using a std::deque as the underlying bulk type!

using namespace std;
using namespace ts;
using namespace ts::server;

#define PREFIX string("[SNAPSHOT] ")

//TeamSpeak: permid=i_channel_needed_permission_modify_power permvalue=75 permskip=0 permnegated=0
//TeaSpeak: perm=i_channel_needed_permission_modify_power value=75 granted=75 flag_skip=0 flag_negated=0
struct SnapshotPermissionEntry {
    std::shared_ptr<permission::PermissionTypeEntry> type = permission::PermissionTypeEntry::unknown;
    permission::PermissionValue value = permNotGranted;
    permission::PermissionValue grant = permNotGranted;
    bool negated = false;
    bool skipped = false;

    SnapshotPermissionEntry(const std::shared_ptr<permission::PermissionTypeEntry>& type, permission::PermissionValue value, permission::PermissionValue grant, bool negated, bool skipped) : type(type), value(value), grant(grant), negated(negated), skipped(skipped) {}
    SnapshotPermissionEntry() = default;

    void write(Command& cmd, int& index, permission::teamspeak::GroupType type, int version) {
        if(version == 0) {
            if(this->value != permNotGranted) {
                for(const auto& name : permission::teamspeak::unmap_key(this->type->name, type)) {
                    cmd[index]["permid"] = name;
                    cmd[index]["permvalue"] = this->value;
                    cmd[index]["permskip"] = this->skipped;
                    cmd[index]["permnegated"] = this->negated;
                    index++;
                }
            }
            if(this->grant != permNotGranted) {
                for(const auto& name : permission::teamspeak::unmap_key(this->type->grantName(), type)) {
                    cmd[index]["permid"] = name;
                    cmd[index]["permvalue"] = this->grant;
                    cmd[index]["permskip"] = false;
                    cmd[index]["permnegated"] = false;
                    index++;
                }
            }
        } else if(version >= 1) {
            cmd[index]["perm"] = this->type->name;
            cmd[index]["value"] = this->value;
            cmd[index]["grant"] = this->grant;
            cmd[index]["flag_skip"] = this->skipped;
            cmd[index]["flag_negated"] = this->negated;
            index++;
        } else {
            logError(0, "Could not write snapshot permission! Invalid version. ({})", version);
        }
    }
};

struct PermissionCommandTuple {
    Command& cmd;
    int& index;
    int version;
    ClientDbId client;
    ChannelId channel;
};

inline bool writePermissions(const shared_ptr<permission::v2::PermissionManager>& manager, Command& cmd, int& index, int version, permission::teamspeak::GroupType type, std::string& error) {
    for(const auto& permission_container : manager->permissions()) {
        auto permission = get<1>(permission_container);
        SnapshotPermissionEntry{
                permission::resolvePermissionData(get<0>(permission_container)),
                permission.flags.value_set ? permission.values.value : permNotGranted,
                permission.flags.grant_set ? permission.values.grant : permNotGranted,
                permission.flags.negate,
                permission.flags.skip
        }.write(cmd, index, type, version);
    }
    return true;
}

inline void writeRelations(const shared_ptr<VirtualServer>& server, GroupTarget type, Command& cmd, int& index, int version) {
    PermissionCommandTuple parm{cmd, index, version, 0, 0};
    auto res = sql::command(server->getSql(), "SELECT `cldbid`, `groups`.`groupId`, `channelId`, `until` FROM `assignedGroups` INNER JOIN `groups` ON `groups`.`serverId` = `assignedGroups`.`serverId` AND `groups`.`groupId` = `assignedGroups`.`groupId` WHERE `groups`.`serverId` = :sid AND `groups`.target = :type",
                            variable{":sid", server->getServerId()},
                            variable{":type", type}
    ).query([](PermissionCommandTuple* commandIndex, int length, char** value, char** name) {
        ClientDbId cldbid = 0;
        ChannelId channelId = 0;
        GroupId gid = 0;
        int64_t until = 0;

        for(int idx = 0; idx < length; idx++) {
            try {
                if(strcmp(name[idx], "cldbid") == 0)
                    cldbid = stoul(value[idx]);
                else if(strcmp(name[idx], "groupId") == 0)
                    gid = stoul(value[idx]);
                else if(strcmp(name[idx], "channelId") == 0)
                    channelId = stoul(value[idx]);
                else if(strcmp(name[idx], "until") == 0)
                    until = stoll(value[idx]);
            } catch (std::exception& ex) {
                logError(0, "Failed to write snapshot group relation (Skipping it)! Message: {} @ {} => {}", ex.what(), name[idx], value[idx]);
                return 0;
            }
        }

        if(commandIndex->channel != channelId) {
            commandIndex->channel = channelId;
            commandIndex->cmd[commandIndex->index]["iid"] = channelId;
        }
        commandIndex->cmd[commandIndex->index]["gid"] = gid;
        commandIndex->cmd[commandIndex->index]["until"] = until;
        commandIndex->cmd[commandIndex->index++]["cldbid"] = cldbid;

        return 0;
    }, &parm);
    LOG_SQL_CMD(res);
}

struct DatabaseMusicbot {
    ClientDbId bot_id;
    ClientDbId bot_owner_id;
    std::string bot_unique_id;
};

bool VirtualServerManager::createServerSnapshot(Command &cmd, shared_ptr<VirtualServer> server, int version, std::string &error) {

    int index = 0;

    if(version == -1) version = 3; //Auto versioned
    if(version < 0 || version > 3) {
        error = "Invalid snapshot version!";
        return false;
    }
    if(version > 0) cmd[index++]["snapshot_version"] = version;
    //Server
    {
        cmd[index]["begin_virtualserver"] = "";
        for(const auto& serverProperty : server->properties()->list_properties(property::FLAG_SNAPSHOT)) {
            if(version == 0) {
                switch (serverProperty.type().property_index) {
                    case property::VIRTUALSERVER_DOWNLOAD_QUOTA:
                    case property::VIRTUALSERVER_UPLOAD_QUOTA:
                    case property::VIRTUALSERVER_MAX_DOWNLOAD_TOTAL_BANDWIDTH:
                    case property::VIRTUALSERVER_MAX_UPLOAD_TOTAL_BANDWIDTH:
                        cmd[index][std::string{serverProperty.type().name}] = (uint64_t) serverProperty.as_or<int64_t>(0);
                    default:
                        break;
                }
            }
            cmd[index][std::string{serverProperty.type().name}] = serverProperty.value();
        }
        cmd[index++]["end_virtualserver"] = "";
    }

    //Channels
    {
        cmd[index]["begin_channels"] = "";
        for(const auto& channel : server->getChannelTree()->channels()) {
            for(const auto& channelProperty : channel->properties()->list_properties(property::FLAG_SNAPSHOT)) {
                if(channelProperty.type() == property::CHANNEL_ID) {
                    cmd[index]["channel_id"] = channelProperty.value();
                } else if(channelProperty.type() == property::CHANNEL_PID) {
                    cmd[index]["channel_pid"] = channelProperty.value();
                } else {
                    cmd[index][std::string{channelProperty.type().name}] = channelProperty.value();
                }
            }
            index++;
        }
        cmd[index++]["end_channels"] = "";
    }

    //Clients
    {
        cmd[index]["begin_clients"] = "";

        struct CallbackArgument {
            Command& command;
            int& index;
            int version;
        };

        CallbackArgument callback_argument{cmd, index, version};
        this->handle->databaseHelper()->listDatabaseClients(server->getServerId(), std::nullopt, std::nullopt, [](void* ptr_argument, const DatabaseClient& client) {
            auto argument = (CallbackArgument*) ptr_argument;

            argument->command[argument->index]["client_id"] = client.client_database_id;
            argument->command[argument->index]["client_unique_id"] = client.client_unique_id;
            argument->command[argument->index]["client_nickname"] = client.client_nickname;
            argument->command[argument->index]["client_created"] = client.client_created;
            argument->command[argument->index]["client_description"] = client.client_description;
            if(argument->version == 0)
                argument->command[argument->index]["client_unread_messages"] = 0;
            argument->index++;
        }, &callback_argument);
        cmd[index++]["end_clients"] = "";
    }

    //music and music bots
    if(version >= 2) {
        cmd[index]["begin_music"] = "";

        /* music bots */
        {
            cmd[index]["begin_bots"] = "";

            deque<DatabaseMusicbot> music_bots;

            auto sql_result = sql::command(server->getSql(), "SELECT `botId`, `uniqueId`, `owner` FROM `musicbots` WHERE `serverId` = :sid", variable{":sid", server->getServerId()}).query([&](int length, string* values, string* names) {
                DatabaseMusicbot data;
                for(int column = 0; column < length; column++) {
                    try {
                        if(names[column] == "botId")
                            data.bot_id = stoll(values[column]);
                        else if(names[column] == "uniqueId")
                            data.bot_unique_id = values[column];
                        else if(names[column] == "owner")
                            data.bot_owner_id = stoll(values[column]);
                    } catch(std::exception& ex) {
                        return;
                    }
                }

                music_bots.emplace_back(data);
            });
            if(!sql_result)
                logError(server->getServerId(), PREFIX + "Failed to write music bots to snapshot. {}", sql_result.fmtStr());

            for(const auto& music_bot : music_bots) {
                auto properties = serverInstance->databaseHelper()->query_properties(server->getServerId(), property::PROP_TYPE_CLIENT, music_bot.bot_id);

                cmd[index]["bot_unique_id"] = music_bot.bot_unique_id;
                cmd[index]["bot_owner_id"] = music_bot.bot_owner_id;
                cmd[index]["bot_id"] = music_bot.bot_id;

                for(const auto& property : properties) {
                    if((property->type->flags & (property::FLAG_SAVE_MUSIC | property::FLAG_SAVE)) == 0) continue;
                    if(property->value == property->type->default_value) continue;

                    cmd[index][std::string{property->type->name}] = property->value;
                }

                index++;
            }
            cmd[index++]["end_bots"] = "";
        }

        /* playlists */
        {
            cmd[index]["begin_playlist"] = "";
            deque<PlaylistId> playlist_ids;

            auto sql_result = sql::command(server->getSql(), "SELECT `playlist_id` FROM `playlists` WHERE `serverId` = :sid", variable{":sid", server->getServerId()}).query([&](int length, string* values, string* names) {
                try {
                    playlist_ids.push_back(stoll(values[0]));
                } catch(std::exception& ex) {
                    return;
                }
            });
            if(!sql_result)
                logError(server->getServerId(), PREFIX + "Failed to write playlists to snapshot. {}", sql_result.fmtStr());

            for(const auto& playlist : playlist_ids) {
                auto properties = serverInstance->databaseHelper()->query_properties(server->getServerId(), property::PROP_TYPE_PLAYLIST, playlist);

                cmd[index]["playlist_id"] = playlist;

                for(const auto& property : properties) {
                    if((property->type->flags & (property::FLAG_SAVE_MUSIC | property::FLAG_SAVE)) == 0) continue;
                    if(property->value == property->type->default_value) continue;

                    cmd[index][std::string{property->type->name}] = property->value;
                }

                index++;
            }

            cmd[index++]["end_playlist"] = "";
        }

        /* playlist info */
        {
            cmd[index]["begin_playlist_songs"] = "";
            //playlist_songs => `serverId` INT NOT NULL, `playlist_id` INT, `song_id` INT, `order_id` INT, `invoker_dbid` INT, `url` TEXT, `url_loader` TEXT, `loaded` BOOL, `metadata` TEXT
            PlaylistId current_playlist = 0;

            auto sql_result = sql::command(server->getSql(),
                    "SELECT `playlist_id`, `song_id`, `order_id`, `invoker_dbid`, `url`, `url_loader`, `loaded`, `metadata` FROM `playlist_songs` WHERE `serverId` = :sid ORDER BY `playlist_id`", variable{":sid", server->getServerId()}
            ).query([&](int length, string* values, string* names) {
                for(int column = 0; column < length; column++) {
                    if(names[column] == "song_id")
                        cmd[index]["song_id"] = values[column];
                    else if(names[column] == "order_id")
                        cmd[index]["song_order"] = values[column];
                    else if(names[column] == "invoker_dbid")
                        cmd[index]["song_invoker"] = values[column];
                    else if(names[column] == "url")
                        cmd[index]["song_url"] = values[column];
                    else if(names[column] == "url_loader")
                        cmd[index]["song_url_loader"] = values[column];
                    else if(names[column] == "loaded")
                        cmd[index]["song_loaded"] = values[column];
                    else if(names[column] == "metadata")
                        cmd[index]["song_metadata"] = values[column];
                    else if(names[column] == "playlist_id") {
                        try {
                            auto playlist_id = stoll(values[column]);
                            if(current_playlist != playlist_id) {
                                cmd[index]["song_playlist_id"] = values[column]; /* song_playlist_id will be only set if the playlist id had changed */
                                current_playlist = playlist_id;
                            }
                        } catch(std::exception& ex) {
                            logError(server->getServerId(), PREFIX + "Failed to parse playlist id. value: {}, message: {}", values[column], ex.what());
                        }
                    }
                }
                index++;
            });
            if(!sql_result)
                logError(server->getServerId(), PREFIX + "Failed to write playlist songs to snapshot. {}", sql_result.fmtStr());
            cmd[index++]["end_playlist_songs"] = "";
        }

        cmd[index++]["end_music"] = "";
    }

    //Permissions
    {
        cmd[index++]["begin_permissions"] = "";
        //Server groups
        {
            //List groups
            {
                cmd[index]["server_groups"] = "";
                auto server_groups = server->group_manager()->server_groups();
                for(const auto& group : server_groups->available_groups(groups::GroupCalculateMode::LOCAL)) {
                    cmd[index]["id"] = group->group_id();
                    cmd[index]["name"] = group->display_name();
                    if(!writePermissions(group->permissions(), cmd, index, version, permission::teamspeak::SERVER, error)) {
                        break;
                    }
                    cmd[index++]["end_group"] = "";
                }
                cmd[index++]["end_groups"] = "";
            }

            //List relations
            {
                cmd[index]["begin_relations"] = "";
                writeRelations(server, GROUPTARGET_SERVER, cmd, index, version);
                cmd[index++]["end_relations"] = "";
            }
        }

        //Channel groups
        {
            //List groups
            {
                cmd[index]["channel_groups"] = "";
                auto server_groups = server->group_manager()->channel_groups();
                for(const auto& group : server_groups->available_groups(groups::GroupCalculateMode::LOCAL)) {
                    cmd[index]["id"] = group->group_id();
                    cmd[index]["name"] = group->display_name();
                    if(!writePermissions(group->permissions(), cmd, index, version, permission::teamspeak::SERVER, error)) {
                        break;
                    }
                    cmd[index++]["end_group"] = "";
                }
                cmd[index++]["end_groups"] = "";
            }

            //List relations
            {
                cmd[index]["begin_relations"] = "";
                writeRelations(server, GROUPTARGET_CHANNEL, cmd, index, version);
                cmd[index++]["end_relations"] = "";
            }
        }

        //Client rights
        {
            cmd[index]["client_flat"] = "";
            PermissionCommandTuple parm{cmd, index, version, 0, 0};
            auto res = sql::command(server->getSql(), "SELECT `id`, `permId`, `value`, `grant`, `flag_skip`, `flag_negate` FROM `permissions` WHERE `serverId` = :sid AND `type` = :type AND `channelId` = 0 ORDER BY `id`",
                                    variable{":sid", server->getServerId()},
                                    variable{":type", permission::SQL_PERM_USER}
            ).query([&](PermissionCommandTuple* commandIndex, int length, char** values, char**names){
                auto type = permission::resolvePermissionData(permission::unknown);
                permission::PermissionValue value = 0, grant = 0;
                bool skipped = false, negated = false;
                ClientDbId cldbid = 0;

                for(int idx = 0; idx < length; idx++) {
                    try {
                        if(strcmp(names[idx], "id") == 0)
                            cldbid = stoul(values[idx] && strlen(values[idx]) > 0 ? values[idx] : "0");
                        else if(strcmp(names[idx], "value") == 0)
                            value = values[idx] && strlen(values[idx]) > 0 ? stoi(values[idx]) : permNotGranted;
                        else if(strcmp(names[idx], "grant") == 0)
                            grant = values[idx] && strlen(values[idx]) > 0 ? stoi(values[idx]) : permNotGranted;
                        else if(strcmp(names[idx], "permId") == 0) {
                            type = permission::resolvePermissionData(values[idx]);
                            if(type->type == permission::unknown) {
                                logError(0, "Could not parse client permission for snapshot (Invalid type {})! Skipping it!", values[idx]);
                                return 0;
                            }
                        }
                        else if(strcmp(names[idx], "flag_skip") == 0)
                            skipped = values[idx] ? strcmp(values[idx], "1") == 0 : false;
                        else if(strcmp(names[idx], "flag_negate") == 0)
                            negated = values[idx] ? strcmp(values[idx], "1") == 0 : false;
                    } catch (std::exception& ex) {
                        logError(0, "Failed to write snapshot client permission (Skipping it)! Message: {} @ {} => {}", ex.what(), names[idx], values[idx]);
                        return 0;
                    }
                }

                if(type->type == permission::unknown) {
                    logError(0, "Could not parse client permission for snapshot (Missing type)! Skipping it!");
                    return 0;
                }
                if(cldbid == 0) {
                    logError(0, "Could not parse client permission for snapshot (Missing cldbid)! Skipping it!");
                    return 0;
                }
                if(value == permNotGranted && grant == permNotGranted && !skipped && !negated) return 0;

                if(commandIndex->client != cldbid) {
                    commandIndex->client = cldbid;
                    commandIndex->cmd[commandIndex->index]["id1"] = cldbid;
                    commandIndex->cmd[commandIndex->index]["id2"] = 0;
                }
                SnapshotPermissionEntry{type, value, grant, negated, skipped}.write(commandIndex->cmd, commandIndex->index, permission::teamspeak::CLIENT, commandIndex->version);
                return 0;
            }, &parm);
            LOG_SQL_CMD(res);
            cmd[index++]["end_flat"] = "";
        }

        //Channel rights
        {
            cmd[index]["channel_flat"] = "";
            PermissionCommandTuple parm{cmd, index, version, 0, 0};
            auto res = sql::command(server->getSql(), "SELECT `channelId`, `permId`, `value`, `grant`, `flag_skip`, `flag_negate` FROM `permissions` WHERE `serverId` = :sid AND `type` = :type",
                                    variable{":sid", server->getServerId()},
                                    variable{":type", permission::SQL_PERM_CHANNEL}).query(
                    [&](PermissionCommandTuple* commandIndex, int length, char** values, char**names){
                        auto type = permission::resolvePermissionData(permission::unknown);
                        permission::PermissionValue value = 0, grant = 0;
                        bool skipped = false, negated = false;
                        ChannelId chid = 0;

                        for(int idx = 0; idx < length; idx++) {
                            try {
                                if(strcmp(names[idx], "channelId") == 0)
                                    chid = stoul(values[idx] && strlen(values[idx]) > 0 ? values[idx] : "0");
                                else if(strcmp(names[idx], "value") == 0)
                                    value = values[idx] && strlen(values[idx]) > 0 ? stoi(values[idx]) : permNotGranted;
                                else if(strcmp(names[idx], "grant") == 0)
                                    grant = values[idx] && strlen(values[idx]) > 0 ? stoi(values[idx]) : permNotGranted;
                                else if(strcmp(names[idx], "permId") == 0) {
                                    type = permission::resolvePermissionData(values[idx]);
                                    if(type->type == permission::unknown) {
                                        logError(0, "Could not parse channel permission for snapshot (Invalid type {})! Skipping it!", values[idx]);
                                        return 0;
                                    }
                                }
                                else if(strcmp(names[idx], "flag_skip") == 0)
                                    skipped = values[idx] ? strcmp(values[idx], "1") == 0 : false;
                                else if(strcmp(names[idx], "flag_negate") == 0)
                                    negated = values[idx] ? strcmp(values[idx], "1") == 0 : false;
                            } catch (std::exception& ex) {
                                logError(0, "Failed to write snapshot channel permission (Skipping it)! Message: {} @ {} => {}", ex.what(), names[idx], values[idx]);
                                return 0;
                            }
                        }

                        if(type->type == permission::unknown) {
                            logError(0, "Could not parse channel permission for snapshot (Missing type)! Skipping it!");
                            return 0;
                        }
                        if(chid == 0) {
                            logError(0, "Could not parse channel permission for snapshot (Missing channel id)! Skipping it!");
                            return 0;
                        }
                        if(value == permNotGranted && grant == permNotGranted && !skipped && !negated) return 0;

                        if(commandIndex->channel != chid) {
                            commandIndex->channel = chid;
                            commandIndex->cmd[commandIndex->index]["id1"] = chid;
                            commandIndex->cmd[commandIndex->index]["id2"] = 0;
                        }
                        SnapshotPermissionEntry{type, value, grant, negated, skipped}.write(commandIndex->cmd, commandIndex->index, permission::teamspeak::CHANNEL, commandIndex->version);
                        return 0;
                    }, &parm);
            LOG_SQL_CMD(res);
            cmd[index++]["end_flat"] = "";
        }

        //Client channel rights
        {
            cmd[index]["channel_client_flat"] = "";
            PermissionCommandTuple parm{cmd, index, version, 0, 0};
            auto res = sql::command(server->getSql(), "SELECT `id`, `channelId`, `permId`, `value`, `grant`, `flag_skip`, `flag_negate` FROM `permissions` WHERE `serverId` = :sid AND `type` = :type AND `channelId` != 0",
                                    variable{":sid", server->getServerId()},
                                    variable{":type", permission::SQL_PERM_USER}).query(
                    [&](PermissionCommandTuple* commandIndex, int length, char** values, char**names){
                        auto type = permission::resolvePermissionData(permission::unknown);
                        permission::PermissionValue value = 0, grant = 0;
                        bool skipped = false, negated = false;
                        ChannelId chid = 0;
                        ClientDbId cldbid = 0;
                        for(int idx = 0; idx < length; idx++) {
                            try {
                                if(strcmp(names[idx], "channelId") == 0)
                                    chid = stoul(values[idx] && strlen(values[idx]) > 0 ? values[idx] : "0");
                                if(strcmp(names[idx], "id") == 0)
                                    cldbid = stoul(values[idx] && strlen(values[idx]) > 0 ? values[idx] : "0");
                                else if(strcmp(names[idx], "value") == 0)
                                    value = values[idx] && strlen(values[idx]) > 0 ? stoi(values[idx]) : permNotGranted;
                                else if(strcmp(names[idx], "grant") == 0)
                                    grant = values[idx] && strlen(values[idx]) > 0 ? stoi(values[idx]) : permNotGranted;
                                else if(strcmp(names[idx], "permId") == 0) {
                                    type = permission::resolvePermissionData(values[idx]);
                                    if(type->type == permission::unknown) {
                                        logError(0, "Could not parse channel client permission for snapshot (Invalid type {})! Skipping it!", values[idx]);
                                        return 0;
                                    }
                                }
                                else if(strcmp(names[idx], "flag_skip") == 0)
                                    skipped = values[idx] ? strcmp(values[idx], "1") == 0 : false;
                                else if(strcmp(names[idx], "flag_negate") == 0)
                                    negated = values[idx] ? strcmp(values[idx], "1") == 0 : false;
                            } catch (std::exception& ex) {
                                logError(0, "Failed to write snapshot channel client permission (Skipping it)! Message: {} @ {} => {}", ex.what(), names[idx], values[idx]);
                                return 0;
                            }
                        }

                        if(type->type == permission::unknown) {
                            logError(0, "Could not parse channel client permission for snapshot (Missing type)! Skipping it!");
                            return 0;
                        }
                        if(chid == 0) {
                            logError(0, "Could not parse channel client permission for snapshot (Missing channel id)! Skipping it!");
                            return 0;
                        }
                        if(cldbid == 0) {
                            logError(0, "Could not parse channel client permission for snapshot (Missing cldbid)! Skipping it!");
                            return 0;
                        }
                        if(value == permNotGranted && grant == permNotGranted && !skipped && !negated) return 0;

                        if(commandIndex->channel != chid || commandIndex->client != cldbid) {
                            commandIndex->channel = chid;
                            commandIndex->client = cldbid;
                            commandIndex->cmd[commandIndex->index]["id1"] = chid;
                            commandIndex->cmd[commandIndex->index]["id2"] = cldbid;
                        }
                        SnapshotPermissionEntry{type, value, grant, negated, skipped}.write(commandIndex->cmd, commandIndex->index, permission::teamspeak::CLIENT, commandIndex->version);
                        return 0;
                    }, &parm);
            LOG_SQL_CMD(res);
            cmd[index++]["end_flat"] = "";
        }
        cmd[index++]["end_permissions"] = "";
    }
    return true;
}