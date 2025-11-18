#pragma once

#include <optional>
#include <mutex>
#include <deque>
#include <string>
#include <memory>
#include <utility>
#include <Definitions.h>
#include <Properties.h>

namespace sql {
    class SqlManager;
}

namespace ts {
    class BasicChannel;
}

namespace ts::server {
    class ConnectedClient;

    namespace groups {
        enum struct GroupAssignmentCalculateMode {
            LOCAL, /* only calculate clients groups for the local server */
            GLOBAL /* use the parent group manager as well, if existing */
        };

        class ServerGroup;
        class ChannelGroup;
        class GroupManager;

        struct ChannelGroupAssignment {
            ClientDbId client_database_id;
            ChannelId channel_id;
            GroupId group_id;
        };

        struct ServerGroupAssignment {
            ClientDbId client_database_id{0};
            GroupId group_id{0};

            /* both fields will only be set on extended info */
            std::optional<std::string> client_display_name{};
            std::optional<std::string> client_unique_id{};
        };

        typedef void TemporaryAssignmentsLock;

        struct InternalChannelGroupAssignment;
        struct InternalServerGroupAssignment;

        enum struct GroupAssignmentResult {
            SUCCESS,
            ADD_ALREADY_MEMBER_OF_GROUP,
            REMOVE_NOT_MEMBER_OF_GROUP,
            SET_ALREADY_MEMBER_OF_GROUP
        };

        class GroupAssignmentManager {
                constexpr static bool kCacheAllClients{true};
            public:
                explicit GroupAssignmentManager(GroupManager* /* manager */);
                ~GroupAssignmentManager();

                /* general load/initialize methods */
                bool initialize(std::string& /* error */);
                bool load_data(std::string& /* error */);
                void unload_data();

                void reset_all();

                /* client specific cache methods */
                void enable_cache_for_client(GroupAssignmentCalculateMode /* mode */, ClientDbId /* client database id */);
                void disable_cache_for_client(GroupAssignmentCalculateMode /* mode */, ClientDbId /* client database id */);

                /* info/query methods */
                [[nodiscard]] std::vector<GroupId> server_groups_of_client(GroupAssignmentCalculateMode /* mode */, ClientDbId /* client database id */);
                [[nodiscard]] std::vector<ChannelGroupAssignment> exact_channel_groups_of_client(GroupAssignmentCalculateMode /* mode */, ClientDbId /* client database id */);
                [[nodiscard]] std::optional<ChannelGroupAssignment> exact_channel_group_of_client(GroupAssignmentCalculateMode /* mode */, ClientDbId /* client database id */, ChannelId /* channel id */);

                /**
                 * Calculate the target channel group for the client.
                 * The parameters `target channel` will contain the channel where the group has been inherited from.
                 * Note: `target channel` will be altered if the result is empty.
                 * @return The target channel group id
                 */
                [[nodiscard]] std::optional<GroupId> calculate_channel_group_of_client(GroupAssignmentCalculateMode /* mode */, ClientDbId /* client database id */, std::shared_ptr<BasicChannel>& /* target channel */);

                [[nodiscard]] std::deque<ServerGroupAssignment> server_group_clients(GroupId /* group id */, bool /* full info */);
                [[nodiscard]] std::deque<std::tuple<GroupId, ChannelId, ClientDbId>> channel_group_list(GroupId /* group id */, ChannelId /* channel id */, ClientDbId /* client database id */);

                [[nodiscard]] bool is_server_group_empty(GroupId /* group id */);
                [[nodiscard]] bool is_channel_group_empty(GroupId /* group id */);

                /* change methods */
                GroupAssignmentResult add_server_group(ClientDbId /* client database id */, GroupId /* group id */, bool /* temporary assignment */);
                GroupAssignmentResult remove_server_group(ClientDbId /* client database id */, GroupId /* group id */);

                GroupAssignmentResult set_channel_group(ClientDbId /* client database id */, GroupId /* group id */, ChannelId /* channel id */, bool /* temporary assignment */);

                [[nodiscard]] std::shared_ptr<TemporaryAssignmentsLock> create_tmp_assignment_lock(ClientDbId /* client database id */);
                void cleanup_temporary_channel_assignment(ClientDbId /* client database id */, ChannelId /* channel */);

                void handle_channel_deleted(ChannelId /* channel id */);
                void handle_server_group_deleted(GroupId /* group id */);
                void handle_channel_group_deleted(GroupId /* group id */);
            private:
                GroupManager* manager_;

                struct ClientCache {
                    ClientDbId client_database_id{0};
                    size_t use_count{0};

                    std::deque<std::unique_ptr<InternalChannelGroupAssignment>> channel_group_assignments{};
                    std::deque<std::unique_ptr<InternalServerGroupAssignment>> server_group_assignments{};

                    std::weak_ptr<TemporaryAssignmentsLock> temp_assignment_lock{};
                };

                std::shared_ptr<std::mutex> client_cache_lock{};
                std::deque<std::shared_ptr<ClientCache>> client_cache{};


                [[nodiscard]] sql::SqlManager* sql_manager();
                [[nodiscard]] ServerId server_id();
        };

    }
}