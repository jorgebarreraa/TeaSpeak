//
// Created by WolverinDEV on 26/06/2020.
//

#include "ActionLogger.h"
#include "../client/ConnectedClient.h"
#include "../client/query/QueryClient.h"

using namespace ts::server;
using namespace ts::server::log;

static FixedHeaderKeys kDefaultHeaderFields{
    .timestamp      = {"timestamp", "BIGINT"},
    .server_id      = {"server_id", "INT"},
    .invoker_id     = {"invoker_database_id", "BIGINT"},
    .invoker_name   = {"invoker_name", "VARCHAR(128)"},
    .action         = {"action", "VARCHAR(64)"}
};

inline Invoker client_to_invoker(const std::shared_ptr<ConnectedClient>& client) {
    return client ? Invoker{
            .name = client->getDisplayName(),
            .unique_id = client->getUid(),
            .database_id = client->getClientDatabaseId()
    } : kInvokerSystem;
}

/* server action logger */
bool ServerActionLogger::setup(int version, std::string &error) {
    if(!TypedActionLogger::setup(version, error))
        return false;

    switch (version) {
        case 0:
        case 1:
            /* up to date, nothing to do */
            return true;

        default:
            error = "invalid database source version";
            return false;
    }
}

ServerActionLogger::ServerActionLogger(ActionLogger* impl, LogGroupSettings* group_settings) : TypedActionLogger{
        impl,
        group_settings,
        "logs_server",
        kDefaultHeaderFields,
        {"reason", "VARCHAR(64)"}
} { }

void ServerActionLogger::log_server_create(ServerId sid, const std::shared_ptr<ConnectedClient> &invoker, ServerCreateReason reason) {
    this->do_log({
            .timestamp = std::chrono::system_clock::now(),
            .server_id = sid,
            .invoker = client_to_invoker(invoker),
            .action = Action::SERVER_CREATE,
    }, kServerCreateReasonName[(int) reason]);
}

void ServerActionLogger::log_server_delete(ServerId sid, const std::shared_ptr<ConnectedClient> &invoker) {
    this->do_log({
            .timestamp = std::chrono::system_clock::now(),
            .server_id = sid,
            .invoker = client_to_invoker(invoker),
            .action = Action::SERVER_DELETE,
    }, "");
}

void ServerActionLogger::log_server_start(ServerId sid, const std::shared_ptr<ConnectedClient> &invoker) {
    this->do_log({
            .timestamp = std::chrono::system_clock::now(),
            .server_id = sid,
            .invoker = client_to_invoker(invoker),
            .action = Action::SERVER_START,
    }, "");
}

void ServerActionLogger::log_server_stop(ServerId sid, const std::shared_ptr<ConnectedClient> &invoker) {
    this->do_log({
            .timestamp = std::chrono::system_clock::now(),
            .server_id = sid,
            .invoker = client_to_invoker(invoker),
            .action = Action::SERVER_STOP,
    }, "");
}

/* -------------------------- server edit logger -------------------------- */
bool ServerEditActionLogger::setup(int version, std::string &error) {
    if(!TypedActionLogger::setup(version, error))
        return false;

    switch (version) {
        case 0:
        case 1:
            /* up to date, nothing to do */
            return true;

        default:
            error = "invalid database source version";
            return false;
    }
}

ServerEditActionLogger::ServerEditActionLogger(ActionLogger* impl, LogGroupSettings* group_settings) : TypedActionLogger{
        impl,
        group_settings,
        "logs_server_edit",
        kDefaultHeaderFields,
        {"property", "VARCHAR(128)"},
        {"value_old", "TEXT"},
        {"value_new", "TEXT"}
} { }

void ServerEditActionLogger::log_server_edit(ServerId sid,
                                             const std::shared_ptr<ConnectedClient>& client,
                                             const property::PropertyDescription &property,
                                             const std::string &old_value,
                                             const std::string &new_value) {
    if(!this->group_settings_->is_activated(sid)) {
        return;
    }

    this->do_log({
            .timestamp = std::chrono::system_clock::now(),
            .server_id = sid,
            .invoker = client_to_invoker(client),
            .action = Action::SERVER_EDIT,
    }, property.name, old_value, new_value);
}

