#pragma once

#include <map>
#include <functional>
#include <deque>
#include <string>
#include <utility>
#include <vector>
#include <memory>
#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <cassert>
#include <cstring> /* for memset */
#include "misc/spin_mutex.h"
#include "Definitions.h"
#include "Variable.h"

#define permNotGranted (-2)
#define PERM_ID_GRANT ((ts::permission::PermissionType) (1U << 15U))

namespace ts {
    class BasicChannel;
    namespace permission {
        typedef int32_t PermissionValue;

        enum PermissionSqlType {
            SQL_PERM_GROUP,
            SQL_PERM_CHANNEL,
            SQL_PERM_USER,
            SQL_PERM_PLAYLIST
        };
        enum PermissionType : uint16_t {
            undefined = (uint16_t) -1,

            permission_id_min = 0, /* we count unknown as defined permission as well */
            unknown = (uint16_t) 0,
            ok = 0,
            type_begin = 1,

            /* global */

            /* global::information */
            b_serverinstance_help_view = type_begin,
            b_serverinstance_version_view,
            b_serverinstance_info_view,
            b_serverinstance_virtualserver_list,
            b_serverinstance_binding_list,
            b_serverinstance_permission_list,
            b_serverinstance_permission_find,

            /* global::vs_management */
            b_virtualserver_create,
            b_virtualserver_delete,
            b_virtualserver_start_any,
            b_virtualserver_stop_any,
            b_virtualserver_change_machine_id,
            b_virtualserver_change_template,

            /* global::administration */
            b_serverquery_login,
            b_serverinstance_textmessage_send,
            b_serverinstance_log_view,
            b_serverinstance_log_add,
            b_serverinstance_stop,

            /* global::settings */
            b_serverinstance_modify_settings,
            b_serverinstance_modify_querygroup,
            b_serverinstance_modify_templates,

            /* virtual_server */

            /* virtual_server::information */
            b_virtualserver_select,
            b_virtualserver_info_view,
            b_virtualserver_connectioninfo_view,
            b_virtualserver_channel_list,
            b_virtualserver_channel_search,
            b_virtualserver_client_list,
            b_virtualserver_client_search,
            b_virtualserver_client_dblist,
            b_virtualserver_client_dbsearch,
            b_virtualserver_client_dbinfo,
            b_virtualserver_permission_find,
            b_virtualserver_custom_search,

            /* virtual_server::administration */
            b_virtualserver_start,
            b_virtualserver_stop,
            b_virtualserver_token_list_all,
            i_virtualserver_token_limit,
            b_virtualserver_token_edit_all,
            b_virtualserver_token_use,
            b_virtualserver_token_delete_all,
            b_virtualserver_log_view,
            b_virtualserver_log_add,
            b_virtualserver_join_ignore_password,
            b_virtualserver_notify_register,
            b_virtualserver_notify_unregister,
            b_virtualserver_snapshot_create,
            b_virtualserver_snapshot_deploy,
            b_virtualserver_permission_reset,

            /* virtual_server::settings */
            b_virtualserver_modify_name,
            b_virtualserver_modify_welcomemessage,
            b_virtualserver_modify_maxchannels,
            b_virtualserver_modify_maxclients,
            b_virtualserver_modify_reserved_slots,
            b_virtualserver_modify_password,
            b_virtualserver_modify_default_servergroup,
            b_virtualserver_modify_default_musicgroup,
            b_virtualserver_modify_default_channelgroup,
            b_virtualserver_modify_default_channeladmingroup,
            b_virtualserver_modify_channel_forced_silence,
            b_virtualserver_modify_complain,
            b_virtualserver_modify_antiflood,
            b_virtualserver_modify_ft_settings,
            b_virtualserver_modify_ft_quotas,
            b_virtualserver_modify_hostmessage,
            b_virtualserver_modify_hostbanner,
            b_virtualserver_modify_hostbutton,
            b_virtualserver_modify_port,
#ifndef LAGENCY
            b_virtualserver_modify_host,
            b_virtualserver_modify_default_messages,
#endif
            b_virtualserver_modify_autostart,
            b_virtualserver_modify_needed_identity_security_level,
            b_virtualserver_modify_priority_speaker_dimm_modificator,
            b_virtualserver_modify_log_settings,
            b_virtualserver_modify_min_client_version,
            b_virtualserver_modify_icon_id,
            b_virtualserver_modify_country_code,
            b_virtualserver_modify_codec_encryption_mode,
            b_virtualserver_modify_temporary_passwords,
            b_virtualserver_modify_temporary_passwords_own,
            b_virtualserver_modify_channel_temp_delete_delay_default,
            b_virtualserver_modify_music_bot_limit,

