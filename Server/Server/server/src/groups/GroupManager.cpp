//
// Created by WolverinDEV on 03/03/2020.
//
#include <string>
#include <misc/utf8.h>
#include <log/LogUtils.h>
#include "./GroupManager.h"
#include "../InstanceHandler.h"

constexpr static auto kGroupMaxNameLength{40};
using namespace ts::server::groups;

GroupManager::GroupManager(sql::SqlManager *sql, ServerId server_id, std::shared_ptr<GroupManager> parent)
    : virtual_server_id_{server_id}, database_{sql}, parent_manager_{std::move(parent)}, assignment_manager_{this}
{
    assert(sql);
}

GroupManager::~GroupManager() = default;

ts::ServerId GroupManager::server_id() {
    return this->virtual_server_id_;
}

sql::SqlManager* GroupManager::sql_manager() {
    return this->database_;
}

bool GroupManager::initialize(const std::shared_ptr<GroupManager>& self, std::string &error) {
    assert(&*self == this);

    if(this->parent_manager_) {
        this->server_groups_ = std::make_shared<ServerGroupManager>(self, this->parent_manager_->server_groups_);
        this->channel_groups_ = std::make_shared<ChannelGroupManager>(self, this->parent_manager_->channel_groups_);
    } else {
        this->server_groups_ = std::make_shared<ServerGroupManager>(self, nullptr);
        this->channel_groups_ = std::make_shared<ChannelGroupManager>(self, nullptr);
    }

    return true;
}

void GroupManager::save_permissions() {
    size_t total_groups_server, total_groups_channel;
    size_t saved_groups_server, saved_groups_channel;

    auto timestamp_0 = std::chrono::system_clock::now();
    this->server_groups_->save_permissions(total_groups_server, saved_groups_server);
    auto timestamp_1 = std::chrono::system_clock::now();
    this->channel_groups_->save_permissions(total_groups_channel, saved_groups_channel);
    auto timestamp_2 = std::chrono::system_clock::now();

    auto time_server_groups = std::chrono::duration_cast<std::chrono::milliseconds>(timestamp_1 - timestamp_0).count();
    auto time_channel_groups = std::chrono::duration_cast<std::chrono::milliseconds>(timestamp_2 - timestamp_1).count();

    if(saved_groups_channel > 0 || saved_groups_channel > 0) {
        debugMessage(this->server_id(), "Saved {}/{} server and {}/{} channel group permissions in {}ms or {}ms",
                     saved_groups_server, total_groups_server,
                     saved_groups_channel, total_groups_channel,
                     time_server_groups, time_channel_groups
        );
    }
}

/* Abstract group manager */
AbstractGroupManager::AbstractGroupManager(
        sql::SqlManager* database,
        DatabaseGroupTarget database_target_,
        ServerId virtual_server_id,
        std::shared_ptr<AbstractGroupManager> parent
) : database_{database}, database_target_{database_target_}, virtual_server_id_{virtual_server_id}, parent_manager_{std::move(parent)} { }

ts::ServerId AbstractGroupManager::server_id() {
    return this->virtual_server_id_;
}

sql::SqlManager *AbstractGroupManager::sql_manager() {
    return this->database_;
}

bool AbstractGroupManager::initialize(std::string &) {
    return true;
}

GroupLoadResult AbstractGroupManager::load_data(bool initialize) {
    (void) initialize;

    std::lock_guard manage_lock{this->group_manage_mutex_};
    {
        std::lock_guard list_lock{this->group_mutex_};
        this->groups_.clear();
    }

    {
        auto command = sql::command{this->sql_manager(), "SELECT * FROM `groups` WHERE `serverId` = :sid AND `target` = :target"};
        command.value(":sid", this->server_id());
        command.value(":target", (uint8_t) this->database_target_);
        auto result = command.query(&AbstractGroupManager::insert_group_from_sql, this);
        if(!result) {
            LOG_SQL_CMD(result);
            return GroupLoadResult::DATABASE_ERROR;
        }
    }

    {
        std::lock_guard list_lock{this->group_mutex_};
        if(this->groups_.empty()) {
            return GroupLoadResult::NO_GROUPS;
        }
    }

    return GroupLoadResult::SUCCESS;
}

