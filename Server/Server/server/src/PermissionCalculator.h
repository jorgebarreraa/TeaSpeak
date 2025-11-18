#pragma once

#include <Definitions.h>
#include <PermissionManager.h>
#include <vector>
#include <memory>

namespace ts {
    class BasicChannel;
}

namespace ts::server {
    class DataClient;
    class VirtualServer;

    namespace groups {
        class ChannelGroup;
        class ServerGroup;
        class GroupManager;
    }

    /**
     * Helper for calculating the client permissions for a certain channel.
     * Note: All functions are not thread save!
     */
    class ClientPermissionCalculator {
        public:
            /* When providing the pointer to the channel the channel tree **should** not be locked in any way! */
            explicit ClientPermissionCalculator(DataClient* /* client */, const std::shared_ptr<BasicChannel>& /* target channel */);
            explicit ClientPermissionCalculator(DataClient* /* client */, ChannelId /* target channel id */);
            explicit ClientPermissionCalculator(
                    const std::shared_ptr<VirtualServer>& /* server */,
                    ClientDbId /* client database id */,
                    ClientType /* client type */,
                    ChannelId /* target channel id */
            );

            /**
             * Calculate the given permissions.
             * This method can be called from everywhere without any locking needed.
             * @param granted
             * @return
             */
            [[nodiscard]] permission::v2::PermissionFlaggedValue calculate_permission(
                    permission::PermissionType,
                    bool granted = false
            );

            /**
             * Calculate the given permissions.
             * This method can be called from everywhere without any locking needed.
             * @param channel
             * @param calculate_granted
             * @return
             */
            [[nodiscard]] std::vector<std::pair<permission::PermissionType, permission::v2::PermissionFlaggedValue>> calculate_permissions(
                    const std::deque<permission::PermissionType>&,
                    bool calculate_granted = false
            );

            /**
             *  Test if the target has the target permission granted.
             *  If the client does not have any value assigned for the permission `false` will be returned.
             */
            [[nodiscard]] bool permission_granted(
                    const permission::PermissionType& /* target permission */,
                    const permission::PermissionValue& /* required value */,
                    bool /* granted permission */ = false
            );

            /**
             *  Test if the target has the target permission granted.
             *  If the client does not have any value assigned for the permission,
             *  `true` will be returned if the `required value` contains no value
             *  otherwise false will be returned.
             *
             *  This method should be used when testing permissions which are allowed by default except if they're
             *  specified otherwise. An example permission would be if we're testing against the channel needed join power.
             */
            [[nodiscard]] bool permission_granted(
                    const permission::PermissionType& /* target permission */,
                    const permission::v2::PermissionFlaggedValue& /* required value */,
                    bool /* granted permission */ = false
            );

            //const PermissionValue& required, const PermissionFlaggedValue& given, bool requires_given = true
        private:
            /* given fields */
            ServerId virtual_server_id;
            ClientDbId client_database_id;
            ClientType client_type;
            std::shared_ptr<BasicChannel> channel_;

            std::shared_ptr<groups::GroupManager> group_manager_{};
            std::function<std::shared_ptr<groups::ChannelGroup>()> default_channel_group{[]{ return nullptr; }};
            std::function<std::shared_ptr<groups::ServerGroup>()> default_server_group{[]{ return nullptr; }};

            /* fields which will be set when calculating permissions */
            std::shared_ptr<permission::v2::PermissionManager> client_permissions_{};

            std::optional<bool> skip_enabled{};

            std::optional<std::shared_ptr<groups::ChannelGroup>> assigned_channel_group_{};
            std::optional<std::vector<std::shared_ptr<groups::ServerGroup>>> assigned_server_groups_{};

            void initialize_client(DataClient* /* client */);
            void initialize_default_groups(const std::shared_ptr<VirtualServer>& /* server */);

            [[nodiscard]] const std::vector<std::shared_ptr<groups::ServerGroup>>& assigned_server_groups();
            [[nodiscard]] const std::shared_ptr<groups::ChannelGroup>& assigned_channel_group();
            [[nodiscard]] const std::shared_ptr<permission::v2::PermissionManager>& client_permissions();
            [[nodiscard]] bool has_global_skip_permission();
    };
}