            /* channel */
            i_channel_min_depth,
            i_channel_max_depth,
            b_channel_group_inheritance_end,
            i_channel_permission_modify_power,
            i_channel_needed_permission_modify_power,

            /* channel::info */
            b_channel_info_view,
            b_virtualserver_channel_permission_list,

            /* channel::create */
            b_channel_create_child,
            b_channel_create_permanent,
            b_channel_create_semi_permanent,
            b_channel_create_temporary,
            b_channel_create_with_topic,
            b_channel_create_with_description,
            b_channel_create_with_password,
            b_channel_create_modify_with_codec_opusvoice,
            b_channel_create_modify_with_codec_opusmusic,
            i_channel_create_modify_with_codec_maxquality,
            i_channel_create_modify_with_codec_latency_factor_min,
            b_channel_create_with_maxclients,
            b_channel_create_with_maxfamilyclients,
            b_channel_create_with_sortorder,
            b_channel_create_with_default,
            b_channel_create_with_needed_talk_power,
            b_channel_create_modify_with_force_password,
            i_channel_create_modify_with_temp_delete_delay,
            i_channel_create_modify_conversation_history_length,
            b_channel_create_modify_conversation_history_unlimited,
            b_channel_create_modify_conversation_mode_private,
            b_channel_create_modify_conversation_mode_public,
            b_channel_create_modify_conversation_mode_none,
            b_channel_create_modify_sidebar_mode,

            /* channel::modify */
            b_channel_modify_parent,
            b_channel_modify_make_default,
            b_channel_modify_make_permanent,
            b_channel_modify_make_semi_permanent,
            b_channel_modify_make_temporary,
            b_channel_modify_name,
            b_channel_modify_topic,
            b_channel_modify_description,
            b_channel_modify_password,
            b_channel_modify_codec,
            b_channel_modify_codec_quality,
            b_channel_modify_codec_latency_factor,
            b_channel_modify_maxclients,
            b_channel_modify_maxfamilyclients,
            b_channel_modify_sortorder,
            b_channel_modify_needed_talk_power,
            i_channel_modify_power,
            i_channel_needed_modify_power,
            b_channel_modify_make_codec_encrypted,
            b_channel_modify_temp_delete_delay,
            b_channel_conversation_message_delete,

            /* channel::delete */
            b_channel_delete_permanent,
            b_channel_delete_semi_permanent,
            b_channel_delete_temporary,
            b_channel_delete_flag_force,
            i_channel_delete_power,
            i_channel_needed_delete_power,

            /* channel::access */
            b_channel_join_permanent,
            b_channel_join_semi_permanent,
            b_channel_join_temporary,
            b_channel_join_ignore_password,
            b_channel_join_ignore_maxclients,
            i_channel_join_power,
            i_channel_needed_join_power,
            b_channel_ignore_join_power,

            i_channel_view_power,
            i_channel_needed_view_power,
            b_channel_ignore_view_power,

            i_channel_subscribe_power,
            i_channel_needed_subscribe_power,
            b_channel_ignore_subscribe_power,

            i_channel_description_view_power,
            i_channel_needed_description_view_power,
            b_channel_ignore_description_view_power,

            /* group */
            i_icon_id,
            i_max_icon_filesize,
            i_max_playlist_size,
            i_max_playlists,
            b_icon_manage,
            b_group_is_permanent,
            i_group_auto_update_type,
            i_group_auto_update_max_value,
            i_group_sort_id,
            i_group_show_name_in_tree,

            /* group::info */
            b_virtualserver_servergroup_list, //Unused
            b_virtualserver_servergroup_permission_list,
            b_virtualserver_servergroup_client_list,

            b_virtualserver_channelgroup_list, //Unused
            b_virtualserver_channelgroup_permission_list,
            b_virtualserver_channelgroup_client_list,

            /* group::create */
            b_virtualserver_servergroup_create,
            b_virtualserver_channelgroup_create,

            /* group::modify */
#ifdef LAGENCY
            i_group_modify_power,
            i_group_needed_modify_power,
            i_group_member_add_power,
            i_group_needed_member_add_power,
            i_group_member_remove_power,
            i_group_needed_member_remove_power,
#else
            //permission patch start
            i_server_group_modify_power,
            i_server_group_needed_modify_power,
            i_server_group_member_add_power,
            i_server_group_self_add_power,
            i_server_group_needed_member_add_power,
            i_server_group_member_remove_power,
            i_server_group_self_remove_power,
            i_server_group_needed_member_remove_power,
            i_channel_group_modify_power,
            i_channel_group_needed_modify_power,
            i_channel_group_member_add_power,
            i_channel_group_self_add_power,
            i_channel_group_needed_member_add_power,
            i_channel_group_member_remove_power,
            i_channel_group_self_remove_power,
            i_channel_group_needed_member_remove_power,

