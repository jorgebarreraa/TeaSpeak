//
// Created by WolverinDEV on 27/02/2021.
//

#include "./PermissionCalculator.h"
#include "./InstanceHandler.h"
#include "./groups/Group.h"
#include "./client/DataClient.h"
#include <PermissionManager.h>
#include <log/LogUtils.h>
#include <src/groups/GroupManager.h>

using namespace ts::server;
using ts::permission::PermissionType;
using ts::permission::v2::PermissionFlaggedValue;

ClientPermissionCalculator::ClientPermissionCalculator(DataClient *client, ChannelId channel_id) {
    /* Note: Order matters! */
    this->initialize_client(client);
    this->initialize_default_groups(client->getServer());

    auto server = client->getServer();
    if(server && channel_id > 0) {
        std::shared_ptr<BasicChannel> channel{};
        try {
            std::shared_lock channel_lock{server->get_channel_tree_lock()};
            this->channel_ = server->getChannelTree()->findChannel(channel_id);
        } catch (std::system_error& e) {
            if(e.code() != std::errc::resource_deadlock_would_occur) {
                throw;
            }

            /* tree already write locked, no need to lock it again */
            this->channel_ = server->getChannelTree()->findChannel(channel_id);
        }
    }
}

ClientPermissionCalculator::ClientPermissionCalculator(DataClient *client, const std::shared_ptr<BasicChannel> &channel) {
    /* Note: Order matters! */
    this->initialize_client(client);
    this->initialize_default_groups(client->getServer());
    this->channel_ = channel;
}

ClientPermissionCalculator::ClientPermissionCalculator(
        const std::shared_ptr<VirtualServer> &server,
        ClientDbId client_database_id,
        ClientType client_type,
        ChannelId channel_id) {

    this->client_database_id = client_database_id;
    this->client_type = client_type;

    if(server) {
        this->virtual_server_id = server->getServerId();
        this->group_manager_ = server->group_manager();

        try {
            std::shared_lock channel_lock{server->get_channel_tree_lock()};
            this->channel_ = server->getChannelTree()->findChannel(channel_id);
        } catch (std::system_error& e) {
            if(e.code() != std::errc::resource_deadlock_would_occur) {
                throw;
            }

            /* tree already write locked, no need to lock it again */
            this->channel_ = server->getChannelTree()->findChannel(channel_id);
        }
    } else {
        this->virtual_server_id = 0;
        this->group_manager_ = serverInstance->group_manager();
    }

    this->initialize_default_groups(server);
}

void ClientPermissionCalculator::initialize_client(DataClient* client) {
    this->virtual_server_id = client->getServerId();
    this->client_database_id = client->getClientDatabaseId();
    this->client_type = client->getType();
    this->client_permissions_ = client->permissions();

    auto server = client->getServer();
    if(server) {
        this->group_manager_ = server->group_manager();
    } else {
        this->group_manager_ = serverInstance->group_manager();
    }
}

void ClientPermissionCalculator::initialize_default_groups(const std::shared_ptr<VirtualServer> &server) {
    if(this->client_type == ClientType::CLIENT_QUERY) {
        this->default_server_group = []{ return serverInstance->guest_query_group(); };
    } else if(server) {
        this->default_server_group = [server]{ return server->default_server_group(); };
    }

    if(server) {
        this->default_channel_group = [server]{ return server->default_channel_group(); };
    }
}

PermissionFlaggedValue ClientPermissionCalculator::calculate_permission(permission::PermissionType permission,
                                                                                        bool granted) {
    auto result = this->calculate_permissions({permission}, granted);
    if(result.empty()) {
        return { permNotGranted, false };
    }

    return result.front().second;
}