/* -------------------------- channel logger -------------------------- */
bool ChannelActionLogger::setup(int version, std::string &error) {
    if(!TypedActionLogger::setup(version, error))
        return false;

    switch (version) {
        case 0:
        case 1:
            /* up to date, nothing to do */
            return true;

        default:
            error = "invalid database source version";
            return false;
    }
}

ChannelActionLogger::ChannelActionLogger(ActionLogger* impl, LogGroupSettings* group_settings) : TypedActionLogger{
        impl,
        group_settings,
        "logs_channel",
        kDefaultHeaderFields,
        {"channel_id", "BIGINT"},
        {"property", "VARCHAR(128)"},
        {"value_old", "TEXT"},
        {"value_new", "TEXT"}
} { }

void ChannelActionLogger::log_channel_create(ServerId sid, const std::shared_ptr<ConnectedClient> &client, ChannelId cid, ChannelType type) {
    if(!this->group_settings_->is_activated(sid)) {
        return;
    }

    this->do_log({
                 .timestamp = std::chrono::system_clock::now(),
                 .server_id = sid,
                 .invoker = client_to_invoker(client),
                 .action = Action::CHANNEL_CREATE,
         }, cid, kChannelTypeName[(int) type], "", "");
}

void ChannelActionLogger::log_channel_edit(ServerId sid,
                                           const std::shared_ptr<ConnectedClient>& client,
                                           ChannelId cid,
                                           const property::PropertyDescription &property,
                                           const std::string &old_value,
                                           const std::string &new_value) {
    if(!this->group_settings_->is_activated(sid)) {
        return;
    }

    this->do_log({
            .timestamp = std::chrono::system_clock::now(),
            .server_id = sid,
            .invoker = client_to_invoker(client),
            .action = Action::CHANNEL_EDIT,
    }, cid, property.name, old_value, new_value);
}

void ChannelActionLogger::log_channel_move(ServerId sid, const std::shared_ptr<ConnectedClient> &client, ChannelId cid, ChannelId opcid, ChannelId npcid, ChannelId oco, ChannelId nco) {
    if(!this->group_settings_->is_activated(sid)) {
        return;
    }

    auto timestamp = std::chrono::system_clock::now();
    if(opcid != npcid) {
        this->do_log({
                .timestamp = timestamp,
                .server_id = sid,
                .invoker = client_to_invoker(client),
                .action = Action::CHANNEL_EDIT,
        }, cid, "channel_parent_id", std::to_string(opcid), std::to_string(npcid));
    }

    if(oco != nco) {
        this->do_log({
                .timestamp = timestamp,
                .server_id = sid,
                .invoker = client_to_invoker(client),
                .action = Action::CHANNEL_EDIT,
        }, cid, "channel_order", std::to_string(oco), std::to_string(nco));
    }
}

void ChannelActionLogger::log_channel_delete(ServerId sid, const std::shared_ptr<ConnectedClient> &client, ChannelId cid, ChannelDeleteReason reason) {
    if(!this->group_settings_->is_activated(sid)) {
        return;
    }

    this->do_log({
             .timestamp = std::chrono::system_clock::now(),
             .server_id = sid,
             .invoker = client_to_invoker(client),
             .action = Action::CHANNEL_DELETE,
     }, cid, kChannelDeleteReasonName[(int) reason], "", "");
}

/* -------------------------- permission logger -------------------------- */
bool PermissionActionLogger::setup(int version, std::string &error) {
    if(!TypedActionLogger::setup(version, error))
        return false;

    switch (version) {
        case 0:
        case 1:
            /* up to date, nothing to do */
            return true;

        default:
            error = "invalid database source version";
            return false;
    }
}