            i_displayed_group_member_add_power,
            i_displayed_group_needed_member_add_power,
            i_displayed_group_member_remove_power,
            i_displayed_group_needed_member_remove_power,
            i_displayed_group_modify_power,
            i_displayed_group_needed_modify_power,
            //permission patch end
#endif
            i_permission_modify_power,
            b_permission_modify_power_ignore,

            /* group::delete */
            b_virtualserver_servergroup_delete,
            b_virtualserver_channelgroup_delete,

            /* client */
            i_client_permission_modify_power,
            i_client_needed_permission_modify_power,
            i_client_max_clones_uid,
            i_client_max_clones_ip,
            i_client_max_clones_hwid,
            i_client_max_idletime,
            i_client_max_avatar_filesize,
            i_client_max_channel_subscriptions,
            i_client_max_channels,
            i_client_max_temporary_channels,
            i_client_max_semi_channels,
            i_client_max_permanent_channels,
            b_client_use_priority_speaker,
            b_client_is_priority_speaker,
            b_client_skip_channelgroup_permissions,
            b_client_force_push_to_talk,
            b_client_ignore_bans,
            b_client_ignore_vpn,
            b_client_ignore_antiflood,
            b_client_enforce_valid_hwid,
            b_client_allow_invalid_packet,
            b_client_allow_invalid_badges,
            b_client_issue_client_query_command,
            b_client_use_reserved_slot,
            b_client_use_channel_commander,
            b_client_request_talker,
            b_client_avatar_delete_other,
            b_client_is_sticky,
            b_client_ignore_sticky,

            b_client_music_create_permanent,
            b_client_music_create_semi_permanent,
            b_client_music_create_temporary,
            b_client_music_modify_permanent,
            b_client_music_modify_semi_permanent,
            b_client_music_modify_temporary,
            i_client_music_create_modify_max_volume,

            i_client_music_limit,
            i_client_music_needed_delete_power,
            i_client_music_delete_power,
            i_client_music_play_power,
            i_client_music_needed_play_power,
            i_client_music_modify_power,
            i_client_music_needed_modify_power,
            i_client_music_rename_power,
            i_client_music_needed_rename_power,

            b_virtualserver_playlist_permission_list,
            b_playlist_create,
            i_playlist_view_power,
            i_playlist_needed_view_power,
            i_playlist_modify_power,
            i_playlist_needed_modify_power,
            i_playlist_permission_modify_power,
            i_playlist_needed_permission_modify_power,
            i_playlist_delete_power,
            i_playlist_needed_delete_power,

            i_playlist_song_add_power,
            i_playlist_song_needed_add_power,
            i_playlist_song_remove_power,
            i_playlist_song_needed_remove_power,
            i_playlist_song_move_power,
            i_playlist_song_needed_move_power,

            /* client::info */
            b_client_info_view,
            b_client_permissionoverview_view,
            b_client_permissionoverview_own,
            b_client_remoteaddress_view,
            i_client_serverquery_view_power,
            i_client_needed_serverquery_view_power,
            b_client_custom_info_view,
            b_client_music_channel_list,
            b_client_music_server_list,
            i_client_music_info,
            i_client_music_needed_info,
            b_virtualserver_channelclient_permission_list,
            b_virtualserver_client_permission_list,

            /* client::admin */
            i_client_kick_from_server_power,
            i_client_needed_kick_from_server_power,
            i_client_kick_from_channel_power,
            i_client_needed_kick_from_channel_power,
            i_client_ban_power,
            i_client_needed_ban_power,
            i_client_move_power,
            i_client_needed_move_power,
            i_client_complain_power,
            i_client_needed_complain_power,
            b_client_complain_list,
            b_client_complain_delete_own,
            b_client_complain_delete,
            b_client_ban_list,
            b_client_ban_list_global,
            b_client_ban_trigger_list,
            b_client_ban_create,
            b_client_ban_create_global,
            b_client_ban_name,
            b_client_ban_ip,
            b_client_ban_hwid,
            b_client_ban_edit,
            b_client_ban_edit_global,
            b_client_ban_delete_own,
            b_client_ban_delete,
            b_client_ban_delete_own_global,
            b_client_ban_delete_global,
            i_client_ban_max_bantime,

            /* client::basics */
            i_client_private_textmessage_power,
            i_client_needed_private_textmessage_power,
            b_client_even_textmessage_send,
            b_client_server_textmessage_send,
            b_client_channel_textmessage_send,
            b_client_offline_textmessage_send,
            i_client_talk_power,
            i_client_needed_talk_power,
            i_client_poke_power,
            i_client_needed_poke_power,
            i_client_poke_max_clients,
            b_client_set_flag_talker,
            i_client_whisper_power,
            i_client_needed_whisper_power,
            b_video_screen,
            b_video_camera,
            i_video_max_kbps,
            i_video_max_streams,
            i_video_max_screen_streams,
            i_video_max_camera_streams,

