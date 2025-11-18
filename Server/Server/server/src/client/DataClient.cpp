#include <log/LogUtils.h>
#include <misc/sassert.h>
#include <misc/hex.h>
#include <misc/base64.h>

#include "./DataClient.h"
#include "../InstanceHandler.h"
#include "../groups/GroupManager.h"
#include "../groups/GroupAssignmentManager.h"

#include "../PermissionCalculator.h"

using namespace std;
using namespace ts;
using namespace ts::server;
using namespace ts::permission;

DataClient::DataClient(sql::SqlManager* database, const std::shared_ptr<VirtualServer>& server) : server(server), sql(database) {
    assert(database);
    this->_properties = DatabaseHelper::default_properties_client(nullptr, ClientType::CLIENT_INTERNAL);
}

DataClient::~DataClient() {
    this->clientPermissions = nullptr;
    this->_properties = nullptr;
}

constexpr static std::string_view kClientLoadCommand{R"(
    SELECT
        `client_database_id`,
        `client_created`,

        `client_last_connected`,
        `client_total_connections`,

        `client_month_upload`,
        `client_month_download`,

        `client_total_upload`,
        `client_total_download`
    FROM `clients_server` WHERE `server_id` = :sid AND `client_unique_id` = :uid
)"};

bool DataClient::loadDataForCurrentServer() {
    auto uniqueId = this->getUid();
    if(uniqueId.empty()) {
        return false;
    }

    auto ref_server = this->server;
    auto server_id = ref_server ? ref_server->getServerId() : 0;

    properties()[property::CLIENT_DATABASE_ID] = 0;
    this->clientPermissions = std::make_shared<permission::v2::PermissionManager>();

    auto properties = this->properties();
    sql::command{this->sql, std::string{kClientLoadCommand}, variable{":uid", uniqueId}, variable{":sid", server_id}}.query([&](int length, std::string* values, std::string* names) {
        auto index{0};

        try {
            assert(names[index] == "client_database_id");
            properties[property::CLIENT_DATABASE_ID] = std::stoull(values[index++]);

            assert(names[index] == "client_created");
            properties[property::CLIENT_CREATED] = std::stoull(values[index++]);

            assert(names[index] == "client_last_connected");
            properties[property::CLIENT_LASTCONNECTED] = std::stoull(values[index++]);

            assert(names[index] == "client_total_connections");
            properties[property::CLIENT_TOTALCONNECTIONS] = std::stoull(values[index++]);

            assert(names[index] == "client_month_upload");
            properties[property::CLIENT_MONTH_BYTES_UPLOADED] = std::stoull(values[index++]);

            assert(names[index] == "client_month_download");
            properties[property::CLIENT_MONTH_BYTES_DOWNLOADED] = std::stoull(values[index++]);

            assert(names[index] == "client_total_upload");
            properties[property::CLIENT_TOTAL_BYTES_UPLOADED] = std::stoull(values[index++]);

            assert(names[index] == "client_total_download");
            properties[property::CLIENT_TOTAL_BYTES_DOWNLOADED] = std::stoull(values[index++]);

            assert(index == length);
        } catch (std::exception& ex) {
            logError(server_id, "Failed to load client {} base properties from database: {} (Index {})",
                     this->getUid(),
                     ex.what(),
                     index - 1
            );
            properties[property::CLIENT_DATABASE_ID] = 0;
        }
    });

    if(this->properties()[property::CLIENT_DATABASE_ID].as_or<ClientDbId>(0) == 0) {
        return false;
    }

    //Load general properties
    std::deque<ts::Property> copied;
    for(const auto& prop : this->_properties->list_properties()){
        if((prop.type().flags & property::FLAG_GLOBAL) == 0) continue;
        if(prop.type().default_value == prop.value()) continue;
        copied.push_back(prop);
    }

    if(!ref_server) {
        if(this->getType() == ClientType::CLIENT_WEB || this->getType() == ClientType::CLIENT_TEAMSPEAK) {
            logCritical(LOG_INSTANCE, "Got a voice or web client, which is unbound to any server!");
        }
        return false;
    }

    this->_properties = serverInstance->databaseHelper()->loadClientProperties(ref_server, this->getClientDatabaseId(), this->getType());

    this->_properties->toggleSave(false);
    for(const auto& e : copied) {
        auto p = this->properties()->get(e.type().type_property, e.type().property_index);
        p = e.value();
        p.setModified(false);
    }
    this->_properties->toggleSave(true);

    this->clientPermissions = serverInstance->databaseHelper()->loadClientPermissionManager(ref_server->getServerId(), this->getClientDatabaseId());

    //Setup / fix stuff
#if 0
    if(!this->properties()[property::CLIENT_FLAG_AVATAR].as<string>().empty()){
        if(
                !ref_server ||
                !serverInstance->getFileServer()->findFile("/avatar_" + this->getAvatarId(),serverInstance->getFileServer()->avatarDirectory(ref_server))) {
            if(config::server::delete_missing_icon_permissions)
                this->properties()[property::CLIENT_FLAG_AVATAR] = "";
        }
    }
#endif

    if(ref_server) {
        this->properties()[property::CLIENT_UNREAD_MESSAGES] = ref_server->letters->unread_letter_count(this->getUid());
    }

    if(this->server) {
        this->temporary_assignments_lock = server->group_manager()->assignments().create_tmp_assignment_lock(this->getClientDatabaseId());
    } else {
        this->temporary_assignments_lock = serverInstance->group_manager()->assignments().create_tmp_assignment_lock(this->getClientDatabaseId());
    }

    return true;
}

