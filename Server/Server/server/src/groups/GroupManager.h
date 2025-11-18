#pragma once

#include <mutex>
#include <deque>
#include <string>
#include <memory>
#include <Definitions.h>
#include <sql/SqlQuery.h>
#include "./GroupAssignmentManager.h"
#include "./Group.h"

namespace ts::server::groups {
    enum struct GroupCalculateMode {
        LOCAL, /* only calculate clients groups for the local server */
        GLOBAL /* use the parent group manager as well, if existing */
    };

    enum struct GroupLoadResult {
        SUCCESS,
        NO_GROUPS,
        DATABASE_ERROR
    };

    enum struct GroupCreateResult {
        SUCCESS,
        NAME_ALREADY_IN_USED,
        NAME_TOO_SHORT,
        NAME_TOO_LONG,
        DATABASE_ERROR
    };

    enum struct GroupCopyResult {
        SUCCESS,
        UNKNOWN_SOURCE_GROUP,
        UNKNOWN_TARGET_GROUP,
        NAME_ALREADY_IN_USE,
        NAME_INVALID,
        DATABASE_ERROR
    };

    enum struct GroupRenameResult {
        SUCCESS,
        INVALID_GROUP_ID,
        NAME_ALREADY_USED,
        NAME_INVALID,
        DATABASE_ERROR
    };

    enum struct GroupDeleteResult {
        SUCCESS,
        INVALID_GROUP_ID,
        /* Artificial result, not used by the delete method but needed */
        GROUP_NOT_EMPTY,
        DATABASE_ERROR
    };

    class GroupManager;
    class AbstractGroupManager : public std::enable_shared_from_this<AbstractGroupManager> {
            friend class Group;
            friend class GroupAssignmentManager;
        public:
            enum struct DatabaseGroupTarget : uint8_t {
                SERVER = 0x00,
                CHANNEL = 0x01
            };

            AbstractGroupManager(
                    sql::SqlManager* /* database */,
                    DatabaseGroupTarget /* database group target */,
                    ServerId /* virtual server id */,
                    std::shared_ptr<AbstractGroupManager> /* parent */
            );

            virtual ~AbstractGroupManager() = default;

            bool initialize(std::string& /* error */);
            GroupLoadResult load_data(bool /* initialize */ = false);
            void unload_data();

            void save_permissions(size_t& /* total groups */, size_t& /* saved groups */);

            /**
             * Reset all known groups and copy the template groups from our group parent (if it isn't null)
             */
            void reset_groups(std::map<GroupId, GroupId>& /* mapping */);
        protected:
            std::shared_ptr<AbstractGroupManager> parent_manager_;
            DatabaseGroupTarget database_target_;

            sql::SqlManager* database_;
            ServerId virtual_server_id_;

            /* recursive_mutex due to the copy group methods */
            std::recursive_mutex group_manage_mutex_{};
            std::mutex group_mutex_{};

            /* I think std::vector is better here because we will iterate more often than add groups */
            std::vector<std::shared_ptr<Group>> groups_{};

            [[nodiscard]] sql::SqlManager* sql_manager();
            [[nodiscard]] ServerId server_id();

            [[nodiscard]] std::shared_ptr<Group> find_group_(std::shared_ptr<AbstractGroupManager>& /* owning manager */, GroupCalculateMode /* mode */, GroupId /* group id */);
            [[nodiscard]] std::shared_ptr<Group> find_group_by_name_(GroupCalculateMode /* mode */, const std::string& /* group name */);
            [[nodiscard]] GroupCreateResult create_group_(GroupType type, const std::string& /* group name */, std::shared_ptr<Group>& /* result */);
            [[nodiscard]] GroupCopyResult copy_group_(GroupId /* group id */, GroupType /* target group type */, const std::string& /* target group name */, std::shared_ptr<Group>& /* result */);
            [[nodiscard]] GroupCopyResult copy_group_permissions_(GroupId /* group id */, GroupId /* target group */);
            [[nodiscard]] GroupRenameResult rename_group_(GroupId /* group id */, const std::string& /* target group name */);
            [[nodiscard]] GroupDeleteResult delete_group_(GroupId /* group id */);

            int insert_group_from_sql(int /* length */, std::string* /* values */, std::string* /* columns */);

            virtual std::shared_ptr<Group> allocate_group(
                    GroupId /* id */,
                    GroupType /* type */,
                    std::string /* name */,
                    std::shared_ptr<permission::v2::PermissionManager> /* permissions */
            ) = 0;
    };