PermissionActionLogger::PermissionActionLogger(ActionLogger* impl, LogGroupSettings* group_settings) : TypedActionLogger{
        impl,
        group_settings,
        "logs_permission",
        kDefaultHeaderFields,
        {"target", "VARCHAR(64)"},
        {"id1", "BIGINT"},
        {"id1_name", "VARCHAR(128)"},
        {"id2", "BIGINT"},
        {"id2_name", "VARCHAR(128)"},
        {"permission", "VARCHAR(64)"},
        {"old_value", "BIGINT"},
        {"old_negated", "INT(1)"},
        {"old_skipped", "INT(1)"},
        {"new_value", "BIGINT"},
        {"new_negated", "INT(1)"},
        {"new_skipped", "INT(1)"}
} { }

void PermissionActionLogger::log_permission_add_value(
        ServerId sid,
        const std::shared_ptr<ConnectedClient>& client,
        PermissionTarget target,
        uint64_t id1, const std::string& id1_name,
        uint64_t id2, const std::string& id2_name,
        const permission::PermissionTypeEntry& permission,
        int32_t new_value,
        bool new_negated,
        bool new_skipped) {
    if(!this->group_settings_->is_activated(sid)) {
        return;
    }

    this->do_log({
             .timestamp = std::chrono::system_clock::now(),
             .server_id = sid,
             .invoker = client_to_invoker(client),
             .action = Action::PERMISSION_ADD_VALUE,
     }, kPermissionTargetName[(int) target], id1, id1_name, id2, id2_name, permission.name, 0, false, false, new_value, new_negated, new_skipped);
}

void PermissionActionLogger::log_permission_add_grant(
        ServerId sid,
        const std::shared_ptr<ConnectedClient>& client,
        PermissionTarget target,
        uint64_t id1, const std::string& id1_name,
        uint64_t id2, const std::string& id2_name,
        const permission::PermissionTypeEntry& permission,
        int32_t new_value) {
    if(!this->group_settings_->is_activated(sid)) {
        return;
    }


    this->do_log({
             .timestamp = std::chrono::system_clock::now(),
             .server_id = sid,
             .invoker = client_to_invoker(client),
             .action = Action::PERMISSION_ADD_GRANT,
     }, kPermissionTargetName[(int) target], id1, id1_name, id2, id2_name, permission.name, 0, false, false, new_value, false, false);
}

void PermissionActionLogger::log_permission_edit_value(
        ServerId sid,
        const std::shared_ptr<ConnectedClient>& client,
        PermissionTarget target,
        uint64_t id1, const std::string& id1_name,
        uint64_t id2, const std::string& id2_name,
        const permission::PermissionTypeEntry& permission,
        int32_t old_value,
        bool old_negated,
        bool old_skipped,
        int32_t new_value,
        bool new_negated,
        bool new_skipped) {
    if(!this->group_settings_->is_activated(sid)) {
        return;
    }

    this->do_log({
             .timestamp = std::chrono::system_clock::now(),
             .server_id = sid,
             .invoker = client_to_invoker(client),
             .action = Action::PERMISSION_EDIT_VALUE,
     }, kPermissionTargetName[(int) target], id1, id1_name, id2, id2_name, permission.name, old_value, old_negated, old_skipped, new_value, new_negated, new_skipped);
}

void PermissionActionLogger::log_permission_edit_grant(
        ServerId sid,
        const std::shared_ptr<ConnectedClient>& client,
        PermissionTarget target,
        uint64_t id1, const std::string& id1_name,
        uint64_t id2, const std::string& id2_name,
        const permission::PermissionTypeEntry& permission,
        int32_t old_value,
        int32_t new_value) {
    if(!this->group_settings_->is_activated(sid)) {
        return;
    }


    this->do_log({
             .timestamp = std::chrono::system_clock::now(),
             .server_id = sid,
             .invoker = client_to_invoker(client),
             .action = Action::PERMISSION_EDIT_GRANT,
     }, kPermissionTargetName[(int) target], id1, id1_name, id2, id2_name, permission.name, old_value, false, false, new_value, false, false);
}

