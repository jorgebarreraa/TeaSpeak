#pragma once

/** Permission tests against required values **/
/* use this for any action which will do something with the server */
#define ACTION_REQUIRES_GLOBAL_PERMISSION_CACHED(required_permission_type, required_value, cache) \
do { \
    if(!permission::v2::permission_granted(required_value, this->calculate_permission(required_permission_type, 0, false))) \
        return command_result{required_permission_type}; \
} while(0)

#define ACTION_REQUIRES_GLOBAL_PERMISSION(required_permission_type, required_value) \
    ACTION_REQUIRES_GLOBAL_PERMISSION_CACHED(required_permission_type, required_value, nullptr)

//TODO: Fixme: Really check for instance permissions!
#define ACTION_REQUIRES_INSTANCE_PERMISSION(required_permission_type, required_value) \
    ACTION_REQUIRES_GLOBAL_PERMISSION_CACHED(required_permission_type, required_value, nullptr)

/* use this for anything which will do something local in relation to the target channel */
#define ACTION_REQUIRES_PERMISSION(required_permission_type, required_value, channel_id) \
do { \
    if(!permission::v2::permission_granted(required_value, this->calculate_permission(required_permission_type, channel_id))) \
        return command_result{required_permission_type}; \
} while(0)

/** Permission tests against groups **/
/* use this when testing a permission against a group */
#define ACTION_REQUIRES_GROUP_PERMISSION(group, required_permission_type, own_permission_type, is_required) \
do { \
    auto _permission_granted = this->calculate_permission(own_permission_type, 0); \
    if(!(group)->permission_granted(required_permission_type, _permission_granted, is_required)) \
        return command_result{own_permission_type}; \
} while(0)

/** Permission tests against channels **/
/* use this when testing a permission against a group */
#define ACTION_REQUIRES_CHANNEL_PERMISSION(channel, required_permission_type, own_permission_type, is_required) \
do { \
    auto _permission_granted = this->calculate_permission(own_permission_type, channel ? channel->channelId() : 0); \
    if(!(channel)->permission_granted(required_permission_type, _permission_granted, is_required)) \
        return command_result{own_permission_type}; \
} while(0)


/* Helper methods for channel resolve */
#define RESOLVE_CHANNEL_R(command, force) \
auto channel_tree = this->server ? this->server->channelTree : serverInstance->getChannelTree().get();\
shared_lock channel_tree_read_lock(this->server ? this->server->channel_tree_mutex : serverInstance->getChannelTreeLock());\
auto channel_id = command.as<ChannelId>(); \
auto l_channel = channel_id ? channel_tree->findLinkedChannel(command.as<ChannelId>()) : nullptr; \
if (!l_channel && (channel_id != 0 || force)) return command_result{error::channel_invalid_id, "Cant resolve channel"}; \

#define RESOLVE_CHANNEL_W(command, force) \
auto channel_tree = this->server ? this->server->channelTree : serverInstance->getChannelTree().get();\
unique_lock channel_tree_write_lock(this->server ? this->server->channel_tree_mutex : serverInstance->getChannelTreeLock());\
auto channel_id = command.as<ChannelId>(); \
auto l_channel = channel_id ? channel_tree->findLinkedChannel(command.as<ChannelId>()) : nullptr; \
if (!l_channel && (channel_id != 0 || force)) return command_result{error::channel_invalid_id, "Cant resolve channel"}; \

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-enum"
inline bool permission_require_granted_value(ts::permission::PermissionType type) {
    using namespace ts;

    switch (type) {
        case permission::i_icon_id:
        case permission::i_max_icon_filesize:
        case permission::i_client_max_avatar_filesize:
        case permission::i_ft_quota_mb_download_per_client:
        case permission::i_ft_quota_mb_upload_per_client:

        case permission::i_client_max_channels:
        case permission::i_client_max_permanent_channels:
        case permission::i_client_max_semi_channels:
        case permission::i_client_max_temporary_channels:
        case permission::i_channel_create_modify_with_temp_delete_delay:
        case permission::i_client_talk_power:
        case permission::i_client_needed_talk_power:
        case permission::b_channel_create_with_needed_talk_power:

        case permission::i_channel_max_depth:
        case permission::i_channel_min_depth:
        case permission::i_client_max_channel_subscriptions:

        case permission::i_client_music_create_modify_max_volume:
        case permission::i_max_playlist_size:
        case permission::i_max_playlists:

        case permission::i_client_poke_max_clients:

        case permission::i_client_ban_max_bantime:
        case permission::i_client_max_idletime:
        case permission::i_group_sort_id:

        case permission::i_video_max_kbps:
            return false;
        default:
            return true;
    }
}

inline bool permission_is_group_property(ts::permission::PermissionType type) {
    using namespace ts;
    switch (type) {
        case permission::i_icon_id:
        case permission::i_group_show_name_in_tree:
        case permission::i_group_sort_id:
        case permission::b_group_is_permanent:
        case permission::i_displayed_group_needed_modify_power:
        case permission::i_displayed_group_needed_member_add_power:
        case permission::i_displayed_group_needed_member_remove_power:
            return true;
        default:
            return false;
    }
}

inline bool permission_is_client_property(ts::permission::PermissionType type) {
    using namespace ts;
    switch (type) {
        case permission::i_icon_id:
        case permission::i_client_talk_power:
        case permission::i_client_max_idletime:
        case permission::i_group_sort_id:
        case permission::i_channel_view_power:
        case permission::b_channel_ignore_view_power:
        case permission::b_client_is_priority_speaker:
            return true;
        default:
            return false;
    }
}
#pragma GCC diagnostic pop


inline ssize_t count_characters(const std::string& in) {
    size_t index = 0;
    size_t count = 0;
    while(index < in.length()){
        count++;

        auto current = (uint8_t) in[index];
        if(current >= 128) { //UTF8 check
            if(current >= 192 && (current <= 193 || current >= 245)) {
                return -1;
            } else if(current >= 194 && current <= 223) {
                if(in.length() - index <= 1) return -1;
                else if((uint8_t) in[index + 1] >= 128 && (uint8_t) in[index + 1] <= 191) index += 1; //Valid
                else return -1;
            } else if(current >= 224 && current <= 239) {
                if(in.length() - index <= 2) return -1;
                else if((uint8_t) in[index + 1] >= 128 && (uint8_t) in[index + 1] <= 191 &&
                        (uint8_t) in[index + 2] >= 128 && (uint8_t) in[index + 2] <= 191) index += 2; //Valid
                else return -1;
            } else if(current >= 240 && current <= 244) {
                if(in.length() - index <= 3) return -1;
                else if((uint8_t) in[index + 1] >= 128 && (uint8_t) in[index + 1] <= 191 &&
                        (uint8_t) in[index + 2] >= 128 && (uint8_t) in[index + 2] <= 191 &&
                        (uint8_t) in[index + 3] >= 128 && (uint8_t) in[index + 3] <= 191) index += 3; //Valid
                else return -1;
            } else {
                return -1;
            }
        }
        index++;
    }
    return count;
}