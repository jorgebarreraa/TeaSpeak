#include <algorithm>
#include <cstring>
#include "misc/memtracker.h"
#include "./PermissionManager.h"
#include "./BasicChannel.h"

using namespace std;
using namespace ts;
using namespace ts::permission;

deque<std::shared_ptr<PermissionTypeEntry>> ts::permission::availablePermissions = deque<std::shared_ptr<PermissionTypeEntry>>{
        make_shared<PermissionTypeEntry>(PermissionType::b_serverinstance_help_view, PermissionGroup::global_info, "b_serverinstance_help_view", "Retrieve information about ServerQuery commands"),
        make_shared<PermissionTypeEntry>(PermissionType::b_serverinstance_version_view, PermissionGroup::global_info, "b_serverinstance_version_view", "Retrieve global server version (including platform and build number)"),
        make_shared<PermissionTypeEntry>(PermissionType::b_serverinstance_info_view, PermissionGroup::global_info, "b_serverinstance_info_view", "Retrieve global server information"),
        make_shared<PermissionTypeEntry>(PermissionType::b_serverinstance_virtualserver_list, PermissionGroup::global_info, "b_serverinstance_virtualserver_list", "List virtual servers stored in the sql"),
        make_shared<PermissionTypeEntry>(PermissionType::b_serverinstance_binding_list, PermissionGroup::global_info, "b_serverinstance_binding_list", "List active IP bindings on multi-homed machines"),
        //Removed due its useless
        make_shared<PermissionTypeEntry>(PermissionType::b_serverinstance_permission_list, PermissionGroup::global_info, "b_serverinstance_permission_list", "List permissions available available on the server instance"),
        make_shared<PermissionTypeEntry>(PermissionType::b_serverinstance_permission_find, PermissionGroup::global_info, "b_serverinstance_permission_find", "Search permission assignments by name or ID"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_create, PermissionGroup::global_vsmanage, "b_virtualserver_create", "Create virtual servers"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_delete, PermissionGroup::global_vsmanage, "b_virtualserver_delete", "Delete virtual servers"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_start_any, PermissionGroup::global_vsmanage, "b_virtualserver_start_any", "Start any virtual server in the server instance"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_stop_any, PermissionGroup::global_vsmanage, "b_virtualserver_stop_any", "Stop any virtual server in the server instance"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_change_machine_id, PermissionGroup::global_vsmanage, "b_virtualserver_change_machine_id", "Change a virtual servers machine ID"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_change_template, PermissionGroup::global_vsmanage, "b_virtualserver_change_template", "Edit virtual server default template values"),
        make_shared<PermissionTypeEntry>(PermissionType::b_serverquery_login, PermissionGroup::global_admin, "b_serverquery_login", "Login to ServerQuery"),
        make_shared<PermissionTypeEntry>(PermissionType::b_serverinstance_textmessage_send, PermissionGroup::global_admin, "b_serverinstance_textmessage_send", "Send text messages to all virtual servers at once"),
        make_shared<PermissionTypeEntry>(PermissionType::b_serverinstance_log_view, PermissionGroup::global_admin, "b_serverinstance_log_view", "Retrieve global server log"),
        make_shared<PermissionTypeEntry>(PermissionType::b_serverinstance_log_add, PermissionGroup::global_admin, "b_serverinstance_log_add", "Write to global server log"),
        make_shared<PermissionTypeEntry>(PermissionType::b_serverinstance_stop, PermissionGroup::global_admin, "b_serverinstance_stop", "Shutdown the server process"),
        make_shared<PermissionTypeEntry>(PermissionType::b_serverinstance_modify_settings, PermissionGroup::global_settings, "b_serverinstance_modify_settings", "Edit global settings"),
        make_shared<PermissionTypeEntry>(PermissionType::b_serverinstance_modify_querygroup, PermissionGroup::global_settings, "b_serverinstance_modify_querygroup", "Edit global ServerQuery groups"),
        make_shared<PermissionTypeEntry>(PermissionType::b_serverinstance_modify_templates, PermissionGroup::global_settings, "b_serverinstance_modify_templates", "Edit global template groups"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_select, PermissionGroup::vs_info, "b_virtualserver_select", "Select a virtual server"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_info_view, PermissionGroup::vs_info, "b_virtualserver_info_view", "Retrieve virtual server information"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_connectioninfo_view, PermissionGroup::vs_info, "b_virtualserver_connectioninfo_view", "Retrieve virtual server connection information"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_channel_list, PermissionGroup::vs_info, "b_virtualserver_channel_list", "List channels on a virtual server"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_channel_search, PermissionGroup::vs_info, "b_virtualserver_channel_search", "Search for channels on a virtual server"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_client_list, PermissionGroup::vs_info, "b_virtualserver_client_list", "List clients online on a virtual server"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_client_search, PermissionGroup::vs_info, "b_virtualserver_client_search", "Search for clients online on a virtual server"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_client_dblist, PermissionGroup::vs_info, "b_virtualserver_client_dblist", "List client identities known by the virtual server"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_client_dbsearch, PermissionGroup::vs_info, "b_virtualserver_client_dbsearch", "Search for client identities known by the virtual server"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_client_dbinfo, PermissionGroup::vs_info, "b_virtualserver_client_dbinfo", "Retrieve client information"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_permission_find, PermissionGroup::vs_info, "b_virtualserver_permission_find", "Find permissions"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_custom_search, PermissionGroup::vs_info, "b_virtualserver_custom_search", "Find custom fields"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_start, PermissionGroup::vs_admin, "b_virtualserver_start", "Start own virtual server"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_stop, PermissionGroup::vs_admin, "b_virtualserver_stop", "Stop own virtual server"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_token_list_all, PermissionGroup::vs_admin, "b_virtualserver_token_list_all", "Allows the client to list all tokens and not only his own"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_token_edit_all, PermissionGroup::vs_admin, "b_virtualserver_token_edit_all", "Edit all generated tokens"),
        make_shared<PermissionTypeEntry>(PermissionType::i_virtualserver_token_limit, PermissionGroup::vs_admin, "i_virtualserver_token_limit", "Max number of pending tokens a client could have"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_token_use, PermissionGroup::vs_admin, "b_virtualserver_token_use", "Use a privilege keys to gain access to groups"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_token_delete_all, PermissionGroup::vs_admin, "b_virtualserver_token_delete_all", "Allows the client to delete all tokens and not only the owned ones"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_log_view, PermissionGroup::vs_admin, "b_virtualserver_log_view", "Retrieve virtual server log"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_log_add, PermissionGroup::vs_admin, "b_virtualserver_log_add", "Write to virtual server log"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_join_ignore_password, PermissionGroup::vs_admin, "b_virtualserver_join_ignore_password", "Join virtual server ignoring its password"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_notify_register, PermissionGroup::vs_admin, "b_virtualserver_notify_register", "Register for server notifications"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_notify_unregister, PermissionGroup::vs_admin, "b_virtualserver_notify_unregister", "Unregister from server notifications"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_snapshot_create, PermissionGroup::vs_admin, "b_virtualserver_snapshot_create", "Create server snapshots"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_snapshot_deploy, PermissionGroup::vs_admin, "b_virtualserver_snapshot_deploy", "Deploy server snapshots"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_permission_reset, PermissionGroup::vs_admin, "b_virtualserver_permission_reset", "Reset the server permission settings to default values"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_modify_name, PermissionGroup::vs_settings, "b_virtualserver_modify_name", "Modify server name"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_modify_welcomemessage, PermissionGroup::vs_settings, "b_virtualserver_modify_welcomemessage", "Modify welcome message"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_modify_maxchannels, PermissionGroup::vs_settings, "b_virtualserver_modify_maxchannels", "Modify servers max channels"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_modify_maxclients, PermissionGroup::vs_settings, "b_virtualserver_modify_maxclients", "Modify servers max clients"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_modify_reserved_slots, PermissionGroup::vs_settings, "b_virtualserver_modify_reserved_slots", "Modify reserved slots"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_modify_password, PermissionGroup::vs_settings, "b_virtualserver_modify_password", "Modify server password"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_modify_default_servergroup, PermissionGroup::vs_settings, "b_virtualserver_modify_default_servergroup", "Modify default Server Group"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_modify_default_musicgroup, PermissionGroup::vs_settings, "b_virtualserver_modify_default_musicgroup", "Modify default music Group"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_modify_default_channelgroup, PermissionGroup::vs_settings, "b_virtualserver_modify_default_channelgroup", "Modify default Channel Group"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_modify_default_channeladmingroup, PermissionGroup::vs_settings, "b_virtualserver_modify_default_channeladmingroup", "Modify default Channel Admin Group"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_modify_channel_forced_silence, PermissionGroup::vs_settings, "b_virtualserver_modify_channel_forced_silence", "Modify channel force silence value"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_modify_complain, PermissionGroup::vs_settings, "b_virtualserver_modify_complain", "Modify individual complain settings"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_modify_antiflood, PermissionGroup::vs_settings, "b_virtualserver_modify_antiflood", "Modify individual antiflood settings"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_modify_ft_settings, PermissionGroup::vs_settings, "b_virtualserver_modify_ft_settings", "Modify file transfer settings"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_modify_ft_quotas, PermissionGroup::vs_settings, "b_virtualserver_modify_ft_quotas", "Modify file transfer quotas"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_modify_hostmessage, PermissionGroup::vs_settings, "b_virtualserver_modify_hostmessage", "Modify individual hostmessage settings"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_modify_hostbanner, PermissionGroup::vs_settings, "b_virtualserver_modify_hostbanner", "Modify individual hostbanner settings"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_modify_hostbutton, PermissionGroup::vs_settings, "b_virtualserver_modify_hostbutton", "Modify individual hostbutton settings"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_modify_port, PermissionGroup::vs_settings, "b_virtualserver_modify_port", "Modify server port"),
#ifndef LAGENCY
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_modify_host, PermissionGroup::vs_settings, "b_virtualserver_modify_host", "Modify server host"),
#endif
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_modify_autostart, PermissionGroup::vs_settings, "b_virtualserver_modify_autostart", "Modify server autostart"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_modify_needed_identity_security_level, PermissionGroup::vs_settings, "b_virtualserver_modify_needed_identity_security_level", "Modify required identity security level"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_modify_priority_speaker_dimm_modificator, PermissionGroup::vs_settings, "b_virtualserver_modify_priority_speaker_dimm_modificator", "Modify priority speaker dimm modificator"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_modify_log_settings, PermissionGroup::vs_settings, "b_virtualserver_modify_log_settings", "Modify log settings"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_modify_min_client_version, PermissionGroup::vs_settings, "b_virtualserver_modify_min_client_version", "Modify min client version"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_modify_icon_id, PermissionGroup::vs_settings, "b_virtualserver_modify_icon_id", "Modify server icon"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_modify_country_code, PermissionGroup::vs_settings, "b_virtualserver_modify_country_code", "Modify servers country code property"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_modify_codec_encryption_mode, PermissionGroup::vs_settings, "b_virtualserver_modify_codec_encryption_mode", "Modify codec encryption mode"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_modify_temporary_passwords, PermissionGroup::vs_settings, "b_virtualserver_modify_temporary_passwords", "Modify temporary serverpasswords"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_modify_temporary_passwords_own, PermissionGroup::vs_settings, "b_virtualserver_modify_temporary_passwords_own", "Modify own temporary serverpasswords"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_modify_channel_temp_delete_delay_default, PermissionGroup::vs_settings, "b_virtualserver_modify_channel_temp_delete_delay_default", "Modify default temporary channel delete delay"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_modify_music_bot_limit, PermissionGroup::vs_settings, "b_virtualserver_modify_music_bot_limit", "Allow client to edit the server music bot limit"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_modify_default_messages, PermissionGroup::vs_settings, "b_virtualserver_modify_default_messages", "Allows the client to edit the default messages"),
        make_shared<PermissionTypeEntry>(PermissionType::i_channel_min_depth, PermissionGroup::channel, "i_channel_min_depth", "Min channel creation depth in hierarchy"),
        make_shared<PermissionTypeEntry>(PermissionType::i_channel_max_depth, PermissionGroup::channel, "i_channel_max_depth", "Max channel creation depth in hierarchy"),
        make_shared<PermissionTypeEntry>(PermissionType::b_channel_group_inheritance_end, PermissionGroup::channel, "b_channel_group_inheritance_end", "Stop inheritance of channel group permissions"),
        make_shared<PermissionTypeEntry>(PermissionType::i_channel_permission_modify_power, PermissionGroup::channel, "i_channel_permission_modify_power", "Modify channel permission power"),
        make_shared<PermissionTypeEntry>(PermissionType::i_channel_needed_permission_modify_power, PermissionGroup::channel, "i_channel_needed_permission_modify_power", "Needed modify channel permission power"),
        make_shared<PermissionTypeEntry>(PermissionType::b_channel_info_view, PermissionGroup::channel_info, "b_channel_info_view", "Retrieve channel information"),
        make_shared<PermissionTypeEntry>(PermissionType::b_channel_create_child, PermissionGroup::channel_create, "b_channel_create_child", "Create sub-channels"),
        make_shared<PermissionTypeEntry>(PermissionType::b_channel_create_permanent, PermissionGroup::channel_create, "b_channel_create_permanent", "Create permanent channels"),
        make_shared<PermissionTypeEntry>(PermissionType::b_channel_create_semi_permanent, PermissionGroup::channel_create, "b_channel_create_semi_permanent", "Create semi-permanent channels"),
        make_shared<PermissionTypeEntry>(PermissionType::b_channel_create_temporary, PermissionGroup::channel_create, "b_channel_create_temporary", "Create temporary channels"),
        make_shared<PermissionTypeEntry>(PermissionType::b_channel_create_with_topic, PermissionGroup::channel_create, "b_channel_create_with_topic", "Create channels with a topic"),
        make_shared<PermissionTypeEntry>(PermissionType::b_channel_create_with_description, PermissionGroup::channel_create, "b_channel_create_with_description", "Create channels with a description"),
        make_shared<PermissionTypeEntry>(PermissionType::b_channel_create_with_password, PermissionGroup::channel_create, "b_channel_create_with_password", "Create password protected channels"),
        make_shared<PermissionTypeEntry>(PermissionType::b_channel_create_modify_with_codec_opusvoice, PermissionGroup::channel_create, "b_channel_create_modify_with_codec_opusvoice", "Create channels using OPUS (voice) codec"),
        make_shared<PermissionTypeEntry>(PermissionType::b_channel_create_modify_with_codec_opusmusic, PermissionGroup::channel_create, "b_channel_create_modify_with_codec_opusmusic", "Create channels using OPUS (music) codec"),
        make_shared<PermissionTypeEntry>(PermissionType::i_channel_create_modify_with_codec_maxquality, PermissionGroup::channel_create, "i_channel_create_modify_with_codec_maxquality", "Create channels with custom codec quality"),
        make_shared<PermissionTypeEntry>(PermissionType::i_channel_create_modify_with_codec_latency_factor_min, PermissionGroup::channel_create, "i_channel_create_modify_with_codec_latency_factor_min", "Create channels with minimal custom codec latency factor"),
        make_shared<PermissionTypeEntry>(PermissionType::b_channel_create_with_maxclients, PermissionGroup::channel_create, "b_channel_create_with_maxclients", "Create channels with custom max clients"),
        make_shared<PermissionTypeEntry>(PermissionType::b_channel_create_with_maxfamilyclients, PermissionGroup::channel_create, "b_channel_create_with_maxfamilyclients", "Create channels with custom max family clients"),
        make_shared<PermissionTypeEntry>(PermissionType::b_channel_create_with_sortorder, PermissionGroup::channel_create, "b_channel_create_with_sortorder", "Create channels with custom sort order"),
        make_shared<PermissionTypeEntry>(PermissionType::b_channel_create_with_default, PermissionGroup::channel_create, "b_channel_create_with_default", "Create default channels"),
        make_shared<PermissionTypeEntry>(PermissionType::b_channel_create_with_needed_talk_power, PermissionGroup::channel_create, "b_channel_create_with_needed_talk_power", "Create channels with needed talk power"),
        make_shared<PermissionTypeEntry>(PermissionType::b_channel_create_modify_with_force_password, PermissionGroup::channel_create, "b_channel_create_modify_with_force_password", "Create new channels only with password"),
        make_shared<PermissionTypeEntry>(PermissionType::i_channel_create_modify_with_temp_delete_delay, PermissionGroup::channel_create, "i_channel_create_modify_with_temp_delete_delay", "Max delete delay for temporary channels"),
        make_shared<PermissionTypeEntry>(PermissionType::i_channel_create_modify_conversation_history_length, PermissionGroup::channel_create, "i_channel_create_modify_conversation_history_length", "Upper limmit for the setting of the max conversation history limit"),
        make_shared<PermissionTypeEntry>(PermissionType::b_channel_create_modify_conversation_history_unlimited, PermissionGroup::channel_create, "b_channel_create_modify_conversation_history_unlimited", "Allows the user to set the channel conversation history to unlimited"),
        make_shared<PermissionTypeEntry>(PermissionType::b_channel_create_modify_conversation_mode_public, PermissionGroup::channel_create, "b_channel_create_modify_conversation_mode_public", "Allows the user to set the channel conversation mode to public"),
        make_shared<PermissionTypeEntry>(PermissionType::b_channel_create_modify_conversation_mode_private, PermissionGroup::channel_create, "b_channel_create_modify_conversation_mode_private", "Allows the user to set the channel conversation mode to private"),
        make_shared<PermissionTypeEntry>(PermissionType::b_channel_create_modify_conversation_mode_none, PermissionGroup::channel_create, "b_channel_create_modify_conversation_mode_none", "Allows the user to set the channel conversation mode to none"),
        make_shared<PermissionTypeEntry>(PermissionType::b_channel_create_modify_sidebar_mode, PermissionGroup::channel_create, "b_channel_create_modify_sidebar_mode", "Allows the user to change the channels sidebar apperiance"),

        make_shared<PermissionTypeEntry>(PermissionType::b_channel_modify_parent, PermissionGroup::channel_modify, "b_channel_modify_parent", "Move channels"),
        make_shared<PermissionTypeEntry>(PermissionType::b_channel_modify_make_default, PermissionGroup::channel_modify, "b_channel_modify_make_default", "Make channel default"),
        make_shared<PermissionTypeEntry>(PermissionType::b_channel_modify_make_permanent, PermissionGroup::channel_modify, "b_channel_modify_make_permanent", "Make channel permanent"),
        make_shared<PermissionTypeEntry>(PermissionType::b_channel_modify_make_semi_permanent, PermissionGroup::channel_modify, "b_channel_modify_make_semi_permanent", "Make channel semi-permanent"),
        make_shared<PermissionTypeEntry>(PermissionType::b_channel_modify_make_temporary, PermissionGroup::channel_modify, "b_channel_modify_make_temporary", "Make channel temporary"),
        make_shared<PermissionTypeEntry>(PermissionType::b_channel_modify_name, PermissionGroup::channel_modify, "b_channel_modify_name", "Modify channel name"),
        make_shared<PermissionTypeEntry>(PermissionType::b_channel_modify_topic, PermissionGroup::channel_modify, "b_channel_modify_topic", "Modify channel topic"),
        make_shared<PermissionTypeEntry>(PermissionType::b_channel_modify_description, PermissionGroup::channel_modify, "b_channel_modify_description", "Modify channel description"),
        make_shared<PermissionTypeEntry>(PermissionType::b_channel_modify_password, PermissionGroup::channel_modify, "b_channel_modify_password", "Modify channel password"),
        make_shared<PermissionTypeEntry>(PermissionType::b_channel_modify_codec, PermissionGroup::channel_modify, "b_channel_modify_codec", "Modify channel codec"),
        make_shared<PermissionTypeEntry>(PermissionType::b_channel_modify_codec_quality, PermissionGroup::channel_modify, "b_channel_modify_codec_quality", "Modify channel codec quality"),
        make_shared<PermissionTypeEntry>(PermissionType::b_channel_modify_codec_latency_factor, PermissionGroup::channel_modify, "b_channel_modify_codec_latency_factor", "Modify channel codec latency factor"),
        make_shared<PermissionTypeEntry>(PermissionType::b_channel_modify_maxclients, PermissionGroup::channel_modify, "b_channel_modify_maxclients", "Modify channels max clients"),
        make_shared<PermissionTypeEntry>(PermissionType::b_channel_modify_maxfamilyclients, PermissionGroup::channel_modify, "b_channel_modify_maxfamilyclients", "Modify channels max family clients"),
        make_shared<PermissionTypeEntry>(PermissionType::b_channel_modify_sortorder, PermissionGroup::channel_modify, "b_channel_modify_sortorder", "Modify channel sort order"),
        make_shared<PermissionTypeEntry>(PermissionType::b_channel_modify_needed_talk_power, PermissionGroup::channel_modify, "b_channel_modify_needed_talk_power", "Change needed channel talk power"),
        make_shared<PermissionTypeEntry>(PermissionType::i_channel_modify_power, PermissionGroup::channel_modify, "i_channel_modify_power", "Channel modify power"),
        make_shared<PermissionTypeEntry>(PermissionType::i_channel_needed_modify_power, PermissionGroup::channel_modify, "i_channel_needed_modify_power", "Needed channel modify power"),
        make_shared<PermissionTypeEntry>(PermissionType::b_channel_modify_make_codec_encrypted, PermissionGroup::channel_modify, "b_channel_modify_make_codec_encrypted", "Make channel codec encrypted"),
        make_shared<PermissionTypeEntry>(PermissionType::b_channel_modify_temp_delete_delay, PermissionGroup::channel_modify, "b_channel_modify_temp_delete_delay", "Modify temporary channel delete delay"),
        make_shared<PermissionTypeEntry>(PermissionType::b_channel_conversation_message_delete, PermissionGroup::channel_modify, "b_channel_conversation_message_delete", "If set the user is able to delete conversation messages"),

        make_shared<PermissionTypeEntry>(PermissionType::b_channel_delete_permanent, PermissionGroup::channel_delete, "b_channel_delete_permanent", "Delete permanent channels"),
        make_shared<PermissionTypeEntry>(PermissionType::b_channel_delete_semi_permanent, PermissionGroup::channel_delete, "b_channel_delete_semi_permanent", "Delete semi-permanent channels"),
        make_shared<PermissionTypeEntry>(PermissionType::b_channel_delete_temporary, PermissionGroup::channel_delete, "b_channel_delete_temporary", "Delete temporary channels"),
        make_shared<PermissionTypeEntry>(PermissionType::b_channel_delete_flag_force, PermissionGroup::channel_delete, "b_channel_delete_flag_force", "Force channel delete"),
        make_shared<PermissionTypeEntry>(PermissionType::i_channel_delete_power, PermissionGroup::channel_delete, "i_channel_delete_power", "Delete channel power"),
        make_shared<PermissionTypeEntry>(PermissionType::i_channel_needed_delete_power, PermissionGroup::channel_delete, "i_channel_needed_delete_power", "Needed delete channel power"),
        make_shared<PermissionTypeEntry>(PermissionType::b_channel_join_permanent, PermissionGroup::channel_access, "b_channel_join_permanent", "Join permanent channels"),
        make_shared<PermissionTypeEntry>(PermissionType::b_channel_join_semi_permanent, PermissionGroup::channel_access, "b_channel_join_semi_permanent", "Join semi-permanent channels"),
        make_shared<PermissionTypeEntry>(PermissionType::b_channel_join_temporary, PermissionGroup::channel_access, "b_channel_join_temporary", "Join temporary channels"),
        make_shared<PermissionTypeEntry>(PermissionType::b_channel_join_ignore_password, PermissionGroup::channel_access, "b_channel_join_ignore_password", "Join channel ignoring its password"),
        make_shared<PermissionTypeEntry>(PermissionType::b_channel_join_ignore_maxclients, PermissionGroup::channel_access, "b_channel_join_ignore_maxclients", "Ignore channels max clients limit"),
        make_shared<PermissionTypeEntry>(PermissionType::b_channel_ignore_view_power, PermissionGroup::channel_access, "b_channel_ignore_view_power", "If set the client see's every channel"),
        make_shared<PermissionTypeEntry>(PermissionType::i_channel_join_power, PermissionGroup::channel_access, "i_channel_join_power", "Channel join power"),
        make_shared<PermissionTypeEntry>(PermissionType::i_channel_needed_join_power, PermissionGroup::channel_access, "i_channel_needed_join_power", "Needed channel join power"),
        make_shared<PermissionTypeEntry>(PermissionType::b_channel_ignore_join_power, PermissionGroup::channel_access, "b_channel_ignore_join_power", "Allows the client to bypass the channel join power"),
        make_shared<PermissionTypeEntry>(PermissionType::i_channel_view_power, PermissionGroup::channel_access, "i_channel_view_power", "Channel view power"),
        make_shared<PermissionTypeEntry>(PermissionType::i_channel_needed_view_power, PermissionGroup::channel_access, "i_channel_needed_view_power", "Needed channel view power"),
        make_shared<PermissionTypeEntry>(PermissionType::i_channel_subscribe_power, PermissionGroup::channel_access, "i_channel_subscribe_power", "Channel subscribe power"),
        make_shared<PermissionTypeEntry>(PermissionType::i_channel_needed_subscribe_power, PermissionGroup::channel_access, "i_channel_needed_subscribe_power", "Needed channel subscribe power"),
        make_shared<PermissionTypeEntry>(PermissionType::b_channel_ignore_subscribe_power, PermissionGroup::channel_access, "b_channel_ignore_subscribe_power", "Allows the client to bypass the subscribe power"),
        make_shared<PermissionTypeEntry>(PermissionType::i_channel_description_view_power, PermissionGroup::channel_access, "i_channel_description_view_power", "Channel description view power"),
        make_shared<PermissionTypeEntry>(PermissionType::i_channel_needed_description_view_power, PermissionGroup::channel_access, "i_channel_needed_description_view_power", "Needed channel needed description view power"),
        make_shared<PermissionTypeEntry>(PermissionType::b_channel_ignore_description_view_power, PermissionGroup::channel_access, "b_channel_ignore_description_view_power", "Allows the client to bypass the channel description view power"),
        make_shared<PermissionTypeEntry>(PermissionType::i_icon_id, PermissionGroup::group, "i_icon_id", "Group icon identifier"),
        make_shared<PermissionTypeEntry>(PermissionType::i_max_icon_filesize, PermissionGroup::group, "i_max_icon_filesize", "Max icon filesize in bytes"),
        make_shared<PermissionTypeEntry>(PermissionType::i_max_playlist_size, PermissionGroup::group, "i_max_playlist_size", "Max songs within one playlist"),
        make_shared<PermissionTypeEntry>(PermissionType::i_max_playlists, PermissionGroup::group, "i_max_playlists", "Max amount of playlists a client could own"),
        make_shared<PermissionTypeEntry>(PermissionType::b_icon_manage, PermissionGroup::group, "b_icon_manage", "Enables icon management"),
        make_shared<PermissionTypeEntry>(PermissionType::b_group_is_permanent, PermissionGroup::group, "b_group_is_permanent", "Group is permanent"),
        make_shared<PermissionTypeEntry>(PermissionType::i_group_auto_update_type, PermissionGroup::group, "i_group_auto_update_type", "Group auto-update type"),
        make_shared<PermissionTypeEntry>(PermissionType::i_group_auto_update_max_value, PermissionGroup::group, "i_group_auto_update_max_value", "Group auto-update max value"),
        make_shared<PermissionTypeEntry>(PermissionType::i_group_sort_id, PermissionGroup::group, "i_group_sort_id", "Group sort id"),
        make_shared<PermissionTypeEntry>(PermissionType::i_group_show_name_in_tree, PermissionGroup::group, "i_group_show_name_in_tree", "Show group name in tree depending on selected mode"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_servergroup_list, PermissionGroup::group_info, "b_virtualserver_servergroup_list", "List server groups"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_servergroup_permission_list, PermissionGroup::group_info, "b_virtualserver_servergroup_permission_list", "Allows the client to view all server group permissions"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_servergroup_client_list, PermissionGroup::group_info, "b_virtualserver_servergroup_client_list", "List clients from a server group"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_channelgroup_list, PermissionGroup::group_info, "b_virtualserver_channelgroup_list", "List channel groups"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_channelgroup_permission_list, PermissionGroup::group_info, "b_virtualserver_channelgroup_permission_list", "Allows the client to view all channel group permissions"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_channelgroup_client_list, PermissionGroup::group_info, "b_virtualserver_channelgroup_client_list", "List clients from a channel group"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_client_permission_list, PermissionGroup::group_info, "b_virtualserver_client_permission_list", "Allows the client to view all client permissions"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_channel_permission_list, PermissionGroup::group_info, "b_virtualserver_channel_permission_list", "Allows the client to view all channel permissions"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_channelclient_permission_list, PermissionGroup::group_info, "b_virtualserver_channelclient_permission_list", "Allows the client to view all client channel permissions"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_playlist_permission_list, PermissionGroup::group_info, "b_virtualserver_playlist_permission_list", "Allows the client to view all playlist permissions"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_servergroup_create, PermissionGroup::group_create, "b_virtualserver_servergroup_create", "Create server groups"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_channelgroup_create, PermissionGroup::group_create, "b_virtualserver_channelgroup_create", "Create channel groups"),