            /* client::modify */
            b_client_modify_description,
            b_client_modify_own_description,
            b_client_use_bbcode_any,
            b_client_use_bbcode_url,
            b_client_use_bbcode_image,
            b_client_modify_dbproperties,
            b_client_delete_dbproperties,
            b_client_create_modify_serverquery_login,
            b_client_query_create,
            b_client_query_create_own,
            b_client_query_list,
            b_client_query_list_own,
            b_client_query_rename,
            b_client_query_rename_own,
            b_client_query_change_password,
            b_client_query_change_own_password,
            b_client_query_change_password_global,
            b_client_query_delete,
            b_client_query_delete_own,

            /* file_transfer */
            b_ft_ignore_password,
            b_ft_transfer_list,
            i_ft_file_upload_power,
            i_ft_needed_file_upload_power,
            i_ft_file_download_power,
            i_ft_needed_file_download_power,
            i_ft_file_delete_power,
            i_ft_needed_file_delete_power,
            i_ft_file_rename_power,
            i_ft_needed_file_rename_power,
            i_ft_file_browse_power,
            i_ft_needed_file_browse_power,
            i_ft_directory_create_power,
            i_ft_needed_directory_create_power,
            i_ft_quota_mb_download_per_client,
            i_ft_quota_mb_upload_per_client,
            i_ft_max_bandwidth_download,
            i_ft_max_bandwidth_upload,

            permission_id_max
        };
        inline PermissionType& operator&=(PermissionType& a, int b) { return a = (PermissionType) ((int) a & b); }

        enum PermissionGroup : uint16_t {
            group_begin,
            global = group_begin,
            global_info = b_serverinstance_permission_find,
            global_vsmanage = b_virtualserver_change_template,
            global_admin = b_serverinstance_stop,
            global_settings = b_serverinstance_modify_templates,

            vs = global_settings, /* we dont have any permissions in here */
            vs_info = b_virtualserver_custom_search,
            vs_admin = b_virtualserver_permission_reset,
#ifdef LEGENCY
            vs_settings = b_virtualserver_modify_channel_temp_delete_delay_default,
#else
            vs_settings = b_virtualserver_modify_music_bot_limit,
#endif

            channel = i_channel_needed_permission_modify_power,
            channel_info = b_virtualserver_channel_permission_list,
            channel_create = b_channel_create_modify_conversation_mode_none,
            channel_modify = b_channel_modify_temp_delete_delay,
            channel_delete = i_channel_needed_delete_power,
            channel_access = b_channel_ignore_description_view_power,

            group = i_group_show_name_in_tree,
            group_info = b_virtualserver_channelgroup_client_list,
            group_create = b_virtualserver_channelgroup_create,
            group_modify = b_permission_modify_power_ignore,
            group_delete = b_virtualserver_channelgroup_delete,
#ifdef LAGENCY
            client = b_client_ignore_sticky,
#else
            client = i_playlist_song_needed_move_power,
#endif
#ifdef LAGENCY
            client_info = b_client_custom_info_view,
#else
            client_info = b_virtualserver_client_permission_list,
#endif
            client_admin = i_client_ban_max_bantime,
            client_basic = i_client_needed_whisper_power,
            client_modify = b_client_query_delete_own,
            ft = i_ft_max_bandwidth_upload,
            group_end
        };

        enum PermissionTestType {
            PERMTEST_HIGHEST,
            PERMTEST_ORDERED,
        };

        struct PermissionTypeEntry {
            static std::shared_ptr<PermissionTypeEntry> unknown;

            PermissionTypeEntry& operator=(const PermissionTypeEntry& other) = delete;
            /*
            PermissionTypeEntry& operator=(const PermissionTypeEntry& other) {
                this->type = other.type;
                this->group = other.group;
                this->name = other.name;
                this->description = other.description;
                this->clientSupported = other.clientSupported;

                this->grant_name = std::string() + (name[0] == 'i' ? "i" : "i") + "_needed_modify_power_" + name.substr(2);
                return *this;
            }

            bool operator==(const PermissionTypeEntry& other) {
                return other.type == this->type;
            }
             */

            PermissionType type;
            PermissionGroup group;
            std::string name;
            std::string grant_name;
            inline std::string grantName(bool useBool = false) const { return this->grant_name; }
            std::string description;

            bool clientSupported = true;

            [[nodiscard]] inline bool is_invalid() const { return this->type == permission::undefined || this->type == permission::unknown; }