std::vector<std::pair<PermissionType, PermissionFlaggedValue>> ClientPermissionCalculator::calculate_permissions(
        const std::deque<permission::PermissionType> &permissions,
        bool calculate_granted
) {
    if(permissions.empty()) {
        return {};
    }

    std::vector<std::pair<PermissionType, PermissionFlaggedValue>> result;
    result.reserve(permissions.size());

    auto client_permissions = this->client_permissions();
    assert(client_permissions);

    /*
     * server_group_data[0] := Server group id
     * server_group_data[1] := Skip flag
     * server_group_data[2] := Negate flag
     * server_group_data[3] := Permission value
     */
    typedef std::tuple<GroupId, bool, bool, permission::PermissionValue> GroupData;
    bool server_group_data_initialized = false;
    std::vector<GroupData> server_group_data;
    GroupData* active_server_group;

    auto initialize_group_data = [&](const permission::PermissionType& permission_type) {
        server_group_data_initialized = true;
        active_server_group = nullptr;

        auto assigned_groups = this->assigned_server_groups();
        server_group_data.resize(assigned_groups.size());
        auto it = server_group_data.begin();
        for(auto& group : assigned_groups) {
            auto group_permissions = group->permissions();
            auto permission_flags = group_permissions->permission_flags(permission_type);

            auto flag_set = calculate_granted ? permission_flags.grant_set : permission_flags.value_set;
            if(!flag_set) {
                continue;
            }

            //TODO: Test if there is may a group channel permissions
            auto value = group_permissions->permission_values(permission_type);
            *it = std::make_tuple(group->group_id(), (bool) permission_flags.skip, (bool) permission_flags.negate, calculate_granted ? value.grant : value.value);
            it++;
        }
        if(it == server_group_data.begin()) {
            return; /* no server group has that permission */
        }

        server_group_data.erase(it, server_group_data.end()); /* remove unneeded */

        auto found_negate = false;
        for(auto& group : server_group_data) {
            if(std::get<2>(group)) {
                found_negate = true;
                break;
            }
        }

        if(found_negate) {
            server_group_data.erase(remove_if(server_group_data.begin(), server_group_data.end(), [](auto data) { return !std::get<2>(data); }), server_group_data.end());
            logTrace(this->virtual_server_id, "[Permission] Found negate flag within server groups. Groups left: {}", server_group_data.size());
            if(server_group_data.empty()) {
                logTrace(this->virtual_server_id, "[Permission] After non negated groups have been kicked out the negated groups are empty! This should not happen! Permission: {}, Client ID: {}", permission_type, this->client_database_id);
            }
            permission::PermissionValue current_lowest = 0;
            for(auto& group : server_group_data) {
                if(!active_server_group || (std::get<3>(group) < current_lowest && std::get<3>(group) != -1)) {
                    current_lowest = std::get<3>(group);
                    active_server_group = &group;
                }
            }
        } else {
            permission::PermissionValue current_highest = 0;
            for(auto& group : server_group_data) {
                if(!active_server_group || (std::get<3>(group) > current_highest || std::get<3>(group) == -1)) {
                    current_highest = std::get<3>(group);
                    active_server_group = &group;
                }
            }
        }
    };

    for(const auto& permission : permissions) {
        server_group_data_initialized = false; /* reset all group data */
        auto client_permission_flags = client_permissions->permission_flags(permission);
        /* lets try to resolve the channel specific permission */
        if(this->channel_ && client_permission_flags.channel_specific) {
            auto data = client_permissions->channel_permission(permission, this->channel_->channelId());
            if(calculate_granted ? data.flags.grant_set : data.flags.value_set) {
                result.push_back({permission, {calculate_granted ? data.values.grant : data.values.value, true}});
                logTrace(this->virtual_server_id, "[Permission] Calculation for client {} of permission {} returned {} (Client channel permission)", this->client_database_id, permission::resolvePermissionData(permission)->name, data.values.value);
                continue;
            }
        }


        bool skip_channel_permissions = !this->channel_ || this->has_global_skip_permission();
        if(!skip_channel_permissions) {
            /* We dont have a global skip flag. Lets see if the target permission has skip enabled */
            if(calculate_granted ? client_permission_flags.grant_set : client_permission_flags.value_set) {
                /* okey the client has the permission, this counts */
                skip_channel_permissions = client_permission_flags.skip;
            } else {
                if(!server_group_data_initialized) {
                    initialize_group_data(permission);
                }

                if(active_server_group) {
                    skip_channel_permissions = std::get<1>(*active_server_group);
                }
            }
        }

        if(!skip_channel_permissions) {
            /* lookup the channel group */
            {
                auto channel_assignment = this->assigned_channel_group();
                if(channel_assignment) {
                    auto group_permissions = channel_assignment->permissions();
                    auto permission_flags = group_permissions->permission_flags(permission);

                    auto flag_set = calculate_granted ? permission_flags.grant_set : permission_flags.value_set;
                    if(flag_set) {
                        auto value = group_permissions->permission_values(permission);
                        result.push_back({permission, {calculate_granted ? value.grant : value.value, true}});
                        logTrace(this->virtual_server_id, "[Permission] Calculation for client {} of permission {} returned {} (Channel group permission)", this->client_database_id, permission::resolvePermissionData(permission)->name, calculate_granted ? value.grant : value.value);
                        continue;
                    }
                }
            }

            /* lookup the channel permissions. Whyever? */
            if(this->channel_) {
                auto channel_permissions = this->channel_->permissions();
                auto data = calculate_granted ? channel_permissions->permission_granted_flagged(permission) : channel_permissions->permission_value_flagged(permission);
                if(data.has_value) {
                    result.push_back({permission, {data.value, true}});
                    logTrace(this->virtual_server_id, "[Permission] Calculation for client {} of permission {} returned {} (Channel permission)", this->client_database_id, permission::resolvePermissionData(permission)->name, data.value);
                    continue;
                }
            }
        }

        if(calculate_granted ? client_permission_flags.grant_set : client_permission_flags.value_set) {
            auto client_value = client_permissions->permission_values(permission);
            result.push_back({permission, {calculate_granted ? client_value.grant : client_value.value, true}});
            logTrace(this->virtual_server_id, "[Permission] Calculation for client {} of permission {} returned {} (Client permission)", this->client_database_id, permission::resolvePermissionData(permission)->name, client_value.value);
            continue;
        }

        if(!server_group_data_initialized) {
            initialize_group_data(permission);
        }

        if(active_server_group) {
            result.push_back({permission, {get<3>(*active_server_group), true}});
            logTrace(this->virtual_server_id, "[Permission] Calculation for client {} of permission {} returned {} (Server group permission of group {})", this->client_database_id, permission::resolvePermissionData(permission)->name, get<3>(*active_server_group), get<0>(*active_server_group));
            continue;
        }

        logTrace(this->virtual_server_id, "[Permission] Calculation for client {} of permission {} returned in no permission.", this->client_database_id, permission::resolvePermissionData(permission)->name);
        result.push_back({permission, { permNotGranted, false }});
    }

    return result;
}