#ifdef LAGENCY
        make_shared<PermissionTypeEntry>(PermissionType::i_group_modify_power, PermissionGroup::group_168, "i_group_modify_power", "Group modify power"),
        make_shared<PermissionTypeEntry>(PermissionType::i_group_needed_modify_power, PermissionGroup::group_168, "i_group_needed_modify_power", "Needed group modify power"),
        make_shared<PermissionTypeEntry>(PermissionType::i_group_member_add_power, PermissionGroup::group_168, "i_group_member_add_power", "Group member add power"),
        make_shared<PermissionTypeEntry>(PermissionType::i_group_needed_member_add_power, PermissionGroup::group_168, "i_group_needed_member_add_power", "Needed group member add power"),
        make_shared<PermissionTypeEntry>(PermissionType::i_group_member_remove_power, PermissionGroup::group_168, "i_group_member_remove_power", "Group member delete power"),
        make_shared<PermissionTypeEntry>(PermissionType::i_group_needed_member_remove_power, PermissionGroup::group_168, "i_group_needed_member_remove_power", "Needed group member delete power"),
#else
        make_shared<PermissionTypeEntry>(PermissionType::i_server_group_modify_power, PermissionGroup::group_modify, "i_server_group_modify_power", "Server group modify power"),
        make_shared<PermissionTypeEntry>(PermissionType::i_server_group_needed_modify_power, PermissionGroup::group_modify, "i_server_group_needed_modify_power", "Needed server group modify power"),
        make_shared<PermissionTypeEntry>(PermissionType::i_server_group_member_add_power, PermissionGroup::group_modify, "i_server_group_member_add_power", "Server group member add power"),
        make_shared<PermissionTypeEntry>(PermissionType::i_server_group_self_add_power, PermissionGroup::group_modify, "i_server_group_self_add_power", "Server group self add power"),
        make_shared<PermissionTypeEntry>(PermissionType::i_server_group_needed_member_add_power, PermissionGroup::group_modify, "i_server_group_needed_member_add_power", "Needed server group member add power"),
        make_shared<PermissionTypeEntry>(PermissionType::i_server_group_member_remove_power, PermissionGroup::group_modify, "i_server_group_member_remove_power", "Server group member delete power"),
        make_shared<PermissionTypeEntry>(PermissionType::i_server_group_self_remove_power, PermissionGroup::group_modify, "i_server_group_self_remove_power", "Server group self delete power"),
        make_shared<PermissionTypeEntry>(PermissionType::i_server_group_needed_member_remove_power, PermissionGroup::group_modify, "i_server_group_needed_member_remove_power", "Needed server group member delete power"),

        make_shared<PermissionTypeEntry>(PermissionType::i_channel_group_modify_power, PermissionGroup::group_modify, "i_channel_group_modify_power", "Channel group modify power"),
        make_shared<PermissionTypeEntry>(PermissionType::i_channel_group_needed_modify_power, PermissionGroup::group_modify, "i_channel_group_needed_modify_power", "Needed channel group modify power"),
        make_shared<PermissionTypeEntry>(PermissionType::i_channel_group_member_add_power, PermissionGroup::group_modify, "i_channel_group_member_add_power", "Channel group member add power"),
        make_shared<PermissionTypeEntry>(PermissionType::i_channel_group_self_add_power, PermissionGroup::group_modify, "i_channel_group_self_add_power", "Channel group self add power"),
        make_shared<PermissionTypeEntry>(PermissionType::i_channel_group_needed_member_add_power, PermissionGroup::group_modify, "i_channel_group_needed_member_add_power", "Needed channel group member add power"),
        make_shared<PermissionTypeEntry>(PermissionType::i_channel_group_member_remove_power, PermissionGroup::group_modify, "i_channel_group_member_remove_power", "Channel group member delete power"),
        make_shared<PermissionTypeEntry>(PermissionType::i_channel_group_self_remove_power, PermissionGroup::group_modify, "i_channel_group_self_remove_power", "Channel group self delete power"),
        make_shared<PermissionTypeEntry>(PermissionType::i_channel_group_needed_member_remove_power, PermissionGroup::group_modify, "i_channel_group_needed_member_remove_power", "Needed channel group member delete power"),

        //old enum mapping
        make_shared<PermissionTypeEntry>(PermissionType::i_displayed_group_member_add_power, PermissionGroup::group_modify, "i_group_member_add_power", "The displayed member add power (Enables/Disabled the group in the select menu)"),
        make_shared<PermissionTypeEntry>(PermissionType::i_displayed_group_needed_member_add_power, PermissionGroup::group_modify, "i_group_needed_member_add_power", "The needed displayed member add power (Enables/Disabled the group in the select menu)"),
        make_shared<PermissionTypeEntry>(PermissionType::i_displayed_group_member_remove_power, PermissionGroup::group_modify, "i_group_member_remove_power", "The displayed member add power (Enables/Disabled the group in the select menu)"),
        make_shared<PermissionTypeEntry>(PermissionType::i_displayed_group_needed_member_remove_power, PermissionGroup::group_modify, "i_group_needed_member_remove_power", "The needed displayed member add power (Enables/Disabled the group in the select menu)"),
        make_shared<PermissionTypeEntry>(PermissionType::i_displayed_group_modify_power, PermissionGroup::group_modify, "i_group_modify_power", "The displayed member add power (Enables/Disabled the group in the select menu)"),
        make_shared<PermissionTypeEntry>(PermissionType::i_displayed_group_needed_modify_power, PermissionGroup::group_modify, "i_group_needed_modify_power", "The needed displayed member add power (Enables/Disabled the group in the select menu)"),

        //new enum mapping (must come AFTER the supported permissions)
        make_shared<PermissionTypeEntry>(PermissionType::i_displayed_group_member_add_power, PermissionGroup::group_modify, "i_displayed_group_member_add_power", "The displayed member add power (Enables/Disabled the group in the select menu)", false),
        make_shared<PermissionTypeEntry>(PermissionType::i_displayed_group_needed_member_add_power, PermissionGroup::group_modify, "i_displayed_group_needed_member_add_power", "The needed displayed member add power (Enables/Disabled the group in the select menu)", false),
        make_shared<PermissionTypeEntry>(PermissionType::i_displayed_group_member_remove_power, PermissionGroup::group_modify, "i_displayed_group_member_remove_power", "The displayed member add power (Enables/Disabled the group in the select menu)", false),
        make_shared<PermissionTypeEntry>(PermissionType::i_displayed_group_needed_member_remove_power, PermissionGroup::group_modify, "i_displayed_group_needed_member_remove_power", "The needed displayed member add power (Enables/Disabled the group in the select menu)", false),
        make_shared<PermissionTypeEntry>(PermissionType::i_displayed_group_modify_power, PermissionGroup::group_modify, "i_displayed_group_modify_power", "The displayed member add power (Enables/Disabled the group in the select menu)", false),
        make_shared<PermissionTypeEntry>(PermissionType::i_displayed_group_needed_modify_power, PermissionGroup::group_modify, "i_displayed_group_needed_modify_power", "The needed displayed member add power (Enables/Disabled the group in the select menu)", false),