            // PermissionTypeEntry(PermissionTypeEntry&& ref) : type(ref.type), group(ref.group), name(ref.name), description(ref.description), clientSupported(ref.clientSupported) {}
            //PermissionTypeEntry(const PermissionTypeEntry& ref) : type(ref.type), group(ref.group), name(ref.name), description(ref.description), clientSupported(ref.clientSupported) {}
            PermissionTypeEntry(PermissionTypeEntry&& ref) = delete;
            PermissionTypeEntry(const PermissionTypeEntry& ref) = delete;
            //PermissionTypeEntry(const PermissionTypeEntry& ref) : type(ref.type), group(ref.group), name(ref.name), description(ref.description), clientSupported(ref.clientSupported) {}

            PermissionTypeEntry(PermissionType type, PermissionGroup group, std::string name, std::string description, bool clientSupported = true) :   type(type),
                                                                                                                                                        group(group),
                                                                                                                                                        name(std::move(name)),
                                                                                                                                                        description(std::move(description)),
                                                                                                                                                        clientSupported(clientSupported) {
                this->grant_name = "i_needed_modify_power_" + this->name.substr(2);
            }
        };

        namespace teamspeak {
            enum GroupType {
                GENERAL,
                SERVER,
                CHANNEL,
                CLIENT
            };

            typedef std::map<GroupType, std::multimap<std::string, std::deque<std::string>>> MapType;
            extern MapType unmapping;
            extern MapType mapping;
            extern std::deque<std::string> map_key(std::string key, GroupType type); //TeamSpeak -> TeaSpeak
            extern std::deque<std::string> unmap_key(std::string key, GroupType type); //TeaSpeak -> TeamSpeak
        }

        namespace update {
            enum GroupUpdateType {
                NONE = 0,

                CHANNEL_GUEST = 10,
                CHANNEL_VOICE = 25,
                CHANNEL_OPERATOR = 35,
                CHANNEL_ADMIN = 40,

                SERVER_GUEST = 15,
                SERVER_NORMAL = 30,
                SERVER_ADMIN = 45,

                QUERY_GUEST = 20,
                QUERY_ADMIN = 50
            };

            struct UpdatePermission {
                UpdatePermission(std::string name, permission::PermissionValue value, permission::PermissionValue granted, bool negated, bool skipped) : name(std::move(name)), value(value), granted(granted), negated(negated), skipped(skipped) {}
                UpdatePermission() = default;

                std::string name;
                permission::PermissionValue value = permNotGranted;
                permission::PermissionValue granted = permNotGranted;

                bool negated = false;
                bool skipped = false;
            };

            struct UpdateEntry {
                GroupUpdateType type;
                UpdatePermission permission;
            };

            extern std::deque<UpdateEntry> migrate; //TeamSpeak -> TeaSpeak
        }

        extern std::deque<std::shared_ptr<PermissionTypeEntry>> availablePermissions;
        extern std::deque<PermissionType> neededPermissions;
        extern std::deque<PermissionGroup> availableGroups;

        void setup_permission_resolve();
        std::shared_ptr<PermissionTypeEntry> resolvePermissionData(PermissionType);
        std::shared_ptr<PermissionTypeEntry> resolvePermissionData(const std::string&);

#define PERM_FLAG_PRIVATE 0b1
#define PERM_FLAG_PUBLIC  0b10

        class PermissionManager;
        struct Permission {
            public:
                Permission(const std::shared_ptr<PermissionTypeEntry>& type, PermissionValue value, PermissionValue grant, uint16_t flagMask, std::shared_ptr<BasicChannel>  ch) : type(type), channel(std::move(ch)) {
                    this->type = type;
                    this->value = value;
                    this->granted = grant;
                    this->flagMask = flagMask;
                }


                Permission(const Permission &) = delete;

                Permission() = delete;
                ~Permission()= default;

                std::shared_ptr<BasicChannel> channel = nullptr;
                ChannelId channelId();

                bool hasValue(){ return value != permNotGranted; }
                bool hasGrant(){ return granted != permNotGranted; }
                std::shared_ptr<PermissionTypeEntry> type;
                PermissionValue value;
                PermissionValue granted;
                uint16_t flagMask;

                bool flag_negate = false;
                bool flag_skip = false;
                bool dbReference = false;
        };

        class PermissionManager {
            public:
                PermissionManager();
                ~PermissionManager();

                std::shared_ptr<Permission> registerPermission(PermissionType, PermissionValue, const std::shared_ptr<BasicChannel>& channel, uint16_t = PERM_FLAG_PUBLIC);
                std::shared_ptr<Permission> registerPermission(const std::shared_ptr<PermissionTypeEntry>&, PermissionValue, const std::shared_ptr<BasicChannel>& channel, uint16_t = PERM_FLAG_PUBLIC);
                //void registerAllPermissions(uint16_t flagMask);