const std::shared_ptr<ts::permission::v2::PermissionManager> & ClientPermissionCalculator::client_permissions() {
    if(!this->client_permissions_) {
        this->client_permissions_ = serverInstance->databaseHelper()->loadClientPermissionManager(this->virtual_server_id, this->client_database_id);

        if(!this->client_permissions_) {
            logCritical(this->virtual_server_id, "Failed to load client permissions for client {}. Using empty permission set.", this->client_database_id);
            this->client_permissions_ = std::make_shared<permission::v2::PermissionManager>();
        }
    }

    return this->client_permissions_;
}

const std::vector<std::shared_ptr<groups::ServerGroup>>& ClientPermissionCalculator::assigned_server_groups() {
    if(this->assigned_server_groups_.has_value()) {
        return *this->assigned_server_groups_;
    }

    auto assignments = this->group_manager_->assignments().server_groups_of_client(groups::GroupAssignmentCalculateMode::GLOBAL, this->client_database_id);

    this->assigned_server_groups_.emplace();
    this->assigned_server_groups_->reserve(assignments.size());

    for(const auto& group_id : assignments) {
        auto group = this->group_manager_->server_groups()->find_group(groups::GroupCalculateMode::GLOBAL, group_id);
        if(group) {
            this->assigned_server_groups_->push_back(group);
        }
    }
    if(this->assigned_server_groups_->empty() && this->default_server_group) {
        auto default_group = this->default_server_group();
        if(default_group) {
            this->assigned_server_groups_->push_back(default_group);
        }
    }

    return *this->assigned_server_groups_;
}