    class ServerGroupManager : public AbstractGroupManager {
        public:
            ServerGroupManager(const std::shared_ptr<GroupManager>& /* owner */, std::shared_ptr<ServerGroupManager> /* parent */);

            [[nodiscard]] inline std::vector<std::shared_ptr<ServerGroup>> available_groups(GroupCalculateMode mode) {
                std::vector<std::shared_ptr<ServerGroup>> result{};
                if(auto manager{std::dynamic_pointer_cast<ServerGroupManager>(this->parent_manager_)}; manager && mode != GroupCalculateMode::LOCAL) {
                    auto result_ = manager->available_groups(mode);
                    result.reserve(result_.size());
                    result.insert(result.end(), result_.begin(), result_.end());
                }

                {
                    std::lock_guard group_lock{this->group_mutex_};
                    result.reserve(this->groups_.size());
                    for(const auto& group : this->groups_) {
                        result.push_back(this->cast_result(group));
                    }
                }

                return result;
            }

            [[nodiscard]] inline std::shared_ptr<ServerGroup> find_group(GroupCalculateMode mode, GroupId group_id) {
                std::shared_ptr<AbstractGroupManager> owning_manager{};
                return this->cast_result(this->find_group_(owning_manager, mode, group_id));
            }

            /**
             *
             * @param owning_manager
             * @param mode
             * @param group_id
             * @return the group if found. `owning_manager` will be set to the owning manager.
             */
            [[nodiscard]] inline std::shared_ptr<ServerGroup> find_group_ext(std::shared_ptr<ServerGroupManager>& owning_manager, GroupCalculateMode mode, GroupId group_id) {
                std::shared_ptr<AbstractGroupManager> owning_manager_;
                auto result = this->cast_result(this->find_group_(owning_manager_, mode, group_id));
                if(owning_manager_) {
                    owning_manager = std::dynamic_pointer_cast<ServerGroupManager>(owning_manager_);
                    assert(owning_manager);
                }
                return result;
            }

            [[nodiscard]] inline std::shared_ptr<ServerGroup> find_group_by_name(GroupCalculateMode mode, const std::string& group_name) {
                return this->cast_result(this->find_group_by_name_(mode, group_name));
            }

            [[nodiscard]] inline GroupCreateResult create_group(GroupType type, const std::string& group_name, std::shared_ptr<ServerGroup>& result) {
                std::shared_ptr<Group> result_;
                auto r = this->create_group_(type, group_name, result_);
                result = this->cast_result(result_);
                return r;
            }

            [[nodiscard]] inline GroupCopyResult copy_group(GroupId group_id, GroupType target_group_type, const std::string& target_group_name, std::shared_ptr<ServerGroup>& result) {
                std::shared_ptr<Group> result_;
                auto r = this->copy_group_(group_id, target_group_type, target_group_name, result_);
                result = this->cast_result(result_);
                return r;
            }

            [[nodiscard]] inline GroupCopyResult copy_group_permissions(GroupId group_id, GroupId target_group) {
                return this->copy_group_permissions_(group_id, target_group);
            }

            [[nodiscard]] inline GroupRenameResult rename_group(GroupId group_id, const std::string& target_group_name) {
                return this->rename_group_(group_id, target_group_name);
            }

            [[nodiscard]] inline GroupDeleteResult delete_group(GroupId group_id) {
                return this->delete_group_(group_id);
            }

        protected:
            [[nodiscard]] std::shared_ptr<Group> allocate_group(GroupId, GroupType, std::string,
                                                  std::shared_ptr<permission::v2::PermissionManager>) override;

            [[nodiscard]] inline std::shared_ptr<ServerGroup> cast_result(std::shared_ptr<Group> result) {
                if(!result) {
                    return nullptr;
                }

                auto casted = std::dynamic_pointer_cast<ServerGroup>(result);
                assert(casted);
                return casted;
            }
    };

    class ChannelGroupManager : public AbstractGroupManager {
        public:
            ChannelGroupManager(const std::shared_ptr<GroupManager>& /* owner */, std::shared_ptr<ChannelGroupManager> /* parent */);

            [[nodiscard]] inline std::vector<std::shared_ptr<ChannelGroup>> available_groups(GroupCalculateMode mode) {
                std::vector<std::shared_ptr<ChannelGroup>> result{};
                if(auto manager{std::dynamic_pointer_cast<ChannelGroupManager>(this->parent_manager_)}; manager && mode != GroupCalculateMode::LOCAL) {
                    auto result_ = manager->available_groups(mode);
                    result.reserve(result_.size());
                    result.insert(result.end(), result_.begin(), result_.end());
                }

                {
                    std::lock_guard group_lock{this->group_mutex_};
                    result.reserve(this->groups_.size());
                    for(const auto& group : this->groups_) {
                        result.push_back(this->cast_result(group));
                    }
                }

                return result;
            }