                bool setPermissionGranted(PermissionType, PermissionValue, const std::shared_ptr<BasicChannel>& channel);
                bool setPermission(PermissionType, PermissionValue, const std::shared_ptr<BasicChannel>& channel);
                bool setPermission(PermissionType, PermissionValue, const std::shared_ptr<BasicChannel>& channel, bool negated, bool skiped);
                void deletePermission(PermissionType, const std::shared_ptr<BasicChannel>& channel);
                bool hasPermission(PermissionType, const std::shared_ptr<BasicChannel>& channel, bool testGlobal);

                /**
                 * @param channel Should be null for general testing
                 * @return <channel>, <global>
                 */
                std::deque<std::shared_ptr<Permission>> getPermission(PermissionType, const std::shared_ptr<BasicChannel>& channel, bool testGlobal = true);

                PermissionValue getPermissionGrand(permission::PermissionTestType test, PermissionType type, const std::shared_ptr<BasicChannel>& channel, PermissionValue def = permNotGranted){
                    PermissionValue result = def;
                    auto perms = this->getPermission(type, channel);
                    if(test == PermissionTestType::PERMTEST_HIGHEST) {
                        PermissionValue higest = permNotGranted;
                        for(const auto &e : perms)
                            if((e->granted > higest || e->granted == -1) && higest != -1) higest = e->granted;
                        if(higest != permNotGranted)
                            result = higest;
                    } else if(test == PermissionTestType::PERMTEST_ORDERED)
                        while(!perms.empty() && result == permNotGranted) {
                            result = perms.front()->granted;
                            perms.pop_front();
                        };
                    return result;
                }

                inline PermissionValue getPermissionValue(PermissionType type, const std::shared_ptr<BasicChannel>& channel = nullptr, PermissionValue default_value = permNotGranted) {
                    return this->getPermissionValue(permission::PERMTEST_ORDERED, type, channel, default_value);
                }

                PermissionValue getPermissionValue(permission::PermissionTestType test, PermissionType type, const std::shared_ptr<BasicChannel>& channel = nullptr, PermissionValue def = permNotGranted) {
                    PermissionValue result = permNotGranted;
                    auto perms = this->getPermission(type, channel);
                    if(test == permission::PERMTEST_HIGHEST) {
                        PermissionValue higest = permNotGranted;
                        for(const auto &e : perms)
                            if((e->value > higest || e->value == -1) && higest != -1) higest = e->value;
                        if(higest != permNotGranted)
                            result = higest;
                    } else if(test == PermissionTestType::PERMTEST_ORDERED)
                        while(!perms.empty() && result == permNotGranted) {
                            result = perms.front()->value;
                            perms.pop_front();
                        };
                    return result == permNotGranted ? def : result;
                }

                std::vector<std::shared_ptr<Permission>> listPermissions(uint16_t = ~0);
                std::vector<std::shared_ptr<Permission>> allPermissions();

                std::deque<std::shared_ptr<Permission>> all_channel_specific_permissions();
                std::deque<std::shared_ptr<Permission>> all_channel_unspecific_permissions();

                void fireUpdate(PermissionType);
                void registerUpdateHandler(const std::function<void(std::shared_ptr<Permission>)> &fn){ updateHandler.push_back(fn); }

                void clearPermissions(){
                    permissions.clear();
                }
            private:
                std::deque<std::shared_ptr<Permission>> permissions;
                std::deque<std::function<void(std::shared_ptr<Permission>)>> updateHandler;
        };

        namespace v2 {
            #pragma pack(push, 1)
            struct PermissionFlags {
                bool database_reference: 1; /* if set the permission is known within the database, else it has tp be inserted */
                bool channel_specific: 1;   /* set if there are channel specific permissions */

                bool value_set: 1;
                bool grant_set: 1;

                bool skip: 1;
                bool negate: 1;

                bool flag_value_update: 1;
                bool flag_grant_update: 1;

                [[nodiscard]] ts_always_inline bool permission_set() const {
                    return this->value_set || this->grant_set;
                }
            };
            static_assert(sizeof(PermissionFlags) == 1);

            struct PermissionValues {
                PermissionValue value;
                PermissionValue grant;
            };
            static_assert(sizeof(PermissionValues) == 8);
            static constexpr PermissionValues empty_permission_values{0, 0};

            struct PermissionContainer {
                PermissionFlags flags;
                PermissionValues values;
            };
            static_assert(sizeof(PermissionContainer) == 9);

            struct ChannelPermissionContainer : public PermissionContainer {
                PermissionType permission;
                ChannelId channel_id;
            };
            static_assert(sizeof(ChannelPermissionContainer) == 19);
            static constexpr v2::PermissionFlags empty_flags = {false, false, false, false, false, false, false};
            static constexpr v2::PermissionContainer empty_channel_permission = {empty_flags, v2::empty_permission_values};