#endif

        make_shared<PermissionTypeEntry>(PermissionType::i_permission_modify_power, PermissionGroup::group_modify, "i_permission_modify_power", "Permission modify power"),
        make_shared<PermissionTypeEntry>(PermissionType::b_permission_modify_power_ignore, PermissionGroup::group_modify, "b_permission_modify_power_ignore", "Ignore needed permission modify power"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_servergroup_delete, PermissionGroup::group_delete, "b_virtualserver_servergroup_delete", "Delete server groups"),
        make_shared<PermissionTypeEntry>(PermissionType::b_virtualserver_channelgroup_delete, PermissionGroup::group_delete, "b_virtualserver_channelgroup_delete", "Delete channel groups"),
        make_shared<PermissionTypeEntry>(PermissionType::i_client_permission_modify_power, PermissionGroup::client, "i_client_permission_modify_power", "Client permission modify power"),
        make_shared<PermissionTypeEntry>(PermissionType::i_client_needed_permission_modify_power, PermissionGroup::client, "i_client_needed_permission_modify_power", "Needed client permission modify power"),
        make_shared<PermissionTypeEntry>(PermissionType::i_client_max_clones_uid, PermissionGroup::client, "i_client_max_clones_uid", "Max additional connections per client identity"),
        make_shared<PermissionTypeEntry>(PermissionType::i_client_max_clones_ip, PermissionGroup::client, "i_client_max_clones_ip", "Max additional connections per client address"),
        make_shared<PermissionTypeEntry>(PermissionType::i_client_max_clones_hwid, PermissionGroup::client, "i_client_max_clones_hwid", "Max additional connections per client hardware id"),
        make_shared<PermissionTypeEntry>(PermissionType::i_client_max_idletime, PermissionGroup::client, "i_client_max_idletime", "Max idle time in seconds"),
        make_shared<PermissionTypeEntry>(PermissionType::i_client_max_avatar_filesize, PermissionGroup::client, "i_client_max_avatar_filesize", "Max avatar filesize in bytes"),
        make_shared<PermissionTypeEntry>(PermissionType::i_client_max_channel_subscriptions, PermissionGroup::client, "i_client_max_channel_subscriptions", "Max channel subscriptions"),
        make_shared<PermissionTypeEntry>(PermissionType::i_client_max_channels, PermissionGroup::client, "i_client_max_channels", "Limit of created channels"),
        make_shared<PermissionTypeEntry>(PermissionType::i_client_max_temporary_channels, PermissionGroup::client, "i_client_max_temporary_channels", "Limit of created temporary channels"),
        make_shared<PermissionTypeEntry>(PermissionType::i_client_max_semi_channels, PermissionGroup::client, "i_client_max_semi_channels", "Limit of created semi-permanent channels"),
        make_shared<PermissionTypeEntry>(PermissionType::i_client_max_permanent_channels, PermissionGroup::client, "i_client_max_permanent_channels", "Limit of created permanent channels"),
        make_shared<PermissionTypeEntry>(PermissionType::b_client_use_priority_speaker, PermissionGroup::client, "b_client_use_priority_speaker", "Allows the client to use priority speaker"),
        make_shared<PermissionTypeEntry>(PermissionType::b_client_is_priority_speaker, PermissionGroup::client, "b_client_is_priority_speaker", "Toogels the client priority speaker mode"),
        make_shared<PermissionTypeEntry>(PermissionType::b_client_skip_channelgroup_permissions, PermissionGroup::client, "b_client_skip_channelgroup_permissions", "Ignore channel group permissions"),
        make_shared<PermissionTypeEntry>(PermissionType::b_client_force_push_to_talk, PermissionGroup::client, "b_client_force_push_to_talk", "Force Push-To-Talk capture mode"),
        make_shared<PermissionTypeEntry>(PermissionType::b_client_ignore_bans, PermissionGroup::client, "b_client_ignore_bans", "Ignore bans"),
        make_shared<PermissionTypeEntry>(PermissionType::b_client_ignore_vpn, PermissionGroup::client, "b_client_ignore_vpn", "Ignore the vpn check"),
        make_shared<PermissionTypeEntry>(PermissionType::b_client_ignore_antiflood, PermissionGroup::client, "b_client_ignore_antiflood", "Ignore antiflood measurements"),
        make_shared<PermissionTypeEntry>(PermissionType::b_client_enforce_valid_hwid, PermissionGroup::client, "b_client_enforce_valid_hwid", "Enforces the client to have a valid hardware id"),
        make_shared<PermissionTypeEntry>(PermissionType::b_client_allow_invalid_packet, PermissionGroup::client, "b_client_allow_invalid_packet", "Allow client to send invalid packets"),
        make_shared<PermissionTypeEntry>(PermissionType::b_client_allow_invalid_badges, PermissionGroup::client, "b_client_allow_invalid_badges", "Allow client to have invalid badges"),
        make_shared<PermissionTypeEntry>(PermissionType::b_client_issue_client_query_command, PermissionGroup::client, "b_client_issue_client_query_command", "Issue query commands from client"),
        make_shared<PermissionTypeEntry>(PermissionType::b_client_use_reserved_slot, PermissionGroup::client, "b_client_use_reserved_slot", "Use an reserved slot"),
        make_shared<PermissionTypeEntry>(PermissionType::b_client_use_channel_commander, PermissionGroup::client, "b_client_use_channel_commander", "Use channel commander"),
        make_shared<PermissionTypeEntry>(PermissionType::b_client_request_talker, PermissionGroup::client, "b_client_request_talker", "Allow to request talk power"),
        make_shared<PermissionTypeEntry>(PermissionType::b_client_avatar_delete_other, PermissionGroup::client, "b_client_avatar_delete_other", "Allow deletion of avatars from other clients"),
        make_shared<PermissionTypeEntry>(PermissionType::b_client_is_sticky, PermissionGroup::client, "b_client_is_sticky", "Client will be sticked to current channel"),
        make_shared<PermissionTypeEntry>(PermissionType::b_client_ignore_sticky, PermissionGroup::client, "b_client_ignore_sticky", "Client ignores sticky flag"),
