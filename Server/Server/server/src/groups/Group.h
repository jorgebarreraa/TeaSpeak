#pragma once

#include <memory>
#include <PermissionManager.h>

namespace ts::server::groups {
    enum GroupType {
        GROUP_TYPE_TEMPLATE = 0x00,
        GROUP_TYPE_NORMAL = 0x01,
        GROUP_TYPE_QUERY = 0x02,

        GROUP_TYPE_UNKNOWN = 0xFF
    };

    enum GroupNameMode {
        GROUP_NAME_MODE_HIDDEN = 0x00,
        GROUP_NAME_MODE_BEFORE = 0x01,
        GROUP_NAME_MODE_BEHIND = 0x02
    };

    typedef uint32_t GroupSortId;
    typedef uint32_t GroupIconId;

    class AbstractGroupManager;

    class Group {
            friend class AbstractGroupManager;

        public:
            Group(ServerId /* server id */, GroupId /* id */, GroupType /* type */, std::string /* name */,
                  std::shared_ptr<permission::v2::PermissionManager> /* permissions */);

            virtual ~Group() = default;

            /* information getters */
            [[nodiscard]] inline ServerId virtual_server_id() const { return this->virtual_server_id_; }

            [[nodiscard]] inline GroupId group_id() const { return this->group_id_; }

            [[nodiscard]] inline GroupType group_type() const { return this->type_; }

            [[nodiscard]] inline const std::string &display_name() const { return this->name_; }

            /* we're not returning a cr here because the permissions might get reloaded */
            [[nodiscard]] inline std::shared_ptr<permission::v2::PermissionManager>
            permissions() { return this->permissions_; }

            [[nodiscard]] inline GroupNameMode name_mode() const {
                assert(this->permissions_);
                auto value = this->permissions_->permission_value_flagged(permission::i_group_show_name_in_tree);
                return value.has_value ? (GroupNameMode) value.value : GroupNameMode::GROUP_NAME_MODE_HIDDEN;
            }

            [[nodiscard]] inline GroupSortId sort_id() const {
                assert(this->permissions_);
                auto value = this->permissions_->permission_value_flagged(permission::i_group_sort_id);
                return value.has_value ? value.value : 0;
            }

            [[nodiscard]] inline GroupIconId icon_id() const {
                assert(this->permissions_);
                auto value = this->permissions_->permission_value_flagged(permission::i_icon_id);
                return value.has_value ? value.value : 0;
            }

            [[nodiscard]] inline bool is_permanent() {
                auto permission_manager = this->permissions_;
                assert(permission_manager);
                const auto data = permission_manager->permission_value_flagged(permission::b_group_is_permanent);
                return data.has_value ? data.value == 1 : false;
            }

            [[nodiscard]] inline permission::PermissionValue update_type() {
                auto permission_manager = this->permissions_;
                assert(permission_manager);
                const auto data = permission_manager->permission_value_flagged(permission::i_group_auto_update_type);
                return data.has_value ? data.value : 0;
            }

            [[nodiscard]] inline bool permission_granted(const permission::PermissionType &permission,
                                                         const permission::v2::PermissionFlaggedValue &granted_value,
                                                         bool require_granted_value) {
                auto permission_manager = this->permissions_;;
                assert(permission_manager);
                const auto data = permission_manager->permission_value_flagged(permission);
                if (!data.has_value) {
                    return !require_granted_value || granted_value.has_value;
                }
                if (!granted_value.has_value) {
                    return false;
                }
                if (data.value == -1) {
                    return granted_value.value == -1;
                }
                return granted_value.value >= data.value;
            }

        private:
            const ServerId virtual_server_id_;
            const GroupId group_id_;
            const GroupType type_;
            std::string name_;

            std::shared_ptr<permission::v2::PermissionManager> permissions_;

            void set_permissions(const std::shared_ptr<permission::v2::PermissionManager> & /* permissions */);
    };

    class ServerGroup : public Group {
        public:
            ServerGroup(ServerId /* server id */, GroupId /* id */, GroupType /* type */, std::string /* name */,
                        std::shared_ptr<permission::v2::PermissionManager> /* permissions */);
    };

    class ChannelGroup : public Group {
        public:
            ChannelGroup(ServerId /* server id */, GroupId /* id */, GroupType /* type */, std::string /* name */,
                         std::shared_ptr<permission::v2::PermissionManager> /* permissions */);
    };
}
DEFINE_TRANSFORMS(ts::server::groups::GroupType, uint8_t);
DEFINE_TRANSFORMS(ts::server::groups::GroupNameMode, uint8_t);