            #pragma pack(pop)

            #pragma pack(push, 1)
            template <size_t element_count>
            struct PermissionContainerBulk {
                PermissionContainer permissions[element_count];

                PermissionContainerBulk() {
                    memset(this->permissions, 0, sizeof(this->permissions));
                }
            };
            #pragma pack(pop)

            enum PermissionUpdateType {
                do_nothing,
                set_value,
                delete_value
            };

            struct PermissionDBUpdateEntry {
                PermissionType permission;
                ChannelId channel_id;

                PermissionValues values;
                PermissionUpdateType update_value;
                PermissionUpdateType update_grant;

                bool flag_db: 1; /* only needs an update if set */
                bool flag_delete: 1;
                bool flag_skip: 1;
                bool flag_negate: 1;
            };

            struct PermissionFlaggedValue {
                PermissionValue value{permNotGranted};
                bool has_value{false};

                [[nodiscard]] constexpr bool has_power() const { return this->has_value && (this->value > 0 || this->value == -1); }
                [[nodiscard]] constexpr bool has_infinite_power() const { return this->has_value && this->value == -1; }
                constexpr bool clear_flag_on_zero() {
                    if(this->has_value && this->value == 0) {
                        this->has_value = false;
                        return true;
                    }
                    return false;
                }

                /**
                 * Set the permission value to zero if the permission hasn't been set.
                 * This could be used to check if a client could do an action on another client
                 * but the client requires at least some power.
                 * @return
                 */
                constexpr auto& zero_if_unset() {
                    if(!this->has_value) {
                        this->has_value = true;
                        this->value = 0;
                    }

                    return *this;
                }

                inline bool operator==(const PermissionFlaggedValue& other) const { return other.value == this->value && other.has_value == this->has_value; }
                inline bool operator!=(const PermissionFlaggedValue& other) const { return !(*this == other); }
            };
            static constexpr PermissionFlaggedValue empty_permission_flagged_value{0, false};


            static constexpr bool permission_granted(const PermissionFlaggedValue& required, const PermissionFlaggedValue& given) {
                if(!required.has_value) {
                    /* The target permission hasn't been set so just check if we've not negated the target */
                    return !given.has_value || given.value >= 0;
                } else if(!given.has_power()) {
                    return false;
                } else if(given.has_infinite_power()) {
                    return true;
                } else if(required.has_infinite_power()) {
                    return false;
                } else {
                    return given.value >= required.value;
                }
            }
            static constexpr bool permission_granted(const PermissionValue& required, const PermissionFlaggedValue& given) {
                return permission_granted({required, true}, given);
            }

            class PermissionManager {
                public:
                    static constexpr size_t PERMISSIONS_BULK_BITS = 4; /* 16 permissions per block */
                    static constexpr size_t PERMISSIONS_BULK_ENTRY_COUNT = 1 << PERMISSIONS_BULK_BITS;
                    static constexpr size_t BULK_COUNT = (PermissionType::permission_id_max / (1 << PERMISSIONS_BULK_BITS)) + ((PermissionType::permission_id_max % PERMISSIONS_BULK_ENTRY_COUNT == 0) ? 0 : 1);
                    static_assert(PERMISSIONS_BULK_ENTRY_COUNT * BULK_COUNT >= PermissionType::permission_id_max);

                    PermissionManager();
                    virtual ~PermissionManager();

                    /* load permissions from the database */
                    void load_permission(const PermissionType&, const PermissionValues& /* values */, bool /* flag skip */, bool /* flag negate */, bool /* value present */,bool /* grant present */);
                    void load_permission(const PermissionType&, const PermissionValues& /* values */, ChannelId /* channel */, bool /* flag skip */, bool /* flag negate */, bool /* value present */,bool /* grant present */);

                    /* general getters/setters */
                    const PermissionFlags permission_flags(const PermissionType&); /* we return a "copy" because the actual permission could be deleted while we're analyzing the flags */
                    ts_always_inline PermissionFlags permission_flags(const std::shared_ptr<PermissionTypeEntry>& permission_info) { return this->permission_flags(permission_info->type); }

                    const PermissionValues permission_values(const PermissionType&);
                    ts_always_inline PermissionValues permission_values(const std::shared_ptr<PermissionTypeEntry>& permission_info) { return this->permission_values(permission_info->type); }

                    const PermissionFlaggedValue permission_value_flagged(const PermissionType&);
                    ts_always_inline PermissionFlaggedValue permission_value_flagged(const std::shared_ptr<PermissionTypeEntry>& permission_info) { return this->permission_value_flagged(permission_info->type); }