#ifndef LAGACY
        make_shared<PermissionTypeEntry>(PermissionType::b_client_music_channel_list, PermissionGroup::client_info, "b_client_music_channel_list", "List all music bots in the current channel"),
        make_shared<PermissionTypeEntry>(PermissionType::b_client_music_server_list, PermissionGroup::client_info, "b_client_music_server_list", "List all music bots on the sderver"),
        make_shared<PermissionTypeEntry>(PermissionType::i_client_music_info, PermissionGroup::client_info, "i_client_music_info", "Permission to view music bot info"),
        make_shared<PermissionTypeEntry>(PermissionType::i_client_music_needed_info, PermissionGroup::client_info, "i_client_music_needed_info", "Required permission to view music bot info"),
#endif
        make_shared<PermissionTypeEntry>(PermissionType::b_client_info_view, PermissionGroup::client_info, "b_client_info_view", "Retrieve client information"),
        make_shared<PermissionTypeEntry>(PermissionType::b_client_permissionoverview_view, PermissionGroup::client_info, "b_client_permissionoverview_view", "Retrieve client permissions overview"),
        make_shared<PermissionTypeEntry>(PermissionType::b_client_permissionoverview_own, PermissionGroup::client_info, "b_client_permissionoverview_own", "Retrieve clients own permissions overview"),
        make_shared<PermissionTypeEntry>(PermissionType::b_client_remoteaddress_view, PermissionGroup::client_info, "b_client_remoteaddress_view", "View client IP address and port"),
        make_shared<PermissionTypeEntry>(PermissionType::i_client_serverquery_view_power, PermissionGroup::client_info, "i_client_serverquery_view_power", "ServerQuery view power"),
        make_shared<PermissionTypeEntry>(PermissionType::i_client_needed_serverquery_view_power, PermissionGroup::client_info, "i_client_needed_serverquery_view_power", "Needed ServerQuery view power"),
        make_shared<PermissionTypeEntry>(PermissionType::b_client_custom_info_view, PermissionGroup::client_info, "b_client_custom_info_view", "View custom fields"),
        make_shared<PermissionTypeEntry>(PermissionType::i_client_kick_from_server_power, PermissionGroup::client_admin, "i_client_kick_from_server_power", "Client kick power from server"),
        make_shared<PermissionTypeEntry>(PermissionType::i_client_needed_kick_from_server_power, PermissionGroup::client_admin, "i_client_needed_kick_from_server_power", "Needed client kick power from server"),
        make_shared<PermissionTypeEntry>(PermissionType::i_client_kick_from_channel_power, PermissionGroup::client_admin, "i_client_kick_from_channel_power", "Client kick power from channel"),
        make_shared<PermissionTypeEntry>(PermissionType::i_client_needed_kick_from_channel_power, PermissionGroup::client_admin, "i_client_needed_kick_from_channel_power", "Needed client kick power from channel"),
        make_shared<PermissionTypeEntry>(PermissionType::i_client_ban_power, PermissionGroup::client_admin, "i_client_ban_power", "Client ban power"),
        make_shared<PermissionTypeEntry>(PermissionType::i_client_needed_ban_power, PermissionGroup::client_admin, "i_client_needed_ban_power", "Needed client ban power"),
        make_shared<PermissionTypeEntry>(PermissionType::i_client_move_power, PermissionGroup::client_admin, "i_client_move_power", "Client move power"),
        make_shared<PermissionTypeEntry>(PermissionType::i_client_needed_move_power, PermissionGroup::client_admin, "i_client_needed_move_power", "Needed client move power"),
        make_shared<PermissionTypeEntry>(PermissionType::i_client_complain_power, PermissionGroup::client_admin, "i_client_complain_power", "Complain power"),
        make_shared<PermissionTypeEntry>(PermissionType::i_client_needed_complain_power, PermissionGroup::client_admin, "i_client_needed_complain_power", "Needed complain power"),
        make_shared<PermissionTypeEntry>(PermissionType::b_client_complain_list, PermissionGroup::client_admin, "b_client_complain_list", "Show complain list"),
        make_shared<PermissionTypeEntry>(PermissionType::b_client_complain_delete_own, PermissionGroup::client_admin, "b_client_complain_delete_own", "Delete own complains"),
        make_shared<PermissionTypeEntry>(PermissionType::b_client_complain_delete, PermissionGroup::client_admin, "b_client_complain_delete", "Delete complains"),
        make_shared<PermissionTypeEntry>(PermissionType::b_client_ban_list, PermissionGroup::client_admin, "b_client_ban_list", "Show banlist"),
        make_shared<PermissionTypeEntry>(PermissionType::b_client_ban_trigger_list, PermissionGroup::client_admin, "b_client_ban_trigger_list", "Show trigger banlist"),
        make_shared<PermissionTypeEntry>(PermissionType::b_client_ban_list_global, PermissionGroup::client_admin, "b_client_ban_list_global", "Show banlist globaly"),
        make_shared<PermissionTypeEntry>(PermissionType::b_client_ban_create, PermissionGroup::client_admin, "b_client_ban_create", "Add a ban"),
        make_shared<PermissionTypeEntry>(PermissionType::b_client_ban_create_global, PermissionGroup::client_admin, "b_client_ban_create_global", "Allow to create global bans"),
        make_shared<PermissionTypeEntry>(PermissionType::b_client_ban_name, PermissionGroup::client_admin, "b_client_ban_name", "Allows the client to ban a client by name"),
        make_shared<PermissionTypeEntry>(PermissionType::b_client_ban_ip, PermissionGroup::client_admin, "b_client_ban_ip", "Allows the client to ban a client by ip"),
        make_shared<PermissionTypeEntry>(PermissionType::b_client_ban_hwid, PermissionGroup::client_admin, "b_client_ban_hwid", "Allows the client to ban a client hardware id"),
        make_shared<PermissionTypeEntry>(PermissionType::b_client_ban_edit, PermissionGroup::client_admin, "b_client_ban_edit", "Allow to edit bans"),
        make_shared<PermissionTypeEntry>(PermissionType::b_client_ban_edit_global, PermissionGroup::client_admin, "b_client_ban_edit_global", "Allow to edit global bans"),
        make_shared<PermissionTypeEntry>(PermissionType::b_client_ban_delete_own, PermissionGroup::client_admin, "b_client_ban_delete_own", "Delete own bans"),
        make_shared<PermissionTypeEntry>(PermissionType::b_client_ban_delete, PermissionGroup::client_admin, "b_client_ban_delete", "Delete bans"),
        make_shared<PermissionTypeEntry>(PermissionType::b_client_ban_delete_own_global, PermissionGroup::client_admin, "b_client_ban_delete_own_global", "Delete own global bans"),
        make_shared<PermissionTypeEntry>(PermissionType::b_client_ban_delete_global, PermissionGroup::client_admin, "b_client_ban_delete_global", "Delete global bans"),
        make_shared<PermissionTypeEntry>(PermissionType::i_client_ban_max_bantime, PermissionGroup::client_admin, "i_client_ban_max_bantime", "Max bantime"),
        make_shared<PermissionTypeEntry>(PermissionType::b_client_even_textmessage_send, PermissionGroup::client_basic, "b_client_even_textmessage_send", "Allows the client to send text messages to himself"),
        make_shared<PermissionTypeEntry>(PermissionType::i_client_private_textmessage_power, PermissionGroup::client_basic, "i_client_private_textmessage_power", "Client private message power"),
        make_shared<PermissionTypeEntry>(PermissionType::i_client_needed_private_textmessage_power, PermissionGroup::client_basic, "i_client_needed_private_textmessage_power", "Needed client private message power"),
        make_shared<PermissionTypeEntry>(PermissionType::b_client_server_textmessage_send, PermissionGroup::client_basic, "b_client_server_textmessage_send", "Send text messages to virtual server"),
        make_shared<PermissionTypeEntry>(PermissionType::b_client_channel_textmessage_send, PermissionGroup::client_basic, "b_client_channel_textmessage_send", "Send text messages to channel"),
        make_shared<PermissionTypeEntry>(PermissionType::b_client_offline_textmessage_send, PermissionGroup::client_basic, "b_client_offline_textmessage_send", "Send offline messages to clients"),
        make_shared<PermissionTypeEntry>(PermissionType::i_client_talk_power, PermissionGroup::client_basic, "i_client_talk_power", "Client talk power"),
        make_shared<PermissionTypeEntry>(PermissionType::i_client_needed_talk_power, PermissionGroup::client_basic, "i_client_needed_talk_power", "Needed client talk power"),
        make_shared<PermissionTypeEntry>(PermissionType::i_client_poke_power, PermissionGroup::client_basic, "i_client_poke_power", "Client poke power"),
        make_shared<PermissionTypeEntry>(PermissionType::i_client_needed_poke_power, PermissionGroup::client_basic, "i_client_needed_poke_power", "Needed client poke power"),
        make_shared<PermissionTypeEntry>(PermissionType::i_client_poke_max_clients, PermissionGroup::client_basic, "i_client_poke_max_clients", "Max amount of clients which could be poked at once"),
        make_shared<PermissionTypeEntry>(PermissionType::b_client_set_flag_talker, PermissionGroup::client_basic, "b_client_set_flag_talker", "Set the talker flag for clients and allow them to speak"),
        make_shared<PermissionTypeEntry>(PermissionType::i_client_whisper_power, PermissionGroup::client_basic, "i_client_whisper_power", "Client whisper power"),
        make_shared<PermissionTypeEntry>(PermissionType::i_client_needed_whisper_power, PermissionGroup::client_basic, "i_client_needed_whisper_power", "Client needed whisper power"),

        make_shared<PermissionTypeEntry>(PermissionType::b_video_screen, PermissionGroup::client_basic, "b_video_screen", "Client can show his screen"),
        make_shared<PermissionTypeEntry>(PermissionType::b_video_camera, PermissionGroup::client_basic, "b_video_camera", "Client can show his video camera"),
        make_shared<PermissionTypeEntry>(PermissionType::i_video_max_kbps, PermissionGroup::client_basic, "i_video_max_kbps", "The maximal bandwidth used by the client to transmit video"),
        make_shared<PermissionTypeEntry>(PermissionType::i_video_max_streams, PermissionGroup::client_basic, "i_video_max_streams", "The maximal number of streams a client can simultaneously receive"),
        make_shared<PermissionTypeEntry>(PermissionType::i_video_max_screen_streams, PermissionGroup::client_basic, "i_video_max_screen_streams", "The maximal number of video streams a client can simultaneously receive"),
        make_shared<PermissionTypeEntry>(PermissionType::i_video_max_camera_streams, PermissionGroup::client_basic, "i_video_max_camera_streams", "The maximal number of camera streams a client can simultaneously receive"),

        make_shared<PermissionTypeEntry>(PermissionType::b_client_modify_description, PermissionGroup::client_modify, "b_client_modify_description", "Edit a clients description"),
        make_shared<PermissionTypeEntry>(PermissionType::b_client_modify_own_description, PermissionGroup::client_modify, "b_client_modify_own_description", "Allow client to edit own description"),
        make_shared<PermissionTypeEntry>(PermissionType::b_client_modify_dbproperties, PermissionGroup::client_modify, "b_client_modify_dbproperties", "Edit a clients properties in the sql"),
        make_shared<PermissionTypeEntry>(PermissionType::b_client_delete_dbproperties, PermissionGroup::client_modify, "b_client_delete_dbproperties", "Delete a clients properties in the sql"),
        make_shared<PermissionTypeEntry>(PermissionType::b_client_create_modify_serverquery_login, PermissionGroup::client_modify, "b_client_create_modify_serverquery_login", "Create or modify own ServerQuery account"),

        make_shared<PermissionTypeEntry>(PermissionType::b_client_query_create, PermissionGroup::client_modify, "b_client_query_create", "Create a ServerQuery account for any user"),
        make_shared<PermissionTypeEntry>(PermissionType::b_client_query_create_own, PermissionGroup::client_modify, "b_client_query_create_own", "Create your own ServerQuery account"),
        make_shared<PermissionTypeEntry>(PermissionType::b_client_query_list, PermissionGroup::client_modify, "b_client_query_list", "List all ServerQuery accounts"),
        make_shared<PermissionTypeEntry>(PermissionType::b_client_query_list_own, PermissionGroup::client_modify, "b_client_query_list_own", "List all own ServerQuery accounts"),
        make_shared<PermissionTypeEntry>(PermissionType::b_client_query_rename, PermissionGroup::client_modify, "b_client_query_rename", "Rename a ServerQuery account"),
        make_shared<PermissionTypeEntry>(PermissionType::b_client_query_rename_own, PermissionGroup::client_modify, "b_client_query_rename_own", "Rename the own ServerQuery account"),
        make_shared<PermissionTypeEntry>(PermissionType::b_client_query_change_password, PermissionGroup::client_modify, "b_client_query_change_password", "Change a server query accounts password"),
        make_shared<PermissionTypeEntry>(PermissionType::b_client_query_change_own_password, PermissionGroup::client_modify, "b_client_query_change_own_password", "Change a query accounts own password"),
        make_shared<PermissionTypeEntry>(PermissionType::b_client_query_change_password_global, PermissionGroup::client_modify, "b_client_query_change_password_global", "Change a global query accounts own password"),
        make_shared<PermissionTypeEntry>(PermissionType::b_client_query_delete, PermissionGroup::client_modify, "b_client_query_delete", "Delete a query accounts password"),
        make_shared<PermissionTypeEntry>(PermissionType::b_client_query_delete_own, PermissionGroup::client_modify, "b_client_query_delete_own", "Delete own query accounts password"),