void PermissionActionLogger::log_permission_remove_value(
        ServerId sid,
        const std::shared_ptr<ConnectedClient>& client,
        PermissionTarget target,
        uint64_t id1, const std::string& id1_name,
        uint64_t id2, const std::string& id2_name,
        const permission::PermissionTypeEntry& permission,
        int32_t old_value,
        bool old_negated,
        bool old_skipped) {
    if(!this->group_settings_->is_activated(sid)) {
        return;
    }

    this->do_log({
             .timestamp = std::chrono::system_clock::now(),
             .server_id = sid,
             .invoker = client_to_invoker(client),
             .action = Action::PERMISSION_REMOVE_VALUE,
     }, kPermissionTargetName[(int) target], id1, id1_name, id2, id2_name, permission.name, old_value, old_negated, old_skipped, 0, false, false);
}

void PermissionActionLogger::log_permission_remove_grant(
        ServerId sid,
        const std::shared_ptr<ConnectedClient>& client,
        PermissionTarget target,
        uint64_t id1, const std::string& id1_name,
        uint64_t id2, const std::string& id2_name,
        const permission::PermissionTypeEntry& permission,
        int32_t old_value) {
    if(!this->group_settings_->is_activated(sid)) {
        return;
    }

    this->do_log({
             .timestamp = std::chrono::system_clock::now(),
             .server_id = sid,
             .invoker = client_to_invoker(client),
             .action = Action::PERMISSION_REMOVE_GRANT,
     }, kPermissionTargetName[(int) target], id1, id1_name, id2, id2_name, permission.name, old_value, false, false, 0, false, false);
}

/* -------------------------- group logger -------------------------- */
constexpr auto kTableLogsGroupCreateSqlite = R"(
    CREATE TABLE `logs_groups` (
        `id` INTEGER PRIMARY KEY NOT NULL,
        `timestamp` BIGINT,
        `server_id` INTEGER,
        `invoker_database_id` BIGINT,
        `invoker_name` VARCHAR(128),
        `action` VARCHAR(64),
        `type` VARCHAR(64),
        `target` VARCHAR(64),
        `group` BIGINT,
        `group_name` VARCHAR(128),
        `source` BIGINT,
        `source_name` VARCHAR(128)
    );
)";

bool GroupActionLogger::setup(int version, std::string &error) {
    if(!TypedActionLogger::setup(version, error))
        return false;

    switch (version) {
        case 0:
        case 1:
            /* up to date, nothing to do */
            return true;

        default:
            error = "invalid database source version";
            return false;
    }
}

GroupActionLogger::GroupActionLogger(ActionLogger* impl, LogGroupSettings* group_settings) : TypedActionLogger{
        impl,
        group_settings,
        "logs_groups",
        kDefaultHeaderFields,
        {"type", "VARCHAR(64)"},
        {"target", "VARCHAR(64)"},
        {"group", "BIGINT"},
        {"group_name", "VARCHAR(128)"},
        {"source", "BIGINT"},
        {"source_name", "VARCHAR(128)"}
} { }

void GroupActionLogger::log_group_create(
        ts::ServerId sid,
        const std::shared_ptr<ConnectedClient> &client,
        GroupTarget target,
        GroupType type,
        uint64_t gid,
        const std::string &name,
        uint64_t sgid,
        const std::string &sname) {
    if(!this->group_settings_->is_activated(sid)) {
        return;
    }

    this->do_log({
             .timestamp = std::chrono::system_clock::now(),
             .server_id = sid,
             .invoker = client_to_invoker(client),
             .action = Action::GROUP_CREATE,
     }, kGroupTargetName[(int) target], kGroupTypeName[(int) type], gid, name, sgid, sname);
}

void GroupActionLogger::log_group_permission_copy(
        ts::ServerId sid,
        const std::shared_ptr<ConnectedClient> &client,
        GroupTarget target,
        GroupType type,
        uint64_t gid,
        const std::string &name,
        uint64_t sgid,
        const std::string &sname) {
    if(!this->group_settings_->is_activated(sid)) {
        return;
    }

    this->do_log({
             .timestamp = std::chrono::system_clock::now(),
             .server_id = sid,
             .invoker = client_to_invoker(client),
             .action = Action::GROUP_PERMISSION_COPY,
     }, kGroupTargetName[(int) target], kGroupTypeName[(int) type], gid, name, sgid, sname);
}