void AbstractGroupManager::unload_data() {
    std::lock_guard manage_lock{this->group_manage_mutex_};
    {
        std::lock_guard list_lock{this->group_mutex_};
        this->groups_.clear();
    }
}

void AbstractGroupManager::reset_groups(std::map<GroupId, GroupId> &mapping) {
    std::lock_guard manage_lock{this->group_manage_mutex_};
    this->unload_data();

    /* Delete all old groups */
    {
        LOG_SQL_CMD(sql::command(this->sql_manager(), "DELETE FROM `permissions` WHERE `serverId` = :serverId AND `type` = :type AND `id` IN (SELECT `groupId` FROM `groups` WHERE `serverId` = :serverId AND `target` = :target)",
                                 variable{":serverId", this->server_id()},
                                 variable{":type", ts::permission::SQL_PERM_GROUP},
                                 variable{":target", (uint8_t) this->database_target_}).execute());

        LOG_SQL_CMD(sql::command(this->sql_manager(), "DELETE FROM `assignedGroups` WHERE `serverId` = :serverId AND `groupId` IN (SELECT `groupId` FROM `groups` WHERE `serverId` = :AND AND `target` = :target)",
                                 variable{":serverId", this->server_id()},
                                 variable{":target", (uint8_t) this->database_target_}).execute());

        LOG_SQL_CMD(sql::command(this->sql_manager(), "DELETE FROM `groups` WHERE `serverId` = :serverId AND `target` = :target",
                                 variable{":serverId", this->server_id()},
                                 variable{":target", (uint8_t) this->database_target_}
        ).execute());
    }

    /* we expect to not have any groups */
    if(auto error = this->load_data(true); error != GroupLoadResult::NO_GROUPS) {
        logCritical(this->server_id(), "Failed to load groups after group unload ({}). There might be no groups loaded now!", (int) error);
    }

    if(this->parent_manager_) {
        for(const auto& group : this->parent_manager_->groups_) {
            if(group->group_type() != GroupType::GROUP_TYPE_TEMPLATE) {
                continue;
            }

            //GroupCopyResult
            std::shared_ptr<Group> created_group{};
            auto result = this->copy_group_(group->group_id(), GroupType::GROUP_TYPE_NORMAL, group->display_name(), created_group);
            switch(result) {
                case GroupCopyResult::SUCCESS:
                    break;

                case GroupCopyResult::NAME_INVALID:
                case GroupCopyResult::NAME_ALREADY_IN_USE:
                case GroupCopyResult::UNKNOWN_TARGET_GROUP:
                case GroupCopyResult::UNKNOWN_SOURCE_GROUP:
                case GroupCopyResult::DATABASE_ERROR:
                    logCritical(this->server_id(), "Failed to copy template group {}: {}", group->group_id(), (uint8_t) result);
                    continue;
            }

            assert(created_group);
            logTrace(this->server_id(), "Copied template group {}. New id: {}", group->group_id(), created_group->group_id());
            mapping[group->group_id()] = created_group->group_id();
        }
    }
}