#ifndef LAGENCY
        make_shared<PermissionTypeEntry>(PermissionType::b_client_music_create_temporary, PermissionGroup::client, "b_client_music_create_temporary", "Permission to create a music bot"),
        make_shared<PermissionTypeEntry>(PermissionType::b_client_music_create_semi_permanent, PermissionGroup::client, "b_client_music_create_semi_permanent", "Allows the client to create semi permanent music bots"),
        make_shared<PermissionTypeEntry>(PermissionType::b_client_music_create_permanent, PermissionGroup::client, "b_client_music_create_permanent", "Allows the client to create permanent music bots"),
        make_shared<PermissionTypeEntry>(PermissionType::b_client_music_modify_temporary, PermissionGroup::client, "b_client_music_modify_temporary", "Permission to make a music bot temporary"),
        make_shared<PermissionTypeEntry>(PermissionType::b_client_music_modify_semi_permanent, PermissionGroup::client, "b_client_music_modify_semi_permanent", "Allows the client to make a bot semi permanent"),
        make_shared<PermissionTypeEntry>(PermissionType::b_client_music_modify_permanent, PermissionGroup::client, "b_client_music_modify_permanent", "Allows the client to make a bot permanent"),
        make_shared<PermissionTypeEntry>(PermissionType::i_client_music_create_modify_max_volume, PermissionGroup::client, "i_client_music_create_modify_max_volume", "Sets the max allowed music bot volume"),
        make_shared<PermissionTypeEntry>(PermissionType::i_client_music_limit, PermissionGroup::client, "i_client_music_limit", "The limit of music bots bound to this client"),
        make_shared<PermissionTypeEntry>(PermissionType::i_client_music_delete_power, PermissionGroup::client, "i_client_music_delete_power", "Power to delete the music bot"),
        make_shared<PermissionTypeEntry>(PermissionType::i_client_music_needed_delete_power, PermissionGroup::client, "i_client_music_needed_delete_power", "Required power to delete the music bot"),
        make_shared<PermissionTypeEntry>(PermissionType::i_client_music_play_power, PermissionGroup::client, "i_client_music_play_power", "Power to play music"),
        make_shared<PermissionTypeEntry>(PermissionType::i_client_music_needed_modify_power, PermissionGroup::client, "i_client_music_needed_modify_power", "Required power to modify the bot settings"),
        make_shared<PermissionTypeEntry>(PermissionType::i_client_music_modify_power, PermissionGroup::client, "i_client_music_modify_power", "Power to modify the music bot settings"),
        make_shared<PermissionTypeEntry>(PermissionType::i_client_music_needed_play_power, PermissionGroup::client, "i_client_music_needed_play_power", "Required power to play music"),
        make_shared<PermissionTypeEntry>(PermissionType::i_client_music_rename_power, PermissionGroup::client, "i_client_music_rename_power", "Power to rename the bot"),
        make_shared<PermissionTypeEntry>(PermissionType::i_client_music_needed_rename_power, PermissionGroup::client, "i_client_music_needed_rename_power", "The required rename power for a music bot"),

        make_shared<PermissionTypeEntry>(PermissionType::b_playlist_create, PermissionGroup::client, "b_playlist_create", "Allows the client to create playlists"),
        make_shared<PermissionTypeEntry>(PermissionType::i_playlist_view_power, PermissionGroup::client, "i_playlist_view_power", "Power to see a playlist, and their songs"),
        make_shared<PermissionTypeEntry>(PermissionType::i_playlist_needed_view_power, PermissionGroup::client, "i_playlist_needed_view_power", "Needed power to see a playlist, and their songs"),
        make_shared<PermissionTypeEntry>(PermissionType::i_playlist_modify_power, PermissionGroup::client, "i_playlist_modify_power", "Power to modify the playlist properties"),
        make_shared<PermissionTypeEntry>(PermissionType::i_playlist_needed_modify_power, PermissionGroup::client, "i_playlist_needed_modify_power", "Needed power to modify the playlist properties"),
        make_shared<PermissionTypeEntry>(PermissionType::i_playlist_permission_modify_power, PermissionGroup::client, "i_playlist_permission_modify_power", "Power to modify the playlist permissions"),
        make_shared<PermissionTypeEntry>(PermissionType::i_playlist_needed_permission_modify_power, PermissionGroup::client, "i_playlist_needed_permission_modify_power", "Needed power to modify the playlist permissions"),
        make_shared<PermissionTypeEntry>(PermissionType::i_playlist_delete_power, PermissionGroup::client, "i_playlist_delete_power", "Power to delete the playlist"),
        make_shared<PermissionTypeEntry>(PermissionType::i_playlist_needed_delete_power, PermissionGroup::client, "i_playlist_needed_delete_power", "Needed power to delete the playlist"),

        make_shared<PermissionTypeEntry>(PermissionType::i_playlist_song_add_power, PermissionGroup::client, "i_playlist_song_add_power", "Power to add songs to a playlist"),
        make_shared<PermissionTypeEntry>(PermissionType::i_playlist_song_needed_add_power, PermissionGroup::client, "i_playlist_song_needed_add_power", "Needed power to add songs to a playlist"),
        make_shared<PermissionTypeEntry>(PermissionType::i_playlist_song_remove_power, PermissionGroup::client, "i_playlist_song_remove_power", "Power to remove songs from a playlist"),
        make_shared<PermissionTypeEntry>(PermissionType::i_playlist_song_needed_remove_power, PermissionGroup::client, "i_playlist_song_needed_remove_power", "Needed power to remove songs from a playlist"),
        make_shared<PermissionTypeEntry>(PermissionType::i_playlist_song_move_power, PermissionGroup::client, "i_playlist_song_move_power", "Power to move songs witin a playlist"),
        make_shared<PermissionTypeEntry>(PermissionType::i_playlist_song_needed_move_power, PermissionGroup::client, "i_playlist_song_needed_move_power", "Needed power to move songs within a playlist"),

        make_shared<PermissionTypeEntry>(PermissionType::b_client_use_bbcode_any, PermissionGroup::client, "b_client_use_bbcode_any", "Allows the client to use any bbcodes"),
        make_shared<PermissionTypeEntry>(PermissionType::b_client_use_bbcode_image, PermissionGroup::client, "b_client_use_bbcode_image", "Allows the client to use img bbcode"),
        make_shared<PermissionTypeEntry>(PermissionType::b_client_use_bbcode_url, PermissionGroup::client, "b_client_use_bbcode_url", "Allows the client to use url bbcode"),
#endif
        make_shared<PermissionTypeEntry>(PermissionType::b_ft_ignore_password, PermissionGroup::ft, "b_ft_ignore_password", "Browse files without channel password"),
        make_shared<PermissionTypeEntry>(PermissionType::b_ft_transfer_list, PermissionGroup::ft, "b_ft_transfer_list", "Retrieve list of running filetransfers"),
        make_shared<PermissionTypeEntry>(PermissionType::i_ft_file_upload_power, PermissionGroup::ft, "i_ft_file_upload_power", "File upload power"),
        make_shared<PermissionTypeEntry>(PermissionType::i_ft_needed_file_upload_power, PermissionGroup::ft, "i_ft_needed_file_upload_power", "Needed file upload power"),
        make_shared<PermissionTypeEntry>(PermissionType::i_ft_file_download_power, PermissionGroup::ft, "i_ft_file_download_power", "File download power"),
        make_shared<PermissionTypeEntry>(PermissionType::i_ft_needed_file_download_power, PermissionGroup::ft, "i_ft_needed_file_download_power", "Needed file download power"),
        make_shared<PermissionTypeEntry>(PermissionType::i_ft_file_delete_power, PermissionGroup::ft, "i_ft_file_delete_power", "File delete power"),
        make_shared<PermissionTypeEntry>(PermissionType::i_ft_needed_file_delete_power, PermissionGroup::ft, "i_ft_needed_file_delete_power", "Needed file delete power"),
        make_shared<PermissionTypeEntry>(PermissionType::i_ft_file_rename_power, PermissionGroup::ft, "i_ft_file_rename_power", "File rename power"),
        make_shared<PermissionTypeEntry>(PermissionType::i_ft_needed_file_rename_power, PermissionGroup::ft, "i_ft_needed_file_rename_power", "Needed file rename power"),
        make_shared<PermissionTypeEntry>(PermissionType::i_ft_file_browse_power, PermissionGroup::ft, "i_ft_file_browse_power", "File browse power"),
        make_shared<PermissionTypeEntry>(PermissionType::i_ft_needed_file_browse_power, PermissionGroup::ft, "i_ft_needed_file_browse_power", "Needed file browse power"),
        make_shared<PermissionTypeEntry>(PermissionType::i_ft_directory_create_power, PermissionGroup::ft, "i_ft_directory_create_power", "Create directory power"),
        make_shared<PermissionTypeEntry>(PermissionType::i_ft_needed_directory_create_power, PermissionGroup::ft, "i_ft_needed_directory_create_power", "Needed create directory power"),
        make_shared<PermissionTypeEntry>(PermissionType::i_ft_quota_mb_download_per_client, PermissionGroup::ft, "i_ft_quota_mb_download_per_client", "Download quota per client in MByte"),
        make_shared<PermissionTypeEntry>(PermissionType::i_ft_quota_mb_upload_per_client, PermissionGroup::ft, "i_ft_quota_mb_upload_per_client", "Upload quota per client in MByte"),

        make_shared<PermissionTypeEntry>(PermissionType::i_ft_max_bandwidth_download, PermissionGroup::ft, "i_ft_max_bandwidth_download", "Maximal download bandwidth allowed for the client"),
        make_shared<PermissionTypeEntry>(PermissionType::i_ft_max_bandwidth_upload, PermissionGroup::ft, "i_ft_max_bandwidth_upload", "Maximal download bandwidth allowed for the client")
};

deque<PermissionType> ts::permission::neededPermissions = {
        b_client_force_push_to_talk,
        b_channel_join_ignore_password,
        b_ft_ignore_password,
        i_client_max_avatar_filesize,
        i_client_max_channel_subscriptions,
        i_permission_modify_power,
        b_virtualserver_servergroup_permission_list,
        b_virtualserver_client_permission_list,
        b_virtualserver_channelgroup_permission_list,
        b_virtualserver_channelclient_permission_list,
        b_virtualserver_playlist_permission_list,
        b_virtualserver_channelgroup_client_list,
        b_client_permissionoverview_view,
        b_client_ban_list,
        b_client_ban_trigger_list,
        b_client_complain_list,
        b_client_complain_delete,
        b_client_complain_delete_own,
        b_virtualserver_log_view,
        b_client_create_modify_serverquery_login,
        b_virtualserver_connectioninfo_view,
        b_client_modify_description,
        b_client_server_textmessage_send,
        b_client_channel_textmessage_send,
#ifdef LAGENCY
i_group_modify_power,
        i_group_member_add_power,
        i_group_member_remove_power,
#else
        i_displayed_group_modify_power,
        i_displayed_group_member_add_power,
        i_displayed_group_member_remove_power,

        i_server_group_modify_power,
        i_server_group_member_add_power,
        i_server_group_self_add_power,
        i_server_group_member_remove_power,
        i_server_group_self_remove_power,

        i_channel_group_modify_power,
        i_channel_group_member_add_power,
        i_channel_group_self_add_power,
        i_channel_group_member_remove_power,
        i_channel_group_self_remove_power,
#endif
        i_ft_file_browse_power,
        b_permission_modify_power_ignore,
        b_virtualserver_modify_hostmessage,
        b_virtualserver_modify_ft_settings,
        b_virtualserver_modify_default_musicgroup,
        b_virtualserver_modify_default_servergroup,
        b_virtualserver_modify_default_channelgroup,
        b_virtualserver_modify_default_channeladmingroup,
        b_virtualserver_modify_hostbanner,
        b_virtualserver_modify_channel_forced_silence,
        b_virtualserver_modify_needed_identity_security_level,
        b_virtualserver_modify_name,
        b_virtualserver_modify_welcomemessage,
        b_virtualserver_modify_maxclients,
        b_virtualserver_modify_password,
        b_virtualserver_modify_complain,
        b_virtualserver_modify_antiflood,
        b_virtualserver_modify_ft_quotas,
        b_virtualserver_modify_hostbutton,
        b_virtualserver_modify_autostart,
        b_virtualserver_modify_port,
        b_virtualserver_modify_host,
        b_virtualserver_modify_log_settings,
        b_virtualserver_modify_priority_speaker_dimm_modificator,
        b_virtualserver_modify_music_bot_limit,
        b_virtualserver_modify_default_messages,
        i_channel_create_modify_conversation_history_length,
        b_channel_create_modify_conversation_history_unlimited,
        b_channel_create_modify_conversation_mode_public,
        b_channel_create_modify_conversation_mode_private,
        b_channel_create_modify_conversation_mode_none,
        b_channel_create_modify_sidebar_mode,
        b_channel_modify_name,
        b_channel_modify_password,
        b_channel_modify_topic,
        b_channel_modify_description,
        b_channel_modify_codec,
        b_channel_modify_codec_quality,
        b_channel_modify_make_permanent,
        b_channel_modify_make_semi_permanent,
        b_channel_modify_make_temporary,
        b_channel_modify_make_default,
        b_channel_modify_maxclients,
        b_channel_modify_maxfamilyclients,
        b_channel_modify_sortorder,
        b_channel_modify_needed_talk_power,
        i_channel_permission_modify_power,
        b_channel_create_child,
        b_channel_create_permanent,
        b_channel_create_temporary,
        b_channel_create_with_topic,
        b_channel_create_with_description,
        b_channel_create_with_password,
        b_channel_create_with_maxclients,
        b_channel_create_with_maxfamilyclients,
        b_channel_create_with_sortorder,
        b_channel_create_with_default,
        b_channel_create_modify_with_codec_opusvoice,
        b_channel_create_modify_with_codec_opusmusic,
        i_channel_create_modify_with_codec_maxquality,
        i_client_serverquery_view_power,
        b_channel_create_semi_permanent,
        b_serverinstance_modify_querygroup,
        b_serverinstance_modify_templates,
        b_virtualserver_channel_permission_list,
        b_channel_delete_permanent,
        b_channel_delete_semi_permanent,
        b_channel_delete_temporary,
        b_channel_delete_flag_force,
        b_client_set_flag_talker,
        b_channel_create_with_needed_talk_power,
        b_virtualserver_token_list_all,
        i_virtualserver_token_limit,
        b_virtualserver_token_use,
        b_virtualserver_token_delete_all,

        b_video_screen,
        b_video_camera,
        i_video_max_kbps,
        i_video_max_streams,
        i_video_max_screen_streams,
        i_video_max_camera_streams,

        /* ban functions */
        b_client_ban_create,
        b_client_ban_create_global,

        b_client_ban_list_global,
        b_client_ban_list,

        b_client_ban_edit,
        b_client_ban_edit_global,

        b_client_ban_name,
        b_client_ban_ip,
        b_client_ban_hwid,

        b_virtualserver_servergroup_list,
        b_virtualserver_servergroup_client_list,
        b_virtualserver_channelgroup_list,
        i_client_ban_max_bantime,
        b_icon_manage,
        i_max_icon_filesize,
        i_max_playlist_size,
        i_max_playlists,

        i_client_needed_whisper_power,
        i_client_whisper_power,

        b_virtualserver_modify_icon_id,
        b_client_modify_own_description,
        b_client_offline_textmessage_send,
        b_virtualserver_client_dblist,
        b_virtualserver_modify_reserved_slots,
        b_channel_conversation_message_delete,
        i_channel_create_modify_with_codec_latency_factor_min,
        b_channel_modify_codec_latency_factor,
        b_channel_modify_make_codec_encrypted,
        b_virtualserver_modify_codec_encryption_mode,
        b_client_use_channel_commander,
        b_virtualserver_servergroup_create,
        b_virtualserver_channelgroup_create,
        b_client_permissionoverview_own,
        i_ft_quota_mb_upload_per_client,
        i_ft_quota_mb_download_per_client,
        b_virtualserver_modify_country_code,
        b_virtualserver_channelgroup_delete,
        b_virtualserver_servergroup_delete,
        b_client_ban_delete,
        b_client_ban_delete_own,
        b_client_ban_delete_global,
        b_client_ban_delete_own_global,
        b_virtualserver_modify_temporary_passwords,
        b_virtualserver_modify_temporary_passwords_own,
        b_client_request_talker,
        b_client_avatar_delete_other,
        b_channel_create_modify_with_force_password,
        b_channel_join_ignore_maxclients,
        b_virtualserver_modify_channel_temp_delete_delay_default,
        i_channel_create_modify_with_temp_delete_delay,
        b_channel_modify_temp_delete_delay,
        i_client_poke_power,

        b_client_remoteaddress_view,

        i_client_music_play_power,
        i_client_music_modify_power,
        i_client_music_rename_power,
        b_client_music_create_temporary,
        b_client_music_create_semi_permanent,
        b_client_music_create_permanent,
        b_client_music_modify_temporary,
        b_client_music_modify_semi_permanent,
        b_client_music_modify_permanent,
        i_client_music_create_modify_max_volume,

        b_playlist_create,
        i_playlist_view_power,
        i_playlist_modify_power,
        i_playlist_permission_modify_power,
        i_playlist_delete_power,

        i_playlist_song_add_power,
        i_playlist_song_remove_power,
        i_playlist_song_move_power,

        b_client_query_create,
        b_client_query_list,
        b_client_query_list_own,
        b_client_query_rename,
        b_client_query_rename_own,
        b_client_query_change_password,
        b_client_query_change_own_password,
        b_client_query_change_password_global,
        b_client_query_delete,
        b_client_query_delete_own,
};

