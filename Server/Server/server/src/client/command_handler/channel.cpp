#include <memory>

#include "../../InstanceHandler.h"
#include "../../PermissionCalculator.h"
#include "../../manager/ActionLogger.h"
#include "../../manager/ConversationManager.h"
#include "../../manager/PermissionNameMapper.h"
#include "../../server/QueryServer.h"
#include "../../server/VoiceServer.h"
#include "../../groups/GroupManager.h"
#include "../ConnectedClient.h"
#include "../InternalClient.h"
#include "../music/MusicClient.h"
#include "../query/QueryClient.h"
#include "../voice/VoiceClient.h"
#include "PermissionManager.h"
#include <algorithm>
#include <bitset>
#include <cstdint>

#include "./bulk_parsers.h"

#include <Properties.h>
#include <bbcode/bbcodes.h>
#include <log/LogUtils.h>
#include <misc/base64.h>
#include <misc/digest.h>
#include <misc/utf8.h>

using namespace std::chrono;
using namespace std;
using namespace ts;
using namespace ts::server;

command_result ConnectedClient::handleCommandChannelGetDescription(Command &cmd) {
    CMD_CHK_AND_INC_FLOOD_POINTS(0);
    RESOLVE_CHANNEL_R(cmd["cid"], true);
    auto channel = dynamic_pointer_cast<BasicChannel>(l_channel->entry);
    assert(channel);

    std::shared_lock channel_lock{this->channel_tree_mutex};
    if(!this->channel_tree->channel_visible(channel)) {
        return ts::command_result{error::channel_invalid_id};
    }

    ClientPermissionCalculator permission_calculator{this, channel};
    if(!permission_calculator.permission_granted(permission::b_channel_ignore_description_view_power, 1)) {
        auto required_view_power = channel->permissions()->permission_value_flagged(permission::i_channel_needed_description_view_power);
        required_view_power.clear_flag_on_zero();

        if(!permission_calculator.permission_granted(permission::i_channel_description_view_power, required_view_power)) {
            return command_result{permission::i_channel_description_view_power};
        }
    }

    auto channel_description = channel->properties()[property::CHANNEL_DESCRIPTION].value();
    auto channel_description_limit = this->getType() == CLIENT_TEAMSPEAK ? 8192 : 131130;

    if(channel_description.length() > channel_description_limit) {
        channel_description = channel_description.substr(0, channel_description_limit - 3);
        channel_description.append("...");
    }

    ts::command_builder notify{this->notify_response_command("notifychanneledited")};
    notify.put_unchecked(0, "cid", channel->channelId());
    notify.put_unchecked(0, "reasonid", "9");
    notify.put_unchecked(0, "channel_description", channel_description);
    this->sendCommand(notify);

    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandChannelSubscribe(Command &cmd) {
    CMD_REF_SERVER(ref_server);
    CMD_RESET_IDLE;

    bool flood_points{false};
    std::deque<std::shared_ptr<BasicChannel>> target_channels{};
    //target_channels.reserve(cmd.bulkCount());

    ts::command_result_bulk result{};
    result.emplace_result_n(cmd.bulkCount(), error::ok);

    {
        std::shared_lock server_channel_lock{this->server->channel_tree_mutex};
        std::lock_guard client_channel_lock{this->channel_tree_mutex};

        for (int index{0}; index < cmd.bulkCount(); index++) {
            auto target_channel_id = cmd[index]["cid"].as<ChannelId>();
            auto local_channel = this->channel_view()->find_channel(target_channel_id);
            if (!local_channel) {
                result.set_result(index, ts::command_result{error::channel_invalid_id});
                continue;
            }

            auto channel = this->server->channelTree->findChannel(target_channel_id);
            if (!channel) {
                result.set_result(index, ts::command_result{error::channel_invalid_id});
                continue;
            }

            target_channels.push_back(channel);
            if (!flood_points && std::chrono::system_clock::now() - local_channel->view_timestamp > seconds(5)) {
                flood_points = true;

                this->increaseFloodPoints(15);
                if(this->shouldFloodBlock()) {
                    result.set_result(index, ts::command_result{error::ban_flooding});
                    continue;
                }
            }
        }

        if (!target_channels.empty()) {
            this->subscribeChannel(target_channels, false, false);
        }
    }

    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandChannelSubscribeAll(Command &cmd) {
    CMD_REQ_SERVER;
    CMD_CHK_AND_INC_FLOOD_POINTS(20);

    {
        std::shared_lock server_channel_lock{this->server->channel_tree_mutex};
        std::lock_guard client_channel_lock{this->channel_tree_mutex};
        this->subscribeChannel(this->server->channelTree->channels(), false, false);
        this->subscribeToAll = true;
    }
    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandChannelUnsubscribe(Command &cmd) {
    CMD_REQ_SERVER;
    CMD_CHK_AND_INC_FLOOD_POINTS(5);

    ts::command_result_bulk result{};
    result.emplace_result_n(cmd.bulkCount(), error::ok);

    {
        std::shared_lock server_channel_lock{this->server->channel_tree_mutex};
        std::lock_guard client_channel_lock{this->channel_tree_mutex};

        std::deque<shared_ptr<BasicChannel>> channels{};
        for (int index = 0; index < cmd.bulkCount(); index++) {
            auto channel = this->server->channelTree->findChannel(cmd["cid"].as<ChannelId>());
            if (!channel) {
                result.set_result(index, ts::command_result{error::channel_invalid_id});
                continue;
            }

            channels.push_front(channel);
        }

        this->unsubscribeChannel(channels, false);
    }

    return command_result{std::move(result)};
}

command_result ConnectedClient::handleCommandChannelUnsubscribeAll(Command &cmd) {
    CMD_REQ_SERVER;
    CMD_CHK_AND_INC_FLOOD_POINTS(25);

    {
        std::shared_lock server_channel_lock{this->server->channel_tree_mutex};
        std::lock_guard client_channel_lock{this->channel_tree_mutex};
        this->unsubscribeChannel(this->server->channelTree->channels(), false);
        this->subscribeToAll = false;
    }

    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandChannelGroupAdd(Command &cmd) {
    return this->handleCommandGroupAdd(cmd, GroupTarget::GROUPTARGET_CHANNEL);
}

command_result ConnectedClient::handleCommandChannelGroupCopy(Command &cmd) {
    return this->handleCommandGroupCopy(cmd, GroupTarget::GROUPTARGET_CHANNEL);
}

command_result ConnectedClient::handleCommandChannelGroupRename(Command &cmd) {
    return this->handleCommandGroupRename(cmd, GroupTarget::GROUPTARGET_CHANNEL);
}

command_result ConnectedClient::handleCommandChannelGroupDel(Command &cmd) {
    return this->handleCommandGroupDel(cmd, GroupTarget::GROUPTARGET_CHANNEL);
}

command_result ConnectedClient::handleCommandChannelGroupList(Command &) {
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(5);
    ACTION_REQUIRES_GLOBAL_PERMISSION(permission::b_virtualserver_channelgroup_list, 1);

    std::optional<ts::command_builder> notify{};
    this->notifyChannelGroupList(notify, this->getType() != ClientType::CLIENT_QUERY);

    this->command_times.servergrouplist = system_clock::now();
    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandChannelGroupClientList(Command &cmd) {
    CMD_REQ_SERVER;
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(5);

    auto target_channel_id = cmd[0].has("cid") ? cmd["cid"].as<ChannelId>() : 0;
    auto target_client_database_id = cmd[0].has("cldbid") ? cmd["cldbid"].as<ClientDbId>() : 0;
    auto target_group_id = cmd[0].has("cgid") ? cmd["cgid"].as<GroupId>() : 0;

    ACTION_REQUIRES_PERMISSION(permission::b_virtualserver_channelgroup_client_list, 1, target_channel_id);

    auto result = this->server->group_manager()->assignments().channel_group_list(target_group_id, target_channel_id, target_client_database_id);
    if(result.empty()) {
        return ts::command_result{error::database_empty_result};
    }


    size_t index{0};
    ts::command_builder notify{this->notify_response_command("notifychannelgroupclientlist"), 64, result.size()};
    for(const auto& entry : result) {
        auto bulk = notify.bulk(index++);
        bulk.put_unchecked("cgid", std::get<0>(entry));
        bulk.put_unchecked("cid", std::get<1>(entry));
        bulk.put_unchecked("cldbid", std::get<2>(entry));
    }
    this->sendCommand(notify);

    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandChannelGroupPermList(Command &cmd) {
    CMD_CHK_AND_INC_FLOOD_POINTS(5);
    ACTION_REQUIRES_GLOBAL_PERMISSION(permission::b_virtualserver_channelgroup_permission_list, 1);

    auto group_manager = this->server ? this->server->group_manager() : serverInstance->group_manager();
    auto channelGroup = group_manager->channel_groups()->find_group(groups::GroupCalculateMode::GLOBAL, cmd["cgid"].as<GroupId>());
    if (!channelGroup) {
        return command_result{error::group_invalid_id};
    }

    if (!this->notifyGroupPermList(channelGroup, cmd.hasParm("permsid"))) {
        return command_result{error::database_empty_result};
    }

    if (this->getType() == ClientType::CLIENT_TEAMSPEAK && this->command_times.last_notify + this->command_times.notify_timeout < system_clock::now()) {
        this->sendTSPermEditorWarning();
    }

    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandChannelGroupAddPerm(Command &cmd) {
    CMD_CHK_AND_INC_FLOOD_POINTS(5);

    auto group_id = cmd["cgid"].as<GroupId>();

    std::shared_ptr<groups::ChannelGroupManager> owning_manager{};
    auto group_manager = this->server ? this->server->group_manager() : serverInstance->group_manager();
    auto group = group_manager->channel_groups()->find_group_ext(owning_manager, groups::GroupCalculateMode::GLOBAL, group_id);

    if(!group) {
        return ts::command_result{error::group_invalid_id};
    }

    ACTION_REQUIRES_GROUP_PERMISSION(group, permission::i_channel_group_needed_modify_power, permission::i_channel_group_modify_power, true);

    auto target_server = group_manager->channel_groups() == owning_manager ? this->server : nullptr;
    return this->executeGroupPermissionEdit(cmd, { group }, target_server, permission::v2::PermissionUpdateType::set_value);
}

command_result ConnectedClient::handleCommandChannelGroupDelPerm(Command &cmd) {
    CMD_CHK_AND_INC_FLOOD_POINTS(5);

    auto group_id = cmd["cgid"].as<GroupId>();

    std::shared_ptr<groups::ChannelGroupManager> owning_manager{};
    auto group_manager = this->server ? this->server->group_manager() : serverInstance->group_manager();
    auto group = group_manager->channel_groups()->find_group_ext(owning_manager, groups::GroupCalculateMode::GLOBAL, group_id);

    if(!group) {
        return ts::command_result{error::group_invalid_id};
    }

    ACTION_REQUIRES_GROUP_PERMISSION(group, permission::i_channel_group_needed_modify_power, permission::i_channel_group_modify_power, true);

    auto target_server = group_manager->channel_groups() == owning_manager ? this->server : nullptr;
    return this->executeGroupPermissionEdit(cmd, { group }, target_server, permission::v2::PermissionUpdateType::delete_value);
}

//TODO: Test if parent or previous is deleted!
command_result ConnectedClient::handleCommandChannelCreate(Command &cmd) {
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(25);
    CMD_CHK_PARM_COUNT(1);

    auto target_tree = this->server ? this->server->channelTree : &*serverInstance->getChannelTree();
    std::shared_lock channel_tree_read_lock{this->server ? this->server->channel_tree_mutex : serverInstance->getChannelTreeLock()};
    ChannelId parent_channel_id = cmd[0].has("cpid") ? cmd["cpid"].as<ChannelId>() : 0;

#define test_permission(required, permission_type)                                                                                                  \
    do {                                                                                                                                            \
        if (!permission::v2::permission_granted(required, this->calculate_permission(permission_type, parent_channel_id, false)))                   \
            return command_result{permission_type};                                                                                                 \
    } while (0)

    std::shared_ptr<TreeView::LinkedTreeEntry> parent;
    std::map<property::ChannelProperties, std::string> new_values{};
    for (const auto &key : cmd[0].keys()) {
        if (key == "return_code") {
            continue;
        }

        if (key == "cpid") {
            if(cmd[key].string().empty()) {
                continue;
            }

            auto value = cmd["cpid"].as<ChannelId>();
            if(value > 0) {
                parent = target_tree->findLinkedChannel(value);
                if(!parent) {
                    return ts::command_result{error::channel_invalid_id};
                }
                test_permission(1, permission::b_channel_create_child);
            }

            new_values[property::CHANNEL_PID] = cmd[key].string();
            continue;
        }

        const auto &property = property::find<property::ChannelProperties>(key);
        if (property == property::CHANNEL_UNDEFINED) {
            logError(this->getServerId(), R"({} Tried to create a channel with a non property channel property "{}" to "{}")", CLIENT_STR_LOG_PREFIX, key, cmd[key].string());
            continue;
        }

        if ((property.flags & property::FLAG_USER_EDITABLE) == 0) {
            logError(this->getServerId(), "{} Tried to create a channel with a property which is not changeable. (Key: {}, Value: \"{}\")", CLIENT_STR_LOG_PREFIX, key, cmd[key].string());
            continue;
        }

        if (!property.validate_input(cmd[key].as<string>())) {
            logError(this->getServerId(), "{} Tried to create a channel with a property with an invalid value. (Key: {}, Value: \"{}\")", CLIENT_STR_LOG_PREFIX, key, cmd[key].string());
            continue;
        }

        if (key == "channel_icon_id") {
            /* the creator has the motify permission by default */
        } else if (key == "channel_order") {
            if(cmd[key].string().empty()) {
                continue;
            }

            test_permission(true, permission::b_channel_create_with_sortorder);
        } else if (key == "channel_flag_default") {
            if(cmd["channel_flag_default"].as<bool>()) {
                test_permission(true, permission::b_channel_create_with_default);
            }
        } else if (key == "channel_name" ||key == "channel_name_phonetic") {
            /* channel create requires a name */
        } else if (key == "channel_topic") {
            if(!cmd[key].string().empty()) {
                test_permission(true, permission::b_channel_create_with_topic);
            }
        } else if (key == "channel_description") {
            auto value = cmd[key].string();
            if(!value.empty()) {
                test_permission(true, permission::b_channel_create_with_description);

                if (!permission::v2::permission_granted(1, this->calculate_permission(permission::b_client_use_bbcode_any, parent_channel_id))) {
                    auto bbcode_image = bbcode::sloppy::has_image(value);
                    auto bbcode_url = bbcode::sloppy::has_url(value);
                    debugMessage(this->getServerId(), "Channel description contains bb codes: Image: {} URL: {}", bbcode_image, bbcode_url);
                    if (bbcode_image && !permission::v2::permission_granted(1, this->calculate_permission(permission::b_client_use_bbcode_image, parent_channel_id))) {
                        return command_result{permission::b_client_use_bbcode_image};
                    }

                    if (bbcode_url && !permission::v2::permission_granted(1, this->calculate_permission(permission::b_client_use_bbcode_url, parent_channel_id))) {
                        return command_result{permission::b_client_use_bbcode_url};
                    }
                }
            }
        } else if (key == "channel_codec") {
            /* TODO: Test for the codec itself! */
        } else if (key == "channel_codec_quality") {
            /* TODO: Test for the value itself! */
        } else if (key == "channel_codec_is_unencrypted") {
            if (cmd["channel_codec_is_unencrypted"].as<bool>()) {
                test_permission(true, permission::b_channel_modify_make_codec_encrypted);
            }
        } else if (key == "channel_needed_talk_power") {
            test_permission(true, permission::b_channel_create_with_needed_talk_power);
        } else if (key == "channel_maxclients" || key == "channel_flag_maxclients_unlimited") {
            test_permission(true, permission::b_channel_create_with_maxclients);
        } else if (key == "channel_maxfamilyclients" || key == "channel_flag_maxfamilyclients_unlimited" || key == "channel_flag_maxfamilyclients_inherited") {
            test_permission(true, permission::b_channel_create_with_maxfamilyclients);
        } else if (key == "channel_flag_permanent" || key == "channel_flag_semi_permanent") {
            if (cmd[0].has("channel_flag_permanent") && cmd["channel_flag_permanent"].as<bool>()) {
                test_permission(true, permission::b_channel_create_permanent);
            } else if (cmd[0].has("channel_flag_semi_permanent") && cmd["channel_flag_semi_permanent"].as<bool>()) {
                test_permission(true, permission::b_channel_create_semi_permanent);
            } else {
                test_permission(true, permission::b_channel_create_temporary);
            }
        } else if (key == "channel_delete_delay") {
            ACTION_REQUIRES_PERMISSION(permission::i_channel_create_modify_with_temp_delete_delay, cmd["channel_delete_delay"].as<permission::PermissionValue>(), parent_channel_id);
        } else if (key == "channel_password" || key == "channel_flag_password") {
            /* no need to test right now */
        } else if (key == "channel_conversation_history_length") {
            auto value = cmd["channel_conversation_history_length"].as<int64_t>();
            if (value == 0) {
                ACTION_REQUIRES_PERMISSION(permission::b_channel_create_modify_conversation_history_unlimited, 1, parent_channel_id);
            } else {
                ACTION_REQUIRES_PERMISSION(permission::i_channel_create_modify_conversation_history_length, 1, parent_channel_id);
            }
        } else if (key == "channel_flag_conversation_private") {
            auto value = cmd["channel_flag_conversation_private"].as<bool>();
            if (value) {
                ACTION_REQUIRES_PERMISSION(permission::b_channel_create_modify_conversation_mode_private, 1, parent_channel_id);
                new_values[property::CHANNEL_CONVERSATION_MODE] = std::to_string(CHANNELCONVERSATIONMODE_PRIVATE);
            } else {
                ACTION_REQUIRES_PERMISSION(permission::b_channel_create_modify_conversation_mode_public, 1, parent_channel_id);
                new_values[property::CHANNEL_CONVERSATION_MODE] = std::to_string(CHANNELCONVERSATIONMODE_PUBLIC);
            }
            continue;
        } else if (key == "channel_conversation_mode") {
            auto value = cmd["channel_conversation_mode"].as<ChannelConversationMode>();
            switch (value) {
                case ChannelConversationMode::CHANNELCONVERSATIONMODE_PRIVATE:
                    ACTION_REQUIRES_PERMISSION(permission::b_channel_create_modify_conversation_mode_private, 1, parent_channel_id);
                    break;
                case ChannelConversationMode::CHANNELCONVERSATIONMODE_PUBLIC:
                    ACTION_REQUIRES_PERMISSION(permission::b_channel_create_modify_conversation_mode_public, 1, parent_channel_id);
                    break;
                case ChannelConversationMode::CHANNELCONVERSATIONMODE_NONE:
                    ACTION_REQUIRES_PERMISSION(permission::b_channel_create_modify_conversation_mode_none, 1, parent_channel_id);
                    break;
                default:
                    return command_result{error::parameter_invalid, "channel_conversation_mode"};
            }
        } else {
            logCritical(
                    this->getServerId(),
                    "{} Tried to change a editable channel property but we haven't found a permission. Please report this error. (Channel property: {})",
                    CLIENT_STR_LOG_PREFIX,
                    key
            );
            continue;
        }

        new_values.emplace((property::ChannelProperties) property.property_index, cmd[key].string());
    }

    /* Fix since the default value of channel_flag_permanent is 1 which should be zero... */
    if(!new_values.contains(property::CHANNEL_FLAG_PERMANENT)) {
        new_values[property::CHANNEL_FLAG_PERMANENT] = std::to_string(false);
    }

    if (!cmd[0]["channel_flag_permanent"].as<bool>() && !this->server) {
        return command_result{error::parameter_invalid, "You can only create a permanent channel"};
    }

    if (cmd[0].has("channel_flag_password") && cmd[0]["channel_flag_password"].as<bool>()) {
        test_permission(1, permission::b_channel_create_with_password);
    } else if (permission::v2::permission_granted(1, this->calculate_permission(permission::b_channel_create_modify_with_force_password, parent_channel_id, false))) {
        return command_result{permission::b_channel_create_modify_with_force_password};
    }

    auto delete_delay = cmd[0].has("channel_delete_delay") ? cmd["channel_delete_delay"].as<permission::PermissionValue>() : 0UL;
    if (delete_delay == 0) {
        if (this->server) {
            new_values[property::CHANNEL_DELETE_DELAY] = this->server->properties()[property::VIRTUALSERVER_CHANNEL_TEMP_DELETE_DELAY_DEFAULT].value();
        } else {
            new_values[property::CHANNEL_DELETE_DELAY] = "60";
        }
    } else {
        test_permission(cmd["channel_delete_delay"].as<permission::PermissionValue>(), permission::i_channel_create_modify_with_temp_delete_delay);
    }


    {
        size_t created_total = 0, created_tmp = 0, created_semi = 0, created_perm = 0;
        auto own_cldbid = this->getClientDatabaseId();
        for (const auto &channel : target_tree->channels()) {
            created_total++;
            if (channel->properties()[property::CHANNEL_CREATED_BY] == own_cldbid) {
                if (channel->properties()[property::CHANNEL_FLAG_PERMANENT].as_or<bool>(false)) {
                    created_perm++;
                } else if (channel->properties()[property::CHANNEL_FLAG_SEMI_PERMANENT].as_or<bool>(false)) {
                    created_semi++;
                } else {
                    created_tmp++;
                }
            }
        }

        if (this->server && created_total >=
                                    this->server->properties()[property::VIRTUALSERVER_MAX_CHANNELS].as_or<uint64_t>(0))
            return command_result{error::channel_limit_reached};

        auto max_channels = this->calculate_permission(permission::i_client_max_channels, parent_channel_id, false);
        if (max_channels.has_value) {
            if (!permission::v2::permission_granted(created_perm + created_semi + created_tmp + 1, max_channels)) {
                return command_result{permission::i_client_max_channels};
            }
        }

        if (cmd[0]["channel_flag_permanent"].as<bool>()) {
            max_channels = this->calculate_permission(permission::i_client_max_permanent_channels, parent_channel_id, false);

            if (max_channels.has_value) {
                if (!permission::v2::permission_granted(created_perm + 1, max_channels)) {
                    return command_result{permission::i_client_max_permanent_channels};
                }
            }
        } else if (cmd[0]["channel_flag_semi_permanent"].as<bool>()) {
            max_channels = this->calculate_permission(permission::i_client_max_semi_channels, parent_channel_id, false);

            if (max_channels.has_value) {
                if (!permission::v2::permission_granted(created_semi + 1, max_channels)) {
                    return command_result{permission::i_client_max_semi_channels};
                }
            }
        } else {
            max_channels = this->calculate_permission(permission::i_client_max_temporary_channels, parent_channel_id, false);

            if (max_channels.has_value) {
                if (!permission::v2::permission_granted(created_tmp + 1, max_channels)) {
                    return command_result{permission::i_client_max_temporary_channels};
                }
            }
        }
    }

    {
        auto min_channel_deep = this->calculate_permission(permission::i_channel_min_depth, parent_channel_id, false);
        auto max_channel_deep = this->calculate_permission(permission::i_channel_max_depth, parent_channel_id, false);

        if (min_channel_deep.has_value || max_channel_deep.has_value) {
            auto channel_deep = 0;
            auto local_parent = parent;
            while (local_parent) {
                channel_deep++;
                {
                    const auto typed_parent = dynamic_pointer_cast<ServerChannel>(local_parent->entry);
                    if (typed_parent->deleted) {
                        return command_result{error::channel_is_deleted, "One of the parents has been deleted"};
                    }
                }
                local_parent = local_parent->parent.lock();
            }

            if (min_channel_deep.has_value && (channel_deep < min_channel_deep.value && !min_channel_deep.has_infinite_power())) return command_result{permission::i_channel_min_depth};
            if (max_channel_deep.has_value && !permission::v2::permission_granted(channel_deep, max_channel_deep)) return command_result{permission::i_channel_max_depth};
        }
    }

    if (!new_values.contains(property::CHANNEL_ORDER)) {
        auto last = parent ? parent->child_head : target_tree->tree_head();
        while (last && last->next) {
            last = last->next;
        }

        if (last) {
            new_values[property::CHANNEL_ORDER] = std::to_string(last->entry->channelId());
        } else {
            new_values[property::CHANNEL_ORDER] = "0";
        }
    }

    channel_tree_read_lock.unlock();

    ChannelId channel_id;
    auto result = this->execute_channel_edit(channel_id, new_values, true);
    if(result.has_error()) {
        return result;
    }

    channel_tree_read_lock.lock();
    auto created_channel = target_tree->findChannel(channel_id);
    if(!created_channel) {
        return ts::command_result{error::vs_critical, "failed to find created channel"};
    }

    created_channel->properties()[property::CHANNEL_CREATED_BY] = this->getClientDatabaseId();
    created_channel->properties()[property::CHANNEL_CREATED_AT] =
            std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    {
        auto default_modify_power = this->calculate_permission(permission::i_channel_modify_power, parent_channel_id, false);
        auto default_delete_power = this->calculate_permission(permission::i_channel_delete_power, parent_channel_id, false);

        auto permission_manager = created_channel->permissions();
        permission_manager->set_permission(
                permission::i_channel_needed_modify_power,
                {default_modify_power.has_value ? default_modify_power.value : 0, 0},
                permission::v2::PermissionUpdateType::set_value,
                permission::v2::PermissionUpdateType::do_nothing);

        permission_manager->set_permission(
                permission::i_channel_needed_delete_power,
                {default_delete_power.has_value ? default_delete_power.value : 0, 0},
                permission::v2::PermissionUpdateType::set_value,
                permission::v2::PermissionUpdateType::do_nothing);
    }

    /* log channel create */
    {
        log::ChannelType log_channel_type;
        switch (created_channel->channelType()) {
            case ChannelType::permanent:
                log_channel_type = log::ChannelType::PERMANENT;
                break;

            case ChannelType::semipermanent:
                log_channel_type = log::ChannelType::SEMI_PERMANENT;
                break;

            case ChannelType::temporary:
                log_channel_type = log::ChannelType::TEMPORARY;
                break;

            default:
                assert(false);
                return ts::command_result{error::vs_critical};
        }

        serverInstance->action_logger()->channel_logger.log_channel_create(this->getServerId(), this->ref(), created_channel->channelId(), log_channel_type);
    }

    if (this->server) {
        const auto self_lock = this->ref();

        auto admin_group_id = this->server->properties()[property::VIRTUALSERVER_DEFAULT_CHANNEL_ADMIN_GROUP].as_or<GroupId>(0);
        auto admin_group = this->server->group_manager()->channel_groups()->find_group(groups::GroupCalculateMode::GLOBAL, admin_group_id);
        if(!admin_group) {
            logError(this->getServerId(), "Missing servers default channel admin group {}.", admin_group_id);
        }

        /* admin_group might still be null since default_channel_group() could return nullptr */
        if(admin_group) {
            this->server->group_manager()->assignments().set_channel_group(this->getClientDatabaseId(), admin_group->group_id(), created_channel->channelId(), false);
            serverInstance->action_logger()->group_assignment_logger.log_group_assignment_remove(
                    this->getServerId(), this->server->getServerRoot(),
                    log::GroupTarget::CHANNEL,
                    admin_group->group_id(), admin_group->display_name(),
                    this->getClientDatabaseId(), this->getDisplayName()
            );
        }

        if (created_channel->channelType() == ChannelType::temporary && (this->getType() == ClientType::CLIENT_TEAMSPEAK || this->getType() == ClientType::CLIENT_WEB || this->getType() == ClientType::CLIENT_TEASPEAK)) {
            channel_tree_read_lock.unlock();

            std::unique_lock channel_tree_write_lock{this->server->channel_tree_mutex};
            this->server->client_move(
                    this->ref(),
                    created_channel,
                    nullptr,
                    "channel created",
                    ViewReasonId::VREASON_USER_ACTION,
                    true,
                    channel_tree_write_lock);
        }
    }

    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandChannelDelete(Command &cmd) {
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(25);

    RESOLVE_CHANNEL_W(cmd["cid"], true);
    auto channel = dynamic_pointer_cast<ServerChannel>(l_channel->entry);
    assert(channel);
    if (channel->deleted) /* channel gets already removed */
        return command_result{error::ok};

    ACTION_REQUIRES_CHANNEL_PERMISSION(channel, permission::i_channel_needed_delete_power, permission::i_channel_delete_power, true);
    for (const auto &ch : channel_tree->channels(channel)) {
        if (ch->defaultChannel())
            return command_result{error::channel_can_not_delete_default};
    }

    if (this->server) {
        auto clients = this->server->getClientsByChannelRoot(channel, false);
        if (!clients.empty())
            ACTION_REQUIRES_PERMISSION(permission::b_channel_delete_flag_force, 1, channel->channelId());

        this->server->delete_channel(channel, this->ref(), "channel deleted", channel_tree_write_lock, false);
    } else {
        auto deleted_channel_ids = channel_tree->deleteChannelRoot(channel);
        for (const auto &channelId : deleted_channel_ids) {
            serverInstance->action_logger()->channel_logger.log_channel_delete(0, this->ref(), channelId, channel->channelId() == channelId ? log::ChannelDeleteReason::USER_ACTION : log::ChannelDeleteReason::PARENT_DELETED);
        }
        this->notifyChannelDeleted(deleted_channel_ids, this->ref());
    }

    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandChannelEdit(Command &cmd) {
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(25);

    RESOLVE_CHANNEL_R(cmd["cid"], true);
    auto channel = dynamic_pointer_cast<ServerChannel>(l_channel->entry);
    assert(channel);

    ACTION_REQUIRES_CHANNEL_PERMISSION(channel, permission::i_channel_needed_modify_power, permission::i_channel_modify_power, true);

    std::map<property::ChannelProperties, std::string> new_values{};
    for (const auto &key : cmd[0].keys()) {
        if (key == "cid") {
            continue;
        }

        if (key == "return_code") {
            continue;
        }

        const auto &property = property::find<property::ChannelProperties>(key);
        if (property == property::CHANNEL_UNDEFINED) {
            logError(this->getServerId(), R"({} Tried to edit a not existing channel property "{}" to "{}")", CLIENT_STR_LOG_PREFIX, key, cmd[key].string());
            continue;
        }

        if ((property.flags & property::FLAG_USER_EDITABLE) == 0) {
            logError(this->getServerId(), "{} Tried to change a channel property which is not changeable. (Key: {}, Value: \"{}\")", CLIENT_STR_LOG_PREFIX, key, cmd[key].string());
            continue;
        }

        if (!property.validate_input(cmd[key].as<string>())) {
            logError(this->getServerId(), "{} Tried to change a channel property to an invalid value. (Key: {}, Value: \"{}\")", CLIENT_STR_LOG_PREFIX, key, cmd[key].string());
            continue;
        }

        if (channel->properties()[property].as<string>() == cmd[key].as<string>()) {
            continue; /* we dont need to update stuff which is the same */
        }

        if (key == "channel_icon_id") {
            ACTION_REQUIRES_CHANNEL_PERMISSION(channel, permission::i_channel_needed_permission_modify_power, permission::i_channel_permission_modify_power, true);
        } else if (key == "channel_order") {
            ACTION_REQUIRES_PERMISSION(permission::b_channel_modify_sortorder, 1, channel_id);
        } else if (key == "channel_flag_default") {
            if(cmd["channel_flag_default"].as<bool>()) {
                ACTION_REQUIRES_GLOBAL_PERMISSION(permission::b_channel_modify_make_default, 1);
            }
        } else if (key == "channel_name") {
            ACTION_REQUIRES_PERMISSION(permission::b_channel_modify_name, 1, channel_id);
        } else if (key == "channel_name_phonetic") {
            ACTION_REQUIRES_PERMISSION(permission::b_channel_modify_name, 1, channel_id);
        } else if (key == "channel_topic") {
            ACTION_REQUIRES_PERMISSION(permission::b_channel_modify_topic, 1, channel_id);
        } else if (key == "channel_description") {
            ACTION_REQUIRES_PERMISSION(permission::b_channel_modify_description, 1, channel_id);
            if (!permission::v2::permission_granted(1, this->calculate_permission(permission::b_client_use_bbcode_any, channel_id))) {
                auto bbcode_image = bbcode::sloppy::has_image(cmd[key]);
                auto bbcode_url = bbcode::sloppy::has_url(cmd[key]);
                debugMessage(this->getServerId(), "Channel description contains bb codes: Image: {} URL: {}", bbcode_image, bbcode_url);
                if (bbcode_image && !permission::v2::permission_granted(1, this->calculate_permission(permission::b_client_use_bbcode_image, channel_id))) {
                    return command_result{permission::b_client_use_bbcode_image};
                }

                if (bbcode_url && !permission::v2::permission_granted(1, this->calculate_permission(permission::b_client_use_bbcode_url, channel_id))) {
                    return command_result{permission::b_client_use_bbcode_url};
                }
            }
        } else if (key == "channel_codec") {
            ACTION_REQUIRES_PERMISSION(permission::b_channel_modify_codec, 1, channel_id);
        } else if (key == "channel_codec_quality") {
            ACTION_REQUIRES_PERMISSION(permission::b_channel_modify_codec_quality, 1, channel_id);
        } else if (key == "channel_codec_is_unencrypted") {
            if (cmd["channel_codec_is_unencrypted"].as<bool>()) {
                ACTION_REQUIRES_PERMISSION(permission::b_channel_modify_make_codec_encrypted, 1, channel_id);
            }
        } else if (key == "channel_needed_talk_power") {
            ACTION_REQUIRES_PERMISSION(permission::b_channel_modify_needed_talk_power, 1, channel_id);
        } else if (key == "channel_maxclients" || key == "channel_flag_maxclients_unlimited") {
            ACTION_REQUIRES_PERMISSION(permission::b_channel_modify_maxclients, 1, channel_id);
        } else if (key == "channel_maxfamilyclients" || key == "channel_flag_maxfamilyclients_unlimited" || key == "channel_flag_maxfamilyclients_inherited") {
            ACTION_REQUIRES_PERMISSION(permission::b_channel_modify_maxfamilyclients, 1, channel_id);
        } else if (key == "channel_flag_permanent" || key == "channel_flag_semi_permanent") {
            if (cmd[0].has("channel_flag_permanent") && cmd["channel_flag_permanent"].as<bool>()) {
                ACTION_REQUIRES_PERMISSION(permission::b_channel_modify_make_permanent, 1, channel_id);
            } else if (cmd[0].has("channel_flag_semi_permanent") && cmd["channel_flag_semi_permanent"].as<bool>()) {
                ACTION_REQUIRES_PERMISSION(permission::b_channel_modify_make_semi_permanent, 1, channel_id);
            } else {
                ACTION_REQUIRES_PERMISSION(permission::b_channel_modify_make_temporary, 1, channel_id);
            }
        } else if (key == "channel_delete_delay") {
            ACTION_REQUIRES_PERMISSION(permission::b_channel_modify_temp_delete_delay, 1, channel_id);
            ACTION_REQUIRES_PERMISSION(permission::i_channel_create_modify_with_temp_delete_delay, cmd["channel_delete_delay"].as<permission::PermissionValue>(), channel_id);
        } else if (key == "channel_password" || key == "channel_flag_password") {
            ACTION_REQUIRES_PERMISSION(permission::b_channel_modify_password, 1, channel_id);
        } else if (key == "channel_conversation_history_length") {
            auto value = cmd["channel_conversation_history_length"].as<int64_t>();
            if (value == 0) {
                ACTION_REQUIRES_PERMISSION(permission::b_channel_create_modify_conversation_history_unlimited, 1, channel_id);
            } else {
                ACTION_REQUIRES_PERMISSION(permission::i_channel_create_modify_conversation_history_length, 1, channel_id);
            }
        } else if (key == "channel_flag_conversation_private") {
            auto value = cmd["channel_flag_conversation_private"].as<bool>();
            if (value) {
                ACTION_REQUIRES_PERMISSION(permission::b_channel_create_modify_conversation_mode_private, 1, channel_id);
                cmd[property::name(property::CHANNEL_CONVERSATION_MODE)] = CHANNELCONVERSATIONMODE_PRIVATE;
            } else {
                ACTION_REQUIRES_PERMISSION(permission::b_channel_create_modify_conversation_mode_public, 1, channel_id);
                cmd[property::name(property::CHANNEL_CONVERSATION_MODE)] = CHANNELCONVERSATIONMODE_PUBLIC;
            }
            continue;
        } else if (key == "channel_conversation_mode") {
            auto value = cmd["channel_conversation_mode"].as<ChannelConversationMode>();
            switch (value) {
                case ChannelConversationMode::CHANNELCONVERSATIONMODE_PRIVATE:
                    ACTION_REQUIRES_PERMISSION(permission::b_channel_create_modify_conversation_mode_private, 1, channel_id);
                    break;
                case ChannelConversationMode::CHANNELCONVERSATIONMODE_PUBLIC:
                    ACTION_REQUIRES_PERMISSION(permission::b_channel_create_modify_conversation_mode_public, 1, channel_id);
                    break;
                case ChannelConversationMode::CHANNELCONVERSATIONMODE_NONE:
                    ACTION_REQUIRES_PERMISSION(permission::b_channel_create_modify_conversation_mode_none, 1, channel_id);
                    break;
                default:
                    return command_result{error::parameter_invalid, "channel_conversation_mode"};
            }
        } else if(key == "channel_sidebar_mode") {
            ACTION_REQUIRES_PERMISSION(permission::b_channel_create_modify_sidebar_mode, 1, channel_id);
        } else {
            logCritical(
                    this->getServerId(),
                    "{} Tried to change a editable channel property but we haven't found a permission. Please report this error. (Channel property: {})",
                    CLIENT_STR_LOG_PREFIX,
                    key
            );
            continue;
        }

        new_values.emplace((property::ChannelProperties) property.property_index, cmd[key].string());
    }

    channel_tree_read_lock.unlock();
    return this->execute_channel_edit(channel_id, new_values, false);
}

/*
 * 1. Basic value validation.
 * 2. Lock the server channel tree in write mode and collect changes
 * 2.1. Apply changes
 * 3. Test if any other channels have been affected and change them (e. g. default channel)
 * 4. notify everyone
 */
ts::command_result ConnectedClient::execute_channel_edit(ChannelId& channel_id, const std::map<property::ChannelProperties, std::string>& values, bool is_channel_create) {
    auto server = this->getServer();

    /*
     * Flags which categories are getting changed.
     * These flags will be set in 2.1.
     */
    bool updating_name{false};
    bool updating_default{false};
    bool updating_password{false};
    bool updating_max_clients{false};
    bool updating_max_family_clients{false};
    bool updating_talk_power{false};
    bool updating_type{false};
    bool updating_sort_order{false};

    /* Step 1: Parse all values which are possible and validate them without any context. */
    for(const auto& [ property, value ] : values) {
        try {
            switch(property) {
                case property::CHANNEL_ID:
                    continue;

                case property::CHANNEL_PID:
                case property::CHANNEL_ORDER: {
                    converter<ChannelId>::from_string_view(value);
                    break;
                }

                case property::CHANNEL_NAME:
                case property::CHANNEL_NAME_PHONETIC: {
                    auto length = utf8::count_characters(value);
                    if(length < 0 || length > 40) {
                        /* max channel name length is 40 */
                        return ts::command_result{error::parameter_invalid, std::string{property::describe(property).name}};
                    }

                    if(length == 0 && property == property::CHANNEL_NAME) {
                        return ts::command_result{error::parameter_invalid, std::string{property::describe(property).name}};
                    }
                    break;
                }

                case property::CHANNEL_TOPIC:
                    if(value.length() > 255) {
                        /* max channel name length is 255 bytes, everythign else will disconnect the client */
                        return ts::command_result{error::parameter_invalid, std::string{property::describe(property).name}};
                    }
                    break;

                case property::CHANNEL_DESCRIPTION:
                    if(value.length() > 8192) {
                        /* max channel name length is 8192 bytes, everythign else will disconnect the client */
                        return ts::command_result{error::parameter_invalid, std::string{property::describe(property).name}};
                    }
                    break;

                case property::CHANNEL_PASSWORD:
                    updating_password = true;
                    break;

                case property::CHANNEL_CODEC: {
                    auto codec_value = converter<uint32_t>::from_string_view(value);
                    if (!(codec_value >= 4 && codec_value <= 5)) {
                        return command_result{error::parameter_invalid, std::string{property::describe(property).name}};
                    }
                    break;
                }

                case property::CHANNEL_CODEC_QUALITY: {
                    auto codec_value = converter<uint32_t>::from_string_view(value);
                    if (codec_value > 10) {
                        return command_result{error::parameter_invalid, std::string{property::describe(property).name}};
                    }
                    break;
                }

                case property::CHANNEL_MAXCLIENTS:
                case property::CHANNEL_MAXFAMILYCLIENTS:  {
                    auto max_clients = converter<int>::from_string_view(value);
                    if(max_clients < -1) {
                        return command_result{error::parameter_invalid, std::string{property::describe(property).name}};
                    }
                    break;
                }

                case property::CHANNEL_FLAG_PERMANENT:
                case property::CHANNEL_FLAG_SEMI_PERMANENT:
                case property::CHANNEL_CODEC_IS_UNENCRYPTED:
                case property::CHANNEL_FLAG_MAXCLIENTS_UNLIMITED:
                case property::CHANNEL_FLAG_MAXFAMILYCLIENTS_UNLIMITED:
                case property::CHANNEL_FLAG_MAXFAMILYCLIENTS_INHERITED: {
                    /* validate value */
                    converter<bool>::from_string_view(value);
                    break;
                }

                case property::CHANNEL_FLAG_PASSWORD: {
                    converter<bool>::from_string_view(value);
                    updating_password = true;
                    break;
                }

                case property::CHANNEL_FLAG_DEFAULT: {
                    if(!converter<bool>::from_string_view(value)) {
                        /* The default channel flag can only be enabled. "Disabling" will be done by enabling the default flag somewhere else. */
                        continue;
                    }
                    break;
                }

                case property::CHANNEL_DELETE_DELAY: {
                    converter<uint32_t>::from_string_view(value);
                    break;
                }

                case property::CHANNEL_ICON_ID: {
                    converter<uint32_t>::from_string_view(value);
                    break;
                }

                case property::CHANNEL_CONVERSATION_HISTORY_LENGTH: {
                    converter<int64_t>::from_string_view(value);
                    break;
                }

                case property::CHANNEL_CONVERSATION_MODE: {
                    switch(converter<ChannelConversationMode>::from_string_view(value)) {
                        case ChannelConversationMode::CHANNELCONVERSATIONMODE_PRIVATE:
                        case ChannelConversationMode::CHANNELCONVERSATIONMODE_PUBLIC:
                        case ChannelConversationMode::CHANNELCONVERSATIONMODE_NONE:
                            break;

                        default:
                            return command_result{error::parameter_invalid, std::string{property::describe(property).name}};
                    }
                    break;
                }

                case property::CHANNEL_SIDEBAR_MODE: {
                    switch (converter<ChannelSidebarMode>::from_string_view(value)) {
                        case ChannelSidebarMode::CHANNELSIDEBARMODE_CONVERSATION:
                        case ChannelSidebarMode::CHANNELSIDEBARMODE_DESCRIPTION:
                        case ChannelSidebarMode::CHANNELSIDEBARMODE_FILE_TRANSFER:
                            break;

                        default:
                            return command_result{error::parameter_invalid, std::string{property::describe(property).name}};
                    }
                    break;
                }

                    /* non editable properties */
                case property::CHANNEL_FLAG_ARE_SUBSCRIBED:
                case property::CHANNEL_FORCED_SILENCE:
                case property::CHANNEL_UNDEFINED:
                case property::CHANNEL_CODEC_LATENCY_FACTOR:
                case property::CHANNEL_SECURITY_SALT:
                case property::CHANNEL_FILEPATH:
                case property::CHANNEL_FLAG_PRIVATE:
                case property::CHANNEL_LAST_LEFT:
                case property::CHANNEL_CREATED_AT:
                case property::CHANNEL_CREATED_BY:
                case property::CHANNEL_ENDMARKER:
                    break;

                case property::CHANNEL_NEEDED_TALK_POWER:
                    converter<int>::from_string_view(value);
                    break;

                default:
                    debugMessage(this->getServerId(), "{} Tried to edit an unknown channel property: {}", (int) property);
                    continue;
            }
        } catch(std::exception&) {
            return command_result{error::parameter_invalid, std::string{property::describe(property).name}};
        }
    }

    auto target_channel_tree = server ? server->getChannelTree() : &*serverInstance->getChannelTree();
    std::unique_lock channel_tree_lock{server ? server->channel_tree_mutex : serverInstance->getChannelTreeLock()};

    struct TemporaryCreatedChannel {
        ServerChannelTree* channel_tree;
        std::shared_ptr<ServerChannel> channel;

        ~TemporaryCreatedChannel() {
            if(!this->channel) { return; }

            channel_tree->deleteChannelRoot(this->channel);
        }
    } temporary_created_channel{target_channel_tree, nullptr};

    std::shared_ptr<TreeView::LinkedTreeEntry> linked_channel;
    std::shared_ptr<ServerChannel> channel;
    if(is_channel_create) {
        if(!values.contains(property::CHANNEL_NAME)) {
            return command_result{error::parameter_invalid, std::string{property::describe(property::CHANNEL_NAME).name}};
        }

        auto parent_id = values.contains(property::CHANNEL_PID) ? converter<ChannelId>::from_string_view(values.at(property::CHANNEL_PID)) : 0;
        auto order_id = values.contains(property::CHANNEL_ORDER) ? converter<ChannelId>::from_string_view(values.at(property::CHANNEL_ORDER)) : 0;
        auto channel_name = values.at(property::CHANNEL_NAME);

        /* checking if the channel name is unique */
        {
            std::shared_ptr<BasicChannel> parent_channel{};
            if(parent_id > 0) {
                parent_channel = target_channel_tree->findChannel(parent_id);
                if(!parent_channel) {
                    return command_result{error::channel_invalid_id};
                }
            }

            if(target_channel_tree->findChannel(channel_name, parent_channel)) {
                return ts::command_result{error::channel_name_inuse};
            }
        }

        channel = dynamic_pointer_cast<ServerChannel>(target_channel_tree->createChannel(parent_id, order_id, channel_name));
        if(!channel) {
            return command_result{error::vs_critical, "channel create failed"};
        }
        temporary_created_channel.channel = channel;

        linked_channel = target_channel_tree->findLinkedChannel(channel->channelId());
        if(!linked_channel) {
            return command_result{error::vs_critical, "missing linked channel"};
        }

        channel_id = channel->channelId();
    } else {
        linked_channel = target_channel_tree->findLinkedChannel(channel_id);
        channel = dynamic_pointer_cast<ServerChannel>(linked_channel->entry);
    }

    if(!channel || channel->deleted) {
        /* channel has not been found or deleted */
        return ts::command_result{error::channel_invalid_id};
    }

    /* Step 2: Remove all not changed properties and test the updates */
    auto channel_properties = channel->properties();
    std::map<property::ChannelProperties, std::string> changed_values{};

    for(auto& [ key, value ] : values) {
        if(channel_properties[key].value() == value) {
            continue;
        }

        switch(key) {
            case property::CHANNEL_NAME:
                updating_name = true;
                break;

            case property::CHANNEL_NAME_PHONETIC:
                /* even though this is a name update, the phonetic name must not be unique */
            case property::CHANNEL_ICON_ID:
            case property::CHANNEL_TOPIC:
            case property::CHANNEL_DESCRIPTION:
            case property::CHANNEL_CODEC:
            case property::CHANNEL_CODEC_QUALITY:
            case property::CHANNEL_CODEC_IS_UNENCRYPTED:
            case property::CHANNEL_DELETE_DELAY:
            case property::CHANNEL_SIDEBAR_MODE:
                break;

            case property::CHANNEL_PASSWORD:
            case property::CHANNEL_FLAG_PASSWORD:
                updating_password = true;
                break;


            case property::CHANNEL_MAXCLIENTS:
            case property::CHANNEL_FLAG_MAXCLIENTS_UNLIMITED:
                updating_max_clients = true;
                break;


            case property::CHANNEL_MAXFAMILYCLIENTS:
            case property::CHANNEL_FLAG_MAXFAMILYCLIENTS_INHERITED:
            case property::CHANNEL_FLAG_MAXFAMILYCLIENTS_UNLIMITED:
                updating_max_family_clients = true;
                break;


            case property::CHANNEL_ORDER:
                if(is_channel_create) {
                    return ts::command_result{error::vs_critical, "having channel order update but channel has been created"};
                }
                updating_sort_order = true;
                break;

            case property::CHANNEL_FLAG_PERMANENT:
            case property::CHANNEL_FLAG_SEMI_PERMANENT:
                updating_type = true;
                break;


            case property::CHANNEL_FLAG_DEFAULT:
                updating_default = true;
                break;

            case property::CHANNEL_NEEDED_TALK_POWER:
                updating_talk_power = true;
                break;

            case property::CHANNEL_CONVERSATION_HISTORY_LENGTH:
            case property::CHANNEL_CONVERSATION_MODE:
                break;

            /* non updatable properties */
            case property::CHANNEL_FLAG_ARE_SUBSCRIBED:
            case property::CHANNEL_FORCED_SILENCE:
            case property::CHANNEL_UNDEFINED:
            case property::CHANNEL_ID:
            case property::CHANNEL_PID:
            case property::CHANNEL_CODEC_LATENCY_FACTOR:
            case property::CHANNEL_SECURITY_SALT:
            case property::CHANNEL_FILEPATH:
            case property::CHANNEL_FLAG_PRIVATE:
            case property::CHANNEL_LAST_LEFT:
            case property::CHANNEL_CREATED_AT:
            case property::CHANNEL_CREATED_BY:
            case property::CHANNEL_ENDMARKER:
                break;

            default:
                logCritical(this->getServerId(), "{} Channel property {} reached context validation context but we don't know how to handle it. Please report this bug!", CLIENT_LOG_PREFIX, property::describe(key).name);
                continue;
        }

        changed_values.emplace(key, std::move(value));
    }

    if(changed_values.empty() && !is_channel_create) {
        /* nothing to change */
        return ts::command_result{error::database_no_modifications};
    }

    auto target_channel_property_value = [&](property::ChannelProperties property) {
        return changed_values.contains(property) ? std::string{changed_values.at(property)} : channel_properties[property].value();
    };

    auto reset_client_limitations = [&]{
        /* Updating the max clients */
        if(!converter<bool>::from_string_view(target_channel_property_value(property::CHANNEL_FLAG_MAXCLIENTS_UNLIMITED))) {
            updating_max_clients = true;
            changed_values[property::CHANNEL_MAXCLIENTS] = "-1";
            changed_values[property::CHANNEL_FLAG_MAXCLIENTS_UNLIMITED] = "1";
        }

        if(!converter<bool>::from_string_view(target_channel_property_value(property::CHANNEL_FLAG_MAXFAMILYCLIENTS_UNLIMITED))) {
            updating_max_family_clients = true;
            changed_values[property::CHANNEL_MAXFAMILYCLIENTS] = "-1";
            changed_values[property::CHANNEL_FLAG_MAXFAMILYCLIENTS_INHERITED] = "0";
            changed_values[property::CHANNEL_FLAG_MAXFAMILYCLIENTS_UNLIMITED] = "1";
        }
    };

    if(updating_name) {
        auto new_name = changed_values[property::CHANNEL_NAME];
        if(target_channel_tree->findChannel(new_name, channel->parent())) {
            return ts::command_result{error::channel_name_inuse};
        }
    }

    if(updating_password) {
        auto password_provided = changed_values.contains(property::CHANNEL_PASSWORD) && !changed_values[property::CHANNEL_PASSWORD].empty();
        if(values.contains(property::CHANNEL_FLAG_PASSWORD)) {
            auto has_password = converter<bool>::from_string_view(changed_values[property::CHANNEL_FLAG_PASSWORD]);
            if(has_password && !password_provided) {
                return command_result{error::parameter_missing, std::string{property::describe(property::CHANNEL_PASSWORD).name}};
            } else if(!has_password && password_provided) {
                return command_result{error::parameter_invalid, std::string{property::describe(property::CHANNEL_FLAG_PASSWORD).name}};
            }
        } else if(password_provided) {
            /* we've a password but the remote was too lazy to set the password flag */
            changed_values[property::CHANNEL_FLAG_PASSWORD] = "1";
        }
    }

    /* We need to calculate the type for other checks as well do don't only calculate it when updating the type */
    ChannelType::ChannelType target_channel_type;

    (void) updating_type;
    {
        auto flag_permanent = converter<bool>::from_string_view(target_channel_property_value(property::CHANNEL_FLAG_PERMANENT));
        auto flag_semi_permanent = converter<bool>::from_string_view(target_channel_property_value(property::CHANNEL_FLAG_SEMI_PERMANENT));

        if(flag_permanent) {
            if(flag_semi_permanent) {
                /* we can't be permanent and semi permanent */

                if(changed_values.contains(property::CHANNEL_FLAG_PERMANENT)) {
                    return command_result{error::channel_invalid_flags, std::string{property::describe(property::CHANNEL_FLAG_PERMANENT).name}};
                } else {
                    return command_result{error::channel_invalid_flags, std::string{property::describe(property::CHANNEL_FLAG_SEMI_PERMANENT).name}};
                }
            }

            target_channel_type = ChannelType::permanent;
        } else if(flag_semi_permanent) {
            target_channel_type = ChannelType::semipermanent;
        } else {
            target_channel_type = ChannelType::temporary;
        }
    }

    if(updating_max_clients) {
        if(changed_values.contains(property::CHANNEL_FLAG_MAXCLIENTS_UNLIMITED)) {
            /* The user explicitly toggled the max clients unlimited flag */
            auto unlimited = converter<bool>::from_string_view(changed_values[property::CHANNEL_FLAG_MAXCLIENTS_UNLIMITED]);
            if(unlimited) {
                /*
                 * Change the max clients if not already done by the user.
                 * We may should test if the user really set them to -1 but nvm.
                 *
                 * Since we must come from a limited channel, else we would not have a change, we can ensure that the previous value isn't -1.
                 * This means that we've definitively a change.
                 */
                changed_values[property::CHANNEL_MAXCLIENTS] = "-1";
            } else if(!changed_values.contains(property::CHANNEL_MAXCLIENTS)) {
                /* if the user enabled max clients it should also provide a value */
                return ts::command_result{error::parameter_missing, std::string{property::describe(property::CHANNEL_FLAG_MAXCLIENTS_UNLIMITED).name}};
            } else {
                auto value = converter<int>::from_string_view(changed_values[property::CHANNEL_MAXCLIENTS]);
                if(value < 0) {
                    return ts::command_result{error::parameter_invalid, std::string{property::describe(property::CHANNEL_MAXCLIENTS).name}};
                }

                /* everything is fine */
            }
        } else if(changed_values.contains(property::CHANNEL_MAXCLIENTS)) {
            /* the user was too lazy to set the flag max clients unlimited property accordingly */
            auto value = converter<int>::from_string_view(changed_values[property::CHANNEL_MAXCLIENTS]);
            if(value >= 0) {
                changed_values[property::CHANNEL_FLAG_MAXCLIENTS_UNLIMITED] = "0";
            } else {
                changed_values[property::CHANNEL_FLAG_MAXCLIENTS_UNLIMITED] = "1";
            }
        } else {
            assert(false);
            logCritical(this->getServerId(), "updating_max_clients has been set without a changed max client channel property.");
        }
    }

    if(updating_max_family_clients) {
        auto unlimited = converter<bool>::from_string_view(target_channel_property_value(property::CHANNEL_FLAG_MAXFAMILYCLIENTS_UNLIMITED));
        auto inherited = converter<bool>::from_string_view(target_channel_property_value(property::CHANNEL_FLAG_MAXFAMILYCLIENTS_INHERITED));

        if(changed_values.contains(property::CHANNEL_FLAG_MAXFAMILYCLIENTS_UNLIMITED) && unlimited) {
            /* The user explicitly enabled the max family clients unlimited flag */

            /*
             * Change the max family clients and the inherited flag if not already done by the user.
             * We may should test if the user really set them to -1 but nvm.
             */
            if(target_channel_property_value(property::CHANNEL_FLAG_MAXFAMILYCLIENTS_INHERITED) != "0") {
                changed_values[property::CHANNEL_FLAG_MAXFAMILYCLIENTS_INHERITED] = "0";
            }
            if(target_channel_property_value(property::CHANNEL_MAXFAMILYCLIENTS) != "-1") {
                changed_values[property::CHANNEL_MAXFAMILYCLIENTS] = "-1";
            }
        } else if(changed_values.contains(property::CHANNEL_FLAG_MAXFAMILYCLIENTS_INHERITED) && inherited) {
            /* The user explicitly enabled the max family clients inherited flag */

            /*
             * Change the max family clients and the unlimized flag if not already done by the user.
             * We may should test if the user really set them to -1 but nvm.
             */
            if(target_channel_property_value(property::CHANNEL_FLAG_MAXFAMILYCLIENTS_UNLIMITED) != "0") {
                changed_values[property::CHANNEL_FLAG_MAXFAMILYCLIENTS_UNLIMITED] = "0";
            }

            if(target_channel_property_value(property::CHANNEL_MAXFAMILYCLIENTS) != "-1") {
                changed_values[property::CHANNEL_MAXFAMILYCLIENTS] = "-1";
            }
        } else if(changed_values.contains(property::CHANNEL_MAXFAMILYCLIENTS)) {
            /* The user explicitly enabled max channel clients */
            auto value = converter<int>::from_string_view(changed_values[property::CHANNEL_MAXFAMILYCLIENTS]);
            if(value < 0) {
                return ts::command_result{error::parameter_invalid, std::string{property::describe(property::CHANNEL_MAXFAMILYCLIENTS).name}};
            }

            /* everythign is fine */
        } else {
            /* If enabling a channel family client limit the user must supply the amount of max clients */
            return ts::command_result{error::parameter_missing, "channel_maxfamilyclients"};
        }
    }

    /*
     * Validating the target channel type.
     * This check required that the max (family) clients have been validated and the flags have been set correctly.
     * Attention: Sub channels will not be checked. If their type does not match they will be updated automatically.
     */
    switch(target_channel_type) {
        case ChannelType::permanent: {
            auto parent = channel->parent();
            if(parent && parent->channelType() != ChannelType::permanent) {
                return ts::command_result{error::channel_parent_not_permanent};
            }
            break;
        }

        case ChannelType::semipermanent: {
            auto parent = channel->parent();
            if(parent && parent->channelType() > ChannelType::semipermanent) {
                return ts::command_result{error::channel_parent_not_permanent};
            }
            break;
        }

        case ChannelType::temporary: {
            /* max (family) client should not be set */
            reset_client_limitations();
            break;
        }

        default:
            assert(false);
            break;
    }

    /*
     * Validating the default channel change.
     * This check required that the channel type and max (family) clients have been validated and the flags have been set correctly.
     */
    if(updating_default) {
        if(target_channel_type != ChannelType::permanent) {
            return command_result{error::channel_default_require_permanent};
        }

        /* validate tree visibility */
        std::shared_ptr<BasicChannel> current_channel{channel};
        while(current_channel) {
            auto permission = current_channel->permissions()->permission_value_flagged(permission::i_channel_needed_view_power);
            if(permission.has_value) {
                return ts::command_result{error::channel_family_not_visible};
            }

            current_channel = current_channel->parent();
        }

        /*
         * Note: all changes of the channel flags happen after the validation.
         * This implies all changes made must ensure that the overall channel flags are valid!
         */

        /* Remove the password if there is any  */
        if(converter<bool>::from_string_view(target_channel_property_value(property::CHANNEL_FLAG_PASSWORD))) {
            updating_password = true;
            changed_values[property::CHANNEL_FLAG_PASSWORD] = "0";
            changed_values[property::CHANNEL_PASSWORD] = "";
        }

        /* Remove all client restrictions */
        reset_client_limitations();
    }

    ChannelId previous_channel_id{0};
    if(updating_sort_order) {
        previous_channel_id = converter<ChannelId>::from_string_view(changed_values[property::CHANNEL_ORDER]);
        auto previous_channel = target_channel_tree->findChannel(previous_channel_id);
        if(!previous_channel && previous_channel_id != 0) {
            return command_result{error::channel_invalid_id};
        }

        if(!target_channel_tree->move_channel(channel, channel->parent(), previous_channel)) {
            return ts::command_result{error::vs_critical, "failed to move channel"};
        }
    }

    std::deque<std::tuple<std::shared_ptr<BasicChannel>, std::vector<property::ChannelProperties>>> child_channel_type_updates{};

    /* updating all child channels */
    {
        std::deque<std::shared_ptr<BasicChannel>> children_left{channel};

        while (!children_left.empty()) {
            auto current_channel = children_left.front();
            children_left.pop_front();

            for (const auto &child : target_channel_tree->channels(current_channel, 1)) {
                if (child == current_channel) {
                    continue;
                }

                if (child->channelType() < target_channel_type) {
                    std::vector<property::ChannelProperties> channel_property_updates{};
                    channel_property_updates.reserve(16);

                    child->updateChannelType(channel_property_updates, target_channel_type);
                    if(target_channel_type == ChannelType::temporary) {
                        if(child->properties()[property::CHANNEL_FLAG_MAXCLIENTS_UNLIMITED].update_value(true)) {
                            channel_property_updates.push_back(property::CHANNEL_FLAG_MAXCLIENTS_UNLIMITED);
                        }

                        if(child->properties()[property::CHANNEL_FLAG_MAXFAMILYCLIENTS_INHERITED].update_value(false)) {
                            channel_property_updates.push_back(property::CHANNEL_FLAG_MAXFAMILYCLIENTS_INHERITED);
                        }

                        if(child->properties()[property::CHANNEL_MAXCLIENTS].update_value(-1)) {
                            channel_property_updates.push_back(property::CHANNEL_MAXCLIENTS);
                        }

                        if(child->properties()[property::CHANNEL_MAXFAMILYCLIENTS].update_value(-1)) {
                            channel_property_updates.push_back(property::CHANNEL_MAXFAMILYCLIENTS);
                        }

                        if(child->properties()[property::CHANNEL_FLAG_MAXFAMILYCLIENTS_UNLIMITED].update_value(true)) {
                            channel_property_updates.push_back(property::CHANNEL_FLAG_MAXFAMILYCLIENTS_UNLIMITED);
                        }
                    }

                    children_left.push_back(child);
                    child_channel_type_updates.push_back(std::make_tuple(child, channel_property_updates));
                }
            }
        }

        /* reverse the order so the tree is at any state valid */
        std::reverse(child_channel_type_updates.begin(), child_channel_type_updates.end());
    }

    std::shared_ptr<BasicChannel> old_default_channel{};
    if(updating_default) {
        old_default_channel = target_channel_tree->getDefaultChannel();
        if(old_default_channel) {
            old_default_channel->properties()[property::CHANNEL_FLAG_DEFAULT] = false;
        }
    }

    auto self_ref = this->ref();
    for(const auto& [ key, value ] : changed_values) {
        if(key == property::CHANNEL_ICON_ID) {
            /* we've to change the permission as well */

            auto icon_id = converter<uint32_t>::from_string_view(value);
            channel->permissions()->set_permission(permission::i_icon_id,
                                                   { (permission::PermissionValue) icon_id, (permission::PermissionValue) icon_id },
                                                   permission::v2::PermissionUpdateType::set_value,
                                                   permission::v2::PermissionUpdateType::do_nothing,
                                                   false,
                                                   false);
        } else if(key == property::CHANNEL_NEEDED_TALK_POWER) {
            /* we've to change the permission as well */

            auto talk_power = converter<permission::PermissionValue>::from_string_view(value);
            channel->permissions()->set_permission(permission::i_client_needed_talk_power,
                                                   { talk_power, talk_power },
                                                   permission::v2::PermissionUpdateType::set_value,
                                                   permission::v2::PermissionUpdateType::do_nothing,
                                                   false,
                                                   false);
        } else if(key == property::CHANNEL_CONVERSATION_HISTORY_LENGTH) {
            if(server) {
                auto conversation_manager = server->conversation_manager();
                if (conversation_manager) {
                    auto conversation = conversation_manager->get(channel->channelId());
                    if (conversation) {
                        conversation->set_history_length(converter<int64_t>::from_string_view(value));
                    }
                }
            }
        }

        auto old_value = channel_properties[key].value();
        channel_properties[key] = value;
        serverInstance->action_logger()->channel_logger.log_channel_edit(this->getServerId(), self_ref, channel_id, property::describe(key), old_value, value);
    }

    std::vector<std::shared_ptr<ConnectedClient>> clients{};
    if(server) {
        clients = server->getClients();
    } else {
        clients.push_back(this->ref());
    }

    std::shared_ptr<TreeView::LinkedTreeEntry> linked_parent_channel{};
    std::shared_ptr<TreeView::LinkedTreeEntry> linked_previous_channel{};
    if(updating_sort_order) {
        auto parent = channel->parent();

        linked_parent_channel = parent ? target_channel_tree->findLinkedChannel(parent->channelId()) : nullptr;
        linked_previous_channel = target_channel_tree->findLinkedChannel(channel->previousChannelId());

        assert(!parent || linked_parent_channel);
        assert(linked_previous_channel || channel->previousChannelId() == 0);
    }

    std::vector<property::ChannelProperties> default_channel_property_updates{
            property::CHANNEL_FLAG_DEFAULT,
    };

    std::vector<property::ChannelProperties> changed_properties{};
    changed_properties.reserve(changed_values.size());
    for(const auto& [ key, _ ] : changed_values) {
        changed_properties.push_back(key);
    }

    for(const auto& client : clients) {
        std::shared_lock disconnect_lock{client->finalDisconnectLock, std::try_to_lock};
        if(!disconnect_lock.owns_lock()) {
            /* client is already disconnecting */
            continue;
        }

        if(client->state != ConnectionState::CONNECTED || client->getType() == ClientType::CLIENT_INTERNAL) {
            /* these clients have no need to receive any updates */
            continue;
        }

        std::unique_lock client_tree_lock{client->channel_tree_mutex};
        for(const auto& [ child_channel, updates ] : child_channel_type_updates) {
            client->notifyChannelEdited(child_channel, updates, self_ref, false);
        }

        if(is_channel_create) {
            auto client_view_channel = client->channel_view()->add_channel(linked_channel);
            if(client_view_channel) {
                client->notifyChannelCreate(channel, client_view_channel->previousChannelId(), self_ref);
            } else {
                /* channel will not be visible for the target client */
                continue;
            }
        } else {
            if(updating_sort_order) {
                auto actions = client->channel_view()->change_order(linked_channel, linked_parent_channel, linked_previous_channel);
                std::deque<ChannelId> deletions{};

                for (const auto &action : actions) {
                    switch (action.first) {
                        case ClientChannelView::NOTHING:
                            continue;

                        case ClientChannelView::ENTER_VIEW:
                            client->notifyChannelShow(action.second->channel(), action.second->previous_channel);
                            break;

                        case ClientChannelView::DELETE_VIEW:
                            deletions.push_back(action.second->channelId());
                            break;

                        case ClientChannelView::MOVE:
                            client->notifyChannelMoved(action.second->channel(), action.second->previous_channel, this->ref());
                            break;

                        case ClientChannelView::REORDER:
                            client->notifyChannelEdited(action.second->channel(), {property::CHANNEL_ORDER}, self_ref, false);
                            break;
                    }
                }

                if (!deletions.empty()) {
                    /*
                     * This should not happen since in worst case we're just moving the channel.
                     * This should not have an effect on the channel visibility itself.
                     */
                    client->notifyChannelHide(deletions, false);
                    continue;
                }
            }

            client->notifyChannelEdited(channel, changed_properties, self_ref, false);
        }

        if(old_default_channel) {
            client->notifyChannelEdited(old_default_channel, default_channel_property_updates, self_ref, false);
        }

        if(updating_talk_power) {
            client->task_update_channel_client_properties.enqueue();
        }
    }

    /* Channel create was successful. Release delete struct. */
    temporary_created_channel.channel = nullptr;
    return command_result{error::ok};
};

command_result ConnectedClient::handleCommandChannelMove(Command &cmd) {
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(25);
    RESOLVE_CHANNEL_W(cmd["cid"], true);
    auto channel = dynamic_pointer_cast<ServerChannel>(l_channel->entry);
    assert(channel);
    if (channel->deleted)
        return command_result{error::channel_is_deleted};

    if (!cmd[0].has("order"))
        cmd["order"] = 0;

    auto l_parent = channel_tree->findLinkedChannel(cmd["cpid"]);
    shared_ptr<TreeView::LinkedTreeEntry> l_order;
    if (cmd[0].has("order")) {
        l_order = channel_tree->findLinkedChannel(cmd["order"]);
        if (!l_order && cmd["order"].as<ChannelId>() != 0) return command_result{error::channel_invalid_id};
    } else {
        l_order = l_parent ? l_parent->child_head : (this->server ? this->server->getChannelTree() : serverInstance->getChannelTree().get())->tree_head();
        while (l_order && l_order->next)
            l_order = l_order->next;
    }

    auto parent = l_parent ? dynamic_pointer_cast<ServerChannel>(l_parent->entry) : nullptr;
    auto order = l_order ? dynamic_pointer_cast<ServerChannel>(l_order->entry) : nullptr;

    if ((parent && parent->deleted) || (order && order->deleted))
        return command_result{error::channel_is_deleted, "parent channel order previous channel has been deleted"};

    if (channel->parent() == parent && channel->channelOrder() == (order ? order->channelId() : 0))
        return command_result{error::ok};

    auto old_parent_channel_id = channel->parent() ? channel->parent()->channelId() : 0;
    auto old_channel_order = channel->channelOrder();

    bool change_parent{channel->parent() != parent};
    bool change_order{(order ? order->channelId() : 0) != channel->channelOrder()};

    if (change_parent)
        ACTION_REQUIRES_PERMISSION(permission::b_channel_modify_parent, 1, channel_id);

    if (change_order)
        ACTION_REQUIRES_PERMISSION(permission::b_channel_modify_sortorder, 1, channel_id);

    {
        auto min_channel_deep = this->calculate_permission(permission::i_channel_min_depth, 0, false);
        auto max_channel_deep = this->calculate_permission(permission::i_channel_max_depth, 0, false);

        if (min_channel_deep.has_value || max_channel_deep.has_value) {
            auto channel_deep = 0;
            auto local_parent = l_parent;
            while (local_parent) {
                channel_deep++;
                local_parent = local_parent->parent.lock();
            }

            if (min_channel_deep.has_value && (channel_deep < min_channel_deep.value && !min_channel_deep.has_infinite_power())) return command_result{permission::i_channel_min_depth};
            if (max_channel_deep.has_value && !permission::v2::permission_granted(channel_deep, max_channel_deep)) return command_result{permission::i_channel_max_depth};
        }
    }
    {
        auto name = channel_tree->findChannel(channel->name(), parent);
        if (name && name != channel) return command_result{error::channel_name_inuse};
    }
    debugMessage(this->getServerId(), "Moving channel {} from old [{} | {}] to [{} | {}]", channel->name(), channel->channelOrder(), channel->parent() ? channel->parent()->channelId() : 0, order ? order->channelId() : 0, parent ? parent->channelId() : 0);

    if (!channel_tree->move_channel(channel, parent, order)) return command_result{error::channel_invalid_order, "Cant change order id"};

    deque<shared_ptr<BasicChannel>> channel_type_updates;
    {
        auto flag_default = channel->defaultChannel();
        auto current_channel = channel;
        do {
            if (flag_default) {
                if (current_channel->channelType() != ChannelType::permanent) {
                    current_channel->setChannelType(ChannelType::permanent);
                    channel_type_updates.push_front(current_channel);
                }
            } else if (current_channel->hasParent()) {
                if (current_channel->channelType() < current_channel->parent()->channelType()) {
                    current_channel->setChannelType(current_channel->parent()->channelType());
                    channel_type_updates.push_front(current_channel);
                }
            }
        } while ((current_channel = dynamic_pointer_cast<ServerChannel>(current_channel->parent())));
    }

    /* FIXME: Test if the new channel family isn't visible by default and if we're currently moving the default channel */

    /* log all the updates */
    serverInstance->action_logger()->channel_logger.log_channel_move(this->getServerId(), this->ref(), channel->channelId(),
                                                                     old_parent_channel_id, channel->parent() ? channel->parent()->channelId() : 0,
                                                                     old_channel_order, channel->channelOrder());

    for (const auto &type_update : channel_type_updates) {
        serverInstance->action_logger()->channel_logger.log_channel_edit(this->getServerId(), this->ref(), type_update->channelId(),
                                                                         property::describe(property::CHANNEL_FLAG_PERMANENT),
                                                                         "",
                                                                         type_update->properties()[property::CHANNEL_FLAG_PERMANENT].value());
        serverInstance->action_logger()->channel_logger.log_channel_edit(this->getServerId(), this->ref(), type_update->channelId(),
                                                                         property::describe(property::CHANNEL_FLAG_SEMI_PERMANENT),
                                                                         "",
                                                                         type_update->properties()[property::CHANNEL_FLAG_SEMI_PERMANENT].value());
    }

    if (this->server) {
        auto self_rev = this->ref();
        this->server->forEachClient([&](const shared_ptr<ConnectedClient> &client) {
            unique_lock channel_lock(client->channel_tree_mutex);
            for (const auto &type_update : channel_type_updates) {
                client->notifyChannelEdited(type_update, {property::CHANNEL_FLAG_PERMANENT, property::CHANNEL_FLAG_SEMI_PERMANENT}, self_rev, false);
            }

            auto actions = client->channel_tree->change_order(l_channel, l_parent, l_order);
            std::deque<ChannelId> deletions;
            for (const auto &action : actions) {
                switch (action.first) {
                    case ClientChannelView::NOTHING:
                        continue;
                    case ClientChannelView::ENTER_VIEW:
                        client->notifyChannelShow(action.second->channel(), action.second->previous_channel);
                        break;
                    case ClientChannelView::DELETE_VIEW:
                        deletions.push_back(action.second->channelId());
                        break;
                    case ClientChannelView::MOVE:
                        client->notifyChannelMoved(action.second->channel(), action.second->previous_channel, this->ref());
                        break;
                    case ClientChannelView::REORDER:
                        client->notifyChannelEdited(action.second->channel(), {property::CHANNEL_ORDER}, self_rev, false);
                        break;
                }
            }

            if (!deletions.empty()) {
                client->notifyChannelHide(deletions, false);
            }
        });
    }

    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandChannelPermList(Command &cmd) {
    CMD_CHK_AND_INC_FLOOD_POINTS(5);

    RESOLVE_CHANNEL_R(cmd["cid"], true);
    auto channel = dynamic_pointer_cast<BasicChannel>(l_channel->entry);
    assert(channel);

    ACTION_REQUIRES_PERMISSION(permission::b_virtualserver_channel_permission_list, 1, channel->channelId());

    if (this->getType() == ClientType::CLIENT_TEAMSPEAK && this->command_times.last_notify + this->command_times.notify_timeout < system_clock::now()) {
        this->sendTSPermEditorWarning();
    }

    auto sids = cmd.hasParm("permsid");

    Command result(this->notify_response_command("notifychannelpermlist"));
    int index = 0;
    result["cid"] = channel->channelId();

    auto permission_mapper = serverInstance->getPermissionMapper();
    auto type = this->getType();
    auto permission_manager = channel->permissions();
    for (const auto &permission_data : permission_manager->permissions()) {
        auto &permission = std::get<1>(permission_data);
        if (permission.flags.value_set) {
            if (sids) {
                result[index]["permsid"] = permission_mapper->permission_name(type, std::get<0>(permission_data));
            } else {
                result[index]["permid"] = std::get<0>(permission_data);
            }

            result[index]["permvalue"] = permission.values.value;
            result[index]["permnegated"] = permission.flags.negate;
            result[index]["permskip"] = permission.flags.skip;
            index++;
        }
        if (permission.flags.grant_set) {
            if (sids) {
                result[index]["permsid"] = permission_mapper->permission_name_grant(type, std::get<0>(permission_data));
            } else {
                result[index]["permid"] = (uint16_t)(std::get<0>(permission_data) | PERM_ID_GRANT);
            }
            result[index]["permvalue"] = permission.values.grant;
            result[index]["permnegated"] = 0;
            result[index]["permskip"] = 0;
            index++;
        }
    }
    if (index == 0)
        return command_result{error::database_empty_result};

    this->sendCommand(result);
    return command_result{error::ok};
}

//
//channel_icon_id=18446744073297259750
//channel_name
//channel_topic
//Desctiption has no extra parm
command_result ConnectedClient::handleCommandChannelAddPerm(Command &cmd) {
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(5);

    RESOLVE_CHANNEL_R(cmd["cid"], true);
    auto channel = dynamic_pointer_cast<BasicChannel>(l_channel->entry);
    assert(channel);

    ACTION_REQUIRES_CHANNEL_PERMISSION(channel, permission::i_channel_needed_permission_modify_power, permission::i_channel_permission_modify_power, true);

    ts::command::bulk_parser::PermissionBulksParser pparser{cmd, true};
    if (!pparser.validate(this->ref(), channel->channelId())) {
        return pparser.build_command_result();
    }

    /* test if we've the default channel */
    bool family_contains_default_channel{false};
    if(channel->properties()[property::CHANNEL_FLAG_DEFAULT].as_or<bool>(false)) {
        family_contains_default_channel = true;
    } else {
        for(const auto& child_channel : channel_tree->channels(channel)) {
            if(child_channel->properties()[property::CHANNEL_FLAG_DEFAULT].as_or<bool>(false)) {
                family_contains_default_channel = true;
                break;
            }
        }
    }

    if(family_contains_default_channel) {
        for (const auto &ppermission : pparser.iterate_valid_permissions()) {
            if(ppermission.is_grant_permission()) {
                continue;
            }

            if(!ppermission.has_value()) {
                continue;
            }

            if(ppermission.permission()->type == permission::i_channel_needed_view_power) {
                return ts::command_result{error::channel_default_require_visible};
            }
        }
    }

    auto permission_manager = channel->permissions();
    auto updateClients = false, update_join_permissions = false, update_channel_properties = false;
    auto channelId = channel->channelId();
    for (const auto &ppermission : pparser.iterate_valid_permissions()) {
        ppermission.apply_to(permission_manager, permission::v2::PermissionUpdateType::set_value);
        ppermission.log_update(serverInstance->action_logger()->permission_logger,
                               this->getServerId(),
                               this->ref(),
                               log::PermissionTarget::CHANNEL,
                               permission::v2::PermissionUpdateType::set_value,
                               channelId, channel->name(),
                               0, "");

        updateClients |= ppermission.is_client_view_property();
        update_join_permissions = ppermission.permission_type() == permission::i_channel_needed_join_power;
        update_channel_properties |= channel->permission_require_property_update(ppermission.permission_type());
    }

    if (update_channel_properties && this->server)
        this->server->update_channel_from_permissions(channel, this->ref());

    if ((updateClients || update_join_permissions) && this->server) {
        this->server->forEachClient([&](std::shared_ptr<ConnectedClient> cl) {
            if (updateClients && cl->currentChannel == channel) {
                cl->task_update_channel_client_properties.enqueue();
            }

            if (update_join_permissions) {
                cl->join_state_id++;
            }
        });
    }

    return pparser.build_command_result();
}

command_result ConnectedClient::handleCommandChannelDelPerm(Command &cmd) {
    CMD_RESET_IDLE;

    RESOLVE_CHANNEL_R(cmd["cid"], true)
    auto channel = dynamic_pointer_cast<BasicChannel>(l_channel->entry);
    assert(channel);

    ACTION_REQUIRES_CHANNEL_PERMISSION(channel, permission::i_channel_needed_permission_modify_power, permission::i_channel_permission_modify_power, true);

    ts::command::bulk_parser::PermissionBulksParser pparser{cmd, false};
    if (!pparser.validate(this->ref(), channel->channelId()))
        return pparser.build_command_result();

    auto permission_manager = channel->permissions();
    auto updateClients = false, update_join_permissions = false, update_channel_properties = false;
    auto channelId = channel->channelId();
    for (const auto &ppermission : pparser.iterate_valid_permissions()) {
        ppermission.apply_to(permission_manager, permission::v2::PermissionUpdateType::delete_value);
        ppermission.log_update(serverInstance->action_logger()->permission_logger,
                               this->getServerId(),
                               this->ref(),
                               log::PermissionTarget::CHANNEL,
                               permission::v2::PermissionUpdateType::delete_value,
                               channelId, channel->name(),
                               0, "");

        updateClients |= ppermission.is_client_view_property();
        update_join_permissions = ppermission.permission_type() == permission::i_channel_needed_join_power;
        update_channel_properties |= channel->permission_require_property_update(ppermission.permission_type());
    }

    if (update_channel_properties && this->server)
        this->server->update_channel_from_permissions(channel, this->ref());

    if ((updateClients || update_join_permissions) && this->server) {
        this->server->forEachClient([&](std::shared_ptr<ConnectedClient> cl) {
            if (updateClients && cl->currentChannel == channel)
                cl->task_update_channel_client_properties.enqueue();
            if (update_join_permissions)
                cl->join_state_id++;
        });
    }

    return pparser.build_command_result();
}

command_result ConnectedClient::handleCommandChannelClientPermList(Command &cmd) {
    CMD_REQ_SERVER;
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(5);
    RESOLVE_CHANNEL_R(cmd["cid"], true);
    auto channel = dynamic_pointer_cast<ServerChannel>(l_channel->entry);
    if (!channel) return command_result{error::vs_critical};
    ACTION_REQUIRES_PERMISSION(permission::b_virtualserver_channelclient_permission_list, 1, channel_id);

    if (!serverInstance->databaseHelper()->validClientDatabaseId(this->server, cmd["cldbid"])) return command_result{error::client_invalid_id};
    auto mgr = serverInstance->databaseHelper()->loadClientPermissionManager(this->getServerId(), cmd["cldbid"].as<ClientDbId>());

    Command res(this->getExternalType() == CLIENT_TEAMSPEAK ? "notifychannelclientpermlist" : "");

    auto permissions = mgr->channel_permissions(channel->channelId());
    if (permissions.empty())
        return command_result{error::database_empty_result};

    int index = 0;
    res[index]["cid"] = channel->channelId();
    res[index]["cldbid"] = cmd["cldbid"].as<ClientDbId>();

    auto sids = cmd.hasParm("permsid");
    auto permission_mapper = serverInstance->getPermissionMapper();
    auto type = this->getType();

    for (const auto &permission_data : permissions) {
        auto &permission = std::get<1>(permission_data);
        if (permission.flags.value_set) {
            if (sids)
                res[index]["permsid"] = permission_mapper->permission_name(type, get<0>(permission_data));
            else
                res[index]["permid"] = get<0>(permission_data);
            res[index]["permvalue"] = permission.values.value;

            res[index]["permnegated"] = permission.flags.negate;
            res[index]["permskip"] = permission.flags.skip;
            index++;
        }


        if (permission.flags.grant_set) {
            if (sids)
                res[index]["permsid"] = permission_mapper->permission_name_grant(type, get<0>(permission_data));
            else
                res[index]["permid"] = (get<0>(permission_data) | PERM_ID_GRANT);
            res[index]["permvalue"] = permission.values.grant;
            res[index]["permnegated"] = 0;
            res[index]["permskip"] = 0;
            index++;
        }
    }

    this->sendCommand(res);
    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandChannelClientDelPerm(Command &cmd) {
    CMD_REF_SERVER(server_ref);
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(5);

    RESOLVE_CHANNEL_R(cmd["cid"], true);
    auto channel = dynamic_pointer_cast<ServerChannel>(l_channel->entry);
    if (!channel) return command_result{error::vs_critical};

    auto target_client_database_id = cmd["cldbid"].as<ClientDbId>();
    ClientPermissionCalculator target_client_permission_calculator{this->server, target_client_database_id, ClientType::CLIENT_TEAMSPEAK, channel_id};
    ACTION_REQUIRES_PERMISSION(permission::i_client_permission_modify_power, target_client_permission_calculator.calculate_permission(permission::i_client_needed_permission_modify_power), channel_id);

    auto target_permission_manager = serverInstance->databaseHelper()->loadClientPermissionManager(this->getServerId(), target_client_database_id);

    ts::command::bulk_parser::PermissionBulksParser pparser{cmd, false};
    if (!pparser.validate(this->ref(), channel->channelId()))
        return pparser.build_command_result();

    bool update_view{false};
    for (const auto &ppermission : pparser.iterate_valid_permissions()) {
        ppermission.apply_to_channel(target_permission_manager, permission::v2::PermissionUpdateType::delete_value, channel->channelId());
        ppermission.log_update(serverInstance->action_logger()->permission_logger,
                               this->getServerId(),
                               this->ref(),
                               log::PermissionTarget::CLIENT_CHANNEL,
                               permission::v2::PermissionUpdateType::delete_value,
                               target_client_database_id, "",
                               channel->channelId(), channel->name());
        update_view |= ppermission.is_client_view_property();
    }

    serverInstance->databaseHelper()->saveClientPermissions(this->server, target_client_database_id, target_permission_manager);

    auto onlineClients = this->server->findClientsByCldbId(target_client_database_id);
    if (!onlineClients.empty()) {
        for (const auto &elm : onlineClients) {
            elm->task_update_needed_permissions.enqueue();

            if (elm->currentChannel == channel) {
                elm->task_update_channel_client_properties.enqueue();
            } else if (update_view) {
                unique_lock client_channel_lock(this->channel_tree_mutex);

                auto elm_channel = elm->currentChannel;
                if (elm_channel) {
                    deque<ChannelId> deleted;
                    for (const auto &update_entry : elm->channel_tree->update_channel_path(l_channel, this->server->channelTree->findLinkedChannel(elm->currentChannel->channelId()))) {
                        if (update_entry.first)
                            elm->notifyChannelShow(update_entry.second->channel(), update_entry.second->previous_channel);
                        else
                            deleted.push_back(update_entry.second->channelId());
                    }
                    if (!deleted.empty())
                        elm->notifyChannelHide(deleted, false); /* we've locked the tree before */
                }
            }

            elm->join_state_id++; /* join permission may changed, all channels need to be recalculate dif needed */
        }
    }

    return pparser.build_command_result();
}

command_result ConnectedClient::handleCommandChannelClientAddPerm(Command &cmd) {
    CMD_REF_SERVER(server_ref);
    CMD_RESET_IDLE;
    CMD_CHK_AND_INC_FLOOD_POINTS(5);


    RESOLVE_CHANNEL_R(cmd["cid"], true);
    auto channel = dynamic_pointer_cast<ServerChannel>(l_channel->entry);
    if (!channel) return command_result{error::vs_critical};

    auto target_client_database_id = cmd["cldbid"].as<ClientDbId>();
    ClientPermissionCalculator target_client_permission_calculator{this->server, cmd["cldbid"], ClientType::CLIENT_TEAMSPEAK, channel_id};
    ACTION_REQUIRES_PERMISSION(permission::i_client_permission_modify_power, target_client_permission_calculator.calculate_permission(permission::i_client_needed_permission_modify_power), channel_id);

    auto target_permissions = serverInstance->databaseHelper()->loadClientPermissionManager(this->getServerId(), target_client_database_id);

    ts::command::bulk_parser::PermissionBulksParser pparser{cmd, true};
    if (!pparser.validate(this->ref(), channel->channelId()))
        return pparser.build_command_result();

    bool update_view{false};
    for (const auto &ppermission : pparser.iterate_valid_permissions()) {
        ppermission.apply_to_channel(target_permissions, permission::v2::PermissionUpdateType::set_value, channel->channelId());
        ppermission.log_update(serverInstance->action_logger()->permission_logger,
                               this->getServerId(),
                               this->ref(),
                               log::PermissionTarget::CLIENT_CHANNEL,
                               permission::v2::PermissionUpdateType::set_value,
                               target_client_database_id, "",
                               channel->channelId(), channel->name());
        update_view |= ppermission.is_client_view_property();
    }

    serverInstance->databaseHelper()->saveClientPermissions(this->server, target_client_database_id, target_permissions);


    auto onlineClients = this->server->findClientsByCldbId(target_client_database_id);
    if (!onlineClients.empty())
        for (const auto &elm : onlineClients) {
            elm->task_update_needed_permissions.enqueue();

            if (elm->currentChannel == channel) {
                elm->task_update_channel_client_properties.enqueue();
            } else if (update_view) {
                unique_lock client_channel_lock(this->channel_tree_mutex);

                auto elm_channel = elm->currentChannel;
                if (elm_channel) {
                    deque<ChannelId> deleted;
                    for (const auto &update_entry : elm->channel_tree->update_channel_path(l_channel, this->server->channelTree->findLinkedChannel(elm->currentChannel->channelId()))) {
                        if (update_entry.first)
                            elm->notifyChannelShow(update_entry.second->channel(), update_entry.second->previous_channel);
                        else
                            deleted.push_back(update_entry.second->channelId());
                    }
                    if (!deleted.empty())
                        elm->notifyChannelHide(deleted, false); /* we've locked the tree before */
                }
            }
            elm->join_state_id++; /* join permission may changed, all channels need to be recalculate dif needed */
        }

    return pparser.build_command_result();
}


command_result ConnectedClient::handleCommandChannelFind(Command &cmd) {
    CMD_CHK_AND_INC_FLOOD_POINTS(5);

    string pattern = cmd["pattern"];
    std::transform(pattern.begin(), pattern.end(), pattern.begin(), ::tolower);

    Command res("");
    int index = 0;
    for (const auto &cl : (this->server ? this->server->channelTree : serverInstance->getChannelTree().get())->channels()) {
        string name = cl->name();
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);
        if (name.find(pattern) != std::string::npos) {
            res[index]["cid"] = cl->channelId();
            res[index]["channel_name"] = cl->name();
            index++;
        }
    }
    if (index == 0) return command_result{error::database_empty_result};
    this->sendCommand(res);

    return command_result{error::ok};
}

command_result ConnectedClient::handleCommandChannelInfo(Command &cmd) {
    std::shared_ptr<BasicChannel> channel = (this->server ? this->server->channelTree : serverInstance->getChannelTree().get())->findChannel(cmd["cid"].as<ChannelId>());
    if (!channel) return command_result{error::channel_invalid_id, "Cant resolve channel"};

    Command res("");

    for (const auto &prop : channel->properties()->list_properties(property::FLAG_CHANNEL_VIEW | property::FLAG_CHANNEL_VARIABLE, this->getType() == CLIENT_TEAMSPEAK ? property::FLAG_NEW : (uint16_t) 0))
        res[prop.type().name] = prop.value();

    res["seconds_empty"] = channel->empty_seconds();
    res["pid"] = res["cpid"].string();
    this->sendCommand(res);

    return command_result{error::ok};
}