void GroupActionLogger::log_group_rename(
        ts::ServerId sid,
        const std::shared_ptr<ConnectedClient> &client,
        GroupTarget target,
        GroupType type,
        uint64_t gid,
        const std::string &name,
        const std::string &sname) {
    if(!this->group_settings_->is_activated(sid)) {
        return;
    }

    this->do_log({
             .timestamp = std::chrono::system_clock::now(),
             .server_id = sid,
             .invoker = client_to_invoker(client),
             .action = Action::GROUP_RENAME,
     }, kGroupTargetName[(int) target], kGroupTypeName[(int) type], gid, name, 0, sname);
}

void GroupActionLogger::log_group_delete(
        ts::ServerId sid,
        const std::shared_ptr<ConnectedClient> &client,
        GroupTarget target,
        GroupType type,
        uint64_t gid,
        const std::string &name) {
    if(!this->group_settings_->is_activated(sid)) {
        return;
    }

    this->do_log({
             .timestamp = std::chrono::system_clock::now(),
             .server_id = sid,
             .invoker = client_to_invoker(client),
             .action = Action::GROUP_DELETE,
     }, kGroupTargetName[(int) target], kGroupTypeName[(int) type], gid, name, 0, "");
}

/* -------------------------- group assignment logger -------------------------- */
bool GroupAssignmentActionLogger::setup(int version, std::string &error) {
    if(!TypedActionLogger::setup(version, error))
        return false;

    switch (version) {
        case 0:
        case 1:
            /* up to date, nothing to do */
            return true;

        default:
            error = "invalid database source version";
            return false;
    }
}

GroupAssignmentActionLogger::GroupAssignmentActionLogger(ActionLogger* impl, LogGroupSettings* group_settings) : TypedActionLogger{
        impl,
        group_settings,
        "logs_group_assignments",
        kDefaultHeaderFields,
        {"target", "VARCHAR(64)"},
        {"group", "BIGINT"},
        {"group_name", "VARCHAR(128)"},
        {"client", "BIGINT"},
        {"client_name", "VARCHAR(128)"}
} { }

void GroupAssignmentActionLogger::log_group_assignment_add(
        ts::ServerId sid,
        const std::shared_ptr<ConnectedClient> &client,
        GroupTarget target,
        uint64_t gid,
        const std::string &gname,
        uint64_t tclient,
        const std::string &client_name) {
    if(!this->group_settings_->is_activated(sid)) {
        return;
    }

    this->do_log({
             .timestamp = std::chrono::system_clock::now(),
             .server_id = sid,
             .invoker = client_to_invoker(client),
             .action = Action::GROUP_ASSIGNMENT_ADD,
     }, kGroupTargetName[(int) target], gid, gname, tclient, client_name);
}

void GroupAssignmentActionLogger::log_group_assignment_remove(
        ts::ServerId sid,
        const std::shared_ptr<ConnectedClient> &client,
        GroupTarget target,
        uint64_t gid,
        const std::string &gname,
        uint64_t tclient,
        const std::string &client_name) {
    if(!this->group_settings_->is_activated(sid)) {
        return;
    }

    this->do_log({
             .timestamp = std::chrono::system_clock::now(),
             .server_id = sid,
             .invoker = client_to_invoker(client),
             .action = Action::GROUP_ASSIGNMENT_REMOVE,
     }, kGroupTargetName[(int) target], gid, gname, tclient, client_name);
}

/* -------------------------- group assignment logger -------------------------- */
bool ClientChannelActionLogger::setup(int version, std::string &error) {
    if(!TypedActionLogger::setup(version, error))
        return false;

    switch (version) {
        case 0:
        case 1:
            /* up to date, nothing to do */
            return true;

        default:
            error = "invalid database source version";
            return false;
    }
}

ClientChannelActionLogger::ClientChannelActionLogger(ActionLogger* impl, LogGroupSettings* group_settings) : TypedActionLogger{
        impl,
        group_settings,
        "logs_client_channel",
        kDefaultHeaderFields,
        {"target_client", "BIGINT"},
        {"target_client_name", "VARCHAR(128)"},
        {"source_channel", "BIGINT"},
        {"source_channel_name", "VARCHAR(128)"},
        {"target_channel", "BIGINT"},
        {"target_channel_name", "VARCHAR(128)"}
} { }