                    const PermissionFlaggedValue permission_granted_flagged(const PermissionType&);
                    ts_always_inline PermissionFlaggedValue permission_granted_flagged(const std::shared_ptr<PermissionTypeEntry>& permission_info) { return this->permission_granted_flagged(permission_info->type); }

                    /* only worth looking up if channel_specific is set */
                    const PermissionContainer channel_permission(const PermissionType& /* permission */, ChannelId /* channel id */);
                    ts_always_inline PermissionContainer channel_permission(const std::shared_ptr<PermissionTypeEntry>& permission_info, ChannelId channel_id) { return this->channel_permission(permission_info->type, channel_id); }

                    /* modifiers */
                    PermissionContainer set_permission(const PermissionType& /* permission */, const PermissionValues& /* values */, const PermissionUpdateType& /* update value */, const PermissionUpdateType& /* update grant */, int /* flag skip */ = -1, int /* flag negate */ = -1);
                    PermissionContainer set_channel_permission(const PermissionType& /* permission */, ChannelId /* channel id */, const PermissionValues& /* values */, const PermissionUpdateType& /* update value */, const PermissionUpdateType& /* update grant */, int /* flag skip */ = -1, int /* flag negate */ = -1);

                    /* bulk info */
                    const std::vector<std::tuple<PermissionType, const PermissionContainer>> permissions();
                    const std::vector<std::tuple<PermissionType, const PermissionContainer>> channel_permissions(ChannelId /* channel id */);
                    const std::vector<std::tuple<PermissionType, ChannelId, const PermissionContainer>> channel_permissions();

                    size_t used_memory();
                    void cleanup();

                    ts_always_inline bool require_db_updates() { return this->requires_db_save; }
                    const std::vector<PermissionDBUpdateEntry> flush_db_updates();
                private:
                    static constexpr size_t PERMISSIONS_BULK_BLOCK_MASK = (~(1 << PERMISSIONS_BULK_BITS)) & ((1 << PERMISSIONS_BULK_BITS) - 1);

                    bool requires_db_save = false;
                    ts_always_inline void trigger_db_update() { this->requires_db_save = true; }

                    spin_mutex block_use_count_lock{};
                    int16_t block_use_count[BULK_COUNT];
                    PermissionContainerBulk<PERMISSIONS_BULK_ENTRY_COUNT>* block_containers[BULK_COUNT];

                    //TODO: Bulk permissions for channels as well, specially because they're client permissions in terms of the music bot!
                    std::shared_mutex channel_list_lock{};
                    std::deque<std::unique_ptr<ChannelPermissionContainer>> _channel_permissions{};

                    ts_always_inline size_t calculate_block(const PermissionType& permission) {
                        return permission >> PERMISSIONS_BULK_BITS;
                    }

                    ts_always_inline size_t calculate_block_index(const PermissionType& permission) {
                        return permission & PERMISSIONS_BULK_BLOCK_MASK;
                    }

                    /**
                     * @param block
                     * @return true if block exists else false
                     */
                    ts_always_inline bool ref_block(size_t block) {
                        std::lock_guard use_lock(this->block_use_count_lock);
                        if(!this->block_containers[block])
                            return false;
                        this->block_use_count[block]++;
                        assert(this->block_use_count[block] > 0);
                        return true;
                    }

                    ts_always_inline void unref_block(size_t block) {
                        std::lock_guard use_lock(this->block_use_count_lock);
                        this->block_use_count[block]--;
                        assert(this->block_use_count[block] >= 0);
                    }

                    ts_always_inline void ref_allocate_block(size_t block) {
                        std::lock_guard use_lock(this->block_use_count_lock);
                        if(!this->block_containers[block])
                            this->block_containers[block] = new PermissionContainerBulk<PERMISSIONS_BULK_ENTRY_COUNT>();
                        this->block_use_count[block]++;
                        assert(this->block_use_count[block] > 0);
                    }
            };
        }
    }
}

inline std::ostream& operator<<(std::ostream& os, const ts::permission::v2::PermissionFlaggedValue& c) {
    if(c.has_value)
        return os << c.value;
    else
        return os << "unset";
}

DEFINE_VARIABLE_TRANSFORM(ts::permission::PermissionType, VARTYPE_INT, std::to_string((int16_t) in), static_cast<ts::permission::PermissionType>(in.as<int16_t>()));
DEFINE_VARIABLE_TRANSFORM(ts::permission::PermissionGroup, VARTYPE_INT, std::to_string((uint16_t) in), static_cast<ts::permission::PermissionGroup>(in.as<uint16_t>()));
DEFINE_VARIABLE_TRANSFORM(ts::permission::PermissionSqlType, VARTYPE_INT, std::to_string((int8_t) in), static_cast<ts::permission::PermissionSqlType>(in.as<int8_t>()));