int AbstractGroupManager::insert_group_from_sql(int length, std::string *values, std::string *names) {
    GroupId group_id{0};
    GroupType group_type{GroupType::GROUP_TYPE_UNKNOWN};
    std::string group_name{};

    for(size_t index = 0; index < length; index++) {
        try {
            if(names[index] == "groupId") {
                group_id = std::stoull(values[index]);
            } else if(names[index] == "target") {
                /* group_target = (GroupTarget) std::stoull(values[index]); */
            } else if(names[index] == "type") {
                group_type = (GroupType) std::stoull(values[index]);
            } else if(names[index] == "displayName") {
                group_name = values[index];
            }
        } catch(std::exception& ex) {
            logWarning(this->server_id(), "Failed to parse group from database. Failed to parse column {} (value: {})", names[index], values[index]);
            return 0;
        }
    }

    if(!group_id) {
        logWarning(this->server_id(), "Failed to query group from database. Invalid values found.");
        return 0;
    }

    auto permissions = serverInstance->databaseHelper()->loadGroupPermissions(this->server_id(), group_id, (uint8_t) this->database_target_);
    auto group = this->allocate_group(group_id, group_type, group_name, permissions);

    std::lock_guard lock{this->group_mutex_};
    this->groups_.push_back(group);
    return 0;
}

void AbstractGroupManager::save_permissions(size_t &total_groups, size_t &saved_groups) {
    std::unique_lock group_lock{this->group_mutex_};
    auto groups = this->groups_;
    group_lock.unlock();

    total_groups = groups.size();
    saved_groups = 0;

    for(auto& group : groups) {
        auto permissions = group->permissions();
        if(permissions->require_db_updates()) {
            serverInstance->databaseHelper()->saveGroupPermissions(this->server_id(), group->group_id(), (uint8_t) this->database_target_, permissions);
            saved_groups++;
        }
    }
}

std::shared_ptr<Group> AbstractGroupManager::find_group_(std::shared_ptr<AbstractGroupManager>& owning_manager, GroupCalculateMode mode, GroupId group_id) {
    {
        std::lock_guard glock{this->group_mutex_};
        auto it = std::find_if(this->groups_.begin(), this->groups_.end(), [&](const std::shared_ptr<Group>& group) {
            return group->group_id() == group_id;
        });

        if(it != this->groups_.end()) {
            owning_manager = this->shared_from_this();
            return *it;
        }
    }

    if(mode == GroupCalculateMode::GLOBAL && this->parent_manager_) {
        owning_manager = this->parent_manager_;
        return this->parent_manager_->find_group_(owning_manager, mode, group_id);
    }

    return nullptr;
}

std::shared_ptr<Group> AbstractGroupManager::find_group_by_name_(GroupCalculateMode mode, const std::string &name) {
    {
        std::string lname{name};
        std::transform(lname.begin(), lname.end(), lname.begin(), ::tolower);

        std::lock_guard glock{this->group_mutex_};
        auto it = std::find_if(this->groups_.begin(), this->groups_.end(),
                               [&](const std::shared_ptr<Group> &group) {
                                   std::string lgroup_name{group->display_name()};
                                   std::transform(lgroup_name.begin(), lgroup_name.end(), lgroup_name.begin(), ::tolower);
                                   return lname == lgroup_name;
                               });

        if(it != this->groups_.end()) {
            return *it;
        }
    }

    if(mode == GroupCalculateMode::GLOBAL && this->parent_manager_) {
        return this->parent_manager_->find_group_by_name_(mode, name);
    }
    return nullptr;
}

GroupCreateResult AbstractGroupManager::create_group_(GroupType type, const std::string &name, std::shared_ptr<Group>& result) {
    auto name_length = utf8::count_characters(name);
    if(name_length <= 0) {
        return GroupCreateResult::NAME_TOO_SHORT;
    } else if(name_length > kGroupMaxNameLength) {
        return GroupCreateResult::NAME_TOO_LONG;
    }

    std::lock_guard manage_lock{this->group_manage_mutex_};
    if(this->find_group_by_name_(GroupCalculateMode::LOCAL, name)) {
        return GroupCreateResult::NAME_ALREADY_IN_USED;
    }

    auto res = sql::command(this->sql_manager(), "INSERT INTO `groups` (`serverId`, `target`, `type`, `displayName`) VALUES (:sid, :target, :type, :name)",
                            variable{":sid", this->server_id()},
                            variable{":target", (uint8_t) this->database_target_},
                            variable{":type", type},
                            variable{":name", name}).execute();

    if(!res) {
        LOG_SQL_CMD(res);
        return GroupCreateResult::DATABASE_ERROR;
    }

    auto group_id = res.last_insert_rowid();
    auto permissions = serverInstance->databaseHelper()->loadGroupPermissions(this->server_id(), group_id, (uint8_t) this->database_target_);
    auto group = this->allocate_group(group_id, type, name, permissions);

    {
        std::lock_guard glock{this->group_mutex_};
        this->groups_.push_back(group);
    }
    result = group;
    return GroupCreateResult::SUCCESS;
}