std::deque<PermissionGroup> permission::availableGroups = {
        global,
        global_info,
        global_vsmanage,
        global_admin,
        global_settings,
        vs,
        vs_info,
        vs_admin,
        vs_settings,
        channel,
        channel_info,
        channel_create,
        channel_modify,
        channel_delete,
        channel_access,
        group,
        group_info,
        group_create,
        group_modify,
        group_delete,
        client,
        client_info,
        client_admin,
        client_basic,
        client_modify,
        ft
};

std::shared_ptr<PermissionTypeEntry> PermissionTypeEntry::unknown = make_shared<PermissionTypeEntry>(PermissionType::unknown, PermissionGroup::global, "unknown", "unknown");

vector<std::shared_ptr<PermissionTypeEntry>> permission_id_map;
void permission::setup_permission_resolve() {
    permission_id_map.resize(permission::permission_id_max);

    for(auto& permission : availablePermissions) {
        if(!permission->clientSupported || permission->type < 0 || permission->type > permission::permission_id_max)
            continue;
        permission_id_map[permission->type] = permission;
    }

    /* fix "holes" as well set the permission id 0 (unknown) */
    for(auto& permission : permission_id_map) {
        if(permission)
            continue;
        permission = PermissionTypeEntry::unknown;
    }
}

std::shared_ptr<PermissionTypeEntry> permission::resolvePermissionData(PermissionType type) {
    if((type & PERM_ID_GRANT) > 0)
        type &= ~PERM_ID_GRANT;

    assert(!permission_id_map.empty());
    if(type < 0 || type >= permission::permission_id_max)
        return PermissionTypeEntry::unknown;

    return permission_id_map[type];
}

std::shared_ptr<PermissionTypeEntry> permission::resolvePermissionData(const std::string& name) {
    for(auto& elm : availablePermissions)
        if(elm->name == name  || elm->grant_name == name)
            return elm;
    return PermissionTypeEntry::unknown;
}

ChannelId Permission::channelId() {
    return this->channel ? this->channel->channelId() : 0;
}

PermissionManager::PermissionManager() {
    memtrack::allocated<PermissionManager>(this);
}
PermissionManager::~PermissionManager() {
    memtrack::freed<PermissionManager>(this);
}

std::shared_ptr<Permission> PermissionManager::registerPermission(ts::permission::PermissionType type,
                                                                  ts::permission::PermissionValue val,
                                                                  const std::shared_ptr<ts::BasicChannel> &channel,
                                                                  uint16_t flag) {
    return this->registerPermission(resolvePermissionData(type), val, channel, flag);
}

std::shared_ptr<Permission> PermissionManager::registerPermission(const std::shared_ptr<PermissionTypeEntry>& type, PermissionValue value, const std::shared_ptr<BasicChannel>& channel, uint16_t flagMask) {
    {
        auto found = getPermission(type->type, channel, false);
        if(!found.empty()) return found.front();
    }
    auto permission = std::make_shared<Permission>(type, value, permNotGranted, flagMask, channel);
    this->permissions.push_back(permission);
    for(auto& elm : this->updateHandler)
        elm(permission);
    return permission;
}

bool PermissionManager::hasPermission(PermissionType type, const std::shared_ptr<BasicChannel>& channel, bool testGlobal) {
    return !this->getPermission(type, channel, testGlobal).empty();
}

bool PermissionManager::setPermission(PermissionType type, PermissionValue value, const std::shared_ptr<BasicChannel>& channel) {
    auto list = this->getPermission(type, channel, false);
    if(list.empty())
        list.push_back(registerPermission(type, value, channel, PERM_FLAG_PUBLIC));

    list.front()->value = value;
    for(const auto& elm : this->updateHandler)
        elm(list.front());

    return true;
}

bool PermissionManager::setPermission(ts::permission::PermissionType type, ts::permission::PermissionValue value, const std::shared_ptr<ts::BasicChannel> &channel, bool negated, bool skiped) {
    auto list = this->getPermission(type, channel, false);
    if(list.empty())
        list.push_back(registerPermission(type, value, channel, PERM_FLAG_PUBLIC));

    list.front()->value = value;
    list.front()->flag_negate = negated;
    list.front()->flag_skip = skiped;

    for(const auto& elm : this->updateHandler)
        elm(list.front());

    return true;
}

bool PermissionManager::setPermissionGranted(PermissionType type, PermissionValue value, const std::shared_ptr<BasicChannel>& channel) {
    auto list = this->getPermission(type, channel, false);
    if(list.empty())
        list.push_back(registerPermission(type, permNotGranted, channel, PERM_FLAG_PUBLIC));
    list.front()->granted = value;

    for(auto& elm : this->updateHandler)
        elm(list.front());

    return true;
}

void PermissionManager::deletePermission(PermissionType type, const std::shared_ptr<BasicChannel>& channel) {
    auto list = getPermission(type, channel, false);
    if(list.empty()) return;
    list.front()->value = permNotGranted;
    //list.front()->deleted = true;
    //this->permissions.erase(std::find(this->permissions.begin(), this->permissions.end(), list.front()));

    for(auto& e : this->updateHandler)
        e(list.front());
}

std::deque<std::shared_ptr<Permission>> PermissionManager::getPermission(PermissionType type, const std::shared_ptr<BasicChannel>& channel, bool testGlobal) {
    std::deque<std::shared_ptr<Permission>> res;

    std::shared_ptr<Permission> channel_permission;
    std::shared_ptr<Permission> global_permission;
    for(const auto &perm : this->permissions)
        if(perm->type->type == type) {
            if(perm->channel == channel)
                channel_permission = perm;
            else if(!perm->channel && testGlobal)
                global_permission = perm;
        }
    if(channel_permission) res.push_back(channel_permission);
    if(global_permission) res.push_back(global_permission);
    return res;
}

std::vector<std::shared_ptr<Permission>> PermissionManager::listPermissions(uint16_t flags) {
    vector<shared_ptr<Permission>> result;
    for(const auto &perm : this->permissions)
        if((perm->flagMask & flags) > 0 || true) //FIXME?
            result.push_back(perm);
    return result;
}

std::vector<std::shared_ptr<Permission>> PermissionManager::allPermissions() {
    return std::vector<std::shared_ptr<Permission>>(this->permissions.begin(), this->permissions.end());
}

std::deque<std::shared_ptr<Permission>> PermissionManager::all_channel_specific_permissions() {
    std::deque<std::shared_ptr<Permission>> result;

    for(const auto& permission : this->permissions)
        if(permission->channel)
            result.push_back(permission);

    return result;
}

std::deque<std::shared_ptr<Permission>> PermissionManager::all_channel_unspecific_permissions() {
    std::deque<std::shared_ptr<Permission>> result;

    for(const auto& permission : this->permissions)
        if(!permission->channel)
            result.push_back(permission);

    return result;
}

teamspeak::MapType teamspeak::unmapping;
teamspeak::MapType teamspeak::mapping;

teamspeak::MapType build_mapping(){
    return {
            {teamspeak::GENERAL, {
                                         {"b_virtualserver_modify_port", {"b_virtualserver_modify_port", "b_virtualserver_modify_host"}},
                                         {"i_client_max_clones_uid", {"i_client_max_clones_uid", "i_client_max_clones_ip", "i_client_max_clones_hwid"}},
                                         {"b_client_ignore_bans", {"b_client_ignore_bans", "b_client_ignore_vpn"}},
                                         {"b_client_ban_list", {"b_client_ban_list", "b_client_ban_list_global"}},

                                         //Permissions which TeaSpeak has but TeamSpeak not
                                         {"", {"b_virtualserver_modify_music_bot_limit"}},
                                         {"", {"b_client_music_channel_list"}},
                                         {"", {"b_client_music_server_list"}},
                                         {"", {"i_client_music_info"}},
                                         {"", {"i_client_music_needed_info"}},

                                         {"", {"b_client_ban_edit"}},
                                         {"", {"b_client_ban_edit_global"}},
                                         {"", {"b_client_ban_create_global"}},
                                         {"", {"b_client_ban_delete_own_global"}},
                                         {"", {"b_client_ban_delete_global"}},

                                         {"", {"b_client_even_textmessage_send"}},
                                         {"", {"b_client_enforce_valid_hwid"}},
                                         {"", {"b_client_allow_invalid_packet"}},
                                         {"", {"b_client_allow_invalid_badges"}},

                                         {"", {"b_client_music_create"}},
                                         {"", {"b_client_music_delete_own"}},

                                         {"", {"i_client_music_limit"}},
                                         {"", {"i_client_music_needed_delete_power"}},
                                         {"", {"i_client_music_delete_power"}},
                                         {"", {"i_client_music_play_power"}},
                                         {"", {"i_client_music_needed_play_power"}},
                                         {"", {"i_client_music_rename_power"}},
                                         {"", {"i_client_music_needed_rename_power"}},

                                         {"", {"b_client_use_bbcode_any"}},
                                         {"", {"b_client_use_bbcode_url"}},
                                         {"", {"b_client_use_bbcode_image"}},

                                         {"", {"b_channel_ignore_view_power"}},
                                         {"", {"i_channel_view_power"}},
                                         {"", {"i_channel_needed_view_power"}},

                                         {"", {"i_client_max_channels"}},
                                         {"", {"i_client_max_temporary_channels"}},
                                         {"", {"i_client_max_semi_channels"}},
                                         {"", {"i_client_max_permanent_channels"}},

                                         {"", {"b_virtualserver_modify_default_messages"}},

                                         {"b_client_ban_list", {"b_client_ban_list", "b_client_ban_trigger_list"}},
                                         {"b_virtualserver_modify_default_servergroup", {"b_virtualserver_modify_default_servergroup", "b_virtualserver_modify_default_musicgroup"}},

                                         {"b_client_ban_create", {"b_client_ban_create", "b_client_ban_name", "b_client_ban_ip", "b_client_ban_hwid"}}
                                 },
            },
            {teamspeak::SERVER, {
                                         {"i_group_modify_power", {"i_server_group_modify_power", "i_channel_group_modify_power", "i_displayed_group_modify_power"}},
                                         {"i_group_member_add_power", {"i_server_group_member_add_power", "i_channel_group_member_add_power", "i_displayed_group_member_add_power"}},
                                         {"i_group_member_remove_power", {"i_server_group_member_remove_power", "i_channel_group_member_remove_power", "i_displayed_group_member_remove_power"}},
                                         {"i_group_needed_modify_power", {"i_server_group_needed_modify_power", "i_channel_group_needed_modify_power", "i_displayed_group_needed_modify_power"}},
                                         {"i_group_needed_member_add_power", {"i_server_group_needed_member_add_power", "i_channel_group_needed_member_add_power", "i_displayed_group_needed_member_add_power"}},
                                         {"i_group_needed_member_remove_power", {"i_server_group_needed_member_remove_power", "i_channel_group_needed_member_remove_power", "i_displayed_group_needed_member_remove_power"}},
                                 },
            },
            {teamspeak::CLIENT, {
                                        {"i_group_modify_power", {"i_server_group_modify_power", "i_channel_group_modify_power", "i_displayed_group_modify_power"}},
                                        {"i_group_member_add_power", {"i_server_group_member_add_power", "i_channel_group_member_add_power", "i_displayed_group_member_add_power"}},
                                        {"i_group_member_remove_power", {"i_server_group_member_remove_power", "i_channel_group_member_remove_power", "i_displayed_group_member_remove_power"}},
                                        {"i_group_needed_modify_power", {"i_server_group_needed_modify_power", "i_channel_group_needed_modify_power", "i_displayed_group_needed_modify_power"}},
                                        {"i_group_needed_member_add_power", {"i_server_group_needed_member_add_power", "i_channel_group_needed_member_add_power", "i_displayed_group_needed_member_add_power"}},
                                        {"i_group_needed_member_remove_power", {"i_server_group_needed_member_remove_power", "i_channel_group_needed_member_remove_power", "i_displayed_group_needed_member_remove_power"}},
                                },
            },
            {teamspeak::CHANNEL, {
                                         {"i_group_modify_power", {"i_channel_group_modify_power", "i_displayed_group_modify_power"}},
                                         {"i_group_member_add_power", {"i_channel_group_member_add_power", "i_displayed_group_member_add_power"}},
                                         {"i_group_member_remove_power", {"i_channel_group_member_remove_power", "i_displayed_group_member_remove_power"}},
                                         {"i_group_needed_modify_power", {"i_channel_group_needed_modify_power", "i_displayed_group_needed_modify_power"}},
                                         {"i_group_needed_member_add_power", {"i_channel_group_needed_member_add_power", "i_displayed_group_needed_member_add_power"}},
                                         {"i_group_needed_member_remove_power", {"i_channel_group_needed_member_remove_power", "i_displayed_group_needed_member_remove_power"}},
                                 },
            },
    };
};