void ClientChannelActionLogger::log_client_join(
        ServerId sid,
        const std::shared_ptr<ConnectedClient>& subject,
        uint64_t target_channel,
        const std::string& target_channel_name) {
    if(!this->group_settings_->is_activated(sid)) {
        return;
    }

    this->do_log({
             .timestamp = std::chrono::system_clock::now(),
             .server_id = sid,
             .invoker = kInvokerSystem,
             .action = Action::CLIENT_JOIN,
     }, subject->getClientDatabaseId(), subject->getDisplayName(), 0, "", target_channel, target_channel_name);
}

void ClientChannelActionLogger::log_client_leave(
        ServerId sid,
        const std::shared_ptr<ConnectedClient>& subject,
        uint64_t source_channel,
        const std::string& source_channel_name) {
    if(!this->group_settings_->is_activated(sid)) {
        return;
    }

    this->do_log({
             .timestamp = std::chrono::system_clock::now(),
             .server_id = sid,
             .invoker = kInvokerSystem,
             .action = Action::CLIENT_LEAVE,
     }, subject->getClientDatabaseId(), subject->getDisplayName(), source_channel, source_channel_name, 0, "");
}

void ClientChannelActionLogger::log_client_move(
        ServerId sid,
        const std::shared_ptr<ConnectedClient> &issuer,
        const std::shared_ptr<ConnectedClient> &subject,
        uint64_t target_channel, const std::string &target_channel_name, uint64_t source_channel, const std::string &source_channel_name) {
    if(!this->group_settings_->is_activated(sid)) {
        return;
    }

    this->do_log({
             .timestamp = std::chrono::system_clock::now(),
             .server_id = sid,
             .invoker = client_to_invoker(issuer),
             .action = Action::CLIENT_MOVE,
     }, subject->getClientId(), subject->getDisplayName(), source_channel, source_channel_name, target_channel, target_channel_name);
}

void ClientChannelActionLogger::log_client_kick(
        ServerId sid,
        const std::shared_ptr<ConnectedClient> &issuer,
        const std::shared_ptr<ConnectedClient> &subject,
        uint64_t target_channel, const std::string &target_channel_name, uint64_t source_channel, const std::string &source_channel_name) {
    if(!this->group_settings_->is_activated(sid)) {
        return;
    }

    this->do_log({
             .timestamp = std::chrono::system_clock::now(),
             .server_id = sid,
             .invoker = client_to_invoker(issuer),
             .action = Action::CLIENT_KICK,
     }, subject->getClientId(), subject->getDisplayName(), source_channel, source_channel_name, target_channel, target_channel_name);
}

/* -------------------------- group assignment logger -------------------------- */
bool ClientEditActionLogger::setup(int version, std::string &error) {
    if(!TypedActionLogger::setup(version, error))
        return false;

    switch (version) {
        case 0:
        case 1:
            /* up to date, nothing to do */
            return true;

        default:
            error = "invalid database source version";
            return false;
    }
}

ClientEditActionLogger::ClientEditActionLogger(ActionLogger* impl, LogGroupSettings* group_settings) : TypedActionLogger{
        impl,
        group_settings,
        "logs_client_edit",
        kDefaultHeaderFields,
        {"target_client", "BIGINT"},
        {"target_client_name", "VARCHAR(128)"},
        {"property", "VARCHAR(128)"},
        {"old_value", "TEXT"},
        {"new_value", "TEXT"}
} { }

void ClientEditActionLogger::log_client_edit(
        ServerId sid,
        const std::shared_ptr<ConnectedClient>& issuer,
        const std::shared_ptr<ConnectedClient>& subject,
        const property::PropertyDescription& property,
        const std::string& old_value,
        const std::string& new_value
) {
    if(!this->group_settings_->is_activated(sid)) {
        return;
    }

    this->do_log({
             .timestamp = std::chrono::system_clock::now(),
             .server_id = sid,
             .invoker = client_to_invoker(issuer),
             .action = Action::CLIENT_EDIT,
     }, subject->getClientId(), subject->getDisplayName(), property.name, old_value, new_value);
}