const std::shared_ptr<groups::ChannelGroup>& ClientPermissionCalculator::assigned_channel_group() {
    if(this->assigned_channel_group_.has_value()) {
        return *this->assigned_channel_group_;
    }

    this->assigned_channel_group_.emplace();
    if(!this->channel_) {
        return *this->assigned_channel_group_;
    }

    std::shared_ptr<BasicChannel> inherited_channel{this->channel_};
    auto channel_group_assignment = this->group_manager_->assignments().calculate_channel_group_of_client(
            groups::GroupAssignmentCalculateMode::GLOBAL,
            this->client_database_id,
            inherited_channel
    );

    if(channel_group_assignment.has_value()) {
        assert(inherited_channel);
        auto channel_group = this->group_manager_->channel_groups()->find_group(groups::GroupCalculateMode::GLOBAL, *channel_group_assignment);
        if(channel_group) {
            this->assigned_channel_group_.emplace(std::move(channel_group));
            logTrace(this->virtual_server_id, "[Permission] Using calculated channel group with id {} (Channel id: {}, Inherited channel id: {}).",
                     *channel_group_assignment, this->channel_->channelId(), inherited_channel->channelId());
        } else {
            logTrace(this->virtual_server_id, "[Permission] Missing calculated channel group with id {} (Channel id: {}, Inherited channel id: {}). Using default channel group.",
                     *channel_group_assignment, this->channel_->channelId(), inherited_channel->channelId());
        }
    } else {
        logTrace(this->virtual_server_id, "[Permission] Using default channel group.");
    }

    if(!this->assigned_channel_group_.has_value()) {
        assigned_channel_group_.emplace(
                this->default_channel_group()
        );
    }

    return *this->assigned_channel_group_;
}

bool ClientPermissionCalculator::has_global_skip_permission() {
    if(this->skip_enabled.has_value()) {
        return *this->skip_enabled;
    }

    /* test for skip permission within the client permission manager */
    {
        auto client_permissions = this->client_permissions();
        auto skip_value = client_permissions->permission_value_flagged(permission::b_client_skip_channelgroup_permissions);
        if(skip_value.has_value) {
            this->skip_enabled = std::make_optional(permission::v2::permission_granted(1, skip_value));
            logTrace(this->virtual_server_id, "[Permission] Found skip permission in client permissions. Value: {}", *this->skip_enabled);
        }
    }

    /* test for skip permission within all server groups */
    if(!this->skip_enabled.has_value()) {
        for(const auto& assignment : this->assigned_server_groups()) {
            auto group_permissions = assignment->permissions();
            auto flagged_value = group_permissions->permission_value_flagged(permission::b_client_skip_channelgroup_permissions);
            if(flagged_value.has_value) {
                this->skip_enabled = std::make_optional(permission::v2::permission_granted(1, flagged_value));
                if(*this->skip_enabled) {
                    logTrace(this->virtual_server_id, "[Permission] Found skip permission in client server group. Group: {} ({})", assignment->group_id(), assignment->display_name());
                    break;
                }
            }
        }
    }

    if(!this->skip_enabled.has_value()) {
        this->skip_enabled = std::make_optional(false);
    }

    return *this->skip_enabled;
}

bool ClientPermissionCalculator::permission_granted(const permission::PermissionType &permission,
                                                    const permission::PermissionValue &required_value, bool granted) {
    return this->permission_granted(permission, { required_value, true }, granted);
}

bool ClientPermissionCalculator::permission_granted(const permission::PermissionType &permission,
                                                    const permission::v2::PermissionFlaggedValue &required_value, bool granted) {
    return permission::v2::permission_granted(required_value, this->calculate_permission(permission, granted));
}