inline teamspeak::MapType build_unmapping() {
    teamspeak::MapType result;

    for(const auto& map : teamspeak::mapping) {
        for(const auto& entry : map.second) {
            for(const auto& key : entry.second) {
                auto& m = result[map.first];
                auto it = m.find(key);
                if(it == m.end())
                    m.insert({key, {}});
                it = m.find(key);
                if(!entry.first.empty()) it->second.push_back(entry.first);
            }
        }
    }

    return result;
};

inline void init_mapping() {
    if(teamspeak::mapping.empty()) teamspeak::mapping = build_mapping();
    if(teamspeak::unmapping.empty()) teamspeak::unmapping = build_unmapping();
}

/*
template <typename T>
inline deque<T> operator+(const deque<T>& a, const deque<T>& b) {
    deque<T> result;
    result.insert(result.end(), a.begin(), a.end());
    result.insert(result.end(), b.begin(), b.end());
    return result;
}
*/

inline deque<string> mmget(teamspeak::MapType& map, teamspeak::GroupType type, const std::string& key) {
    return map[type].count(key) > 0 ? map[type].find(key)->second : deque<string>{};
}

inline std::deque<std::string> map_entry(std::string key, teamspeak::GroupType type, teamspeak::MapType& map_table) {
    init_mapping();

    if(key.find("_needed_modify_power_") == 1) {
        key = key.substr(strlen("x_needed_modify_power_"));


        std::deque<std::string> result{};
        auto mapped_type = mmget(map_table, type, "i_" + key);
        result.insert(result.end(), mapped_type.begin(), mapped_type.end());
        if(type != teamspeak::GroupType::GENERAL) {
            auto mapped_general = mmget(map_table, teamspeak::GroupType::GENERAL, "i_" + key);
            result.insert(result.end(), mapped_general.begin(), mapped_general.end());
        }

        if(result.empty())
            result.push_back("x_" + key);

        for(auto& entry : result)
            entry = "i_needed_modify_power_" + entry.substr(2);
        return result;
    }
    if(map_table[type].count(key) > 0 || map_table[teamspeak::GroupType::GENERAL].count(key) > 0) {
        std::deque<std::string> result{};
        auto mapped_type = mmget(map_table, type, key);
        result.insert(result.end(), mapped_type.begin(), mapped_type.end());
        if(type != teamspeak::GroupType::GENERAL) {
            auto mapped_general = mmget(map_table, teamspeak::GroupType::GENERAL, key);
            result.insert(result.end(), mapped_general.begin(), mapped_general.end());
        }
        return result;
    }
    return {key};
}

std::deque<std::string> teamspeak::map_key(std::string key, GroupType type) {
   return map_entry(key, type, teamspeak::mapping);
}

std::deque<std::string> teamspeak::unmap_key(std::string key, GroupType type) {
    return map_entry(key, type, teamspeak::unmapping);
}


#define AQB(name) \
{update::QUERY_ADMIN, {name, 1, 100, false, false}}, \
{update::SERVER_ADMIN, {name, 1, 75, false, false}}, \

#define AQBG(name) \
{update::QUERY_ADMIN, {name, permNotGranted, 100, false, false}}, \
{update::SERVER_ADMIN, {name, permNotGranted, 75, false, false}}, \

#define AQI(name) \
{update::QUERY_ADMIN, {name, 100, 100, false, false}}, \
{update::SERVER_ADMIN, {name, 75, 75, false, false}}, \

#define AQIG(name) \
{update::QUERY_ADMIN, {name, permNotGranted, 100, false, false}}, \
{update::SERVER_ADMIN, {name, permNotGranted, 75, false, false}}, \

deque<update::UpdateEntry> update::migrate = {
        AQB("b_virtualserver_modify_music_bot_limit")
        {update::QUERY_ADMIN, {"b_client_music_channel_list", 1, 100, false, false}},
        {update::SERVER_ADMIN, {"b_client_music_channel_list", 1, 75, false, false}},
        {update::CHANNEL_ADMIN, {"b_client_music_channel_list", 1, 75, false, false}},

        {update::QUERY_ADMIN, {"b_client_music_server_list", 1, 100, false, false}},
        {update::SERVER_ADMIN, {"b_client_music_server_list", 1, 75, false, false}},

        {update::QUERY_ADMIN, {"i_client_music_info", 100, 100, false, false}},
        {update::SERVER_ADMIN, {"i_client_music_info", 75, 75, false, false}},

        {update::QUERY_ADMIN, {"i_client_music_needed_info", permNotGranted, 100, false, false}},
        {update::SERVER_ADMIN, {"i_client_music_needed_info", permNotGranted, 75, false, false}},

        {update::QUERY_ADMIN, {"b_client_ban_list_global", 1, 100, false, false}},
        {update::SERVER_ADMIN, {"b_client_ban_list_global", 1, 75, false, false}},

        {update::QUERY_ADMIN, {"b_client_ban_edit", 1, 100, false, false}},
        {update::SERVER_ADMIN, {"b_client_ban_edit", 1, 75, false, false}},

        {update::QUERY_ADMIN, {"b_client_ban_create_global", 1, 100, false, false}},
        {update::QUERY_ADMIN, {"b_client_ban_edit_global", 1, 100, false, false}},
        {update::QUERY_ADMIN, {"b_client_ban_delete_own_global", 1, 100, false, false}},
        {update::QUERY_ADMIN, {"b_client_ban_delete_global", 1, 100, false, false}},

        AQB("b_client_even_textmessage_send")
        AQBG("b_client_enforce_valid_hwid")
        AQB("b_client_allow_invalid_packet")
        AQB("b_client_allow_invalid_badges")

        AQB("b_client_music_create")
        AQB("b_client_music_delete_own")

        AQI("i_client_music_limit")
        AQIG("i_client_music_needed_delete_power")
        AQI("i_client_music_delete_power")
        AQI("i_client_music_play_power")
        AQIG("i_client_music_needed_play_power")
        AQI("i_client_music_rename_power")
        AQIG("i_client_music_needed_rename_power")


        AQB("b_client_use_bbcode_any")
        AQB("b_client_use_bbcode_url")
        AQB("b_client_use_bbcode_image")

        {update::QUERY_ADMIN, {"b_channel_ignore_view_power", 1, 100, false, false}},
        AQI("i_channel_view_power")
        AQIG("i_channel_needed_view_power")

        AQB("b_client_ignore_vpn")

        AQIG("i_client_max_channels")
        AQIG("i_client_max_temporary_channels")
        AQIG("i_client_max_semi_channels")
        AQIG("i_client_max_permanent_channels")
        {update::SERVER_NORMAL, {"i_client_max_channels", 2, permNotGranted, false, false}},
        {update::SERVER_GUEST, {"i_client_max_channels", 1, permNotGranted, false, false}},

        AQB("b_virtualserver_modify_default_messages")
        AQB("b_virtualserver_modify_default_musicgroup")
        AQB("b_channel_ignore_join_power")
        AQB("b_client_ban_trigger_list")
};

v2::PermissionManager::PermissionManager() {
    memset(this->block_use_count, 0, sizeof(this->block_use_count));
    memset(this->block_containers, 0, sizeof(this->block_containers));
}

v2::PermissionManager::~PermissionManager() {
    for(auto& block : this->block_containers)
        delete block;
}

void v2::PermissionManager::load_permission(const ts::permission::PermissionType &permission, const ts::permission::v2::PermissionValues &values, bool flag_skip, bool flag_negate, bool flag_value, bool flag_grant) {
    if(permission < 0 || permission >= PermissionType::permission_id_max)
        return;

    const auto block = this->calculate_block(permission);
    this->ref_allocate_block(block);

    auto& data = this->block_containers[block]->permissions[this->calculate_block_index(permission)];
    data.values = values;
    data.flags.database_reference = true;
    data.flags.skip = flag_skip;
    data.flags.negate = flag_negate;
    data.flags.value_set = flag_value;
    data.flags.grant_set = flag_grant;
    this->unref_block(block);
}

void v2::PermissionManager::load_permission(const ts::permission::PermissionType &permission, const ts::permission::v2::PermissionValues &values, ChannelId channel_id, bool flag_skip, bool flag_negate, bool flag_value, bool flag_grant) {
    if(permission < 0 || permission >= PermissionType::permission_id_max)
        return;

    unique_lock channel_perm_lock(this->channel_list_lock);
    ChannelPermissionContainer* permission_container = nullptr;
    for(auto& entry : this->_channel_permissions)
        if(entry->permission == permission && entry->channel_id == channel_id) {
            permission_container = &*entry;
            break;
        }

    if(!permission_container) {
        auto container = make_unique<ChannelPermissionContainer>();
        container->permission = permission;
        container->channel_id = channel_id;
        permission_container = &*container;
        this->_channel_permissions.push_back(std::move(container));

        /* now set the channel flag for that permission */
        const auto block = this->calculate_block(permission);
        this->ref_allocate_block(block);

        auto& data = this->block_containers[block]->permissions[this->calculate_block_index(permission)];
        data.flags.channel_specific = true;
        this->unref_block(block);
    }

    permission_container->values = values;
    permission_container->flags.database_reference = true;
    permission_container->flags.skip = flag_skip;
    permission_container->flags.negate = flag_negate;
    permission_container->flags.value_set = flag_value;
    permission_container->flags.grant_set = flag_grant;
}

const v2::PermissionFlags v2::PermissionManager::permission_flags(const ts::permission::PermissionType &permission) {
    if(permission < 0 || permission >= PermissionType::permission_id_max)
        return empty_flags;

    const auto block = this->calculate_block(permission);
    if(!this->ref_block(block))
        return empty_flags;

    PermissionFlags result{this->block_containers[block]->permissions[this->calculate_block_index(permission)].flags};
    this->unref_block(block);
    return result;
}

const v2::PermissionValues v2::PermissionManager::permission_values(const ts::permission::PermissionType &permission) {
    if(permission < 0 || permission >= PermissionType::permission_id_max)
        return v2::empty_permission_values;

    const auto block = this->calculate_block(permission);
    if(!this->ref_block(block))
        return v2::empty_permission_values; /* TODO: may consider to throw an exception because the existence should be checked by getting the permission flags */

    v2::PermissionValues data{this->block_containers[block]->permissions[this->calculate_block_index(permission)].values};
    this->unref_block(block);
    return data;
}

const v2::PermissionFlaggedValue v2::PermissionManager::permission_value_flagged(const ts::permission::PermissionType &permission) {
    if(permission < 0 || permission >= PermissionType::permission_id_max)
        return v2::empty_permission_flagged_value;

    const auto block = this->calculate_block(permission);
    if(!this->ref_block(block))
        return v2::empty_permission_flagged_value;

    auto& data = this->block_containers[block]->permissions[this->calculate_block_index(permission)];
    v2::PermissionFlaggedValue result{data.values.value, data.flags.value_set};
    this->unref_block(block);
    return result;
}

const v2::PermissionFlaggedValue v2::PermissionManager::permission_granted_flagged(const ts::permission::PermissionType &permission) {
    if(permission < 0 || permission >= PermissionType::permission_id_max)
        return v2::empty_permission_flagged_value;

    const auto block = this->calculate_block(permission);
    if(!this->ref_block(block))
        return v2::empty_permission_flagged_value;

    auto& data = this->block_containers[block]->permissions[this->calculate_block_index(permission)];
    v2::PermissionFlaggedValue result{data.values.grant, data.flags.grant_set};
    this->unref_block(block);
    return result;
}

