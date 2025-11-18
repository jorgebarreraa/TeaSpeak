//
// Created by WolverinDEV on 08/03/2021.
//
#include <memory>

#include <bitset>
#include <algorithm>
#include "../ConnectedClient.h"
#include "../InternalClient.h"
#include "../../server/VoiceServer.h"
#include "../voice/VoiceClient.h"
#include "../../InstanceHandler.h"
#include "../../server/QueryServer.h"
#include "../music/MusicClient.h"
#include "../query/QueryClient.h"
#include "../../manager/ConversationManager.h"
#include "../../manager/PermissionNameMapper.h"
#include "../../manager/ActionLogger.h"
#include "../../groups/GroupManager.h"

#include "helpers.h"
#include "./bulk_parsers.h"

#include <misc/digest.h>
#include <misc/rnd.h>
#include <bbcode/bbcodes.h>

using namespace std::chrono;
using namespace std;
using namespace ts;
using namespace ts::server;


command_result ConnectedClient::handleCommandGroupAdd(Command &cmd, GroupTarget group_target) {
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(5);

    log::GroupTarget log_group_target;
    switch(group_target) {
        case GroupTarget::GROUPTARGET_SERVER:
            ACTION_REQUIRES_GLOBAL_PERMISSION(permission::b_virtualserver_channelgroup_create, 1);
            log_group_target = log::GroupTarget::SERVER;
            break;

        case GroupTarget::GROUPTARGET_CHANNEL:
            ACTION_REQUIRES_GLOBAL_PERMISSION(permission::b_virtualserver_servergroup_create, 1);
            log_group_target = log::GroupTarget::CHANNEL;
            break;

        default:
            return ts::command_result{error::vs_critical, "internal invalid group target"};
    }

    std::shared_ptr<groups::GroupManager> group_manager{nullptr};

    log::GroupType log_group_type;
    auto group_type = cmd[0].has("type") ? cmd["type"].as<groups::GroupType>() : groups::GroupType::GROUP_TYPE_NORMAL;
    switch (group_type) {
        case groups::GroupType::GROUP_TYPE_QUERY:
            ACTION_REQUIRES_GLOBAL_PERMISSION(permission::b_serverinstance_modify_querygroup, 1);
            log_group_type = log::GroupType::QUERY;
            group_manager = serverInstance->group_manager();
            break;

        case groups::GroupType::GROUP_TYPE_TEMPLATE:
            ACTION_REQUIRES_GLOBAL_PERMISSION(permission::b_serverinstance_modify_templates, 1);
            log_group_type = log::GroupType::TEMPLATE;
            group_manager = serverInstance->group_manager();
            break;

        case groups::GroupType::GROUP_TYPE_NORMAL:
            if (!this->server) {
                return ts::command_result{error::parameter_invalid, "you cant create normal groups on the template server!"};
            }

            log_group_type = log::GroupType::NORMAL;
            group_manager = this->server->group_manager();
            break;

        case groups::GroupType::GROUP_TYPE_UNKNOWN:
        default:
            return command_result{error::parameter_invalid, "type"};
    }

    std::shared_ptr<groups::Group> group;
    groups::GroupCreateResult result;

    std::string notify_name;
    std::string notify_id_key;
    switch(group_target) {
        case GroupTarget::GROUPTARGET_SERVER: {
            std::shared_ptr<groups::ServerGroup> s_group;
            result = group_manager->server_groups()->create_group(group_type, cmd["name"].string(), s_group);
            group = s_group;

            notify_name = this->notify_response_command("notifyservergroupadded");
            notify_id_key = "sgid";
            break;
        }
        case GroupTarget::GROUPTARGET_CHANNEL: {
            std::shared_ptr<groups::ChannelGroup> c_group;
            result = group_manager->channel_groups()->create_group(group_type, cmd["name"].string(), c_group);
            group = c_group;

            notify_name = this->notify_response_command("notifychannelgroupadded");
            notify_id_key = "cgid";
            break;
        }

        default:
            assert(false);
            result = groups::GroupCreateResult::DATABASE_ERROR;
            break;
    }

    switch(result) {
        case groups::GroupCreateResult::SUCCESS:
            break;

        case groups::GroupCreateResult::NAME_TOO_SHORT:
        case groups::GroupCreateResult::NAME_TOO_LONG:
            return command_result{error::parameter_invalid, "name"};

        case groups::GroupCreateResult::NAME_ALREADY_IN_USED:
            return command_result{error::group_name_inuse};

        case groups::GroupCreateResult::DATABASE_ERROR:
        default:
            return command_result{error::vs_critical};
    }

    assert(group);
    assert(!notify_id_key.empty());
    serverInstance->action_logger()->group_logger.log_group_create(this->getServerId(), this->ref(), log_group_target, log_group_type, group->group_id(), group->display_name(), 0, "");

    {
        ts::command_builder notify{notify_name};
        notify.put_unchecked(0, notify_id_key, group->group_id());
        this->sendCommand(notify);
    }

    group->permissions()->set_permission(permission::b_group_is_permanent, {1, 0}, permission::v2::set_value, permission::v2::do_nothing);

    std::deque<std::shared_ptr<VirtualServer>> server_updates{};
    switch(group_type) {
        case groups::GROUP_TYPE_QUERY:
        case groups::GROUP_TYPE_TEMPLATE:
            server_updates = serverInstance->getVoiceServerManager()->serverInstances();
            break;

        case groups::GROUP_TYPE_NORMAL:
        case groups::GROUP_TYPE_UNKNOWN:
        default:
            server_updates.push_back(this->server);
            break;
    }

    for(const auto& server : server_updates) {
        if(!server) {
            continue;
        }

        switch(group_target) {
            case GroupTarget::GROUPTARGET_SERVER:
                server->enqueue_notify_server_group_list();
                break;

            case GroupTarget::GROUPTARGET_CHANNEL:
                server->enqueue_notify_channel_group_list();
                break;
        }
    }

    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandGroupCopy(Command &cmd, GroupTarget group_target) {
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(5);

    auto ref_server = this->server;
    auto group_manager = ref_server ? ref_server->group_manager() : serverInstance->group_manager();

    log::GroupTarget log_group_target;
    GroupId source_group_id, target_group_id;
    std::shared_ptr<groups::Group> source_group{}, target_group{};
    bool target_group_global;

    switch(group_target) {
        case GroupTarget::GROUPTARGET_SERVER: {
            std::shared_ptr<groups::ServerGroupManager> owning_manager{};

            source_group_id = cmd["ssgid"].as<GroupId>();
            target_group_id = cmd[0].has("tsgid") ? cmd["tsgid"].as<GroupId>() : 0;

            source_group = group_manager->server_groups()->find_group(groups::GroupCalculateMode::GLOBAL, source_group_id);
            target_group = group_manager->server_groups()->find_group_ext(owning_manager, groups::GroupCalculateMode::GLOBAL, target_group_id);
            target_group_global = owning_manager != group_manager->server_groups();

            log_group_target = log::GroupTarget::SERVER;
            break;
        }

        case GroupTarget::GROUPTARGET_CHANNEL: {
            std::shared_ptr<groups::ChannelGroupManager> owning_manager{};

            source_group_id = cmd["scgid"].as<GroupId>();
            target_group_id = cmd[0].has("tcgid") ? cmd["tcgid"].as<GroupId>() : 0;

            source_group = group_manager->channel_groups()->find_group(groups::GroupCalculateMode::GLOBAL, source_group_id);
            target_group = group_manager->channel_groups()->find_group_ext(owning_manager, groups::GroupCalculateMode::GLOBAL, target_group_id);
            target_group_global = owning_manager != group_manager->channel_groups();

            log_group_target = log::GroupTarget::CHANNEL;
            break;
        }
    }

    if (!source_group) {
        return command_result{error::group_invalid_id, "invalid source group id"};
    }

    if (target_group_id > 0 && !target_group) {
        return command_result{error::group_invalid_id, "invalid target group"};
    }

    const auto group_type_modifiable = [&](groups::GroupType type) {
        switch (type) {
            case groups::GroupType::GROUP_TYPE_TEMPLATE:
                if (!permission::v2::permission_granted(1, this->calculate_permission(permission::b_serverinstance_modify_templates, 0))) {
                    return permission::b_serverinstance_modify_templates;
                }

                break;
            case groups::GroupType::GROUP_TYPE_QUERY:
                if (!permission::v2::permission_granted(1, this->calculate_permission(permission::b_serverinstance_modify_querygroup, 0))) {
                    return permission::b_serverinstance_modify_querygroup;
                }

                break;

            case groups::GroupType::GROUP_TYPE_NORMAL:
                if (!permission::v2::permission_granted(1, this->calculate_permission(permission::b_virtualserver_channelgroup_create, 0))) {
                    return permission::b_virtualserver_channelgroup_create;
                }
                break;

            case groups::GroupType::GROUP_TYPE_UNKNOWN:
            default:
                break;
        }
        return permission::undefined;
    };

    {
        auto result = group_type_modifiable(source_group->group_type());
        if (result != permission::undefined) {
            return command_result{result};
        }
    }

    auto global_update = false;
    if (target_group) {
        /* Copy permissions into an already existing group */
        if(!target_group->permission_granted(permission::i_channel_group_needed_modify_power, this->calculate_permission(permission::i_channel_group_modify_power, 0), true)) {
            return ts::command_result{permission::i_channel_group_needed_modify_power};
        }

        {
            auto result = group_type_modifiable(target_group->group_type());
            if (result != permission::undefined) {
                return command_result{result};
            }
        }

        groups::GroupCopyResult result;
        switch(group_target) {
            case GroupTarget::GROUPTARGET_SERVER:
                result = group_manager->server_groups()->copy_group_permissions(source_group_id, target_group_id);
                break;

            case GroupTarget::GROUPTARGET_CHANNEL:
                result = group_manager->channel_groups()->copy_group_permissions(source_group_id, target_group_id);
                break;
                
            default:
                return command_result{error::vs_critical};
        }

        switch(result) {
            case groups::GroupCopyResult::SUCCESS:
                break;

            case groups::GroupCopyResult::UNKNOWN_SOURCE_GROUP:
                return command_result{error::vs_critical, "internal unknown source group"};

            case groups::GroupCopyResult::UNKNOWN_TARGET_GROUP:
                return command_result{error::vs_critical, "internal unknown target group"};

            case groups::GroupCopyResult::DATABASE_ERROR:
                return command_result{error::vs_critical, "database error"};

            case groups::GroupCopyResult::NAME_ALREADY_IN_USE:
                return command_result{error::group_name_inuse};

            case groups::GroupCopyResult::NAME_INVALID:
            default:
                return command_result{error::vs_critical};
        }

        log::GroupType log_group_type;
        switch (target_group->group_type()) {
            case groups::GroupType::GROUP_TYPE_QUERY:
                log_group_type = log::GroupType::QUERY;
                break;

            case groups::GroupType::GROUP_TYPE_TEMPLATE:
                log_group_type = log::GroupType::TEMPLATE;
                break;

            case groups::GroupType::GROUP_TYPE_NORMAL:
            case groups::GroupType::GROUP_TYPE_UNKNOWN:
            default:
                log_group_type = log::GroupType::NORMAL;
                break;
        }

        serverInstance->action_logger()->group_logger.log_group_permission_copy(target_group->group_type() != groups::GroupType::GROUP_TYPE_NORMAL ? 0 : this->getServerId(),
                                                                                this->ref(), log_group_target, log_group_type, target_group->group_id(), target_group->display_name(), source_group->group_id(), source_group->display_name());

        global_update = !this->server || target_group_global;
    } else {
        /* Copy permissions into a new group */
        auto target_type = cmd["type"].as<groups::GroupType>();

        log::GroupType log_group_type;
        switch (target_type) {
            case groups::GroupType::GROUP_TYPE_QUERY:
                log_group_type = log::GroupType::QUERY;
                break;

            case groups::GroupType::GROUP_TYPE_TEMPLATE:
                log_group_type = log::GroupType::TEMPLATE;
                break;

            case groups::GroupType::GROUP_TYPE_NORMAL:
                log_group_type = log::GroupType::NORMAL;
                break;

            case groups::GroupType::GROUP_TYPE_UNKNOWN:
            default:
                return command_result{error::parameter_invalid, "type"};
        }

        {
            auto result = group_type_modifiable(target_type);
            if (result != permission::undefined) {
                return command_result{result};
            }
        }

        if (!ref_server && target_type == groups::GroupType::GROUP_TYPE_NORMAL) {
            return command_result{error::parameter_invalid, "You cant create normal groups on the template server!"};
        }

        std::shared_ptr<groups::Group> created_group{};
        std::string notify_name, notify_id_key;
        groups::GroupCopyResult result;

        switch(group_target) {
            case GroupTarget::GROUPTARGET_SERVER: {
                std::shared_ptr<groups::ServerGroup> s_group{};
                result = group_manager->server_groups()->copy_group(source_group_id, target_type, cmd["name"].string(), s_group);
                created_group = s_group;

                notify_name = this->notify_response_command("notifyservergroupcopied");
                notify_id_key = "sgid";
                break;
            }
            case GroupTarget::GROUPTARGET_CHANNEL: {
                std::shared_ptr<groups::ChannelGroup> c_group{};
                result = group_manager->channel_groups()->copy_group(source_group_id, target_type, cmd["name"].string(), c_group);
                created_group = c_group;

                notify_name = this->notify_response_command("notifychannelgroupcopied");
                notify_id_key = "cgid";
                break;
            }
            default: {
                result = groups::GroupCopyResult::DATABASE_ERROR;
                break;
            }
        }

        switch(result) {
            case groups::GroupCopyResult::SUCCESS:
                break;

            case groups::GroupCopyResult::UNKNOWN_SOURCE_GROUP:
                return command_result{error::vs_critical, "internal unknown source group"};

            case groups::GroupCopyResult::UNKNOWN_TARGET_GROUP:
                return command_result{error::vs_critical, "internal unknown target group"};

            case groups::GroupCopyResult::DATABASE_ERROR:
                return command_result{error::vs_critical, "database error"};

            case groups::GroupCopyResult::NAME_ALREADY_IN_USE:
                return command_result{error::group_name_inuse};

            case groups::GroupCopyResult::NAME_INVALID:
                return command_result{error::parameter_invalid, "name"};

            default:
                return command_result{error::vs_critical};
        }

        assert(!notify_name.empty());
        assert(created_group);
        serverInstance->action_logger()->group_logger.log_group_create(target_type != groups::GroupType::GROUP_TYPE_NORMAL ? 0 : this->getServerId(),
                                                                       this->ref(), log_group_target, log_group_type, created_group->group_id(), cmd["name"], source_group->group_id(), source_group->display_name());

        {
            ts::command_builder notify{notify_name};
            notify.put_unchecked(0, notify_id_key, created_group->group_id());
            this->sendCommand(notify);
        }

        global_update = !this->server;
    }

    std::deque<std::shared_ptr<VirtualServer>> server_updates{};
    if(global_update) {
        server_updates = serverInstance->getVoiceServerManager()->serverInstances();
    } else {
        server_updates.push_back(this->server);
    }

    for(const auto& server : server_updates) {
        if(!server) {
            continue;
        }

        switch(group_target) {
            case GroupTarget::GROUPTARGET_SERVER:
                server->enqueue_notify_server_group_list();
                break;

            case GroupTarget::GROUPTARGET_CHANNEL:
                server->enqueue_notify_channel_group_list();
                break;
        }
    }

    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandGroupRename(Command &cmd, GroupTarget group_target) {
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(5);

    auto group_manager = this->server ? this->server->group_manager() : serverInstance->group_manager();

    log::GroupTarget log_group_target;
    std::shared_ptr<groups::Group> group{};
    GroupId group_id;

    switch(group_target) {
        case GroupTarget::GROUPTARGET_SERVER:
            group_id = cmd["sgid"].as<GroupId>();
            group = group_manager->server_groups()->find_group(groups::GroupCalculateMode::GLOBAL, group_id);

            if(!group) {
                return ts::command_result{error::group_invalid_id};
            }

            ACTION_REQUIRES_GROUP_PERMISSION(group, permission::i_server_group_needed_modify_power, permission::i_server_group_modify_power, true);

            log_group_target = log::GroupTarget::SERVER;
            break;

        case GroupTarget::GROUPTARGET_CHANNEL:
            group_id = cmd["cgid"].as<GroupId>();
            group = group_manager->channel_groups()->find_group(groups::GroupCalculateMode::GLOBAL, group_id);

            if(!group) {
                return ts::command_result{error::group_invalid_id};
            }

            ACTION_REQUIRES_GROUP_PERMISSION(group, permission::i_channel_group_needed_modify_power, permission::i_channel_group_modify_power, true);

            log_group_target = log::GroupTarget::CHANNEL;
            break;

        default:
            return ts::command_result{error::vs_critical, "internal invalid group target"};
    }
    assert(group);

    auto group_type = group->group_type();
    log::GroupType log_group_type;
    if (group_type == groups::GroupType::GROUP_TYPE_QUERY) {
        ACTION_REQUIRES_GLOBAL_PERMISSION(permission::b_serverinstance_modify_querygroup, 1);
        log_group_type = log::GroupType::QUERY;
    } else if (group_type == groups::GroupType::GROUP_TYPE_TEMPLATE) {
        ACTION_REQUIRES_GLOBAL_PERMISSION(permission::b_serverinstance_modify_templates, 1);
        log_group_type = log::GroupType::TEMPLATE;
    } else {
        log_group_type = log::GroupType::NORMAL;
    }

    auto old_name = group->display_name();
    groups::GroupRenameResult rename_result;
    switch(group_target) {
        case GroupTarget::GROUPTARGET_SERVER:
            rename_result = group_manager->server_groups()->rename_group(group->group_id(), cmd["name"].string());
            break;

        case GroupTarget::GROUPTARGET_CHANNEL:
            rename_result = group_manager->channel_groups()->rename_group(group->group_id(), cmd["name"].string());
            break;

        default:
            assert(false);
            rename_result = groups::GroupRenameResult::DATABASE_ERROR;
            break;
    }

    switch(rename_result) {
        case groups::GroupRenameResult::SUCCESS:
            break;

        case groups::GroupRenameResult::INVALID_GROUP_ID:
            return ts::command_result{error::vs_critical, "internal invalid group id"};

        case groups::GroupRenameResult::NAME_INVALID:
            return ts::command_result{error::parameter_invalid, "name"};

        case groups::GroupRenameResult::NAME_ALREADY_USED:
            return ts::command_result{error::group_name_inuse};

        case groups::GroupRenameResult::DATABASE_ERROR:
        default:
            return ts::command_result{error::vs_critical};
    }

    ServerId log_server_id;
    std::deque<std::shared_ptr<VirtualServer>> server_updates{};
    switch(group_type) {
        case groups::GroupType::GROUP_TYPE_QUERY:
        case groups::GroupType::GROUP_TYPE_TEMPLATE:
            log_server_id = 0;
            server_updates = serverInstance->getVoiceServerManager()->serverInstances();
            break;

        case groups::GroupType::GROUP_TYPE_NORMAL:
            /* Normal groups can only exist on regular servers */
            assert(this->server);
            server_updates.push_back(this->server);
            log_server_id = this->getServerId();
            break;

        case groups::GroupType::GROUP_TYPE_UNKNOWN:
        default:
            log_server_id = 0;
            assert(false);
            break;
    }

    serverInstance->action_logger()->group_logger.log_group_rename(log_server_id, this->ref(), log_group_target, log_group_type, group->group_id(), group->display_name(), old_name);

    for(const auto& server : server_updates) {
        if(!server) {
            continue;
        }

        switch(group_target) {
            case GroupTarget::GROUPTARGET_SERVER:
                server->enqueue_notify_server_group_list();
                break;

            case GroupTarget::GROUPTARGET_CHANNEL:
                server->enqueue_notify_channel_group_list();
                break;
        }
    }

    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandGroupDel(Command &cmd, GroupTarget group_target) {
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(5);

    auto ref_server = this->server;
    auto group_manager = ref_server ? ref_server->group_manager() : serverInstance->group_manager();

    log::GroupTarget log_group_target;
    std::shared_ptr<groups::Group> group{};
    GroupId group_id;

    switch (group_target) {
        case GroupTarget::GROUPTARGET_SERVER:
            group_id = cmd["sgid"].as<GroupId>();
            group = group_manager->server_groups()->find_group(groups::GroupCalculateMode::GLOBAL, group_id);

            if(!group) {
                return ts::command_result{error::group_invalid_id};
            }

            ACTION_REQUIRES_GROUP_PERMISSION(group, permission::i_server_group_needed_modify_power, permission::i_server_group_modify_power, true);

            log_group_target = log::GroupTarget::SERVER;
            break;

        case GroupTarget::GROUPTARGET_CHANNEL:
            group_id = cmd["cgid"].as<GroupId>();
            group = group_manager->channel_groups()->find_group(groups::GroupCalculateMode::GLOBAL, group_id);

            if(!group) {
                return ts::command_result{error::group_invalid_id};
            }

            ACTION_REQUIRES_GROUP_PERMISSION(group, permission::i_channel_group_needed_modify_power, permission::i_channel_group_modify_power, true);

            log_group_target = log::GroupTarget::CHANNEL;
            break;

        default:
            return ts::command_result{error::vs_critical, "internal invalid group target"};
    }
    assert(group);

    /* Test if the group is one of the default group */
    switch (group_target) {
        case GroupTarget::GROUPTARGET_SERVER:
            if(this->server) {
                if(this->server->properties()[property::VIRTUALSERVER_DEFAULT_SERVER_GROUP] == group->group_id()) {
                    return command_result{error::parameter_invalid, "Could not delete default server group!"};
                }
            }

            if(serverInstance->properties()[property::SERVERINSTANCE_TEMPLATE_SERVERADMIN_GROUP] == group->group_id()) {
                return command_result{error::parameter_invalid, "Could not delete instance default server admin group!"};
            }

            if(serverInstance->properties()[property::SERVERINSTANCE_TEMPLATE_SERVERDEFAULT_GROUP] == group->group_id()) {
                return command_result{error::parameter_invalid, "Could not delete instance default server group!"};
            }

            if(serverInstance->properties()[property::SERVERINSTANCE_GUEST_SERVERQUERY_GROUP] == group->group_id()) {
                return command_result{error::parameter_invalid, "Could not delete instance default guest server query group!"};
            }

            break;

        case GroupTarget::GROUPTARGET_CHANNEL:
            if (this->server) {
                if (this->server->properties()[property::VIRTUALSERVER_DEFAULT_CHANNEL_GROUP] == group->group_id()) {
                    return command_result{error::parameter_invalid, "Could not delete default channel group!"};
                }
                if (this->server->properties()[property::VIRTUALSERVER_DEFAULT_CHANNEL_ADMIN_GROUP] == group->group_id()) {
                    return command_result{error::parameter_invalid, "Could not delete default channel admin group!"};
                }
            }

            if (serverInstance->properties()[property::SERVERINSTANCE_TEMPLATE_CHANNELDEFAULT_GROUP] == group->group_id()) {
                return command_result{error::parameter_invalid, "Could not delete instance default channel group!"};
            }

            if(serverInstance->properties()[property::SERVERINSTANCE_TEMPLATE_CHANNELADMIN_GROUP] == group->group_id()) {
                return command_result{error::parameter_invalid, "Could not delete instance default channel admin group!"};
            }

            break;
    }

    bool global_update;
    log::GroupType log_group_type;
    ServerId log_server_id;
    switch (group->group_type()) {
        case groups::GroupType::GROUP_TYPE_QUERY:
            ACTION_REQUIRES_GLOBAL_PERMISSION(permission::b_serverinstance_modify_querygroup, 1);
            log_server_id = 0;
            log_group_type = log::GroupType::QUERY;
            global_update = true;
            break;

        case groups::GroupType::GROUP_TYPE_TEMPLATE:
            ACTION_REQUIRES_GLOBAL_PERMISSION(permission::b_serverinstance_modify_templates, 1);
            log_server_id = 0;
            log_group_type = log::GroupType::TEMPLATE;
            global_update = true;
            break;

        case groups::GroupType::GROUP_TYPE_NORMAL:
            switch(group_target) {
                case GroupTarget::GROUPTARGET_SERVER:
                    ACTION_REQUIRES_GLOBAL_PERMISSION(permission::b_virtualserver_servergroup_delete, 1);
                    break;

                case GroupTarget::GROUPTARGET_CHANNEL:
                    ACTION_REQUIRES_GLOBAL_PERMISSION(permission::b_virtualserver_channelgroup_delete, 1);
                    break;
            }

            log_server_id = this->getServerId();
            log_group_type = log::GroupType::NORMAL;
            global_update = false;
            break;

        case groups::GroupType::GROUP_TYPE_UNKNOWN:
        default:
            return ts::command_result{error::vs_critical};
    }

    auto force_delete = cmd[0].has("force") ? cmd["force"].as<bool>() : false;

    groups::GroupDeleteResult result;
    switch (group_target) {
        case GroupTarget::GROUPTARGET_SERVER:
            if(!force_delete && !group_manager->assignments().is_server_group_empty(group->group_id())) {
                result = groups::GroupDeleteResult::GROUP_NOT_EMPTY;
                break;
            }

            result = group_manager->server_groups()->delete_group(group->group_id());
            break;

        case GroupTarget::GROUPTARGET_CHANNEL:
            if(!force_delete && !group_manager->assignments().is_channel_group_empty(group->group_id())) {
                result = groups::GroupDeleteResult::GROUP_NOT_EMPTY;
                break;
            }

            result = group_manager->channel_groups()->delete_group(group->group_id());
            break;

        default:
            assert(false);
            return ts::command_result{error::vs_critical};
    }

    switch(result) {
        case groups::GroupDeleteResult::SUCCESS:
            break;

        case groups::GroupDeleteResult::GROUP_NOT_EMPTY:
            return ts::command_result{error::group_not_empty};

        case groups::GroupDeleteResult::INVALID_GROUP_ID:
        case groups::GroupDeleteResult::DATABASE_ERROR:
        default:
            return ts::command_result{error::vs_critical};
    }

    serverInstance->action_logger()->group_logger.log_group_delete(log_server_id, this->ref(), log_group_target, log_group_type, group->group_id(), group->display_name());

    std::deque<std::shared_ptr<VirtualServer>> server_updates{};
    if(global_update) {
        server_updates = serverInstance->getVoiceServerManager()->serverInstances();
    } else {
        server_updates.push_back(this->server);
    }

    for(const auto& server : server_updates) {
        if(!server) {
            continue;
        }

        switch(group_target) {
            case GroupTarget::GROUPTARGET_SERVER:
                server->enqueue_notify_server_group_list();
                server->group_manager()->assignments().handle_server_group_deleted(group->group_id());
                server->tokenManager->handle_server_group_deleted(group->group_id());
                break;

            case GroupTarget::GROUPTARGET_CHANNEL:
                server->enqueue_notify_channel_group_list();
                server->group_manager()->assignments().handle_channel_group_deleted(group->group_id());
                server->tokenManager->handle_channel_group_deleted(group->group_id());
                break;
        }

        this->server->forEachClient([&](const std::shared_ptr<ConnectedClient>& client) {
            bool groups_changed;
            client->update_displayed_client_groups(groups_changed, groups_changed);

            if(groups_changed) {
                client->task_update_needed_permissions.enqueue();
                client->task_update_channel_client_properties.enqueue();
            }
        });
    }

    return command_result{error::ok};
}

command_result ConnectedClient::executeGroupPermissionEdit(Command &cmd,
                                                           const std::vector<std::shared_ptr<groups::Group>> &groups,
                                                           const std::shared_ptr<VirtualServer> &target_server,
                                                           permission::v2::PermissionUpdateType mode) {
    ts::command::bulk_parser::PermissionBulksParser pparser{cmd, mode == permission::v2::PermissionUpdateType::set_value};
    if (!pparser.validate(this->ref(), 0)) {
        return pparser.build_command_result();
    }

    bool update_channel_group_list{false}, update_server_group_list{false};
    for(const auto& group : groups) {
        bool* update_group_list;
        log::PermissionTarget log_group_target;

        if(dynamic_pointer_cast<groups::ServerGroup>(group)) {
            update_group_list = &update_server_group_list;
            log_group_target = log::PermissionTarget::SERVER_GROUP;
        } else {
            update_group_list = &update_channel_group_list;
            log_group_target = log::PermissionTarget::CHANNEL_GROUP;
        }

        for (const auto &ppermission : pparser.iterate_valid_permissions()) {
            ppermission.apply_to(group->permissions(), mode);
            ppermission.log_update(serverInstance->action_logger()->permission_logger,
                                   this->getServerId(),
                                   this->ref(),
                                   log_group_target,
                                   mode,
                                   0, "",
                                   group->group_id(), group->display_name());

            *update_group_list |= ppermission.is_group_property();
        }
    }

    std::deque<std::shared_ptr<VirtualServer>> server_updates{};
    if(target_server) {
        server_updates.push_back(target_server);
    } else {
        server_updates = serverInstance->getVoiceServerManager()->serverInstances();
    }

    for(const auto& server : server_updates) {
        if(!server) {
            continue;
        }

        if(update_server_group_list) {
            server->enqueue_notify_server_group_list();
        }

        if(update_channel_group_list) {
            server->enqueue_notify_channel_group_list();
        }

        btree::set<ClientId> updated_clients{};
        for(const auto& group : groups) {
            if(auto s_group{dynamic_pointer_cast<groups::ServerGroup>(group)}; s_group) {
                server->forEachClient([&](const std::shared_ptr<ConnectedClient> &client) {
                    if(updated_clients.count(client->getClientId()) > 0) {
                        return;
                    }

                    if(client->serverGroupAssigned(s_group)) {
                        updated_clients.emplace(client->getClientId());
                        client->task_update_channel_client_properties.enqueue();
                        client->task_update_needed_permissions.enqueue();
                        client->join_state_id++;
                    }
                });
            } else if(auto c_group{dynamic_pointer_cast<groups::ChannelGroup>(group)}; c_group) {
                server->forEachClient([&](const std::shared_ptr<ConnectedClient> &client) {
                    if(updated_clients.count(client->getClientId()) > 0) {
                        return;
                    }

                    auto channel = client->getChannel();
                    if(client->channelGroupAssigned(c_group, channel)) {
                        updated_clients.emplace(client->getClientId());
                        client->task_update_channel_client_properties.enqueue();
                        client->task_update_needed_permissions.enqueue();
                        client->join_state_id++;
                    }
                });
            } else {
                assert(false);
            }
        }
    }

    return pparser.build_command_result();
}