std::vector<std::pair<permission::PermissionType, permission::v2::PermissionFlaggedValue>> DataClient::calculate_permissions(
        const std::deque<permission::PermissionType> &permissions,
        ChannelId channel,
        bool granted) {

    ts::server::ClientPermissionCalculator calculator{this, channel};
    return calculator.calculate_permissions(permissions, granted);
}

permission::v2::PermissionFlaggedValue DataClient::calculate_permission(
        permission::PermissionType permission, ChannelId channel, bool granted) {
    ts::server::ClientPermissionCalculator calculator{this, channel};
    return calculator.calculate_permission(permission, granted);
}

std::vector<std::shared_ptr<groups::ServerGroup>> DataClient::assignedServerGroups() {
    auto ref_server = this->server;
    auto group_manager = ref_server ? ref_server->group_manager() : serverInstance->group_manager();

    auto assignments = group_manager->assignments().server_groups_of_client(groups::GroupAssignmentCalculateMode::GLOBAL, this->getClientDatabaseId());

    std::vector<std::shared_ptr<groups::ServerGroup>> result{};
    result.reserve(assignments.size());

    for(const auto& group_id : assignments) {
        auto group = group_manager->server_groups()->find_group(groups::GroupCalculateMode::GLOBAL, group_id);
        if(group) {
            result.push_back(group);
        }
    }

    if(result.empty() && ref_server) {
        auto default_group_id = ref_server->properties()[property::VIRTUALSERVER_DEFAULT_SERVER_GROUP].as_or<GroupId>(0);
        auto default_group = group_manager->server_groups()->find_group(groups::GroupCalculateMode::GLOBAL, default_group_id);
        if(default_group) {
            result.push_back(default_group);
        }
    }

    return result;
}

std::shared_ptr<groups::ChannelGroup> DataClient::assignedChannelGroup(std::shared_ptr<BasicChannel> &channel) {
    auto ref_server = this->server;
    assert(channel);
    if(!channel || !ref_server) {
        return nullptr;
    }

    std::shared_ptr<BasicChannel> original_channel{channel};
    auto group_manager = ref_server ? ref_server->group_manager() : serverInstance->group_manager();
    auto assigned_group_id = group_manager->assignments().calculate_channel_group_of_client(groups::GroupAssignmentCalculateMode::GLOBAL, this->getClientDatabaseId(), channel);

    std::shared_ptr<groups::ChannelGroup> result{};
    if(assigned_group_id.has_value()) {
        result = group_manager->channel_groups()->find_group(groups::GroupCalculateMode::GLOBAL, *assigned_group_id);
    }

    if(!result) {
        result = ref_server->default_channel_group();
        channel = original_channel;
    }

    assert(result);
    return result;
}

bool DataClient::serverGroupAssigned(const shared_ptr<groups::ServerGroup> &group) {
    auto ref_server = this->server;
    auto group_manager = ref_server ? ref_server->group_manager() : serverInstance->group_manager();

    auto assignments = group_manager->assignments().server_groups_of_client(groups::GroupAssignmentCalculateMode::GLOBAL, this->getClientDatabaseId());
    for(const auto& assigned_group_id : assignments) {
        if(assigned_group_id == group->group_id()) {
            return true;
        }
    }

    return false;
}

bool DataClient::channelGroupAssigned(const shared_ptr<groups::ChannelGroup> &group, const shared_ptr<BasicChannel> &channel) {
    if(!channel) {
        return false;
    }

    std::shared_ptr<BasicChannel> interited_channel{channel};
    return this->assignedChannelGroup(interited_channel) == group;
}

std::string DataClient::getAvatarId() {
    return hex::hex(base64::validate(this->getUid()) ? base64::decode(this->getUid()) : this->getUid(), 'a', 'q');
}
