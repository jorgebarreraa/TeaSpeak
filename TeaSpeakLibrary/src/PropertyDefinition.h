
#define FLAG_SS (FLAG_SNAPSHOT | FLAG_SAVE)
#define FLAG_SERVER_VV (FLAG_SERVER_VARIABLE | FLAG_SERVER_VIEW)
#define FLAG_SERVER_VVSS  (FLAG_SERVER_VV | FLAG_SS)

#define FLAG_CLIENT_VV (FLAG_CLIENT_VARIABLE | FLAG_CLIENT_VIEW)
#define FLAG_CLIENT_VVSS  (FLAG_CLIENT_VV | FLAG_SS)

#define V(key, lkey, flags) key, lkey, "0", TYPE_UNSIGNED_NUMBER, flags
#define F(key, lkey, flags) key, lkey, "0", TYPE_FLOAT, flags

#ifdef EXTERNALIZE_PROPERTY_DEFINITIONS
decltype(property::property_list) property::
#else
constexpr auto
#endif
property_list = std::array<PropertyDescription, impl::property_count()>{
    PropertyDescription{UNKNOWN_UNDEFINED, "undefined", "", TYPE_UNKNOWN, 0},

    /* virtual server properties */
    PropertyDescription{VIRTUALSERVER_UNDEFINED, "undefined", "", TYPE_UNKNOWN, 0}, //Must be at index 0!
    PropertyDescription{VIRTUALSERVER_UNIQUE_IDENTIFIER, "virtualserver_unique_identifier", "", TYPE_STRING, FLAG_SERVER_VV | FLAG_SNAPSHOT},
    PropertyDescription{VIRTUALSERVER_NAME, "virtualserver_name", "Another TeaSpeak server software user", TYPE_STRING, FLAG_SERVER_VVSS | FLAG_USER_EDITABLE},
    PropertyDescription{VIRTUALSERVER_WELCOMEMESSAGE, "virtualserver_welcomemessage", "Welcome on another TeaSpeak server. (Download now and a license fee is not your cup of tea! [URL]www.teaspeak.de[/URL])", TYPE_STRING, FLAG_SERVER_VVSS | FLAG_USER_EDITABLE},
    PropertyDescription{VIRTUALSERVER_PLATFORM, "virtualserver_platform", "undefined", TYPE_STRING, FLAG_SERVER_VIEW},
    PropertyDescription{VIRTUALSERVER_VERSION, "virtualserver_version", "undefined", TYPE_STRING, FLAG_SERVER_VIEW},
    PropertyDescription{VIRTUALSERVER_MAXCLIENTS, "virtualserver_maxclients", "120", TYPE_UNSIGNED_NUMBER, FLAG_SERVER_VVSS | FLAG_USER_EDITABLE},
    PropertyDescription{VIRTUALSERVER_PASSWORD, "virtualserver_password", "", TYPE_STRING, FLAG_SS | FLAG_USER_EDITABLE},
    PropertyDescription{VIRTUALSERVER_CLIENTS_ONLINE, "virtualserver_clientsonline", "0", TYPE_UNSIGNED_NUMBER, FLAG_SERVER_VARIABLE},
    PropertyDescription{VIRTUALSERVER_QUERYCLIENTS_ONLINE, "virtualserver_queryclientsonline", "0", TYPE_UNSIGNED_NUMBER, FLAG_SERVER_VARIABLE},                         //only available on request (=> requestServerVariables)
    PropertyDescription{VIRTUALSERVER_CHANNELS_ONLINE, "virtualserver_channelsonline", "0", TYPE_UNSIGNED_NUMBER, FLAG_SERVER_VARIABLE},                   //only available on request (=> requestServerVariables),
    PropertyDescription{VIRTUALSERVER_CREATED, "virtualserver_created", "0", TYPE_UNSIGNED_NUMBER, FLAG_SERVER_VIEW | FLAG_SS},                           //available when connected, stores the time when the server was created
    PropertyDescription{VIRTUALSERVER_UPTIME, "virtualserver_uptime", "0", TYPE_UNSIGNED_NUMBER, FLAG_SERVER_VARIABLE},                            //only available on request (=> requestServerVariables), the time since the server was started

    PropertyDescription{VIRTUALSERVER_CODEC_ENCRYPTION_MODE, "virtualserver_codec_encryption_mode", "0", TYPE_UNSIGNED_NUMBER, FLAG_SERVER_VVSS | FLAG_USER_EDITABLE},             //available and always up-to-date when connected
    PropertyDescription{VIRTUALSERVER_KEYPAIR, "virtualserver_keypair", "", TYPE_STRING, FLAG_SS},                                     //internal use
    PropertyDescription{VIRTUALSERVER_HOSTMESSAGE, "virtualserver_hostmessage", "Welcome", TYPE_STRING, FLAG_SERVER_VVSS | FLAG_USER_EDITABLE},                                 //available when connected, not updated while connected
    PropertyDescription{VIRTUALSERVER_HOSTMESSAGE_MODE, "virtualserver_hostmessage_mode", "1", TYPE_STRING, FLAG_SERVER_VVSS | FLAG_USER_EDITABLE},                            //available when connected, not updated while connected
    PropertyDescription{VIRTUALSERVER_FILEBASE, "virtualserver_filebase", "", TYPE_STRING, FLAG_SERVER_VARIABLE | FLAG_SS | FLAG_USER_EDITABLE},                                    //not available to clients, stores the folder used for file transfers
    PropertyDescription{VIRTUALSERVER_DEFAULT_SERVER_GROUP, "virtualserver_default_server_group", "0", TYPE_UNSIGNED_NUMBER, FLAG_SERVER_VVSS | FLAG_USER_EDITABLE},                        //the manager permissions server group that a new manager gets assigned
    PropertyDescription{VIRTUALSERVER_DEFAULT_MUSIC_GROUP, "virtualserver_default_music_group", "0", TYPE_UNSIGNED_NUMBER, FLAG_SERVER_VVSS | FLAG_USER_EDITABLE},                        //the manager permissions server group that a new manager gets assigned
    PropertyDescription{VIRTUALSERVER_DEFAULT_CHANNEL_GROUP, "virtualserver_default_channel_group", "0", TYPE_UNSIGNED_NUMBER, FLAG_SERVER_VVSS | FLAG_USER_EDITABLE},                       //the channel permissions group that a new manager gets assigned when joining a channel
    PropertyDescription{VIRTUALSERVER_FLAG_PASSWORD, "virtualserver_flag_password", "0", TYPE_BOOL, FLAG_SERVER_VARIABLE | FLAG_SS | FLAG_USER_EDITABLE},                               //only available on request (=> requestServerVariables)
    PropertyDescription{VIRTUALSERVER_DEFAULT_CHANNEL_ADMIN_GROUP, "virtualserver_default_channel_admin_group", "0", TYPE_UNSIGNED_NUMBER, FLAG_SERVER_VARIABLE | FLAG_SERVER_VIEW | FLAG_SS | FLAG_USER_EDITABLE},                 //the channel permissions group that a manager gets assigned when creating a channel
    PropertyDescription{VIRTUALSERVER_MAX_DOWNLOAD_TOTAL_BANDWIDTH, "virtualserver_max_download_total_bandwidth", "-1", TYPE_SIGNED_NUMBER, FLAG_SERVER_VARIABLE | FLAG_SS | FLAG_USER_EDITABLE},                //only available on request (=> requestServerVariables)
    PropertyDescription{VIRTUALSERVER_MAX_UPLOAD_TOTAL_BANDWIDTH, "virtualserver_max_upload_total_bandwidth", "-1", TYPE_SIGNED_NUMBER, FLAG_SERVER_VARIABLE | FLAG_SS | FLAG_USER_EDITABLE},                  //only available on request (=> requestServerVariables)
    PropertyDescription{VIRTUALSERVER_HOSTBANNER_URL, "virtualserver_hostbanner_url", "", TYPE_STRING, FLAG_SERVER_VVSS | FLAG_USER_EDITABLE},                              //available when connected, always up-to-date
    PropertyDescription{VIRTUALSERVER_HOSTBANNER_GFX_URL, "virtualserver_hostbanner_gfx_url", "", TYPE_STRING, FLAG_SERVER_VVSS | FLAG_USER_EDITABLE},                          //available when connected, always up-to-date
    PropertyDescription{VIRTUALSERVER_HOSTBANNER_GFX_INTERVAL, "virtualserver_hostbanner_gfx_interval", "0", TYPE_UNSIGNED_NUMBER, FLAG_SERVER_VVSS | FLAG_USER_EDITABLE},                     //available when connected, always up-to-date
    PropertyDescription{VIRTUALSERVER_HOSTBANNER_MODE, "virtualserver_hostbanner_mode", "0", TYPE_UNSIGNED_NUMBER, FLAG_SERVER_VVSS | FLAG_USER_EDITABLE},                             //available when connected, always up-to-date
    PropertyDescription{VIRTUALSERVER_COMPLAIN_AUTOBAN_COUNT, "virtualserver_complain_autoban_count", "5", TYPE_UNSIGNED_NUMBER, FLAG_SERVER_VARIABLE | FLAG_SS | FLAG_USER_EDITABLE},                      //only available on request (=> requestServerVariables)
    PropertyDescription{VIRTUALSERVER_COMPLAIN_AUTOBAN_TIME, "virtualserver_complain_autoban_time", "5", TYPE_UNSIGNED_NUMBER, FLAG_SERVER_VARIABLE | FLAG_SS | FLAG_USER_EDITABLE},                       //only available on request (=> requestServerVariables)
    PropertyDescription{VIRTUALSERVER_COMPLAIN_REMOVE_TIME, "virtualserver_complain_remove_time", "5", TYPE_UNSIGNED_NUMBER, FLAG_SERVER_VARIABLE | FLAG_SS | FLAG_USER_EDITABLE},                        //only available on request (=> requestServerVariables)
    PropertyDescription{VIRTUALSERVER_MIN_CLIENTS_IN_CHANNEL_BEFORE_FORCED_SILENCE, "virtualserver_min_clients_in_channel_before_forced_silence", "20", TYPE_UNSIGNED_NUMBER, FLAG_SERVER_VARIABLE | FLAG_SS | FLAG_USER_EDITABLE},//only available on request (=> requestServerVariables)
    PropertyDescription{VIRTUALSERVER_PRIORITY_SPEAKER_DIMM_MODIFICATOR, "virtualserver_priority_speaker_dimm_modificator", "-18", TYPE_FLOAT, FLAG_SERVER_VVSS | FLAG_USER_EDITABLE},           //available when connected, always up-to-date
    PropertyDescription{VIRTUALSERVER_ID, "virtualserver_id", "0", TYPE_UNSIGNED_NUMBER, FLAG_SERVER_VIEW},                                          //available when connected
    PropertyDescription{VIRTUALSERVER_ANTIFLOOD_POINTS_TICK_REDUCE, "virtualserver_antiflood_points_tick_reduce", "25", TYPE_UNSIGNED_NUMBER, FLAG_SERVER_VARIABLE | FLAG_SS | FLAG_USER_EDITABLE},                //only available on request (=> requestServerVariables)
    PropertyDescription{VIRTUALSERVER_ANTIFLOOD_POINTS_NEEDED_COMMAND_BLOCK, "virtualserver_antiflood_points_needed_command_block", "150", TYPE_UNSIGNED_NUMBER, FLAG_SERVER_VARIABLE | FLAG_SS | FLAG_USER_EDITABLE},       //only available on request (=> requestServerVariables)
    PropertyDescription{VIRTUALSERVER_ANTIFLOOD_POINTS_NEEDED_IP_BLOCK, "virtualserver_antiflood_points_needed_ip_block", "300", TYPE_UNSIGNED_NUMBER, FLAG_SERVER_VARIABLE | FLAG_SS | FLAG_USER_EDITABLE},            //only available on request (=> requestServerVariables)
    PropertyDescription{VIRTUALSERVER_CLIENT_CONNECTIONS, "virtualserver_client_connections", "0", TYPE_UNSIGNED_NUMBER, FLAG_SERVER_VARIABLE | FLAG_SS},                          //only available on request (=> requestServerVariables)
    PropertyDescription{VIRTUALSERVER_QUERY_CLIENT_CONNECTIONS, "virtualserver_query_client_connections", "0", TYPE_UNSIGNED_NUMBER, FLAG_SERVER_VARIABLE | FLAG_SS},                    //only available on request (=> requestServerVariables)
    PropertyDescription{VIRTUALSERVER_HOSTBUTTON_TOOLTIP, "virtualserver_hostbutton_tooltip", "", TYPE_STRING, FLAG_SERVER_VVSS | FLAG_USER_EDITABLE},                          //available when connected, always up-to-date
    PropertyDescription{VIRTUALSERVER_HOSTBUTTON_URL, "virtualserver_hostbutton_url", "", TYPE_STRING, FLAG_SERVER_VVSS | FLAG_USER_EDITABLE},                              //available when connected, always up-to-date
    PropertyDescription{VIRTUALSERVER_HOSTBUTTON_GFX_URL, "virtualserver_hostbutton_gfx_url", "", TYPE_STRING, FLAG_SERVER_VVSS | FLAG_USER_EDITABLE},                          //available when connected, always up-to-date
    PropertyDescription{VIRTUALSERVER_DOWNLOAD_QUOTA, "virtualserver_download_quota", "-1", TYPE_SIGNED_NUMBER, FLAG_SERVER_VARIABLE | FLAG_SAVE | FLAG_USER_EDITABLE},                              //only available on request (=> requestServerVariables)
    PropertyDescription{VIRTUALSERVER_UPLOAD_QUOTA, "virtualserver_upload_quota", "-1", TYPE_SIGNED_NUMBER, FLAG_SERVER_VARIABLE | FLAG_SAVE | FLAG_USER_EDITABLE},                                //only available on request (=> requestServerVariables)
    PropertyDescription{VIRTUALSERVER_MONTH_BYTES_DOWNLOADED, "virtualserver_month_bytes_downloaded", "0", TYPE_UNSIGNED_NUMBER, FLAG_SERVER_VARIABLE | FLAG_SAVE},                      //only available on request (=> requestServerVariables)
    PropertyDescription{VIRTUALSERVER_MONTH_BYTES_UPLOADED, "virtualserver_month_bytes_uploaded", "0", TYPE_UNSIGNED_NUMBER, FLAG_SERVER_VARIABLE | FLAG_SAVE},                        //only available on request (=> requestServerVariables)
    PropertyDescription{VIRTUALSERVER_TOTAL_BYTES_DOWNLOADED, "virtualserver_total_bytes_downloaded", "0", TYPE_UNSIGNED_NUMBER, FLAG_SERVER_VARIABLE | FLAG_SAVE},                      //only available on request (=> requestServerVariables)
    PropertyDescription{VIRTUALSERVER_TOTAL_BYTES_UPLOADED, "virtualserver_total_bytes_uploaded", "0", TYPE_UNSIGNED_NUMBER, FLAG_SERVER_VARIABLE | FLAG_SAVE},                        //only available on request (=> requestServerVariables)
    PropertyDescription{VIRTUALSERVER_PORT, "virtualserver_port", "9987", TYPE_UNSIGNED_NUMBER, FLAG_SERVER_VARIABLE | FLAG_SS | FLAG_USER_EDITABLE},                                        //only available on request (=> requestServerVariables)
    PropertyDescription{VIRTUALSERVER_HOST, "virtualserver_host", "0.0.0.0,::", TYPE_STRING, FLAG_SERVER_VARIABLE | FLAG_SS | FLAG_USER_EDITABLE},                                          //internal use | contains comma separated ip list
    PropertyDescription{VIRTUALSERVER_AUTOSTART, "virtualserver_autostart", "1", TYPE_BOOL, FLAG_SERVER_VARIABLE | FLAG_SS | FLAG_USER_EDITABLE},                                   //only available on request (=> requestServerVariables)
    PropertyDescription{VIRTUALSERVER_MACHINE_ID, "virtualserver_machine_id", "", TYPE_STRING, FLAG_SERVER_VARIABLE | FLAG_SS | FLAG_USER_EDITABLE},                                  //only available on request (=> requestServerVariables)
    PropertyDescription{VIRTUALSERVER_NEEDED_IDENTITY_SECURITY_LEVEL, "virtualserver_needed_identity_security_level", "8", TYPE_UNSIGNED_NUMBER, FLAG_SERVER_VARIABLE | FLAG_SS | FLAG_USER_EDITABLE},              //only available on request (=> requestServerVariables)
    PropertyDescription{VIRTUALSERVER_LOG_CLIENT, "virtualserver_log_client", "1", TYPE_BOOL, FLAG_SERVER_VARIABLE | FLAG_SS | FLAG_USER_EDITABLE},                                  //only available on request (=> requestServerVariables)
    PropertyDescription{VIRTUALSERVER_LOG_QUERY, "virtualserver_log_query", "1", TYPE_BOOL, FLAG_SERVER_VARIABLE | FLAG_SS | FLAG_USER_EDITABLE},                                   //only available on request (=> requestServerVariables)
    PropertyDescription{VIRTUALSERVER_LOG_CHANNEL, "virtualserver_log_channel", "1", TYPE_BOOL, FLAG_SERVER_VARIABLE | FLAG_SS | FLAG_USER_EDITABLE},                                 //only available on request (=> requestServerVariables)
    PropertyDescription{VIRTUALSERVER_LOG_PERMISSIONS, "virtualserver_log_permissions", "1", TYPE_BOOL, FLAG_SERVER_VARIABLE | FLAG_SS | FLAG_USER_EDITABLE},                             //only available on request (=> requestServerVariables)
    PropertyDescription{VIRTUALSERVER_LOG_SERVER, "virtualserver_log_server", "1", TYPE_BOOL, FLAG_SERVER_VARIABLE | FLAG_SS | FLAG_USER_EDITABLE},                                  //only available on request (=> requestServerVariables)
    PropertyDescription{VIRTUALSERVER_LOG_FILETRANSFER, "virtualserver_log_filetransfer", "1", TYPE_BOOL, FLAG_SERVER_VARIABLE | FLAG_SS | FLAG_USER_EDITABLE},                            //only available on request (=> requestServerVariables)
    PropertyDescription{VIRTUALSERVER_NAME_PHONETIC, "virtualserver_name_phonetic", "", TYPE_STRING, FLAG_SERVER_VVSS | FLAG_USER_EDITABLE},                               //available when connected, always up-to-date
    PropertyDescription{VIRTUALSERVER_ICON_ID, "virtualserver_icon_id", "0", TYPE_UNSIGNED_NUMBER, FLAG_SERVER_VVSS | FLAG_USER_EDITABLE},                                     //available when connected, always up-to-date
    PropertyDescription{VIRTUALSERVER_RESERVED_SLOTS, "virtualserver_reserved_slots", "0", TYPE_UNSIGNED_NUMBER, FLAG_SERVER_VARIABLE | FLAG_SS | FLAG_USER_EDITABLE},                              //available when connected, always up-to-date
    PropertyDescription{VIRTUALSERVER_TOTAL_PACKETLOSS_SPEECH, "virtualserver_total_packetloss_speech", "0", TYPE_UNSIGNED_NUMBER, FLAG_SERVER_VARIABLE},                     //only available on request (=> requestServerVariables)
    PropertyDescription{VIRTUALSERVER_TOTAL_PACKETLOSS_KEEPALIVE, "virtualserver_total_packetloss_keepalive", "0", TYPE_FLOAT, FLAG_SERVER_VARIABLE},                  //only available on request (=> requestServerVariables)
    PropertyDescription{VIRTUALSERVER_TOTAL_PACKETLOSS_CONTROL, "virtualserver_total_packetloss_control", "0", TYPE_FLOAT, FLAG_SERVER_VARIABLE},                    //only available on request (=> requestServerVariables)
    PropertyDescription{VIRTUALSERVER_TOTAL_PACKETLOSS_TOTAL, "virtualserver_total_packetloss_total", "0", TYPE_FLOAT, FLAG_SERVER_VARIABLE},                      //only available on request (=> requestServerVariables)
    PropertyDescription{VIRTUALSERVER_TOTAL_PING, "virtualserver_total_ping", "0", TYPE_FLOAT, FLAG_SERVER_VARIABLE},                                  //only available on request (=> requestServerVariables)
    PropertyDescription{VIRTUALSERVER_AUTOGENERATED_PRIVILEGEKEY, "virtualserver_autogenerated_privilegekey", "", TYPE_STRING, FLAG_SAVE},                  //internal use
    PropertyDescription{VIRTUALSERVER_ASK_FOR_PRIVILEGEKEY, "virtualserver_ask_for_privilegekey", "1",  TYPE_BOOL, FLAG_SERVER_VV | FLAG_SAVE | FLAG_USER_EDITABLE},                        //available when connected
    PropertyDescription{VIRTUALSERVER_CHANNEL_TEMP_DELETE_DELAY_DEFAULT, "virtualserver_channel_temp_delete_delay_default", "60", TYPE_UNSIGNED_NUMBER, FLAG_SERVER_VVSS | FLAG_USER_EDITABLE},           //available when connected, always up-to-date
    PropertyDescription{VIRTUALSERVER_MIN_CLIENT_VERSION, "virtualserver_min_client_version", "1445512488", TYPE_UNSIGNED_NUMBER, FLAG_SERVER_VARIABLE | FLAG_USER_EDITABLE},                          //only available on request (=> requestServerVariables)
    PropertyDescription{VIRTUALSERVER_MIN_ANDROID_VERSION, "virtualserver_min_android_version", "1407159763", TYPE_UNSIGNED_NUMBER, FLAG_SERVER_VARIABLE | FLAG_USER_EDITABLE},                         //only available on request (=> requestServerVariables)
    PropertyDescription{VIRTUALSERVER_MIN_IOS_VERSION, "virtualserver_min_ios_version", "1407159763", TYPE_UNSIGNED_NUMBER, FLAG_SERVER_VARIABLE | FLAG_USER_EDITABLE},                             //only available on request (=> requestServerVariables)
    PropertyDescription{VIRTUALSERVER_MIN_WINPHONE_VERSION, "virtualserver_min_winphone_version", "1407159763", TYPE_UNSIGNED_NUMBER, FLAG_SERVER_VARIABLE | FLAG_USER_EDITABLE},                           //only available on request (=> requestServerVariables)
    PropertyDescription{VIRTUALSERVER_MAX_CHANNELS, "virtualserver_max_channels", "1000", TYPE_UNSIGNED_NUMBER, FLAG_SERVER_VARIABLE | FLAG_USER_EDITABLE},                           //only available on request (=> requestServerVariables)

    PropertyDescription{VIRTUALSERVER_LAST_CLIENT_CONNECT, "virtualserver_last_client_connect", "0", TYPE_UNSIGNED_NUMBER, FLAG_SERVER_VARIABLE | FLAG_NEW | FLAG_SS},                           //only available on request (=> requestServerVariables)
    PropertyDescription{VIRTUALSERVER_LAST_QUERY_CONNECT, "virtualserver_last_query_connect", "0", TYPE_UNSIGNED_NUMBER, FLAG_SERVER_VARIABLE | FLAG_NEW | FLAG_SS},                           //only available on request (=> requestServerVariables)
    PropertyDescription{VIRTUALSERVER_LAST_CLIENT_DISCONNECT, "virtualserver_last_client_disconnect", "0", TYPE_UNSIGNED_NUMBER, FLAG_SERVER_VARIABLE | FLAG_NEW | FLAG_SS},                           //only available on request (=> requestServerVariables)
    PropertyDescription{VIRTUALSERVER_LAST_QUERY_DISCONNECT, "virtualserver_last_query_disconnect", "0", TYPE_UNSIGNED_NUMBER, FLAG_SERVER_VARIABLE | FLAG_NEW | FLAG_SS},                           //only available on request (=> requestServerVariables)

    PropertyDescription{VIRTUALSERVER_WEB_HOST, "virtualserver_web_host", "0.0.0.0", TYPE_STRING, FLAG_SERVER_VARIABLE | FLAG_NEW | FLAG_SS | FLAG_USER_EDITABLE},                           //only available on request (=> requestServerVariables)
    PropertyDescription{VIRTUALSERVER_WEB_PORT, "virtualserver_web_port", "0", TYPE_UNSIGNED_NUMBER, FLAG_SERVER_VARIABLE | FLAG_NEW | FLAG_SS | FLAG_USER_EDITABLE},                           //only available on request (=> requestServerVariables)

    PropertyDescription{VIRTUALSERVER_DEFAULT_CLIENT_DESCRIPTION, "virtualserver_default_client_description", "", TYPE_STRING, FLAG_SERVER_VARIABLE | FLAG_NEW | FLAG_SS | FLAG_USER_EDITABLE},                           //only available on request (=> requestServerVariables)
    PropertyDescription{VIRTUALSERVER_DEFAULT_CHANNEL_DESCRIPTION, "virtualserver_default_channel_description", "", TYPE_STRING, FLAG_SERVER_VARIABLE | FLAG_NEW | FLAG_SS | FLAG_USER_EDITABLE},                           //only available on request (=> requestServerVariables)
    PropertyDescription{VIRTUALSERVER_DEFAULT_CHANNEL_TOPIC, "virtualserver_default_channel_topic", "", TYPE_STRING, FLAG_SERVER_VARIABLE | FLAG_NEW | FLAG_SS | FLAG_USER_EDITABLE},                           //only available on request (=> requestServerVariables)

    PropertyDescription{VIRTUALSERVER_MUSIC_BOT_LIMIT, "virtualserver_music_bot_limit", "-1", TYPE_SIGNED_NUMBER, FLAG_SERVER_VARIABLE | FLAG_NEW | FLAG_SS | FLAG_USER_EDITABLE},                           //only available on request (=> requestServerVariables)
    PropertyDescription{VIRTUALSERVER_SPOKEN_TIME, "virtualserver_spoken_time", "0", TYPE_UNSIGNED_NUMBER, FLAG_INTERNAL | FLAG_NEW | FLAG_SAVE},                           //only available on request (=> requestServerVariables)
    PropertyDescription{VIRTUALSERVER_DISABLE_IP_SAVING, "virtualserver_disable_ip_saving", "0", TYPE_BOOL, FLAG_INTERNAL | FLAG_NEW | FLAG_SAVE | FLAG_USER_EDITABLE},                           //only available on request (=> requestServerVariables)
    PropertyDescription{VIRTUALSERVER_COUNTRY_CODE, "virtualserver_country_code", "XX", TYPE_STRING, FLAG_SERVER_VV | FLAG_SAVE | FLAG_USER_EDITABLE},                           //available when connected

    /* channel properties */
    PropertyDescription{CHANNEL_UNDEFINED, "undefined", "", TYPE_UNKNOWN, 0}, //Must be at index 0!
    PropertyDescription{CHANNEL_ID, "cid", "0", TYPE_UNSIGNED_NUMBER, FLAG_CHANNEL_VIEW | FLAG_SS},
    PropertyDescription{CHANNEL_PID, "cpid", "0", TYPE_UNSIGNED_NUMBER, FLAG_CHANNEL_VIEW | FLAG_SS},
    PropertyDescription{CHANNEL_NAME, "channel_name", "undefined", TYPE_STRING, FLAG_CHANNEL_VIEW | FLAG_SS | FLAG_USER_EDITABLE},                       //Available for all channels that are "in view", always up-to-date
    PropertyDescription{CHANNEL_TOPIC, "channel_topic", "", TYPE_STRING, FLAG_CHANNEL_VIEW | FLAG_SS | FLAG_USER_EDITABLE},                          //Available for all channels that are "in view", always up-to-date
    PropertyDescription{CHANNEL_DESCRIPTION, "channel_description", "", TYPE_STRING, FLAG_CHANNEL_VARIABLE | FLAG_SS | FLAG_USER_EDITABLE},                    //Must be requested (=> requestChannelDescription)
    PropertyDescription{CHANNEL_PASSWORD, "channel_password", "0", TYPE_STRING, FLAG_SS | FLAG_USER_EDITABLE},                       //not available manager side
    PropertyDescription{CHANNEL_CODEC, "channel_codec", "4", TYPE_UNSIGNED_NUMBER, FLAG_CHANNEL_VIEW | FLAG_SS | FLAG_USER_EDITABLE},                          //Available for all channels that are "in view", always up-to-date
    PropertyDescription{CHANNEL_CODEC_QUALITY, "channel_codec_quality", "7", TYPE_UNSIGNED_NUMBER, FLAG_CHANNEL_VIEW | FLAG_SS | FLAG_USER_EDITABLE},                  //Available for all channels that are "in view", always up-to-date
    PropertyDescription{CHANNEL_MAXCLIENTS, "channel_maxclients", "-1", TYPE_SIGNED_NUMBER, FLAG_CHANNEL_VIEW | FLAG_SS | FLAG_USER_EDITABLE},                     //Available for all channels that are "in view", always up-to-date
    PropertyDescription{CHANNEL_MAXFAMILYCLIENTS, "channel_maxfamilyclients", "-1", TYPE_SIGNED_NUMBER, FLAG_CHANNEL_VIEW | FLAG_SS | FLAG_USER_EDITABLE},               //Available for all channels that are "in view", always up-to-date
    PropertyDescription{CHANNEL_ORDER, "channel_order", "0", TYPE_UNSIGNED_NUMBER, FLAG_CHANNEL_VIEW | FLAG_SS | FLAG_USER_EDITABLE},                          //Available for all channels that are "in view", always up-to-date
    PropertyDescription{CHANNEL_FLAG_PERMANENT, "channel_flag_permanent", "1", TYPE_BOOL, FLAG_CHANNEL_VIEW | FLAG_SS | FLAG_USER_EDITABLE},                 //Available for all channels that are "in view", always up-to-date
    PropertyDescription{CHANNEL_FLAG_SEMI_PERMANENT, "channel_flag_semi_permanent", "0", TYPE_BOOL, FLAG_CHANNEL_VIEW | FLAG_SS | FLAG_USER_EDITABLE},            //Available for all channels that are "in view", always up-to-date
    PropertyDescription{CHANNEL_FLAG_DEFAULT, "channel_flag_default", "0", TYPE_BOOL, FLAG_CHANNEL_VIEW | FLAG_SS | FLAG_USER_EDITABLE},                   //Available for all channels that are "in view", always up-to-date
    PropertyDescription{CHANNEL_FLAG_PASSWORD, "channel_flag_password", "0", TYPE_BOOL, FLAG_CHANNEL_VIEW | FLAG_SS | FLAG_USER_EDITABLE},                  //Available for all channels that are "in view", always up-to-date
    PropertyDescription{CHANNEL_CODEC_LATENCY_FACTOR, "channel_codec_latency_factor", "1", TYPE_UNSIGNED_NUMBER, FLAG_CHANNEL_VIEW | FLAG_SS | FLAG_USER_EDITABLE},           //Available for all channels that are "in view", always up-to-date
    PropertyDescription{CHANNEL_CODEC_IS_UNENCRYPTED, "channel_codec_is_unencrypted", "1", TYPE_BOOL, FLAG_CHANNEL_VIEW | FLAG_SS | FLAG_USER_EDITABLE},           //Available for all channels that are "in view", always up-to-date
    PropertyDescription{CHANNEL_SECURITY_SALT, "channel_security_salt", "", TYPE_STRING, FLAG_SS},                  //Not available manager side, not used in teamspeak, only SDK. Sets the options+salt for security hash.
    PropertyDescription{CHANNEL_DELETE_DELAY, "channel_delete_delay", "0", TYPE_UNSIGNED_NUMBER, FLAG_CHANNEL_VIEW | FLAG_SS | FLAG_USER_EDITABLE},                   //How many seconds to wait before deleting this channel
    PropertyDescription{CHANNEL_FLAG_MAXCLIENTS_UNLIMITED, "channel_flag_maxclients_unlimited", "1", TYPE_BOOL, FLAG_CHANNEL_VIEW | FLAG_SS | FLAG_USER_EDITABLE},      //Available for all channels that are "in view", always up-to-date
    PropertyDescription{CHANNEL_FLAG_MAXFAMILYCLIENTS_UNLIMITED, "channel_flag_maxfamilyclients_unlimited", "1", TYPE_BOOL, FLAG_CHANNEL_VIEW | FLAG_SS | FLAG_USER_EDITABLE},//Available for all channels that are "in view", always up-to-date
    PropertyDescription{CHANNEL_FLAG_MAXFAMILYCLIENTS_INHERITED, "channel_flag_maxfamilyclients_inherited", "0", TYPE_BOOL, FLAG_CHANNEL_VIEW | FLAG_SS | FLAG_USER_EDITABLE},//Available for all channels that are "in view", always up-to-date
    PropertyDescription{CHANNEL_FLAG_ARE_SUBSCRIBED, "channel_flag_are_subscribed", "1", TYPE_BOOL, FLAG_INTERNAL},            //Only available manager side, stores whether we are subscribed to this channel
    PropertyDescription{CHANNEL_FILEPATH, "channel_filepath", "", TYPE_STRING, FLAG_SS},                       //not available manager side, the folder used for file-transfers for this channel
    PropertyDescription{CHANNEL_NEEDED_TALK_POWER, "channel_needed_talk_power", "0", TYPE_UNSIGNED_NUMBER, FLAG_CHANNEL_VIEW | FLAG_SS | FLAG_USER_EDITABLE},              //Available for all channels that are "in view", always up-to-date
    PropertyDescription{CHANNEL_FORCED_SILENCE, "channel_forced_silence", "0", TYPE_BOOL, FLAG_CHANNEL_VIEW | FLAG_SS | FLAG_USER_EDITABLE},                 //Available for all channels that are "in view", always up-to-date
    PropertyDescription{CHANNEL_NAME_PHONETIC, "channel_name_phonetic", "", TYPE_STRING, FLAG_CHANNEL_VIEW | FLAG_SS | FLAG_USER_EDITABLE},                  //Available for all channels that are "in view", always up-to-date
    PropertyDescription{CHANNEL_ICON_ID, "channel_icon_id", "0", TYPE_UNSIGNED_NUMBER, FLAG_CHANNEL_VIEW | FLAG_SS | FLAG_USER_EDITABLE},                        //Available for all channels that are "in view", always up-to-date
    PropertyDescription{CHANNEL_FLAG_PRIVATE, "channel_flag_private", "0", TYPE_BOOL, FLAG_CHANNEL_VIEW | FLAG_SS},                   //Available for all channels that are "in view", always up-to-date
    PropertyDescription{CHANNEL_LAST_LEFT, "channel_last_left", "0", TYPE_UNSIGNED_NUMBER, FLAG_SAVE | FLAG_CHANNEL_VIEW | FLAG_CHANNEL_VARIABLE | FLAG_NEW},                   //Available for all channels that are "in view", always up-to-date
    PropertyDescription{CHANNEL_CREATED_AT, "channel_created_at", "0", TYPE_UNSIGNED_NUMBER, FLAG_SS | FLAG_CHANNEL_VIEW | FLAG_CHANNEL_VARIABLE | FLAG_NEW},                   //Available for all channels that are "in view", always up-to-date
    PropertyDescription{CHANNEL_CREATED_BY, "channel_created_by", "0", TYPE_UNSIGNED_NUMBER, FLAG_SS | FLAG_CHANNEL_VIEW | FLAG_CHANNEL_VARIABLE | FLAG_NEW},                   //Available for all channels that are "in view", always up-to-date
    PropertyDescription{CHANNEL_CONVERSATION_HISTORY_LENGTH, "channel_conversation_history_length", "1500", TYPE_SIGNED_NUMBER, FLAG_SS | FLAG_CHANNEL_VIEW | FLAG_CHANNEL_VARIABLE | FLAG_NEW | FLAG_USER_EDITABLE},
    PropertyDescription{CHANNEL_CONVERSATION_MODE, "channel_conversation_mode", "0", TYPE_UNSIGNED_NUMBER, FLAG_SS | FLAG_CHANNEL_VIEW | FLAG_CHANNEL_VARIABLE | FLAG_NEW | FLAG_USER_EDITABLE},
    PropertyDescription{CHANNEL_SIDEBAR_MODE, "channel_sidebar_mode", "0", TYPE_UNSIGNED_NUMBER, FLAG_SS | FLAG_CHANNEL_VIEW | FLAG_CHANNEL_VARIABLE | FLAG_NEW | FLAG_USER_EDITABLE},

    /* group properties, this may gets removed */
    PropertyDescription{GROUP_UNDEFINED, "undefined", "", TYPE_UNKNOWN, 0},
    PropertyDescription{GROUP_ID, "gid", "0", TYPE_UNSIGNED_NUMBER, FLAG_INTERNAL},
    PropertyDescription{GROUP_TYPE, "type", "0", TYPE_UNSIGNED_NUMBER, FLAG_GROUP_VIEW},
    PropertyDescription{GROUP_NAME, "name", "Undefined group", TYPE_STRING, FLAG_GROUP_VIEW},
    PropertyDescription{GROUP_SORTID, "sortid", "0", TYPE_UNSIGNED_NUMBER, FLAG_GROUP_VIEW},
    PropertyDescription{GROUP_SAVEDB, "savedb", "0", TYPE_BOOL, FLAG_GROUP_VIEW},
    PropertyDescription{GROUP_NAMEMODE, "namemode", "0", TYPE_UNSIGNED_NUMBER, FLAG_GROUP_VIEW},
    PropertyDescription{GROUP_ICONID, "iconid", "0", TYPE_UNSIGNED_NUMBER, FLAG_GROUP_VIEW},

    /* client properties */
    PropertyDescription{CLIENT_UNDEFINED, "undefined", "undefined", TYPE_UNKNOWN, 0},
    PropertyDescription{CLIENT_UNIQUE_IDENTIFIER, "client_unique_identifier", "", TYPE_STRING, FLAG_CLIENT_VIEW | FLAG_SNAPSHOT | FLAG_GLOBAL},           //automatically up-to-date for any manager "in view", can be used to identify this particular manager installation
    PropertyDescription{CLIENT_NICKNAME, "client_nickname", "undefined", TYPE_STRING, FLAG_CLIENT_VIEW | FLAG_SAVE_MUSIC | FLAG_SNAPSHOT | FLAG_GLOBAL | FLAG_USER_EDITABLE},                        //automatically up-to-date for any manager "in view"
    PropertyDescription{CLIENT_VERSION, "client_version", "unknown", TYPE_STRING, FLAG_CLIENT_VVSS | FLAG_USER_EDITABLE | FLAG_GLOBAL | FLAG_SAVE_MUSIC},                         //for other clients than ourself, this needs to be requested (=> requestClientVariables)
    PropertyDescription{CLIENT_PLATFORM, "client_platform", "unknown", TYPE_STRING, FLAG_CLIENT_VVSS | FLAG_USER_EDITABLE | FLAG_GLOBAL | FLAG_SAVE_MUSIC},                        //for other clients than ourself, this needs to be requested (=> requestClientVariables)
    PropertyDescription{CLIENT_FLAG_TALKING, "client_flag_talking", "0", TYPE_BOOL, FLAG_INTERNAL},                    //automatically up-to-date for any manager that can be heard (in room / whisper)
    PropertyDescription{CLIENT_INPUT_MUTED, "client_input_muted", "0", TYPE_BOOL, FLAG_CLIENT_VIEW | FLAG_USER_EDITABLE},                     //automatically up-to-date for any manager "in view", this clients microphone mute status
    PropertyDescription{CLIENT_OUTPUT_MUTED, "client_output_muted", "0", TYPE_BOOL, FLAG_CLIENT_VIEW | FLAG_USER_EDITABLE},                    //automatically up-to-date for any manager "in view", this clients headphones/speakers/mic combined mute status
    PropertyDescription{CLIENT_OUTPUTONLY_MUTED, "client_outputonly_muted", "0", TYPE_BOOL, FLAG_CLIENT_VIEW | FLAG_USER_EDITABLE},                //automatically up-to-date for any manager "in view", this clients headphones/speakers only mute status
    PropertyDescription{CLIENT_INPUT_HARDWARE, "client_input_hardware", "0", TYPE_BOOL, FLAG_CLIENT_VIEW | FLAG_USER_EDITABLE},                  //automatically up-to-date for any manager "in view", this clients microphone hardware status (is the capture device opened?)
    PropertyDescription{CLIENT_OUTPUT_HARDWARE, "client_output_hardware", "0", TYPE_BOOL, FLAG_CLIENT_VIEW | FLAG_USER_EDITABLE},                 //automatically up-to-date for any manager "in view", this clients headphone/speakers hardware status (is the playback device opened?)
    PropertyDescription{CLIENT_DEFAULT_CHANNEL, "client_default_channel", "", TYPE_STRING, FLAG_INTERNAL},                 //only usable for ourself, the default channel we used to connect on our last connection attempt
    PropertyDescription{CLIENT_DEFAULT_CHANNEL_PASSWORD, "client_default_channel_password", "", TYPE_STRING, FLAG_INTERNAL},        //internal use
    PropertyDescription{CLIENT_SERVER_PASSWORD, "client_server_password", "", TYPE_STRING, FLAG_INTERNAL},                 //internal use
    PropertyDescription{CLIENT_META_DATA, "client_meta_data", "", TYPE_STRING, FLAG_CLIENT_VIEW| FLAG_GLOBAL|FLAG_USER_EDITABLE},                       //automatically up-to-date for any manager "in view", not used by TeamSpeak, free storage for sdk users
    PropertyDescription{CLIENT_IS_RECORDING, "client_is_recording", "0", TYPE_BOOL, FLAG_CLIENT_VIEW | FLAG_USER_EDITABLE},                    //automatically up-to-date for any manager "in view"
    PropertyDescription{CLIENT_VERSION_SIGN, "client_version_sign", "", TYPE_STRING, FLAG_INTERNAL},                    //sign
    PropertyDescription{CLIENT_SECURITY_HASH, "client_security_hash", "", TYPE_STRING, FLAG_INTERNAL},                   //SDK use, not used by teamspeak. Hash is provided by an outside source. A channel will use the security salt + other manager data to calculate a hash, which must be the same as the one provided here.

    //Rare properties
    PropertyDescription{CLIENT_KEY_OFFSET, "client_key_offset", "0", TYPE_UNSIGNED_NUMBER, FLAG_INTERNAL},                      //internal use
    PropertyDescription{CLIENT_LOGIN_NAME, "client_login_name", "", TYPE_STRING, FLAG_CLIENT_VARIABLE| FLAG_GLOBAL},                      //used for serverquery clients, makes no sense on normal clients currently
    PropertyDescription{CLIENT_LOGIN_PASSWORD, "client_login_password", "", TYPE_STRING, FLAG_INTERNAL| FLAG_GLOBAL},                  //used for serverquery clients, makes no sense on normal clients currently
    PropertyDescription{CLIENT_DATABASE_ID, "client_database_id", "0", TYPE_UNSIGNED_NUMBER, FLAG_CLIENT_VIEW | FLAG_GLOBAL},                     //automatically up-to-date for any manager "in view", only valid with PERMISSION feature, holds database manager id
    PropertyDescription{CLIENT_ID, "clid", "0", TYPE_UNSIGNED_NUMBER, FLAG_CLIENT_VV},                     //clid!
    PropertyDescription{CLIENT_HARDWARE_ID, "hwid", "", TYPE_STRING, FLAG_SAVE},                     //hwid!
    PropertyDescription{CLIENT_CHANNEL_GROUP_ID, "client_channel_group_id", "0", TYPE_UNSIGNED_NUMBER, FLAG_CLIENT_VIEW},                //automatically up-to-date for any manager "in view", only valid with PERMISSION feature, holds database manager id
    PropertyDescription{CLIENT_SERVERGROUPS, "client_servergroups", "0", TYPE_STRING, FLAG_CLIENT_VIEW},                    //automatically up-to-date for any manager "in view", only valid with PERMISSION feature, holds all servergroups manager belongs too
    PropertyDescription{CLIENT_CREATED, "client_created", "0", TYPE_UNSIGNED_NUMBER, FLAG_CLIENT_VARIABLE | FLAG_SAVE_MUSIC | FLAG_SNAPSHOT | FLAG_GLOBAL},                         //this needs to be requested (=> requestClientVariables), first time this manager connected to this server
    PropertyDescription{CLIENT_LASTCONNECTED, "client_lastconnected", "0", TYPE_UNSIGNED_NUMBER, FLAG_CLIENT_VARIABLE | FLAG_SNAPSHOT | FLAG_GLOBAL},                   //this needs to be requested (=> requestClientVariables), last time this manager connected to this server
    PropertyDescription{CLIENT_TOTALCONNECTIONS, "client_totalconnections", "0", TYPE_UNSIGNED_NUMBER, FLAG_CLIENT_VARIABLE | FLAG_CLIENT_VIEW | FLAG_SNAPSHOT | FLAG_GLOBAL},                //this needs to be requested (=> requestClientVariables), how many times this manager connected to this server
    PropertyDescription{CLIENT_AWAY, "client_away", "0", TYPE_BOOL, FLAG_CLIENT_VIEW | FLAG_USER_EDITABLE},                            //automatically up-to-date for any manager "in view", this clients away status
    PropertyDescription{CLIENT_AWAY_MESSAGE, "client_away_message", "", TYPE_STRING, FLAG_CLIENT_VIEW | FLAG_USER_EDITABLE},                    //automatically up-to-date for any manager "in view", this clients away message
    PropertyDescription{CLIENT_TYPE, "client_type", "0", TYPE_UNSIGNED_NUMBER, FLAG_CLIENT_VIEW | FLAG_GLOBAL},                            //automatically up-to-date for any manager "in view", determines if this is a real manager or a server-query connection
    PropertyDescription{CLIENT_TYPE_EXACT, "client_type_exact", "0", TYPE_UNSIGNED_NUMBER, FLAG_CLIENT_VIEW | FLAG_NEW | FLAG_GLOBAL},                            //automatically up-to-date for any manager "in view", determines if this is a real manager or a server-query connection
    PropertyDescription{CLIENT_FLAG_AVATAR, "client_flag_avatar", "", TYPE_STRING, FLAG_CLIENT_VIEW | FLAG_SAVE | FLAG_USER_EDITABLE},                     //automatically up-to-date for any manager "in view", this manager got an avatar
    PropertyDescription{CLIENT_TALK_POWER, "client_talk_power", "0", TYPE_UNSIGNED_NUMBER, FLAG_CLIENT_VIEW},                      //automatically up-to-date for any manager "in view", only valid with PERMISSION feature, holds database manager id
    PropertyDescription{CLIENT_TALK_REQUEST, "client_talk_request", "0", TYPE_BOOL, FLAG_CLIENT_VIEW | FLAG_USER_EDITABLE},                    //automatically up-to-date for any manager "in view", only valid with PERMISSION feature, holds timestamp where manager requested to talk
    PropertyDescription{CLIENT_TALK_REQUEST_MSG, "client_talk_request_msg", "", TYPE_STRING, FLAG_CLIENT_VIEW | FLAG_USER_EDITABLE},                //automatically up-to-date for any manager "in view", only valid with PERMISSION feature, holds matter for the request
    PropertyDescription{CLIENT_DESCRIPTION, "client_description", "", TYPE_STRING, FLAG_CLIENT_VIEW | FLAG_SS | FLAG_USER_EDITABLE},                     //automatically up-to-date for any manager "in view"
    PropertyDescription{CLIENT_IS_TALKER, "client_is_talker", "0", TYPE_BOOL, FLAG_CLIENT_VIEW | FLAG_USER_EDITABLE},                       //automatically up-to-date for any manager "in view"
    PropertyDescription{CLIENT_MONTH_BYTES_UPLOADED, "client_month_bytes_uploaded", "0", TYPE_UNSIGNED_NUMBER, FLAG_CLIENT_VARIABLE | FLAG_SAVE | FLAG_GLOBAL},            //this needs to be requested (=> requestClientVariables)
    PropertyDescription{CLIENT_MONTH_BYTES_DOWNLOADED, "client_month_bytes_downloaded", "0", TYPE_UNSIGNED_NUMBER, FLAG_CLIENT_VARIABLE | FLAG_SAVE | FLAG_GLOBAL},          //this needs to be requested (=> requestClientVariables)
    PropertyDescription{CLIENT_TOTAL_BYTES_UPLOADED, "client_total_bytes_uploaded", "0", TYPE_UNSIGNED_NUMBER, FLAG_CLIENT_VARIABLE | FLAG_SAVE | FLAG_GLOBAL},            //this needs to be requested (=> requestClientVariables)
    PropertyDescription{CLIENT_TOTAL_BYTES_DOWNLOADED, "client_total_bytes_downloaded", "0", TYPE_UNSIGNED_NUMBER, FLAG_CLIENT_VARIABLE | FLAG_SAVE | FLAG_GLOBAL},          //this needs to be requested (=> requestClientVariables)
    PropertyDescription{CLIENT_TOTAL_ONLINE_TIME, "client_total_online_time", "0", TYPE_UNSIGNED_NUMBER, FLAG_CLIENT_VARIABLE | FLAG_SAVE | FLAG_NEW},          //this needs to be requested (=> requestClientVariables)
    PropertyDescription{CLIENT_MONTH_ONLINE_TIME, "client_month_online_time", "0", TYPE_UNSIGNED_NUMBER, FLAG_CLIENT_VARIABLE | FLAG_SAVE | FLAG_NEW},          //this needs to be requested (=> requestClientVariables)
    PropertyDescription{CLIENT_IS_PRIORITY_SPEAKER, "client_is_priority_speaker", "0", TYPE_BOOL, FLAG_CLIENT_VIEW | FLAG_USER_EDITABLE},             //automatically up-to-date for any manager "in view"
    PropertyDescription{CLIENT_UNREAD_MESSAGES, "client_unread_messages", "0", TYPE_UNSIGNED_NUMBER, FLAG_CLIENT_VIEW},                 //automatically up-to-date for any manager "in view"
    PropertyDescription{CLIENT_NICKNAME_PHONETIC, "client_nickname_phonetic", "", TYPE_STRING, FLAG_CLIENT_VIEW | FLAG_USER_EDITABLE},               //automatically up-to-date for any manager "in view"
    PropertyDescription{CLIENT_NEEDED_SERVERQUERY_VIEW_POWER, "client_needed_serverquery_view_power", "0", TYPE_UNSIGNED_NUMBER, FLAG_CLIENT_VIEW},   //automatically up-to-date for any manager "in view"
    PropertyDescription{CLIENT_DEFAULT_TOKEN, "client_default_token", "", TYPE_STRING, FLAG_INTERNAL},                   //only usable for ourself, the default token we used to connect on our last connection attempt
    PropertyDescription{CLIENT_ICON_ID, "client_icon_id", "0", TYPE_UNSIGNED_NUMBER, FLAG_CLIENT_VIEW | FLAG_CLIENT_VARIABLE},                         //automatically up-to-date for any manager "in view"
    PropertyDescription{CLIENT_IS_CHANNEL_COMMANDER, "client_is_channel_commander", "0", TYPE_BOOL, FLAG_CLIENT_VIEW | FLAG_USER_EDITABLE | FLAG_SAVE_MUSIC},            //automatically up-to-date for any manager "in view"
    PropertyDescription{CLIENT_COUNTRY, "client_country", "TS", TYPE_STRING, FLAG_CLIENT_VIEW | FLAG_CLIENT_VARIABLE | FLAG_GLOBAL | FLAG_SAVE_MUSIC | FLAG_USER_EDITABLE},                         //automatically up-to-date for any manager "in view"
    PropertyDescription{CLIENT_CHANNEL_GROUP_INHERITED_CHANNEL_ID, "client_channel_group_inherited_channel_id", "0", TYPE_UNSIGNED_NUMBER, FLAG_CLIENT_VIEW}, //automatically up-to-date for any manager "in view", only valid with PERMISSION feature, contains channel_id where the channel_group_id is set from
    PropertyDescription{CLIENT_BADGES, "client_badges", "", TYPE_STRING, FLAG_CLIENT_VIEW | FLAG_USER_EDITABLE},                          //automatically up-to-date for any manager "in view", stores icons for partner badges
    PropertyDescription{CLIENT_MYTEAMSPEAK_ID, "client_myteamspeak_id", "", TYPE_STRING, FLAG_CLIENT_VIEW | FLAG_SS | FLAG_USER_EDITABLE},                          //automatically up-to-date for any manager "in view", stores icons for partner badges
    PropertyDescription{CLIENT_INTEGRATIONS, "client_integrations", "", TYPE_STRING, FLAG_CLIENT_VIEW | FLAG_USER_EDITABLE},                          //automatically up-to-date for any manager "in view", stores icons for partner badges
    PropertyDescription{CLIENT_ACTIVE_INTEGRATIONS_INFO, "client_active_integrations_info", "", TYPE_STRING, FLAG_INTERNAL | FLAG_USER_EDITABLE},

    //Using FLAG_GLOBAL here,lse they will be overridden on clientinit
    PropertyDescription{CLIENT_TEAFORO_ID, "client_teaforo_id", "0", TYPE_UNSIGNED_NUMBER, FLAG_NEW | FLAG_CLIENT_VIEW | FLAG_GLOBAL},
    PropertyDescription{CLIENT_TEAFORO_NAME, "client_teaforo_name", "", TYPE_STRING, FLAG_NEW | FLAG_CLIENT_VIEW | FLAG_GLOBAL},
    PropertyDescription{CLIENT_TEAFORO_FLAGS, "client_teaforo_flags", "0", TYPE_UNSIGNED_NUMBER, FLAG_NEW | FLAG_CLIENT_VIEW | FLAG_GLOBAL},

    //Music bot stuff
    PropertyDescription{CLIENT_OWNER, "client_owner", "0", TYPE_UNSIGNED_NUMBER, FLAG_NEW | FLAG_CLIENT_VIEW},
    PropertyDescription{CLIENT_BOT_TYPE, "client_bot_type", "0", TYPE_UNSIGNED_NUMBER, FLAG_NEW | FLAG_SAVE_MUSIC | FLAG_USER_EDITABLE | FLAG_CLIENT_VIEW},
    PropertyDescription{CLIENT_LAST_CHANNEL, "client_last_channel", "0", TYPE_UNSIGNED_NUMBER, FLAG_NEW | FLAG_INTERNAL | FLAG_SAVE_MUSIC},
    PropertyDescription{CLIENT_PLAYER_STATE, "player_state", "0", TYPE_UNSIGNED_NUMBER, FLAG_NEW | FLAG_CLIENT_VIEW | FLAG_SAVE_MUSIC},
    PropertyDescription{CLIENT_PLAYER_VOLUME, "player_volume", "1", TYPE_FLOAT, FLAG_NEW | FLAG_SAVE_MUSIC | FLAG_CLIENT_VIEW | FLAG_USER_EDITABLE},
    PropertyDescription{CLIENT_PLAYLIST_ID, "client_playlist_id", "0", TYPE_UNSIGNED_NUMBER, FLAG_NEW | FLAG_CLIENT_VARIABLE | FLAG_SAVE_MUSIC},
    PropertyDescription{CLIENT_DISABLED, "client_disabled", "0", TYPE_BOOL, FLAG_NEW | FLAG_CLIENT_VARIABLE},
    PropertyDescription{CLIENT_UPTIME_MODE, "client_uptime_mode", "0", TYPE_UNSIGNED_NUMBER, FLAG_NEW | FLAG_CLIENT_VARIABLE | FLAG_USER_EDITABLE | FLAG_SAVE_MUSIC},
    PropertyDescription{CLIENT_FLAG_NOTIFY_SONG_CHANGE, "client_flag_notify_song_change", "1", TYPE_BOOL, FLAG_NEW | FLAG_CLIENT_VARIABLE | FLAG_USER_EDITABLE | FLAG_SAVE_MUSIC},

    /* connection list properties */
    PropertyDescription{CONNECTION_UNDEFINED, "undefined", "", TYPE_UNKNOWN, 0},
    PropertyDescription{CONNECTION_PING, "connection_ping", "0", TYPE_UNSIGNED_NUMBER, 0},                                        //average latency for a round trip through and back this connection
    PropertyDescription{CONNECTION_PING_DEVIATION, "connection_ping_deviation", "0", TYPE_UNSIGNED_NUMBER, 0},                                  //standard deviation of the above average latency
    PropertyDescription{CONNECTION_CONNECTED_TIME, "connection_connected_time", "0", TYPE_UNSIGNED_NUMBER, 0},                                  //how long the connection exists already
    PropertyDescription{CONNECTION_IDLE_TIME, "connection_idle_time", "0", TYPE_UNSIGNED_NUMBER, 0},                                       //how long since the last action of this manager
    PropertyDescription{CONNECTION_CLIENT_IP, "connection_client_ip", "", TYPE_STRING, FLAG_SAVE},                                      //NEED DB SAVE! //IP of this manager (as seen from the server side)
    PropertyDescription{CONNECTION_CLIENT_PORT, "connection_client_port", "0", TYPE_UNSIGNED_NUMBER, 0},                                     //Port of this manager (as seen from the server side)
    PropertyDescription{CONNECTION_SERVER_IP, "connection_server_ip", "", TYPE_STRING, 0},                                       //IP of the server (seen from the manager side) - only available on yourself, not for remote clients, not available server side
    PropertyDescription{CONNECTION_SERVER_PORT, "connection_server_port", "0", TYPE_UNSIGNED_NUMBER, 0},                                     //Port of the server (seen from the manager side) - only available on yourself, not for remote clients, not available server side

    PropertyDescription{V(CONNECTION_PACKETS_SENT_SPEECH, "connection_packets_sent_speech", 0)},                             //how many Speech packets were sent through this connection
    PropertyDescription{V(CONNECTION_PACKETS_SENT_KEEPALIVE, "connection_packets_sent_keepalive", 0)},
    PropertyDescription{V(CONNECTION_PACKETS_SENT_CONTROL, "connection_packets_sent_control", 0)},
    PropertyDescription{V(CONNECTION_PACKETS_SENT_TOTAL, "connection_packets_sent_total", FLAG_CLIENT_INFO)},                              //how many packets were sent totally (this is PACKETS_SENT_SPEECH + PACKETS_SENT_KEEPALIVE + PACKETS_SENT_CONTROL)
    PropertyDescription{V(CONNECTION_BYTES_SENT_SPEECH, "connection_bytes_sent_speech", 0)},
    PropertyDescription{V(CONNECTION_BYTES_SENT_KEEPALIVE, "connection_bytes_sent_keepalive", 0)},
    PropertyDescription{V(CONNECTION_BYTES_SENT_CONTROL, "connection_bytes_sent_control", 0)},
    PropertyDescription{V(CONNECTION_BYTES_SENT_TOTAL, "connection_bytes_sent_total", FLAG_CLIENT_INFO)},
    PropertyDescription{V(CONNECTION_PACKETS_RECEIVED_SPEECH, "connection_packets_received_speech", 0)},
    PropertyDescription{V(CONNECTION_PACKETS_RECEIVED_KEEPALIVE, "connection_packets_received_keepalive", 0)},
    PropertyDescription{V(CONNECTION_PACKETS_RECEIVED_CONTROL, "connection_packets_received_control", 0)},
    PropertyDescription{V(CONNECTION_PACKETS_RECEIVED_TOTAL, "connection_packets_received_total", FLAG_CLIENT_INFO)},
    PropertyDescription{V(CONNECTION_BYTES_RECEIVED_SPEECH, "connection_bytes_received_speech", 0)},
    PropertyDescription{V(CONNECTION_BYTES_RECEIVED_KEEPALIVE, "connection_bytes_received_keepalive", 0)},
    PropertyDescription{V(CONNECTION_BYTES_RECEIVED_CONTROL, "connection_bytes_received_control", 0)},
    PropertyDescription{V(CONNECTION_BYTES_RECEIVED_TOTAL, "connection_bytes_received_total", FLAG_CLIENT_INFO)},
    PropertyDescription{F(CONNECTION_PACKETLOSS_SPEECH, "connection_packetloss_speech", 0)},
    PropertyDescription{F(CONNECTION_PACKETLOSS_KEEPALIVE, "connection_packetloss_keepalive", 0)},
    PropertyDescription{F(CONNECTION_PACKETLOSS_CONTROL, "connection_packetloss_control", 0)},
    PropertyDescription{F(CONNECTION_PACKETLOSS_TOTAL, "connection_packetloss_total", FLAG_CLIENT_INFO)},                                //the probability with which a packet round trip failed because a packet was lost
    PropertyDescription{F(CONNECTION_SERVER2CLIENT_PACKETLOSS_SPEECH, "connection_server2client_packetloss_speech", 0)},                 //the probability with which a speech packet failed from the server to the manager
    PropertyDescription{F(CONNECTION_SERVER2CLIENT_PACKETLOSS_KEEPALIVE, "connection_server2client_packetloss_keepalive", 0)},
    PropertyDescription{F(CONNECTION_SERVER2CLIENT_PACKETLOSS_CONTROL, "connection_server2client_packetloss_control", 0)},
    PropertyDescription{F(CONNECTION_SERVER2CLIENT_PACKETLOSS_TOTAL, "connection_server2client_packetloss_total", FLAG_CLIENT_INFO)},
    PropertyDescription{F(CONNECTION_CLIENT2SERVER_PACKETLOSS_SPEECH, "connection_client2server_packetloss_speech", 0)},
    PropertyDescription{F(CONNECTION_CLIENT2SERVER_PACKETLOSS_KEEPALIVE, "connection_client2server_packetloss_keepalive", 0)},
    PropertyDescription{F(CONNECTION_CLIENT2SERVER_PACKETLOSS_CONTROL, "connection_client2server_packetloss_control", 0)},
    PropertyDescription{F(CONNECTION_CLIENT2SERVER_PACKETLOSS_TOTAL, "connection_client2server_packetloss_total", FLAG_CLIENT_INFO)},
    PropertyDescription{V(CONNECTION_BANDWIDTH_SENT_LAST_SECOND_SPEECH, "connection_bandwidth_sent_last_second_speech", 0)},               //howmany bytes of speech packets we sent during the last second
    PropertyDescription{V(CONNECTION_BANDWIDTH_SENT_LAST_SECOND_KEEPALIVE, "connection_bandwidth_sent_last_second_keepalive", 0)},
    PropertyDescription{V(CONNECTION_BANDWIDTH_SENT_LAST_SECOND_CONTROL, "connection_bandwidth_sent_last_second_control", 0)},
    PropertyDescription{V(CONNECTION_BANDWIDTH_SENT_LAST_SECOND_TOTAL, "connection_bandwidth_sent_last_second_total", FLAG_CLIENT_INFO)},
    PropertyDescription{V(CONNECTION_BANDWIDTH_SENT_LAST_MINUTE_SPEECH, "connection_bandwidth_sent_last_minute_speech", 0)},               //howmany bytes/s of speech packets we sent in average during the last minute
    PropertyDescription{V(CONNECTION_BANDWIDTH_SENT_LAST_MINUTE_KEEPALIVE, "connection_bandwidth_sent_last_minute_keepalive", 0)},
    PropertyDescription{V(CONNECTION_BANDWIDTH_SENT_LAST_MINUTE_CONTROL, "connection_bandwidth_sent_last_minute_control", 0)},
    PropertyDescription{V(CONNECTION_BANDWIDTH_SENT_LAST_MINUTE_TOTAL, "connection_bandwidth_sent_last_minute_total", FLAG_CLIENT_INFO)},
    PropertyDescription{V(CONNECTION_BANDWIDTH_RECEIVED_LAST_SECOND_SPEECH, "connection_bandwidth_received_last_second_speech", 0)},
    PropertyDescription{V(CONNECTION_BANDWIDTH_RECEIVED_LAST_SECOND_KEEPALIVE, "connection_bandwidth_received_last_second_keepalive", 0)},
    PropertyDescription{V(CONNECTION_BANDWIDTH_RECEIVED_LAST_SECOND_CONTROL, "connection_bandwidth_received_last_second_control", 0)},
    PropertyDescription{V(CONNECTION_BANDWIDTH_RECEIVED_LAST_SECOND_TOTAL, "connection_bandwidth_received_last_second_total", FLAG_CLIENT_INFO)},
    PropertyDescription{V(CONNECTION_BANDWIDTH_RECEIVED_LAST_MINUTE_SPEECH, "connection_bandwidth_received_last_minute_speech", 0)},
    PropertyDescription{V(CONNECTION_BANDWIDTH_RECEIVED_LAST_MINUTE_KEEPALIVE, "connection_bandwidth_received_last_minute_keepalive", 0)},
    PropertyDescription{V(CONNECTION_BANDWIDTH_RECEIVED_LAST_MINUTE_CONTROL, "connection_bandwidth_received_last_minute_control", 0)},
    PropertyDescription{V(CONNECTION_BANDWIDTH_RECEIVED_LAST_MINUTE_TOTAL, "connection_bandwidth_received_last_minute_total", FLAG_CLIENT_INFO)},

    //Rare properties
    PropertyDescription{V(CONNECTION_FILETRANSFER_BANDWIDTH_SENT, "connection_filetransfer_bandwidth_sent", FLAG_CLIENT_INFO)},                     //how many bytes per second are currently being sent by file transfers
    PropertyDescription{V(CONNECTION_FILETRANSFER_BANDWIDTH_RECEIVED, "connection_filetransfer_bandwidth_received", FLAG_CLIENT_INFO)},                 //how many bytes per second are currently being received by file transfers
    PropertyDescription{V(CONNECTION_FILETRANSFER_BYTES_RECEIVED_TOTAL, "connection_filetransfer_bytes_received_total", FLAG_CLIENT_INFO)},               //how many bytes we received in total through file transfers
    PropertyDescription{V(CONNECTION_FILETRANSFER_BYTES_SENT_TOTAL, "connection_filetransfer_bytes_sent_total", FLAG_CLIENT_INFO)},                   //how many bytes we sent in total through file transfers

    /* server instance properties */
    PropertyDescription{SERVERINSTANCE_UNDEFINED, "undefined", "", TYPE_UNKNOWN, 0},
    PropertyDescription{SERVERINSTANCE_DATABASE_VERSION, "serverinstance_database_version", "0", TYPE_UNSIGNED_NUMBER, FLAG_INSTANCE_VARIABLE},
    PropertyDescription{SERVERINSTANCE_PERMISSIONS_VERSION, "serverinstance_permissions_version", "0", TYPE_UNSIGNED_NUMBER, FLAG_INSTANCE_VARIABLE},
    PropertyDescription{SERVERINSTANCE_FILETRANSFER_HOST, "serverinstance_filetransfer_host", "0.0.0.0,[::]", TYPE_STRING, FLAG_INSTANCE_VARIABLE | FLAG_SAVE},
    PropertyDescription{SERVERINSTANCE_FILETRANSFER_PORT, "serverinstance_filetransfer_port", "30303", TYPE_UNSIGNED_NUMBER, FLAG_INSTANCE_VARIABLE | FLAG_SAVE},
    PropertyDescription{SERVERINSTANCE_FILETRANSFER_MAX_CONNECTIONS, "serverinstance_filetransfer_max_connections", "100", TYPE_UNSIGNED_NUMBER, FLAG_INSTANCE_VARIABLE | FLAG_SAVE},
    PropertyDescription{SERVERINSTANCE_FILETRANSFER_MAX_CONNECTIONS_PER_IP, "serverinstance_filetransfer_max_connections_per_ip", "20", TYPE_UNSIGNED_NUMBER, FLAG_INSTANCE_VARIABLE | FLAG_SAVE},
    PropertyDescription{SERVERINSTANCE_QUERY_HOST, "serverinstance_query_host", "0.0.0.0,[::]", TYPE_STRING, FLAG_INSTANCE_VARIABLE | FLAG_SAVE},
    PropertyDescription{SERVERINSTANCE_QUERY_PORT, "serverinstance_query_port", "10101", TYPE_UNSIGNED_NUMBER, FLAG_INSTANCE_VARIABLE | FLAG_SAVE},
    PropertyDescription{SERVERINSTANCE_QUERY_MAX_CONNECTIONS, "serverinstance_query_max_connections", "100", TYPE_UNSIGNED_NUMBER, FLAG_INSTANCE_VARIABLE | FLAG_SAVE},
    PropertyDescription{SERVERINSTANCE_QUERY_MAX_CONNECTIONS_PER_IP, "serverinstance_query_max_connections_per_ip", "3", TYPE_UNSIGNED_NUMBER, FLAG_INSTANCE_VARIABLE | FLAG_SAVE},
    PropertyDescription{SERVERINSTANCE_MONTHLY_TIMESTAMP, "serverinstance_monthly_timestamp", "0", TYPE_UNSIGNED_NUMBER, FLAG_INSTANCE_VARIABLE | FLAG_SAVE},
    PropertyDescription{SERVERINSTANCE_MAX_DOWNLOAD_TOTAL_BANDWIDTH, "serverinstance_max_download_total_bandwidth", "-1", TYPE_SIGNED_NUMBER, FLAG_INSTANCE_VARIABLE | FLAG_SAVE},
    PropertyDescription{SERVERINSTANCE_MAX_UPLOAD_TOTAL_BANDWIDTH, "serverinstance_max_upload_total_bandwidth", "-1", TYPE_SIGNED_NUMBER, FLAG_INSTANCE_VARIABLE | FLAG_SAVE},
    PropertyDescription{SERVERINSTANCE_SERVERQUERY_FLOOD_COMMANDS, "serverinstance_serverquery_flood_commands", "3", TYPE_UNSIGNED_NUMBER, FLAG_INSTANCE_VARIABLE | FLAG_SAVE},       //how many commands we can issue while in the SERVERINSTANCE_SERVERQUERY_FLOOD_TIME window
    PropertyDescription{SERVERINSTANCE_SERVERQUERY_FLOOD_TIME, "serverinstance_serverquery_flood_time", "1", TYPE_UNSIGNED_NUMBER, FLAG_INSTANCE_VARIABLE | FLAG_SAVE},           //time window in seconds for max command execution check
    PropertyDescription{SERVERINSTANCE_SERVERQUERY_BAN_TIME, "serverinstance_serverquery_ban_time", "600", TYPE_UNSIGNED_NUMBER, FLAG_INSTANCE_VARIABLE | FLAG_SAVE},             //how many seconds someone get banned if he floods
    PropertyDescription{SERVERINSTANCE_TEMPLATE_SERVERADMIN_GROUP, "serverinstance_template_serveradmin_group", "0", TYPE_UNSIGNED_NUMBER, FLAG_INSTANCE_VARIABLE | FLAG_SAVE},
    PropertyDescription{SERVERINSTANCE_TEMPLATE_SERVERDEFAULT_GROUP, "serverinstance_template_serverdefault_group", "0", TYPE_UNSIGNED_NUMBER, FLAG_INSTANCE_VARIABLE | FLAG_SAVE},
    PropertyDescription{SERVERINSTANCE_TEMPLATE_CHANNELADMIN_GROUP, "serverinstance_template_channeladmin_group", "0", TYPE_UNSIGNED_NUMBER, FLAG_INSTANCE_VARIABLE | FLAG_SAVE},
    PropertyDescription{SERVERINSTANCE_TEMPLATE_CHANNELDEFAULT_GROUP, "serverinstance_template_channeldefault_group", "0", TYPE_UNSIGNED_NUMBER, FLAG_INSTANCE_VARIABLE | FLAG_SAVE},
    PropertyDescription{SERVERINSTANCE_TEMPLATE_MUSICDEFAULT_GROUP, "serverinstance_template_musicdefault_group", "0", TYPE_UNSIGNED_NUMBER, FLAG_INSTANCE_VARIABLE | FLAG_SAVE},
    PropertyDescription{SERVERINSTANCE_GUEST_SERVERQUERY_GROUP, "serverinstance_guest_serverquery_group", "0", TYPE_UNSIGNED_NUMBER, FLAG_INSTANCE_VARIABLE | FLAG_SAVE},
    PropertyDescription{SERVERINSTANCE_ADMIN_SERVERQUERY_GROUP, "serverinstance_admin_serverquery_group", "0", TYPE_UNSIGNED_NUMBER, FLAG_INSTANCE_VARIABLE | FLAG_SAVE},
    PropertyDescription{SERVERINSTANCE_PENDING_CONNECTIONS_PER_IP, "serverinstance_pending_connections_per_ip", "0", TYPE_UNSIGNED_NUMBER, FLAG_INSTANCE_VARIABLE | FLAG_SAVE},
    PropertyDescription{SERVERINSTANCE_SPOKEN_TIME_TOTAL, "serverinstance_spoken_time_total", "0", TYPE_UNSIGNED_NUMBER, FLAG_INTERNAL | FLAG_SAVE | FLAG_NEW},
    PropertyDescription{SERVERINSTANCE_SPOKEN_TIME_DELETED, "serverinstance_spoken_time_deleted", "0", TYPE_UNSIGNED_NUMBER, FLAG_INTERNAL | FLAG_SAVE | FLAG_NEW},
    PropertyDescription{SERVERINSTANCE_SPOKEN_TIME_ALIVE, "serverinstance_spoken_time_alive", "0", TYPE_UNSIGNED_NUMBER, FLAG_INTERNAL | FLAG_SAVE | FLAG_NEW},
    PropertyDescription{SERVERINSTANCE_SPOKEN_TIME_VARIANZ, "serverinstance_spoken_time_varianz", "0", TYPE_UNSIGNED_NUMBER, FLAG_INTERNAL | FLAG_SAVE | FLAG_NEW},
    PropertyDescription{SERVERINSTANCE_VIRTUAL_SERVER_ID_INDEX, "serverinstance_virtual_server_id_index", "1", TYPE_UNSIGNED_NUMBER, FLAG_SAVE | FLAG_INSTANCE_VARIABLE | FLAG_NEW},
    PropertyDescription{SERVERINSTANCE_UNIQUE_ID, "serverinstance_unique_id", "", TYPE_STRING, FLAG_INTERNAL | FLAG_SAVE | FLAG_NEW},

    /* playlist properties */
    PropertyDescription{PLAYLIST_UNDEFINED, "undefined", "", TYPE_UNKNOWN, 0},
    PropertyDescription{PLAYLIST_ID, "playlist_id", "0", TYPE_UNSIGNED_NUMBER, FLAG_PLAYLIST_VARIABLE},
    PropertyDescription{PLAYLIST_TITLE, "playlist_title", "Yet another playlist", TYPE_STRING, FLAG_PLAYLIST_VARIABLE | FLAG_USER_EDITABLE | FLAG_SAVE},
    PropertyDescription{PLAYLIST_DESCRIPTION, "playlist_description", "", TYPE_STRING, FLAG_PLAYLIST_VARIABLE | FLAG_USER_EDITABLE | FLAG_SAVE},
    PropertyDescription{PLAYLIST_TYPE, "playlist_type", "0", TYPE_UNSIGNED_NUMBER, FLAG_PLAYLIST_VARIABLE | FLAG_SAVE},
    PropertyDescription{PLAYLIST_OWNER_DBID, "playlist_owner_dbid", "0", TYPE_UNSIGNED_NUMBER, FLAG_PLAYLIST_VARIABLE | FLAG_SAVE},
    PropertyDescription{PLAYLIST_OWNER_NAME, "playlist_owner_name", "0", TYPE_STRING, FLAG_PLAYLIST_VARIABLE | FLAG_SAVE},
    PropertyDescription{PLAYLIST_MAX_SONGS, "playlist_max_songs", "-1", TYPE_SIGNED_NUMBER, FLAG_PLAYLIST_VARIABLE | FLAG_USER_EDITABLE | FLAG_SAVE},
    PropertyDescription{PLAYLIST_FLAG_DELETE_PLAYED, "playlist_flag_delete_played", "1", TYPE_BOOL, FLAG_PLAYLIST_VARIABLE | FLAG_USER_EDITABLE | FLAG_SAVE},
    PropertyDescription{PLAYLIST_FLAG_FINISHED, "playlist_flag_finished", "0", TYPE_BOOL, FLAG_PLAYLIST_VARIABLE | FLAG_USER_EDITABLE | FLAG_SAVE},
    PropertyDescription{PLAYLIST_REPLAY_MODE, "playlist_replay_mode", "0", TYPE_UNSIGNED_NUMBER, FLAG_PLAYLIST_VARIABLE | FLAG_USER_EDITABLE | FLAG_SAVE},
    PropertyDescription{PLAYLIST_CURRENT_SONG_ID, "playlist_current_song_id", "0", TYPE_UNSIGNED_NUMBER, FLAG_PLAYLIST_VARIABLE | FLAG_SAVE}
};

#undef str_
#undef V
#undef F

#undef FLAG_SS
#undef FLAG_SERVER_VV
#undef FLAG_SERVER_VVSS
#undef FLAG_CLIENT_VV
#undef FLAG_CLIENT_VVSS