/* -------------------------- file transfer logger -------------------------- */
bool FilesActionLogger::setup(int version, std::string &error) {
    if(!TypedActionLogger::setup(version, error))
        return false;

    switch (version) {
        case 0:
        case 1:
            /* up to date, nothing to do */
            return true;

        default:
            error = "invalid database source version";
            return false;
    }
}

FilesActionLogger::FilesActionLogger(ActionLogger* impl, LogGroupSettings* group_settings) : TypedActionLogger{
        impl,
        group_settings,
        "logs_files",
        kDefaultHeaderFields,
        {"source_channel_id", "BIGINT"},
        {"source_path", "VARCHAR(256)"},
        {"target_channel_id", "BIGINT"},
        {"target_path", "VARCHAR(256)"}
} { }

void FilesActionLogger::log_file_upload(
        ServerId sid,
        const std::shared_ptr<ConnectedClient> &issuer,
        uint64_t channel_id,
        const std::string &path) {
    if(!this->group_settings_->is_activated(sid)) {
        return;
    }

    this->do_log({
             .timestamp = std::chrono::system_clock::now(),
             .server_id = sid,
             .invoker = client_to_invoker(issuer),
             .action = Action::FILE_UPLOAD,
     }, 0, "", channel_id, path);
}

void FilesActionLogger::log_file_download(
        ServerId sid,
        const std::shared_ptr<ConnectedClient> &issuer,
        uint64_t channel_id,
        const std::string &path) {
    if(!this->group_settings_->is_activated(sid)) {
        return;
    }

    this->do_log({
             .timestamp = std::chrono::system_clock::now(),
             .server_id = sid,
             .invoker = client_to_invoker(issuer),
             .action = Action::FILE_DOWNLOAD,
     }, channel_id, path, 0, "");
}

void FilesActionLogger::log_file_rename(
        ServerId sid,
        const std::shared_ptr<ConnectedClient> &issuer,
        uint64_t old_channel_id,
        const std::string &old_name,
        uint64_t mew_channel_id,
        const std::string &new_name) {
    if(!this->group_settings_->is_activated(sid)) {
        return;
    }

    this->do_log({
             .timestamp = std::chrono::system_clock::now(),
             .server_id = sid,
             .invoker = client_to_invoker(issuer),
             .action = Action::FILE_RENAME,
     }, old_channel_id, old_name, mew_channel_id, new_name);
}

void FilesActionLogger::log_file_directory_create(
        ServerId sid,
        const std::shared_ptr<ConnectedClient> &issuer,
        uint64_t channel_id,
        const std::string &path) {
    if(!this->group_settings_->is_activated(sid)) {
        return;
    }

    this->do_log({
             .timestamp = std::chrono::system_clock::now(),
             .server_id = sid,
             .invoker = client_to_invoker(issuer),
             .action = Action::FILE_DIRECTORY_CREATE,
     }, 0, "", channel_id, path);
}

void FilesActionLogger::log_file_delete(
        ServerId sid,
        const std::shared_ptr<ConnectedClient> &issuer,
        uint64_t channel_id,
        const std::string &path) {
    if(!this->group_settings_->is_activated(sid)) {
        return;
    }

    this->do_log({
             .timestamp = std::chrono::system_clock::now(),
             .server_id = sid,
             .invoker = client_to_invoker(issuer),
             .action = Action::FILE_DELETE,
     }, channel_id, path, 0, "");
}

/* -------------------------- custom logger -------------------------- */
bool CustomLogger::setup(int version, std::string &error) {
    if(!TypedActionLogger::setup(version, error))
        return false;

    switch (version) {
        case 0:
        case 1:
            /* up to date, nothing to do */
            return true;

        default:
            error = "invalid database source version";
            return false;
    }
}

CustomLogger::CustomLogger(ActionLogger* impl, LogGroupSettings* group_settings) : TypedActionLogger{
        impl,
        group_settings,
        "logs_custom",
        kDefaultHeaderFields,
        {"message", "TEXT"},
} { }