            [[nodiscard]] inline std::shared_ptr<ChannelGroup> find_group(GroupCalculateMode mode, GroupId group_id) {
                std::shared_ptr<AbstractGroupManager> owning_manager{};
                return this->cast_result(this->find_group_(owning_manager, mode, group_id));
            }

            /**
             *
             * @param owning_manager
             * @param mode
             * @param group_id
             * @return the group if found. `owning_manager` will be set to the owning manager.
             */
            [[nodiscard]] inline std::shared_ptr<ChannelGroup> find_group_ext(std::shared_ptr<ChannelGroupManager>& owning_manager, GroupCalculateMode mode, GroupId group_id) {
                std::shared_ptr<AbstractGroupManager> owning_manager_;
                auto result = this->cast_result(this->find_group_(owning_manager_, mode, group_id));
                if(owning_manager_) {
                    owning_manager = std::dynamic_pointer_cast<ChannelGroupManager>(owning_manager_);
                    assert(owning_manager);
                }
                return result;
            }

            [[nodiscard]] inline std::shared_ptr<ChannelGroup> find_group_by_name(GroupCalculateMode mode, const std::string& group_name) {
                return this->cast_result(this->find_group_by_name_(mode, group_name));
            }

            [[nodiscard]] inline GroupCreateResult create_group(GroupType type, const std::string& group_name, std::shared_ptr<ChannelGroup>& result) {
                std::shared_ptr<Group> result_;
                auto r = this->create_group_(type, group_name, result_);
                result = this->cast_result(result_);
                return r;
            }

            [[nodiscard]] inline GroupCopyResult copy_group(GroupId group_id, GroupType target_group_type, const std::string& target_group_name, std::shared_ptr<ChannelGroup>& result) {
                std::shared_ptr<Group> result_;
                auto r = this->copy_group_(group_id, target_group_type, target_group_name, result_);
                result = this->cast_result(result_);
                return r;
            }

            [[nodiscard]] inline GroupCopyResult copy_group_permissions(GroupId group_id, GroupId target_group) {
                return this->copy_group_permissions_(group_id, target_group);
            }

            [[nodiscard]] inline GroupRenameResult rename_group(GroupId group_id, const std::string& target_group_name) {
                return this->rename_group_(group_id, target_group_name);
            }

            [[nodiscard]] inline GroupDeleteResult delete_group(GroupId group_id) {
                return this->delete_group_(group_id);
            }
        private:
            std::shared_ptr<Group> allocate_group(GroupId, GroupType, std::string,
                                                  std::shared_ptr<permission::v2::PermissionManager>) override;

            [[nodiscard]] inline std::shared_ptr<ChannelGroup> cast_result(std::shared_ptr<Group> result) {
                if(!result) {
                    return nullptr;
                }

                auto casted = std::dynamic_pointer_cast<ChannelGroup>(result);
                assert(casted);
                return casted;
            }
    };

    class GroupManager {
            friend class Group;
            friend class ServerGroupManager;
            friend class ChannelGroupManager;
            friend class GroupAssignmentManager;
        public:
            GroupManager(sql::SqlManager* /* database */, ServerId /* virtual server id */, std::shared_ptr<GroupManager> /* parent */);
            ~GroupManager();

            bool initialize(const std::shared_ptr<GroupManager>& /* self ref, */, std::string& /* error */);

            [[nodiscard]] inline const std::shared_ptr<GroupManager>& parent_manager() { return this->parent_manager_; }
            void save_permissions();

            [[nodiscard]] inline GroupAssignmentManager& assignments() { return this->assignment_manager_; }
            [[nodiscard]] inline const std::shared_ptr<ServerGroupManager>& server_groups() { return this->server_groups_; }
            [[nodiscard]] inline const std::shared_ptr<ChannelGroupManager>& channel_groups() { return this->channel_groups_; }
        private:
            sql::SqlManager* database_;
            ServerId virtual_server_id_;

            std::shared_ptr<GroupManager> parent_manager_;

            std::shared_ptr<ServerGroupManager> server_groups_{};
            std::shared_ptr<ChannelGroupManager> channel_groups_{};

            GroupAssignmentManager assignment_manager_;

            [[nodiscard]] sql::SqlManager* sql_manager();
            [[nodiscard]] ServerId server_id();
    };
}