const v2::PermissionContainer v2::PermissionManager::channel_permission(const PermissionType &permission, ts::ChannelId channel_id) {
    if(permission < 0 || permission >= PermissionType::permission_id_max)
        return empty_channel_permission;

    shared_lock channel_perm_lock(this->channel_list_lock);
    for(auto& entry : this->_channel_permissions)
        if(entry->permission == permission && entry->channel_id == channel_id)
            return v2::PermissionContainer{entry->flags, entry->values};
    return empty_channel_permission;
}

inline v2::PermissionContainer duplicate_permission_container(const v2::PermissionContainer& original) {
    v2::PermissionContainer result{};
    result.flags = original.flags;
    result.values.grant = original.values.grant;
    result.values.value = original.values.value;
    return result;
}

static v2::PermissionContainer kEmptyPermissionContainer{
        .flags = v2::PermissionFlags{
                .database_reference = false,
                .channel_specific = false,

                .value_set = false,
                .grant_set = false,

                .skip = false,
                .negate = false,

                .flag_value_update = false,
                .flag_grant_update = false
        },
        .values = v2::PermissionValues{0, 0}
};

v2::PermissionContainer v2::PermissionManager::set_permission(const PermissionType &permission, const v2::PermissionValues &values, const v2::PermissionUpdateType &action_value, const v2::PermissionUpdateType &action_grant, int flag_skip, int flag_negate) {
    if(permission < 0 || permission >= PermissionType::permission_id_max)
        return kEmptyPermissionContainer;

    const auto block = this->calculate_block(permission);
    this->ref_allocate_block(block);

    auto& data = this->block_containers[block]->permissions[this->calculate_block_index(permission)];
    auto old_state = duplicate_permission_container(data);

    if(action_value == v2::PermissionUpdateType::set_value) {
        data.flags.value_set = true;
        data.flags.flag_value_update = true;
        data.values.value = values.value;
    } else if(action_value == v2::PermissionUpdateType::delete_value) {
        data.flags.value_set = false;
        data.flags.flag_value_update = true;
        data.values.value = permNotGranted; /* required for the database else it does not "deletes" the value */
    }

    if(action_grant == v2::PermissionUpdateType::set_value) {
        data.flags.grant_set = true;
        data.flags.flag_grant_update = true;
        data.values.grant = values.grant;
    } else if(action_grant == v2::PermissionUpdateType::delete_value) {
        data.flags.grant_set = false;
        data.flags.flag_grant_update = true;
        data.values.grant = permNotGranted; /* required for the database else it does not "deletes" the value */
    }

    if(flag_skip >= 0) {
        data.flags.flag_value_update = true;
        data.flags.skip = flag_skip == 1;
    }

    if(flag_negate >= 0) {
        data.flags.flag_value_update = true;
        data.flags.negate = flag_negate == 1;
    }

    this->unref_block(block);
    this->trigger_db_update();

    return old_state;
}

v2::PermissionContainer v2::PermissionManager::set_channel_permission(const PermissionType &permission, ChannelId channel_id, const v2::PermissionValues &values, const v2::PermissionUpdateType &action_value, const v2::PermissionUpdateType &action_grant, int flag_skip, int flag_negate) {
    if(permission < 0 || permission >= PermissionType::permission_id_max)
        return kEmptyPermissionContainer;

    unique_lock channel_perm_lock(this->channel_list_lock);
    ChannelPermissionContainer* permission_container = nullptr;
    for(auto& entry : this->_channel_permissions)
        if(entry->permission == permission && entry->channel_id == channel_id) {
            permission_container = &*entry;
            break;
        }

    /* register a new permission if we have no permission already */
    if(!permission_container) { /* if the permission isn't set then we have to register it again */
        if(action_value != v2::PermissionUpdateType::set_value && action_grant != v2::PermissionUpdateType::set_value) {
            return kEmptyPermissionContainer; /* we were never willing to set this permission */
        }

        {
            auto container = make_unique<ChannelPermissionContainer>();
            container->permission = permission;
            container->channel_id = channel_id;
            permission_container = &*container;
            this->_channel_permissions.push_back(std::move(container));
        }

        /* now set the channel flag for that permission */
        {
            const auto block = this->calculate_block(permission);
            this->ref_allocate_block(block);

            auto& data = this->block_containers[block]->permissions[this->calculate_block_index(permission)];
            data.flags.channel_specific = true;
            this->unref_block(block);
        }
    }
    auto old_state = duplicate_permission_container(*permission_container);

    if(action_value == v2::PermissionUpdateType::set_value) {
        permission_container->flags.value_set = true;
        permission_container->flags.flag_value_update = true;
        permission_container->values.value = values.value;
    } else if(action_value == v2::PermissionUpdateType::delete_value) {
        permission_container->flags.value_set = false;
        permission_container->flags.flag_value_update = true;
    }

    if(action_grant == v2::PermissionUpdateType::set_value) {
        permission_container->flags.grant_set = true;
        permission_container->flags.flag_grant_update = true;
        permission_container->values.grant = values.grant;
    } else if(action_grant == v2::PermissionUpdateType::delete_value) {
        permission_container->flags.grant_set = false;
        permission_container->flags.flag_grant_update = true;
    }

    if(flag_skip >= 0) {
        permission_container->flags.flag_value_update = true;
        permission_container->flags.skip = flag_skip == 1;
    }

    if(flag_negate >= 0) {
        permission_container->flags.flag_value_update = true;
        permission_container->flags.negate = flag_negate == 1;
    }

    if(!permission_container->flags.permission_set()) { /* unregister the permission again because its unset, we delete the channel permission as soon we've flushed the updates */
        auto other_channel_permission = std::find_if(this->_channel_permissions.begin(), this->_channel_permissions.end(), [&](unique_ptr<ChannelPermissionContainer>& perm) { return perm->permission == permission && perm->flags.permission_set(); });
        if(other_channel_permission == this->_channel_permissions.end()) { /* no more channel specific permissions c*/
            const auto block = this->calculate_block(permission);
            if(this->ref_block(block)) {
                this->block_containers[block]->permissions[this->calculate_block_index(permission)].flags.channel_specific = false;
                this->unref_block(block);
            }
        }
    }
    this->trigger_db_update();
    return old_state;
}

const std::vector<std::tuple<PermissionType, const v2::PermissionContainer>> v2::PermissionManager::permissions() {
    std::unique_lock use_lock(this->block_use_count_lock);
    decltype(this->block_containers) block_containers; /* save the states/nullptr's */
    memcpy(block_containers, this->block_containers, sizeof(this->block_containers));
    size_t block_count = 0;
    for(size_t index = 0; index < BULK_COUNT; index++) {
        if(block_containers[index]) {
            block_count++;
            this->block_use_count[index]++;
        }
    }
    use_lock.unlock();

    vector<tuple<PermissionType, const v2::PermissionContainer>> result;
    result.reserve(block_count * PERMISSIONS_BULK_ENTRY_COUNT);

    for(size_t block_index = 0; block_index < BULK_COUNT; block_index++) {
        auto& block = block_containers[block_index];
        if(!block)
            continue;

        for(size_t permission_index = 0; permission_index < PERMISSIONS_BULK_ENTRY_COUNT; permission_index++) {
            auto& permission = block->permissions[permission_index];
            if(!permission.flags.permission_set())
                continue;

            result.emplace_back((PermissionType) (block_index * PERMISSIONS_BULK_ENTRY_COUNT + permission_index), permission);
        }
    }
    result.shrink_to_fit();

    use_lock.lock();
    for(size_t index = 0; index < BULK_COUNT; index++) {
        if(block_containers[index])
            this->block_use_count[index]--;
    }
    use_lock.unlock();

    return result;
}

const vector<tuple<PermissionType, const v2::PermissionContainer>> v2::PermissionManager::channel_permissions(ts::ChannelId channel_id) {
    shared_lock channel_perm_lock(this->channel_list_lock);

    vector<tuple<PermissionType, const v2::PermissionContainer>> result;
    for(auto& entry : this->_channel_permissions)
        if((channel_id == entry->channel_id) && (entry->flags.value_set || entry->flags.grant_set))
            result.emplace_back(entry->permission, v2::PermissionContainer{entry->flags, entry->values});
    return result;
}

const vector<tuple<PermissionType, ChannelId, const v2::PermissionContainer>> v2::PermissionManager::channel_permissions() {
    shared_lock channel_perm_lock(this->channel_list_lock);

    vector<tuple<PermissionType, ChannelId, const v2::PermissionContainer>> result;
    for(auto& entry : this->_channel_permissions)
        if(entry->flags.value_set || entry->flags.grant_set)
            result.emplace_back(entry->permission, entry->channel_id, v2::PermissionContainer{entry->flags, entry->values});
    return result;
}

const std::vector<v2::PermissionDBUpdateEntry> v2::PermissionManager::flush_db_updates() {
    if(!this->requires_db_save)
        return {};

    this->requires_db_save = false;
    std::vector<v2::PermissionDBUpdateEntry> result;

    {
        lock_guard use_lock(this->block_use_count_lock);
        size_t block_count = 0;
        for (auto &block_container : block_containers) {
            if (block_container) {
                block_count++;
            }
        }
        result.reserve(block_count * PERMISSIONS_BULK_ENTRY_COUNT);

        for(size_t block_index = 0; block_index < BULK_COUNT; block_index++) {
            auto& block = block_containers[block_index];
            if(!block)
                continue;

            for(size_t permission_index = 0; permission_index < PERMISSIONS_BULK_ENTRY_COUNT; permission_index++) {
                auto& permission = block->permissions[permission_index];

                if(!permission.flags.flag_value_update && !permission.flags.flag_grant_update)
                    continue;

                /* we only need an update it the permission has a DB reference or we will set the permission */
                if(permission.flags.database_reference || permission.flags.permission_set()) {
                    /*
                        PermissionType permission;
                        ChannelId channel_id;

                        PermissionValues values;
                        PermissionUpdateType update_value;
                        PermissionUpdateType update_grant;

                        bool flag_db: 1;
                        bool flag_delete: 1;
                        bool flag_skip: 1;
                        bool flag_negate: 1;
                     */
                    auto& entry = result.emplace_back(v2::PermissionDBUpdateEntry{
                            (PermissionType) (block_index * PERMISSIONS_BULK_ENTRY_COUNT + permission_index),
                            (ChannelId) 0,

                            permission.values,
                            (PermissionUpdateType) (permission.flags.flag_value_update ? (permission.flags.value_set ? PermissionUpdateType::set_value : PermissionUpdateType::delete_value) : PermissionUpdateType::do_nothing),
                            (PermissionUpdateType) (permission.flags.flag_grant_update ? (permission.flags.grant_set ? PermissionUpdateType::set_value : PermissionUpdateType::delete_value) : PermissionUpdateType::do_nothing),

                            (bool) permission.flags.database_reference,
                            (bool) !permission.flags.permission_set(), /* db delete */
                            (bool) permission.flags.skip,
                            (bool) permission.flags.negate
                    });

                    /* required for the database */
                    if(!permission.flags.value_set)
                        entry.values.value = permNotGranted;

                    if(!permission.flags.grant_set)
                        entry.values.grant = permNotGranted;

                    permission.flags.database_reference = permission.flags.permission_set();
                }

                permission.flags.flag_value_update = false;
                permission.flags.flag_grant_update = false;
            }
        }
    }
    {
        lock_guard chanel_lock(this->channel_list_lock);
        for(size_t index = 0; index < this->_channel_permissions.size(); index++) {
            auto& permission = this->_channel_permissions[index];
            if(!permission->flags.flag_value_update && !permission->flags.flag_grant_update)
                continue;


            /* we only need an update it the permission has a DB reference or we will set the permission */
            if(permission->flags.database_reference || permission->flags.permission_set()) {

                auto& entry = result.emplace_back(v2::PermissionDBUpdateEntry{
                        permission->permission,
                        permission->channel_id,

                        permission->values,
                        (PermissionUpdateType) (permission->flags.flag_value_update ? (permission->flags.value_set ? PermissionUpdateType::set_value : PermissionUpdateType::delete_value) : PermissionUpdateType::do_nothing),
                        (PermissionUpdateType) (permission->flags.flag_grant_update ? (permission->flags.grant_set ? PermissionUpdateType::set_value : PermissionUpdateType::delete_value) : PermissionUpdateType::do_nothing),

                        (bool) permission->flags.database_reference,
                        (bool) !permission->flags.permission_set(), /* db delete */
                        (bool) permission->flags.skip,
                        (bool) permission->flags.negate
                });

                /* required for the database */
                if(!permission->flags.value_set)
                    entry.values.value = permNotGranted;

                if(!permission->flags.grant_set)
                    entry.values.grant = permNotGranted;

                permission->flags.database_reference = permission->flags.permission_set();
            }

            permission->flags.flag_value_update = false;
            permission->flags.flag_grant_update = false;
            if(!permission->flags.permission_set()) {
                this->_channel_permissions.erase(this->_channel_permissions.begin() + index);
                index--;
            }
        }
    }

    return result;
}

size_t v2::PermissionManager::used_memory() {
    size_t result = sizeof(*this);

    for (auto &block_container : block_containers) {
        if (block_container)
            result += sizeof(PermissionContainerBulk<PERMISSIONS_BULK_ENTRY_COUNT>);
    }

    {

        shared_lock channel_lock(this->channel_list_lock);
        result += this->_channel_permissions.size() * (sizeof(ChannelPermissionContainer) + sizeof(unique_ptr<ChannelPermissionContainer>));
    }

    return result;
}

void v2::PermissionManager::cleanup() {
    lock_guard use_lock(this->block_use_count_lock);

    for (auto &block_container : block_containers) {
        if (!block_container) continue;

        bool used = false;
        for(auto& permission : block_container->permissions) {
            if(permission.flags.value_set || permission.flags.grant_set || permission.flags.channel_specific) {
                used = true;
                break;
            }
        }
        if(used)
            continue;

        delete block_container;
        block_container = nullptr;
    }
}