GroupCopyResult AbstractGroupManager::copy_group_(GroupId source, GroupType target_type, const std::string &display_name, std::shared_ptr<Group>& result) {
    std::lock_guard manage_lock{this->group_manage_mutex_};

    auto create_result = this->create_group_(target_type, display_name, result);
    switch(create_result) {
        case GroupCreateResult::SUCCESS:
            break;

        case GroupCreateResult::DATABASE_ERROR:
            return GroupCopyResult::DATABASE_ERROR;

        case GroupCreateResult::NAME_ALREADY_IN_USED:
            return GroupCopyResult::NAME_ALREADY_IN_USE;

        case GroupCreateResult::NAME_TOO_LONG:
        case GroupCreateResult::NAME_TOO_SHORT:
            return GroupCopyResult::NAME_INVALID;
    }

    assert(result);
    return this->copy_group_permissions_(source, result->group_id());
}

/* FIXME: This implementation is flawed since it may operates on parent groups without locking them properly. */
GroupCopyResult AbstractGroupManager::copy_group_permissions_(GroupId source, GroupId target) {
    std::lock_guard manage_lock{this->group_manage_mutex_};

    std::shared_ptr<AbstractGroupManager> source_owning_manager{};
    auto source_group = this->find_group_(source_owning_manager, groups::GroupCalculateMode::GLOBAL, source);
    if(!source_group) {
        return GroupCopyResult::UNKNOWN_SOURCE_GROUP;
    }

    std::shared_ptr<AbstractGroupManager> target_owning_manager{};
    auto target_group = this->find_group_(target_owning_manager, groups::GroupCalculateMode::GLOBAL, target);
    if(!target_group) {
        return GroupCopyResult::UNKNOWN_TARGET_GROUP;
    }

    if(source_group->permissions()->require_db_updates()) {
        /* TODO: Somehow flush all pending changes */
    }

    {
        auto res = sql::command(this->sql_manager(), "DELETE FROM `permissions` WHERE `serverId` = :sid AND `type` = :type AND `id` = :id",
                                variable{":sid", source_owning_manager->server_id()},
                                variable{":type", ts::permission::SQL_PERM_GROUP},
                                variable{":id", target}).execute();
        if(!res) {
            LOG_SQL_CMD(res);
            return GroupCopyResult::DATABASE_ERROR;
        }
    }

    {
        constexpr static auto kSqlCommand = "INSERT INTO `permissions` (`serverId`, `type`, `id`, `channelId`, `permId`, `value`, `grant`, `flag_skip`, `flag_negate`) "
                                            "SELECT :tsid AS `serverId`, `type`, :target AS `id`, 0 AS `channelId`, `permId`, `value`,`grant`,`flag_skip`, `flag_negate` FROM `permissions` WHERE `serverId` = :ssid AND `type` = :type AND `id` = :source";
        auto res = sql::command(this->sql_manager(), kSqlCommand,
                           variable{":ssid", source_owning_manager->server_id()},
                           variable{":tsid", target_owning_manager->server_id()},
                           variable{":type", ts::permission::SQL_PERM_GROUP},
                           variable{":source", source},
                           variable{":target", target}).execute();
        if(!res) {

            LOG_SQL_CMD(res);
            return GroupCopyResult::DATABASE_ERROR;
        }
    }

    target_group->set_permissions(serverInstance->databaseHelper()->loadGroupPermissions(target_owning_manager->server_id(), target, (uint8_t) this->database_target_));
    return GroupCopyResult::SUCCESS;
}