void CustomLogger::add_log_message(
        ServerId sid,
        const std::shared_ptr<ConnectedClient> &issuer,
        const std::string &message) {
    if(!this->group_settings_->is_activated(sid)) {
        return;
    }

    this->do_log({
             .timestamp = std::chrono::system_clock::now(),
             .server_id = sid,
             .invoker = client_to_invoker(issuer),
             .action = Action::CUSTOM_LOG,
     }, message);
}

#if 0


//Table: `id`, `timestamp`, `server_id`, `invoker_database_id`, `invoker_name`, `action`, `ip`, `username`, `result`
    class QueryAuthenticateLogger : public TypedActionLogger<std::string, std::string, bool> {
        public:
            void log_query_authenticate(
                    ServerId /* server id */,
                    const std::shared_ptr<ConnectedClient>& /* query */,
                    const std::string& /* username (if empty he logs out) */,
                    bool /* success */
            );
    };

    //Table: `id`, `timestamp`, `server_id`, `invoker_database_id`, `invoker_name`, `action`, `ip`, `username`, `source_server`
    class QueryServerLogger : public TypedActionLogger<std::string, std::string, bool> {
        public:
            void log_query_switch(
                    ServerId /* server id */,
                    const std::shared_ptr<ConnectedClient>& /* query */,
                    const std::string& /* authenticateed username */,
                    ServerId /* source */
            );
    };
#endif

/* -------------------------- query authenticate logger -------------------------- */
bool QueryAuthenticateLogger::setup(int version, std::string &error) {
    if(!TypedActionLogger::setup(version, error))
        return false;

    switch (version) {
        case 0:
        case 1:
            /* up to date, nothing to do */
            return true;

        default:
            error = "invalid database source version";
            return false;
    }
}

QueryAuthenticateLogger::QueryAuthenticateLogger(ActionLogger* impl, LogGroupSettings* group_settings) : TypedActionLogger{
        impl,
        group_settings,
        "logs_query_authenticate",
        kDefaultHeaderFields,
        {"ip", "VARCHAR(64)"},
        {"username", "VARCHAR(128)"},
        {"result", "INT(1)"}
} { }

void QueryAuthenticateLogger::log_query_authenticate(
        ServerId server,
        const std::shared_ptr<QueryClient> &query,
        const std::string &username,
        QueryAuthenticateResult result) {
    if(!this->group_settings_->is_activated(server)) {
        return;
    }

    this->do_log({
         .timestamp = std::chrono::system_clock::now(),
         .server_id = server,
         .invoker = client_to_invoker(query),
         .action = Action::QUERY_AUTHENTICATE,
     }, query->getLoggingPeerIp(), username, kQueryAuthenticateResultName[(int) result]);
}

/* -------------------------- query server logger -------------------------- */
bool QueryServerLogger::setup(int version, std::string &error) {
    if(!TypedActionLogger::setup(version, error))
        return false;

    switch (version) {
        case 0:
        case 1:
            /* up to date, nothing to do */
            return true;

        default:
            error = "invalid database source version";
            return false;
    }
}

QueryServerLogger::QueryServerLogger(ActionLogger* impl, LogGroupSettings* group_settings) : TypedActionLogger{
        impl,
        group_settings,
        "logs_query_server",
        kDefaultHeaderFields,
        {"ip", "VARCHAR(64)"},
        {"username", "VARCHAR(128)"},
        {"other_server", "INT"}
} { }

void QueryServerLogger::log_query_switch(const std::shared_ptr<QueryClient> &query, const std::string &username, ServerId source, ServerId target) {
    if(this->group_settings_->is_activated(source)) {
        this->do_log({
             .timestamp = std::chrono::system_clock::now(),
             .server_id = source,
             .invoker = client_to_invoker(query),
             .action = Action::QUERY_LEAVE,
         }, query->getLoggingPeerIp(), username, target);
    }

    if(this->group_settings_->is_activated(source)) {
        this->do_log({
             .timestamp = std::chrono::system_clock::now(),
             .server_id = target,
             .invoker = client_to_invoker(query),
             .action = Action::QUERY_JOIN,
         }, query->getLoggingPeerIp(), username, target);
    }
}