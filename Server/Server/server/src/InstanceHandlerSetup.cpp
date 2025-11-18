#include <deque>
#include <tuple>
#include <log/LogUtils.h>
#include "InstanceHandler.h"
#include "src/client/InternalClient.h"
#include "src/server/QueryServer.h"
#include "./groups/GroupManager.h"

using namespace std;
using namespace std::chrono;
using namespace ts;
using namespace ts::server;

#define TEST_COMMENT if(line.find('#') == 0 || line.empty()) continue
#define PERMISSION_TEMPLATE_FILE "resources/permissions.template"

struct GroupInfo {
    /**
     * 0 = Query
     * 1 = Server
     * 2 = Channel
     */
    int target;
    std::deque<string> properties;
    std::string name;
    /* permission type, value, granted, skip, negate */
    std::deque<std::tuple<permission::PermissionType, permission::PermissionValue, permission::PermissionValue, bool, bool>> permissions;
};

/* TODO may use a transaction here? */
bool InstanceHandler::setupDefaultGroups() {
    debugMessage(LOG_INSTANCE, "Creating new instance groups");
    deque<shared_ptr<GroupInfo>> groups;

    ifstream in(PERMISSION_TEMPLATE_FILE);
    if(!in) {
        logCritical(LOG_INSTANCE, "Could not open default permissions file {}", PERMISSION_TEMPLATE_FILE);
        return false;
    }
    string line;
    while(getline(in, line)){
        TEST_COMMENT;

        if(line != "--start") {
            logCritical(LOG_INSTANCE, R"(Permission template file contains invalid start line ("{}", expected "{}")!)", line, "--start");
            return false;
        }
        auto group = make_shared<GroupInfo>();
        while(true){
            getline(in, line);
            TEST_COMMENT;

            if(line == "--end") break;
            if(line.find("name:")  == 0) {
                group->name = line.substr(5);
                continue;
            }
            if(line.find("target:")  == 0) {
                group->target = stoi(line.substr(7));
                continue;
            }
            if(line.find("property:")  == 0) {
                group->properties.push_back(line.substr(9));
                continue;
            }

            if(line.find("permission:") == 0) {
                line = line.substr(11);
                auto assign_index = line.find('=');

                string permission_name = line.substr(0, assign_index);
                string string_value = line.substr(assign_index + 1);
                string string_granted, string_skip, string_negate;

                if(string_value.find(',') != -1) {
                    string_granted = string_value.substr(string_value.find(',') + 1);
                    string_value = string_value.substr(0, string_value.find(','));
                }
                if(string_granted.find(',') != -1) {
                    string_skip = string_granted.substr(string_granted.find(',') + 1);
                    string_granted = string_granted.substr(0, string_granted.find(','));
                }
                if(string_skip.find(',') != -1) {
                    string_negate = string_skip.substr(string_skip.find(',') + 1);
                    string_skip = string_skip.substr(0, string_skip.find(','));
                }

                auto permInfo = permission::resolvePermissionData(permission_name);
                if(permInfo->type == permission::unknown){
                    logError(LOG_INSTANCE, "Default permission file contains unknown permission. Key: {}", permission_name);
                    continue;
                }

                permission::PermissionValue permission_value;
                try {
                    permission_value = stoi(string_value);
                } catch(std::exception& ex) {
                    logError(LOG_INSTANCE, "Failed to parse value for key {}. Value: {}", permission_name, string_value);
                    continue;
                }

                permission::PermissionValue permission_granted = permNotGranted;
                if(!string_granted.empty()) {
                    try {
                        permission_granted = stoi(string_granted);
                    } catch(std::exception& ex) {
                        logError(LOG_INSTANCE, "Failed to parse granted value for key {}. Value: {}", permission_name, string_granted);
                        continue;
                    }
                }

                bool flag_skip = string_skip == "true" || string_skip == "1";
                bool flag_negate = string_negate == "true" || string_negate == "1";

                group->permissions.emplace_back(make_tuple(permInfo->type, permission_value, permission_granted, flag_skip, flag_negate));
            }
        }
        groups.push_back(group);
    }

    debugMessage(LOG_INSTANCE, "Read {} default groups", groups.size());
    for(const auto& info : groups) {
        debugMessage(LOG_INSTANCE, "Creating default group {} with type {}", info->name, to_string(info->target));
        //Query groups

        std::shared_ptr<groups::Group> created_group{};
        groups::GroupCreateResult create_result{};

        if(info->target == 2) {
            std::shared_ptr<groups::ChannelGroup> c_group{};
            create_result = serverInstance->group_manager()->channel_groups()->create_group(groups::GroupType::GROUP_TYPE_TEMPLATE, info->name, c_group);
            created_group = c_group;
        } else {
            std::shared_ptr<groups::ServerGroup> s_group{};
            create_result = serverInstance->group_manager()->server_groups()->create_group(info->target == 0 ? groups::GroupType::GROUP_TYPE_QUERY : groups::GroupType::GROUP_TYPE_TEMPLATE, info->name, s_group);
            created_group = s_group;
        }

        switch(create_result) {
            case groups::GroupCreateResult::SUCCESS:
                break;

            case groups::GroupCreateResult::DATABASE_ERROR:
                logCritical(LOG_INSTANCE, "Failed to insert template group {} (Database error)", info->name);
                return false;

            case groups::GroupCreateResult::NAME_TOO_LONG:
            case groups::GroupCreateResult::NAME_ALREADY_IN_USED:
            case groups::GroupCreateResult::NAME_TOO_SHORT:
                logCritical(LOG_INSTANCE, "Failed to insert template group {} (Name issue)", info->name);
                return false;

            default:
                logCritical(LOG_INSTANCE, "Failed to insert template group {} (Unkown error)", info->name);
                return false;
        }

        for(auto perm : info->permissions) {
            created_group->permissions()->set_permission(get<0>(perm), {get<1>(perm), get<2>(perm)}, get<1>(perm) == permNotGranted ? permission::v2::do_nothing : permission::v2::set_value, get<2>(perm) == permNotGranted ? permission::v2::do_nothing : permission::v2::set_value, get<3>(perm), get<4>(perm));
        }

        for(const auto& property : info->properties) {
            const auto& prop = property::find<property::InstanceProperties>(property);
            if(prop.is_undefined()) {
                logCritical(LOG_INSTANCE, "Invalid template property name: " + property);
            } else {
                this->properties()[prop] = created_group->group_id();
            }
        }
    }
    this->save_group_permissions();
    this->getSql()->pool->threads()->wait_for(); //Wait for all permissions to flush
    return true;
}