GroupRenameResult AbstractGroupManager::rename_group_(GroupId group_id, const std::string &name) {
    std::lock_guard manage_lock{this->group_manage_mutex_};

    std::shared_ptr<AbstractGroupManager> owning_manager{};
    auto group = this->find_group_(owning_manager, groups::GroupCalculateMode::LOCAL, group_id);
    if(!group) {
        /* Group hasn't been found locally. Trying our parent manager. */
        if(this->parent_manager_) {
            return this->parent_manager_->rename_group_(group_id, name);
        }

        return GroupRenameResult::INVALID_GROUP_ID;
    }

    assert(&*owning_manager == this);
    auto name_length = utf8::count_characters(name);
    if(name_length <= 0 || name_length > kGroupMaxNameLength) {
        return GroupRenameResult::NAME_INVALID;
    }

    if(this->find_group_by_name_(GroupCalculateMode::LOCAL, name)) {
        return GroupRenameResult::NAME_ALREADY_USED;
    }

    sql::command(this->sql_manager(), "UPDATE `groups` SET `displayName` = :name WHERE `serverId` = :server AND `groupId` = :group_id AND `target` = :target",
                 variable{":server", this->server_id()},
                 variable{":target", (uint8_t) this->database_target_},
                 variable{":name", name},
                 variable{":group_id", group_id}).executeLater().waitAndGetLater(LOG_SQL_CMD, {-1, "future failed"});
    group->name_ = name;
    return GroupRenameResult::SUCCESS;
}

GroupDeleteResult AbstractGroupManager::delete_group_(GroupId group_id) {
    std::lock_guard manage_lock{this->group_manage_mutex_};

    {
        std::unique_lock glock{this->group_mutex_};
        auto it = std::find_if(this->groups_.begin(), this->groups_.end(), [&](const std::shared_ptr<Group>& group) {
            return group->group_id() == group_id;
        });

        if(it == this->groups_.end()) {
            if(this->parent_manager_) {
                glock.unlock();
                return this->parent_manager_->delete_group_(group_id);
            }

            return GroupDeleteResult::INVALID_GROUP_ID;
        }

        this->groups_.erase(it);
    }

    sql::command(this->sql_manager(), "DELETE FROM `groups` WHERE `serverId` = :server AND `groupId` = :group_id AND `target` = :target",
                 variable{":server", this->server_id()},
                 variable{":target", (uint8_t) this->database_target_},
                 variable{":group_id", group_id}).executeLater().waitAndGetLater(LOG_SQL_CMD, {-1, "future failed"});
    return GroupDeleteResult::SUCCESS;
}

/* Server group manager */
ServerGroupManager::ServerGroupManager(const std::shared_ptr<GroupManager> &handle, std::shared_ptr<ServerGroupManager> parent)
    : AbstractGroupManager{handle->sql_manager(), DatabaseGroupTarget::SERVER, handle->server_id(), parent}
{

}
std::shared_ptr<Group> ServerGroupManager::allocate_group(GroupId id, GroupType type, std::string name,
                                                          std::shared_ptr<permission::v2::PermissionManager> permissions) {
    return std::make_shared<ServerGroup>(this->server_id(), id, type, name, permissions);
}


/* Channel group manager */
ChannelGroupManager::ChannelGroupManager(const std::shared_ptr<GroupManager> &handle, std::shared_ptr<ChannelGroupManager> parent)
        : AbstractGroupManager{handle->sql_manager(), DatabaseGroupTarget::CHANNEL, handle->server_id(), parent}
{

}
std::shared_ptr<Group> ChannelGroupManager::allocate_group(GroupId id, GroupType type, std::string name,
                                                          std::shared_ptr<permission::v2::PermissionManager> permissions) {
    return std::make_shared<ChannelGroup>(this->server_id(), id, type, name, permissions);
}