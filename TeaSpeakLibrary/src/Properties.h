#pragma once

#include <utility>
#include "misc/memtracker.h"
#include <ThreadPool/Mutex.h>
#include "Variable.h"
#include <map>
#include <deque>
#include <vector>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>
#include <functional>
#include <any>
#include <array>

#include "misc/spin_lock.h"
#include "converters/converter.h"

#ifdef NDEBUG
    #define EXTERNALIZE_PROPERTY_DEFINITIONS
#endif

#define PROPERTIES_DEFINED

namespace ts {
    namespace property {
        enum PropertyType {
            PROP_TYPE_SERVER = 0,
            PROP_TYPE_CHANNEL = 1,
            PROP_TYPE_GROUP = 2,
            PROP_TYPE_CLIENT = 3,
            PROP_TYPE_INSTANCE = 4,
            PROP_TYPE_CONNECTION = 5,
            PROP_TYPE_PLAYLIST = 6,
            PROP_TYPE_UNKNOWN = 7,
            PROP_TYPE_MAX
        };

        static constexpr const char *PropertyType_Names[7] = {
                "SERVER",
                "CHANNEL",
                "GROUP",
                "CLIENT",
                "INSTANCE",
                "CONNECTION",
                "UNKNOWN"
        };

        enum ValueType {
            TYPE_UNKNOWN,
            TYPE_STRING,
            TYPE_BOOL,
            TYPE_SIGNED_NUMBER,
            TYPE_UNSIGNED_NUMBER,
            TYPE_FLOAT
        };

        typedef uint32_t flag_type;
        enum flag : flag_type {
            FLAG_BEGIN = 0b1,
            FLAG_INTERNAL = FLAG_BEGIN << 1UL, //Just for internal usage
            FLAG_GLOBAL = FLAG_INTERNAL << 1UL, //Not server bound
            FLAG_SNAPSHOT = FLAG_GLOBAL << 1UL, //Saved within snapshots
            FLAG_SAVE = FLAG_SNAPSHOT << 1UL, //Saved to database
            FLAG_SAVE_MUSIC = FLAG_SAVE << 1UL, //Saved to database
            FLAG_NEW = FLAG_SAVE_MUSIC << 1UL, //Its a non TeamSpeak property
            FLAG_SERVER_VARIABLE = FLAG_NEW << 1UL,
            FLAG_SERVER_VIEW = FLAG_SERVER_VARIABLE << 1UL,
            FLAG_CLIENT_VARIABLE = FLAG_SERVER_VIEW << 1UL,
            FLAG_CLIENT_VIEW = FLAG_CLIENT_VARIABLE << 1UL,
            FLAG_CLIENT_INFO = FLAG_CLIENT_VIEW << 1UL,
            FLAG_CHANNEL_VARIABLE = FLAG_CLIENT_INFO << 1UL,
            FLAG_CHANNEL_VIEW = FLAG_CHANNEL_VARIABLE << 1UL,
            FLAG_GROUP_VIEW = FLAG_CHANNEL_VIEW << 1UL,
            FLAG_INSTANCE_VARIABLE = FLAG_GROUP_VIEW << 1UL,
            FLAG_USER_EDITABLE = FLAG_INSTANCE_VARIABLE << 1UL,
            FLAG_PLAYLIST_VARIABLE = FLAG_USER_EDITABLE << 1UL,
        };
        static constexpr const char *flag_names[sizeof(flag) * 8] =
                {"UNDEFINED", "FLAG_INTERNAL", "FLAG_GLOBAL", "FLAG_SNAPSHOT", "FLAG_SAVE", "FLAG_SAVE_MUSIC", "FLAG_NEW",
                 "FLAG_SERVER_VARIABLE", "FLAG_SERVER_VIEW", "FLAG_CLIENT_VARIABLE", "FLAG_CLIENT_VIEW", "FLAG_CLIENT_INFO",
                 "FLAG_CHANNEL_VARIABLE", "FLAG_CHANNEL_VIEW", "FLAG_GROUP_VIEW", "FLAG_INSTANCE_VARIABLE", "FLAG_USER_EDITABLE","FLAG_PLAYLIST_VARIABLE"};

        enum UnknownProperties {
            UNKNOWN_UNDEFINED,
            UNKNOWN_BEGINMARKER,
            UNKNOWN_ENDMARKER = UNKNOWN_BEGINMARKER
        };

        enum InstanceProperties {
            SERVERINSTANCE_UNDEFINED,
            SERVERINSTANCE_BEGINMARKER,
            SERVERINSTANCE_DATABASE_VERSION = SERVERINSTANCE_BEGINMARKER,
            SERVERINSTANCE_PERMISSIONS_VERSION,

            SERVERINSTANCE_FILETRANSFER_HOST,
            SERVERINSTANCE_FILETRANSFER_PORT,
            SERVERINSTANCE_FILETRANSFER_MAX_CONNECTIONS,
            SERVERINSTANCE_FILETRANSFER_MAX_CONNECTIONS_PER_IP,

            SERVERINSTANCE_QUERY_HOST,
            SERVERINSTANCE_QUERY_PORT,
            SERVERINSTANCE_QUERY_MAX_CONNECTIONS,
            SERVERINSTANCE_QUERY_MAX_CONNECTIONS_PER_IP,

            SERVERINSTANCE_MONTHLY_TIMESTAMP,
            SERVERINSTANCE_MAX_DOWNLOAD_TOTAL_BANDWIDTH,
            SERVERINSTANCE_MAX_UPLOAD_TOTAL_BANDWIDTH,
            SERVERINSTANCE_SERVERQUERY_FLOOD_COMMANDS,       //how many commands we can issue while in the SERVERINSTANCE_SERVERQUERY_FLOOD_TIME window
            SERVERINSTANCE_SERVERQUERY_FLOOD_TIME,           //time window in seconds for max command execution check
            SERVERINSTANCE_SERVERQUERY_BAN_TIME,             //how many seconds someone get banned if he floods
            SERVERINSTANCE_TEMPLATE_SERVERADMIN_GROUP,
            SERVERINSTANCE_TEMPLATE_SERVERDEFAULT_GROUP,
            SERVERINSTANCE_TEMPLATE_CHANNELADMIN_GROUP,
            SERVERINSTANCE_TEMPLATE_CHANNELDEFAULT_GROUP,
            SERVERINSTANCE_TEMPLATE_MUSICDEFAULT_GROUP,
            SERVERINSTANCE_GUEST_SERVERQUERY_GROUP,
            SERVERINSTANCE_ADMIN_SERVERQUERY_GROUP,
            SERVERINSTANCE_PENDING_CONNECTIONS_PER_IP,
            SERVERINSTANCE_SPOKEN_TIME_TOTAL,
            SERVERINSTANCE_SPOKEN_TIME_DELETED,
            SERVERINSTANCE_SPOKEN_TIME_ALIVE,
            SERVERINSTANCE_SPOKEN_TIME_VARIANZ,

            SERVERINSTANCE_VIRTUAL_SERVER_ID_INDEX,

            SERVERINSTANCE_UNIQUE_ID,
            SERVERINSTANCE_ENDMARKER,
        };

        enum VirtualServerProperties {
            VIRTUALSERVER_UNDEFINED = 0,
            VIRTUALSERVER_BEGINMARKER,
            VIRTUALSERVER_UNIQUE_IDENTIFIER = VIRTUALSERVER_BEGINMARKER,             //available when connected, can be used to identify this particular server installation
            VIRTUALSERVER_NAME,                              //available and always up-to-date when connected
            VIRTUALSERVER_WELCOMEMESSAGE,                    //available when connected,  (=> requestServerVariables)
            VIRTUALSERVER_PLATFORM,                          //available when connected
            VIRTUALSERVER_VERSION,                           //available when connected
            VIRTUALSERVER_MAXCLIENTS,                        //only available on request (=> requestServerVariables), stores the maximum number of clients that may currently join the server
            VIRTUALSERVER_PASSWORD,                          //not available to clients, the server password
            VIRTUALSERVER_CLIENTS_ONLINE,                    //only available on request (=> requestServerVariables),
            VIRTUALSERVER_QUERYCLIENTS_ONLINE,                         //only available on request (=> requestServerVariables)
            VIRTUALSERVER_CHANNELS_ONLINE,                   //only available on request (=> requestServerVariables),
            VIRTUALSERVER_CREATED,                           //available when connected, stores the time when the server was created
            VIRTUALSERVER_UPTIME,                            //only available on request (=> requestServerVariables), the time since the server was started

            VIRTUALSERVER_CODEC_ENCRYPTION_MODE,             //available and always up-to-date when connected
            VIRTUALSERVER_KEYPAIR,                                     //internal use
            VIRTUALSERVER_HOSTMESSAGE,                                 //available when connected, not updated while connected
            VIRTUALSERVER_HOSTMESSAGE_MODE,                            //available when connected, not updated while connected
            VIRTUALSERVER_FILEBASE,                                    //not available to clients, stores the folder used for file transfers
            VIRTUALSERVER_DEFAULT_SERVER_GROUP,                        //the client permissions server group that a new client gets assigned
            VIRTUALSERVER_DEFAULT_MUSIC_GROUP,                         //the client permissions server group that a new client gets assigned
            VIRTUALSERVER_DEFAULT_CHANNEL_GROUP,                       //the channel permissions group that a new client gets assigned when joining a channel
            VIRTUALSERVER_FLAG_PASSWORD,                               //only available on request (=> requestServerVariables)
            VIRTUALSERVER_DEFAULT_CHANNEL_ADMIN_GROUP,                 //the channel permissions group that a client gets assigned when creating a channel
            VIRTUALSERVER_MAX_DOWNLOAD_TOTAL_BANDWIDTH,                //only available on request (=> requestServerVariables)
            VIRTUALSERVER_MAX_UPLOAD_TOTAL_BANDWIDTH,                  //only available on request (=> requestServerVariables)
            VIRTUALSERVER_HOSTBANNER_URL,                              //available when connected, always up-to-date
            VIRTUALSERVER_HOSTBANNER_GFX_URL,                          //available when connected, always up-to-date
            VIRTUALSERVER_HOSTBANNER_GFX_INTERVAL,                     //available when connected, always up-to-date
            VIRTUALSERVER_HOSTBANNER_MODE,                             //available when connected, always up-to-date
            VIRTUALSERVER_COMPLAIN_AUTOBAN_COUNT,                      //only available on request (=> requestServerVariables)
            VIRTUALSERVER_COMPLAIN_AUTOBAN_TIME,                       //only available on request (=> requestServerVariables)
            VIRTUALSERVER_COMPLAIN_REMOVE_TIME,                        //only available on request (=> requestServerVariables)
            VIRTUALSERVER_MIN_CLIENTS_IN_CHANNEL_BEFORE_FORCED_SILENCE,//only available on request (=> requestServerVariables)
            VIRTUALSERVER_PRIORITY_SPEAKER_DIMM_MODIFICATOR,           //available when connected, always up-to-date
            VIRTUALSERVER_ID,                                          //available when connected
            VIRTUALSERVER_ANTIFLOOD_POINTS_TICK_REDUCE,                //only available on request (=> requestServerVariables)
            VIRTUALSERVER_ANTIFLOOD_POINTS_NEEDED_COMMAND_BLOCK,       //only available on request (=> requestServerVariables)
            VIRTUALSERVER_ANTIFLOOD_POINTS_NEEDED_IP_BLOCK,            //only available on request (=> requestServerVariables)
            VIRTUALSERVER_CLIENT_CONNECTIONS,                          //only available on request (=> requestServerVariables)
            VIRTUALSERVER_QUERY_CLIENT_CONNECTIONS,                    //only available on request (=> requestServerVariables)
            VIRTUALSERVER_HOSTBUTTON_TOOLTIP,                          //available when connected, always up-to-date
            VIRTUALSERVER_HOSTBUTTON_URL,                              //available when connected, always up-to-date
            VIRTUALSERVER_HOSTBUTTON_GFX_URL,                          //available when connected, always up-to-date
            VIRTUALSERVER_DOWNLOAD_QUOTA,                              //only available on request (=> requestServerVariables)
            VIRTUALSERVER_UPLOAD_QUOTA,                                //only available on request (=> requestServerVariables)
            VIRTUALSERVER_MONTH_BYTES_DOWNLOADED,                      //only available on request (=> requestServerVariables)
            VIRTUALSERVER_MONTH_BYTES_UPLOADED,                        //only available on request (=> requestServerVariables)
            VIRTUALSERVER_TOTAL_BYTES_DOWNLOADED,                      //only available on request (=> requestServerVariables)
            VIRTUALSERVER_TOTAL_BYTES_UPLOADED,                        //only available on request (=> requestServerVariables)
            VIRTUALSERVER_PORT,                                        //only available on request (=> requestServerVariables)
            VIRTUALSERVER_HOST,                                        //internal use | contains comma separated ip list
            VIRTUALSERVER_AUTOSTART,                                   //only available on request (=> requestServerVariables)
            VIRTUALSERVER_MACHINE_ID,                                  //only available on request (=> requestServerVariables)
            VIRTUALSERVER_NEEDED_IDENTITY_SECURITY_LEVEL,              //only available on request (=> requestServerVariables)
            VIRTUALSERVER_LOG_CLIENT,                                  //only available on request (=> requestServerVariables)
            VIRTUALSERVER_LOG_QUERY,                                   //only available on request (=> requestServerVariables)
            VIRTUALSERVER_LOG_CHANNEL,                                 //only available on request (=> requestServerVariables)
            VIRTUALSERVER_LOG_PERMISSIONS,                             //only available on request (=> requestServerVariables)
            VIRTUALSERVER_LOG_SERVER,                                  //only available on request (=> requestServerVariables)
            VIRTUALSERVER_LOG_FILETRANSFER,                            //only available on request (=> requestServerVariables)
            VIRTUALSERVER_NAME_PHONETIC,                               //available when connected, always up-to-date
            VIRTUALSERVER_ICON_ID,                                     //available when connected, always up-to-date
            VIRTUALSERVER_RESERVED_SLOTS,                              //available when connected, always up-to-date
            VIRTUALSERVER_TOTAL_PACKETLOSS_SPEECH,                     //only available on request (=> requestServerVariables)
            VIRTUALSERVER_TOTAL_PACKETLOSS_KEEPALIVE,                  //only available on request (=> requestServerVariables)
            VIRTUALSERVER_TOTAL_PACKETLOSS_CONTROL,                    //only available on request (=> requestServerVariables)
            VIRTUALSERVER_TOTAL_PACKETLOSS_TOTAL,                      //only available on request (=> requestServerVariables)
            VIRTUALSERVER_TOTAL_PING,                                  //only available on request (=> requestServerVariables)
            VIRTUALSERVER_WEBLIST_ENABLED,                             //only available on request (=> requestServerVariables)
            VIRTUALSERVER_AUTOGENERATED_PRIVILEGEKEY,                  //internal use
            VIRTUALSERVER_ASK_FOR_PRIVILEGEKEY,                        //available when connected
            VIRTUALSERVER_CHANNEL_TEMP_DELETE_DELAY_DEFAULT,           //available when connected, always up-to-date
            VIRTUALSERVER_MIN_CLIENT_VERSION,                          //only available on request (=> requestServerVariables)
            VIRTUALSERVER_MIN_ANDROID_VERSION,                         //only available on request (=> requestServerVariables)
            VIRTUALSERVER_MIN_IOS_VERSION,                             //only available on request (=> requestServerVariables)
            VIRTUALSERVER_MIN_WINPHONE_VERSION,                        //only available on request (=> requestServerVariables)

            VIRTUALSERVER_MAX_CHANNELS,

            VIRTUALSERVER_LAST_CLIENT_CONNECT,
            VIRTUALSERVER_LAST_QUERY_CONNECT,
            VIRTUALSERVER_LAST_CLIENT_DISCONNECT,
            VIRTUALSERVER_LAST_QUERY_DISCONNECT,

            VIRTUALSERVER_WEB_HOST,
            VIRTUALSERVER_WEB_PORT,

            VIRTUALSERVER_DEFAULT_CLIENT_DESCRIPTION,
            VIRTUALSERVER_DEFAULT_CHANNEL_DESCRIPTION,
            VIRTUALSERVER_DEFAULT_CHANNEL_TOPIC,

            VIRTUALSERVER_MUSIC_BOT_LIMIT,
            VIRTUALSERVER_SPOKEN_TIME,
            VIRTUALSERVER_DISABLE_IP_SAVING,

            VIRTUALSERVER_COUNTRY_CODE,
            VIRTUALSERVER_ENDMARKER
        };

        enum ChannelProperties {
            CHANNEL_UNDEFINED,
            CHANNEL_BEGINMARKER,
            CHANNEL_ID = CHANNEL_BEGINMARKER,
            CHANNEL_PID,
            CHANNEL_NAME,                       //Available for all channels that are "in view", always up-to-date
            CHANNEL_TOPIC,                          //Available for all channels that are "in view", always up-to-date
            CHANNEL_DESCRIPTION,                    //Must be requested (=> requestChannelDescription)
            CHANNEL_PASSWORD,                       //not available client side
            CHANNEL_CODEC,                          //Available for all channels that are "in view", always up-to-date
            CHANNEL_CODEC_QUALITY,                  //Available for all channels that are "in view", always up-to-date
            CHANNEL_MAXCLIENTS,                     //Available for all channels that are "in view", always up-to-date
            CHANNEL_MAXFAMILYCLIENTS,               //Available for all channels that are "in view", always up-to-date
            CHANNEL_ORDER,                          //Available for all channels that are "in view", always up-to-date
            CHANNEL_FLAG_PERMANENT,                 //Available for all channels that are "in view", always up-to-date
            CHANNEL_FLAG_SEMI_PERMANENT,            //Available for all channels that are "in view", always up-to-date
            CHANNEL_FLAG_DEFAULT,                   //Available for all channels that are "in view", always up-to-date
            CHANNEL_FLAG_PASSWORD,                  //Available for all channels that are "in view", always up-to-date
            CHANNEL_CODEC_LATENCY_FACTOR,           //Available for all channels that are "in view", always up-to-date
            CHANNEL_CODEC_IS_UNENCRYPTED,           //Available for all channels that are "in view", always up-to-date
            CHANNEL_SECURITY_SALT,                  //Not available client side, not used in teamspeak, only SDK. Sets the options+salt for security hash.
            CHANNEL_DELETE_DELAY,                   //How many seconds to wait before deleting this channel
            CHANNEL_FLAG_MAXCLIENTS_UNLIMITED,      //Available for all channels that are "in view", always up-to-date
            CHANNEL_FLAG_MAXFAMILYCLIENTS_UNLIMITED,//Available for all channels that are "in view", always up-to-date
            CHANNEL_FLAG_MAXFAMILYCLIENTS_INHERITED,//Available for all channels that are "in view", always up-to-date
            CHANNEL_FLAG_ARE_SUBSCRIBED,            //Only available client side, stores whether we are subscribed to this channel
            CHANNEL_FILEPATH,                       //not available client side, the folder used for file-transfers for this channel
            CHANNEL_NEEDED_TALK_POWER,              //Available for all channels that are "in view", always up-to-date
            CHANNEL_FORCED_SILENCE,                 //Available for all channels that are "in view", always up-to-date
            CHANNEL_NAME_PHONETIC,                  //Available for all channels that are "in view", always up-to-date
            CHANNEL_ICON_ID,                        //Available for all channels that are "in view", always up-to-date
            CHANNEL_FLAG_PRIVATE,                   //Available for all channels that are "in view", always up-to-date

            CHANNEL_LAST_LEFT,
            CHANNEL_CREATED_AT,
            CHANNEL_CREATED_BY,

            CHANNEL_CONVERSATION_HISTORY_LENGTH,
            CHANNEL_FLAG_CONVERSATION_PRIVATE,

            CHANNEL_ENDMARKER
        };

        enum GroupProperties {
            GROUP_UNDEFINED,
            GROUP_BEGINMARKER,
            GROUP_ID = GROUP_BEGINMARKER,
            GROUP_TYPE,
            GROUP_NAME,
            GROUP_SORTID,
            GROUP_SAVEDB,
            GROUP_NAMEMODE,
            GROUP_ICONID,
            GROUP_ENDMARKER
        };

        enum ClientProperties {
            CLIENT_UNDEFINED,
            CLIENT_BEGINMARKER,
            CLIENT_UNIQUE_IDENTIFIER = CLIENT_BEGINMARKER,           //automatically up-to-date for any client "in view", can be used to identify this particular client installation
            CLIENT_NICKNAME,                        //automatically up-to-date for any client "in view"
            CLIENT_VERSION,                         //for other clients than ourself, this needs to be requested (=> requestClientVariables)
            CLIENT_PLATFORM,                        //for other clients than ourself, this needs to be requested (=> requestClientVariables)
            CLIENT_FLAG_TALKING,                    //automatically up-to-date for any client that can be heard (in room / whisper)
            CLIENT_INPUT_MUTED,                     //automatically up-to-date for any client "in view", this clients microphone mute status
            CLIENT_OUTPUT_MUTED,                    //automatically up-to-date for any client "in view", this clients headphones/speakers/mic combined mute status
            CLIENT_OUTPUTONLY_MUTED,                //automatically up-to-date for any client "in view", this clients headphones/speakers only mute status
            CLIENT_INPUT_HARDWARE,                  //automatically up-to-date for any client "in view", this clients microphone hardware status (is the capture device opened?)
            CLIENT_OUTPUT_HARDWARE,                 //automatically up-to-date for any client "in view", this clients headphone/speakers hardware status (is the playback device opened?)
            CLIENT_DEFAULT_CHANNEL,                 //only usable for ourself, the default channel we used to connect on our last connection attempt
            CLIENT_DEFAULT_CHANNEL_PASSWORD,        //internal use
            CLIENT_SERVER_PASSWORD,                 //internal use
            CLIENT_META_DATA,                       //automatically up-to-date for any client "in view", not used by TeamSpeak, free storage for sdk users
            CLIENT_IS_RECORDING,                    //automatically up-to-date for any client "in view"
            CLIENT_VERSION_SIGN,                    //sign
            CLIENT_SECURITY_HASH,                   //SDK use, not used by teamspeak. Hash is provided by an outside source. A channel will use the security salt + other client data to calculate a hash, which must be the same as the one provided here.

            //Rare properties
            CLIENT_KEY_OFFSET,                      //internal use
            CLIENT_LOGIN_NAME,                      //used for serverquery clients, makes no sense on normal clients currently
            CLIENT_LOGIN_PASSWORD,                  //used for serverquery clients, makes no sense on normal clients currently
            CLIENT_DATABASE_ID,                     //automatically up-to-date for any client "in view", only valid with PERMISSION feature, holds database client id
            CLIENT_ID,                     //clid!
            CLIENT_HARDWARE_ID,                     //hwid!
            CLIENT_CHANNEL_GROUP_ID,                //automatically up-to-date for any client "in view", only valid with PERMISSION feature, holds database client id
            CLIENT_SERVERGROUPS,                    //automatically up-to-date for any client "in view", only valid with PERMISSION feature, holds all servergroups client belongs too
            CLIENT_CREATED,                         //this needs to be requested (=> requestClientVariables), first time this client connected to this server
            CLIENT_LASTCONNECTED,                   //this needs to be requested (=> requestClientVariables), last time this client connected to this server
            CLIENT_TOTALCONNECTIONS,                //this needs to be requested (=> requestClientVariables), how many times this client connected to this server
            CLIENT_AWAY,                            //automatically up-to-date for any client "in view", this clients away status
            CLIENT_AWAY_MESSAGE,                    //automatically up-to-date for any client "in view", this clients away message
            CLIENT_TYPE,                            //automatically up-to-date for any client "in view", determines if this is a real client or a server-query connection
            CLIENT_TYPE_EXACT,                            //automatically up-to-date for any client "in view", determines if this is a real client or a server-query connection
            CLIENT_FLAG_AVATAR,                     //automatically up-to-date for any client "in view", this client got an avatar
            CLIENT_TALK_POWER,                      //automatically up-to-date for any client "in view", only valid with PERMISSION feature, holds database client id
            CLIENT_TALK_REQUEST,                    //automatically up-to-date for any client "in view", only valid with PERMISSION feature, holds timestamp where client requested to talk
            CLIENT_TALK_REQUEST_MSG,                //automatically up-to-date for any client "in view", only valid with PERMISSION feature, holds matter for the request
            CLIENT_DESCRIPTION,                     //automatically up-to-date for any client "in view"
            CLIENT_IS_TALKER,                       //automatically up-to-date for any client "in view"
            CLIENT_MONTH_BYTES_UPLOADED,            //this needs to be requested (=> requestClientVariables)
            CLIENT_MONTH_BYTES_DOWNLOADED,          //this needs to be requested (=> requestClientVariables)
            CLIENT_TOTAL_BYTES_UPLOADED,            //this needs to be requested (=> requestClientVariables)
            CLIENT_TOTAL_BYTES_DOWNLOADED,          //this needs to be requested (=> requestClientVariables)
            CLIENT_TOTAL_ONLINE_TIME,
            CLIENT_MONTH_ONLINE_TIME,
            CLIENT_IS_PRIORITY_SPEAKER,             //automatically up-to-date for any client "in view"
            CLIENT_UNREAD_MESSAGES,                 //automatically up-to-date for any client "in view"
            CLIENT_NICKNAME_PHONETIC,               //automatically up-to-date for any client "in view"
            CLIENT_NEEDED_SERVERQUERY_VIEW_POWER,   //automatically up-to-date for any client "in view"
            CLIENT_DEFAULT_TOKEN,                   //only usable for ourself, the default token we used to connect on our last connection attempt
            CLIENT_ICON_ID,                         //automatically up-to-date for any client "in view"
            CLIENT_IS_CHANNEL_COMMANDER,            //automatically up-to-date for any client "in view"
            CLIENT_COUNTRY,                         //automatically up-to-date for any client "in view"
            CLIENT_CHANNEL_GROUP_INHERITED_CHANNEL_ID, //automatically up-to-date for any client "in view", only valid with PERMISSION feature, contains channel_id where the channel_group_id is set from
            CLIENT_BADGES,                          //automatically up-to-date for any client "in view", stores icons for partner badges

            CLIENT_MYTEAMSPEAK_ID,
            CLIENT_INTEGRATIONS,
            CLIENT_ACTIVE_INTEGRATIONS_INFO,

            CLIENT_TEAFORO_ID,
            CLIENT_TEAFORO_NAME,
            CLIENT_TEAFORO_FLAGS,

            //Music bot stuff
            CLIENT_OWNER,
            CLIENT_BOT_TYPE,
            CLIENT_LAST_CHANNEL,
            CLIENT_PLAYER_STATE,
            CLIENT_PLAYER_VOLUME,
            CLIENT_PLAYLIST_ID,
            CLIENT_DISABLED,
            CLIENT_UPTIME_MODE,
            CLIENT_FLAG_NOTIFY_SONG_CHANGE,

            CLIENT_ENDMARKER
        };

        enum ConnectionProperties {
            CONNECTION_UNDEFINED,
            CONNECTION_BEGINMARKER,
            CONNECTION_PING = CONNECTION_BEGINMARKER,                                        //average latency for a round trip through and back this connection
            CONNECTION_PING_DEVIATION,                                  //standard deviation of the above average latency
            CONNECTION_CONNECTED_TIME,                                  //how long the connection exists already
            CONNECTION_IDLE_TIME,                                       //how long since the last action of this client
            CONNECTION_CLIENT_IP,                                      //NEED DB SAVE! //IP of this client (as seen from the server side)
            CONNECTION_CLIENT_PORT,                                     //Port of this client (as seen from the server side)
            CONNECTION_SERVER_IP,                                       //IP of the server (seen from the client side) - only available on yourself, not for remote clients, not available server side
            CONNECTION_SERVER_PORT,                                     //Port of the server (seen from the client side) - only available on yourself, not for remote clients, not available server side
            CONNECTION_PACKETS_SENT_SPEECH,                             //how many Speech packets were sent through this connection
            CONNECTION_PACKETS_SENT_KEEPALIVE,
            CONNECTION_PACKETS_SENT_CONTROL,
            CONNECTION_PACKETS_SENT_TOTAL,                              //how many packets were sent totally (this is PACKETS_SENT_SPEECH + PACKETS_SENT_KEEPALIVE + PACKETS_SENT_CONTROL)
            CONNECTION_BYTES_SENT_SPEECH,
            CONNECTION_BYTES_SENT_KEEPALIVE,
            CONNECTION_BYTES_SENT_CONTROL,
            CONNECTION_BYTES_SENT_TOTAL,
            CONNECTION_PACKETS_RECEIVED_SPEECH,
            CONNECTION_PACKETS_RECEIVED_KEEPALIVE,
            CONNECTION_PACKETS_RECEIVED_CONTROL,
            CONNECTION_PACKETS_RECEIVED_TOTAL,
            CONNECTION_BYTES_RECEIVED_SPEECH,
            CONNECTION_BYTES_RECEIVED_KEEPALIVE,
            CONNECTION_BYTES_RECEIVED_CONTROL,
            CONNECTION_BYTES_RECEIVED_TOTAL,
            CONNECTION_PACKETLOSS_SPEECH,
            CONNECTION_PACKETLOSS_KEEPALIVE,
            CONNECTION_PACKETLOSS_CONTROL,
            CONNECTION_PACKETLOSS_TOTAL,                                //the probability with which a packet round trip failed because a packet was lost
            CONNECTION_SERVER2CLIENT_PACKETLOSS_SPEECH,                 //the probability with which a speech packet failed from the server to the client
            CONNECTION_SERVER2CLIENT_PACKETLOSS_KEEPALIVE,
            CONNECTION_SERVER2CLIENT_PACKETLOSS_CONTROL,
            CONNECTION_SERVER2CLIENT_PACKETLOSS_TOTAL,
            CONNECTION_CLIENT2SERVER_PACKETLOSS_SPEECH,
            CONNECTION_CLIENT2SERVER_PACKETLOSS_KEEPALIVE,
            CONNECTION_CLIENT2SERVER_PACKETLOSS_CONTROL,
            CONNECTION_CLIENT2SERVER_PACKETLOSS_TOTAL,
            CONNECTION_BANDWIDTH_SENT_LAST_SECOND_SPEECH,               //howmany bytes of speech packets we sent during the last second
            CONNECTION_BANDWIDTH_SENT_LAST_SECOND_KEEPALIVE,
            CONNECTION_BANDWIDTH_SENT_LAST_SECOND_CONTROL,
            CONNECTION_BANDWIDTH_SENT_LAST_SECOND_TOTAL,
            CONNECTION_BANDWIDTH_SENT_LAST_MINUTE_SPEECH,               //howmany bytes/s of speech packets we sent in average during the last minute
            CONNECTION_BANDWIDTH_SENT_LAST_MINUTE_KEEPALIVE,
            CONNECTION_BANDWIDTH_SENT_LAST_MINUTE_CONTROL,
            CONNECTION_BANDWIDTH_SENT_LAST_MINUTE_TOTAL,
            CONNECTION_BANDWIDTH_RECEIVED_LAST_SECOND_SPEECH,
            CONNECTION_BANDWIDTH_RECEIVED_LAST_SECOND_KEEPALIVE,
            CONNECTION_BANDWIDTH_RECEIVED_LAST_SECOND_CONTROL,
            CONNECTION_BANDWIDTH_RECEIVED_LAST_SECOND_TOTAL,
            CONNECTION_BANDWIDTH_RECEIVED_LAST_MINUTE_SPEECH,
            CONNECTION_BANDWIDTH_RECEIVED_LAST_MINUTE_KEEPALIVE,
            CONNECTION_BANDWIDTH_RECEIVED_LAST_MINUTE_CONTROL,
            CONNECTION_BANDWIDTH_RECEIVED_LAST_MINUTE_TOTAL,

            //Rare properties
            CONNECTION_FILETRANSFER_BANDWIDTH_SENT,                     //how many bytes per second are currently being sent by file transfers
            CONNECTION_FILETRANSFER_BANDWIDTH_RECEIVED,                 //how many bytes per second are currently being received by file transfers

            CONNECTION_FILETRANSFER_BYTES_RECEIVED_TOTAL,               //how many bytes we received in total through file transfers
            CONNECTION_FILETRANSFER_BYTES_SENT_TOTAL,                   //how many bytes we sent in total through file transfers

            CONNECTION_ENDMARKER
        };

        enum PlaylistProperties {
            PLAYLIST_UNDEFINED,
            PLAYLIST_BEGINMARKER,

            PLAYLIST_ID = PLAYLIST_BEGINMARKER,
            PLAYLIST_TITLE,
            PLAYLIST_DESCRIPTION,
            PLAYLIST_TYPE,

            PLAYLIST_OWNER_DBID,
            PLAYLIST_OWNER_NAME,

            PLAYLIST_MAX_SONGS,
            PLAYLIST_FLAG_DELETE_PLAYED,
            PLAYLIST_FLAG_FINISHED,
            PLAYLIST_REPLAY_MODE, /* 0 = normal | 1 = loop list | 2 = loop entry | 3 = shuffle */

            PLAYLIST_CURRENT_SONG_ID,

            PLAYLIST_ENDMARKER
        };

        /*
        enum PlayList {
            PLAYLIST_OWNER,
            PLAYLIST_NAME,
            PLAYLIST_REPEAT_MODE,
            PLAYLIST_DELETE_PLAYED_SONGS
        };
         */

        namespace impl {
            constexpr inline auto property_count() {
                size_t result{0};
                result += VIRTUALSERVER_ENDMARKER;
                result += CHANNEL_ENDMARKER;
                result += CLIENT_ENDMARKER;
                result += GROUP_ENDMARKER;
                result += SERVERINSTANCE_ENDMARKER;
                result += CONNECTION_ENDMARKER;
                result += PLAYLIST_ENDMARKER;
                result += UNKNOWN_ENDMARKER;
                return result;
            }

            extern bool validateInput(const std::string& input, ValueType type);
        }

        template<typename>
        constexpr inline PropertyType type_from_enum();

        template<>
        constexpr inline PropertyType type_from_enum<VirtualServerProperties>() { return PropertyType::PROP_TYPE_SERVER; }

        template<>
        constexpr inline PropertyType type_from_enum<ChannelProperties>() { return PropertyType::PROP_TYPE_CHANNEL; }

        template<>
        constexpr inline PropertyType type_from_enum<ClientProperties>() { return PropertyType::PROP_TYPE_CLIENT; }

        template<>
        constexpr inline PropertyType type_from_enum<ConnectionProperties>() { return PropertyType::PROP_TYPE_CONNECTION; }

        template<>
        constexpr inline PropertyType type_from_enum<GroupProperties>() { return PropertyType::PROP_TYPE_GROUP; }

        template<>
        constexpr inline PropertyType type_from_enum<InstanceProperties>() { return PropertyType::PROP_TYPE_INSTANCE; }

        template<>
        constexpr inline PropertyType type_from_enum<PlaylistProperties>() { return PropertyType::PROP_TYPE_PLAYLIST; }

        template<>
        constexpr inline PropertyType type_from_enum<UnknownProperties>() { return PropertyType::PROP_TYPE_UNKNOWN; }

        struct PropertyDescription {
            std::string_view name{};
            std::string_view default_value{};

            //TODO: Rename these sucky variables
            ValueType type_value{property::ValueType::TYPE_UNKNOWN};
            PropertyType type_property{PropertyType::PROP_TYPE_UNKNOWN};

            int property_index{0};
            flag_type flags{0};

            template <typename PropertyEnumType, typename std::enable_if<std::is_enum<PropertyEnumType>::value, int>::type = 0>
            constexpr PropertyDescription(PropertyEnumType property, std::string_view name, std::string_view default_value, property::ValueType value_type, flag_type flags) noexcept
                : name{name}, default_value{default_value}, type_value{value_type}, type_property{type_from_enum<PropertyEnumType>()},
                  property_index{(int) property}, flags{flags} { }
            PropertyDescription(const PropertyDescription&) = delete;
            PropertyDescription(PropertyDescription&&) = default;

            constexpr inline bool operator==(const PropertyDescription& other) const {
                return this->property_index == other.property_index && this->type_property == other.type_property;
            }

            template <typename PropertyEnumType, typename std::enable_if<std::is_enum<PropertyEnumType>::value, int>::type = 0>
            constexpr inline bool operator==(const PropertyEnumType& other) const {
                return this->property_index == (int) other && this->type_property == type_from_enum<PropertyEnumType>();
            }

            [[nodiscard]] inline bool is_undefined() const { return property_index == 0; }
            [[nodiscard]] inline bool validate_input(const std::string& value) const { return impl::validateInput(value, this->type_value); }
        };
        constexpr static PropertyDescription undefined_property_description{UnknownProperties::UNKNOWN_UNDEFINED, "undefined", "", ValueType::TYPE_UNKNOWN, 0};

        struct PropertyListInfo {
            std::array<size_t, PropertyType::PROP_TYPE_MAX> begin_index{}; /* inclusive */
            std::array<size_t, PropertyType::PROP_TYPE_MAX> end_index{}; /* exclusive */
        };

#ifdef EXTERNALIZE_PROPERTY_DEFINITIONS
        extern std::array<PropertyDescription, impl::property_count()> property_list;
#else
        #include "./PropertyDefinition.h"

        constexpr inline auto property_count() {
            return property_list.size();
        }
#endif

#ifdef EXTERNALIZE_PROPERTY_DEFINITIONS
    #define const_modifier
#else
    #define const_modifier constexpr
#endif
        namespace impl {
            const_modifier inline size_t property_type_begin(PropertyType type) {
                size_t index{0};
                for(; index < property_list.size(); index++)
                    if(property_list[index].type_property == type)
                        return index;
                return property_list.size();
            }

            const_modifier inline size_t property_type_end(size_t begin, PropertyType type) {
                size_t index{begin};
                for(; index < property_list.size() - 1; index++)
                    if(property_list[index + 1].type_property != type)
                        return index + 1;
                return property_list.size();
            }

            const_modifier inline PropertyListInfo list_info() noexcept {
                std::array<size_t, PropertyType::PROP_TYPE_MAX> begin_index{
                    /* We're using numbers here so we don't mess up the order. This would be fatal */
                    property_type_begin((PropertyType) 0),
                    property_type_begin((PropertyType) 1),
                    property_type_begin((PropertyType) 2),
                    property_type_begin((PropertyType) 3),
                    property_type_begin((PropertyType) 4),
                    property_type_begin((PropertyType) 5),
                    property_type_begin((PropertyType) 6),
                    property_type_begin((PropertyType) 7)
                };

                return {
                        begin_index,
                        std::array<size_t, PropertyType::PROP_TYPE_MAX>{
                            /* We're using numbers here so we don't mess up the order. This would be fatal */
                            property_type_end(begin_index[0], (PropertyType) 0),
                            property_type_end(begin_index[1], (PropertyType) 1),
                            property_type_end(begin_index[2], (PropertyType) 2),
                            property_type_end(begin_index[3], (PropertyType) 3),
                            property_type_end(begin_index[4], (PropertyType) 4),
                            property_type_end(begin_index[5], (PropertyType) 5),
                            property_type_end(begin_index[6], (PropertyType) 6),
                            property_type_end(begin_index[7], (PropertyType) 7)
                        }
                };
            }
        }
#ifdef EXTERNALIZE_PROPERTY_DEFINITIONS
        extern const PropertyListInfo property_list_info;
#else
        constexpr auto property_list_info = impl::list_info();
#endif

        template <typename PropertyEnumType, typename std::enable_if<std::is_enum<PropertyEnumType>::value, int>::type = 0>
        const_modifier inline const auto& describe(PropertyEnumType type) {
            static_assert(type_from_enum<PropertyEnumType>() < property_list_info.end_index.size());
            const auto begin = property_list_info.begin_index[type_from_enum<PropertyEnumType>()];
            const auto end = property_list_info.end_index[type_from_enum<PropertyEnumType>()];
            const auto idx = begin + (size_t) type;
            if(idx >= end) return undefined_property_description;
            return property_list[idx];
        }

        inline const auto& describe(PropertyType type, size_t index) {
            if(type >= property_list_info.end_index.size()) return undefined_property_description;
            const auto begin = property_list_info.begin_index[type];
            const auto end = property_list_info.end_index[type];
            const auto idx = begin + index;
            if(idx >= end) return undefined_property_description;
            return property_list[idx];
        }

        inline const auto& find(PropertyType type, const std::string_view& name) {
            if(type >= property_list_info.end_index.size()) return undefined_property_description;

            constexpr static auto buffer_size{128}; /* no property is longer than 128 bytes */
            if(name.size() >= buffer_size) return undefined_property_description;
            char buffer[buffer_size];
            for(size_t index{0}; index < name.size(); index++)
                buffer[index] = (char) tolower(name[index]);

            const std::string_view lower_name{buffer, name.size()};
            const auto begin = property_list_info.begin_index[type];
            const auto end = property_list_info.end_index[type];
            for(size_t index{begin}; index < end; index++)
                if(property_list[index].name == lower_name)
                    return property_list[index];
            return property_list[begin]; /* begin index MUST be the undefined */
        }

        template <typename PropertyEnumType, typename std::enable_if<std::is_enum<PropertyEnumType>::value, int>::type = 0>
        inline const auto& find(const std::string_view& name) {
            return find(type_from_enum<PropertyEnumType>(), name);
        }

        template <typename PropertyEnumType, typename std::enable_if<std::is_enum<PropertyEnumType>::value, int>::type = 0>
        inline std::vector<const PropertyDescription*> list() {
            constexpr auto type = type_from_enum<PropertyEnumType>();
            if(type >= property_list_info.end_index.size()) return {};

            const auto begin = property_list_info.begin_index[type];
            const auto end = property_list_info.end_index[type];
            return {property_list.begin() + begin, property_list.begin() + end};
        }

        template <typename PropertyEnumType, typename std::enable_if<std::is_enum<PropertyEnumType>::value, int>::type = 0>
        const_modifier inline const std::string_view& name(PropertyEnumType type) { /* defining the return type here to help out my IDE a bit ;) */
            return describe<PropertyEnumType>(type).name;
        }

        template <typename PropertyEnumType, typename std::enable_if<std::is_enum<PropertyEnumType>::value, int>::type = 0>
        const_modifier inline size_t property_count() {
            const auto begin = property_list_info.begin_index[type_from_enum<PropertyEnumType>()];
            const auto end = property_list_info.end_index[type_from_enum<PropertyEnumType>()];
            return end - begin;
        }

#undef const_modifier
    }
    class Properties;

    struct PropertyData {
        spin_lock value_lock;
        std::any casted_value;
        std::string value;
        const property::PropertyDescription* description;

        bool flag_db_reference;
        bool flag_modified;
    };

#ifdef WIN32
    #pragma warning( push )
    #pragma warning( disable : 4200 )
#endif
    struct PropertyBundle {
        property::PropertyType type;
        size_t length;
        PropertyData properties[0];
    };
#ifdef WIN32
    #pragma warning( pop )
#endif

    template <typename T>
    struct PropertyAccess {
        inline static T get(PropertyData* data_ptr) {
            std::lock_guard lock(data_ptr->value_lock);
            if(data_ptr->casted_value.type() == typeid(T))
                return std::any_cast<T>(data_ptr->casted_value);

            data_ptr->casted_value = ts::converter<T>::from_string_view(std::string_view{data_ptr->value});
            return std::any_cast<T>(data_ptr->casted_value);
        }
    };

    template <>
    struct PropertyAccess<std::string> {
        inline static std::string get(PropertyData* data_ptr) { return data_ptr->value; }
    };

    struct PropertyWrapper {
        friend class Properties;
        public:
            bool operator==(const PropertyWrapper& other) {
                if(this->data_ptr == other.data_ptr)
                    return true;

                return this->data_ptr->value == other.data_ptr->value;
            }

            template <typename T>
            bool operator==(const T& other){
                return this->as<T>() == other;
            }
            template <typename T>
            bool operator!=(const T& other){
                return !operator==(other);
            }

            //Math operators
            PropertyWrapper&operator++(){ return operator=(as<int64_t>() + 1); }
            PropertyWrapper&operator++(int){ return operator=(as<int64_t>() + 1); }
            PropertyWrapper&operator+=(uint16_t val){ return operator=(as<uint16_t>() + val); }
            PropertyWrapper&operator+=(int64_t val){ return operator=(as<int64_t>() + val); }
            PropertyWrapper&operator+=(uint64_t val){ return operator=(as<uint64_t>() + val); }

            [[nodiscard]] bool hasDbReference() const { return this->data_ptr->flag_db_reference; }
            void setDbReference(bool flag){ this->data_ptr->flag_db_reference = flag; }

            [[nodiscard]] bool isModified() const { return this->data_ptr->flag_modified; }
            void setModified(bool flag){ this->data_ptr->flag_modified = flag; }

            template <typename T>
            [[nodiscard]] T as() const {
                static_assert(ts::converter<T>::supported, "as<T> isn't supported for type");

                return PropertyAccess<T>::get(this->data_ptr);
            }


            template <typename T>
            [[nodiscard]] operator T(){ return this->as<T>(); }

            template <typename T>
            [[nodiscard]] T as_save() const {
                try {
                    std::lock_guard lock(this->data_ptr->value_lock);
                    if(this->data_ptr->casted_value.type() == typeid(T))
                        return std::any_cast<T>(this->data_ptr->casted_value);

                    this->data_ptr->casted_value = ts::converter<T>::from_string_view(this->data_ptr->value);
                    return std::any_cast<T>(this->data_ptr->casted_value);
                } catch(std::exception&) {
                    return T{};
                }
            }

            [[nodiscard]] const property::PropertyDescription& type() const { return *this->data_ptr->description; }
            [[nodiscard]] std::string value() const {
                std::lock_guard lock{this->data_ptr->value_lock};
                return this->data_ptr->value;
            }

            void value(const std::string &value, bool trigger_update = true){
                {
                    std::lock_guard lock(this->data_ptr->value_lock);
                    if(this->data_ptr->value == value)
                        return;
                    this->data_ptr->casted_value.reset();
                    this->data_ptr->value = value;
                }
                if(trigger_update)
                    this->trigger_update();
            }

            [[nodiscard]] const std::string_view& default_value() const {
                return this->type().default_value;
            }

            template <typename T>
            PropertyWrapper& operator=(const T& value) {
                static_assert(ts::converter<T>::supported, "type isn't supported for type");

                {
                    std::any any_value{value};
                    auto value_string = ts::converter<T>::to_string(any_value);

                    std::lock_guard lock(this->data_ptr->value_lock);
                    if(value_string == this->data_ptr->value)
                        return *this;
                    this->data_ptr->casted_value = any_value;
                    this->data_ptr->value = value_string;
                }
                this->trigger_update();

                return *this;
            }


            PropertyWrapper& operator=(const std::string& value) {
                this->value(value);
                return *this;
            }
            PropertyWrapper& operator=(const char* value) {
                this->value(value);
                return *this;
            }

            template <int N>
            PropertyWrapper& operator=(char(value)[N]) {
                this->value(value);
                return *this;
            }

            void trigger_update();

            PropertyWrapper(Properties* /* handle */, PropertyData* /* ptr */, std::shared_ptr<PropertyBundle>  /* bundle */);

            // Proxy constructor for properties() method - wraps entire Properties collection
            explicit PropertyWrapper(const std::shared_ptr<Properties>& properties_ptr)
                : handle(nullptr), data_ptr(nullptr), properties_proxy(properties_ptr) {}

            [[nodiscard]] inline Properties* get_handle() { return this->handle; }

            // Proxy operator[] - delegates to wrapped Properties when in proxy mode
            template <typename T, typename std::enable_if<std::is_enum<T>::value, int>::type = 0>
            PropertyWrapper operator[](T type) {
                if(this->properties_proxy) {
                    return (*this->properties_proxy)[type];
                }
                // Should not happen - return invalid wrapper
                return PropertyWrapper(nullptr, nullptr, nullptr);
            }

            // as_unchecked - same as as<T>() but different name for compatibility
            template <typename T>
            [[nodiscard]] T as_unchecked() const {
                return this->as<T>();
            }

            // as_or - like as_save() but with custom default value
            template <typename T>
            [[nodiscard]] T as_or(const T& default_value) const {
                try {
                    std::lock_guard lock(this->data_ptr->value_lock);
                    if(this->data_ptr->casted_value.type() == typeid(T))
                        return std::any_cast<T>(this->data_ptr->casted_value);

                    this->data_ptr->casted_value = ts::converter<T>::from_string_view(this->data_ptr->value);
                    return std::any_cast<T>(this->data_ptr->casted_value);
                } catch(std::exception&) {
                    return default_value;
                }
            }

        private:
            Properties* handle = nullptr;
            PropertyData* data_ptr = nullptr;
            std::shared_ptr<PropertyBundle> bundle_lock;
            std::shared_ptr<Properties> properties_proxy;  // Used when acting as proxy to Properties collection
    };
    typedef PropertyWrapper Property;
    typedef std::function<void(PropertyWrapper&)> PropertyNotifyFn;

    class Properties {
            friend struct PropertyWrapper;
        public:
            Properties();
            ~Properties();
            Properties(const Properties&) = delete;
            Properties(Properties&&) = default;

            std::vector<PropertyWrapper> list_properties(property::flag_type flagMask = (property::flag_type) ~0UL, property::flag_type negatedFlagMask = 0);
            std::vector<PropertyWrapper> all_properties();

            template <typename Type>
            bool register_property_type() {
                constexpr auto type = property::type_from_enum<Type>();
                return this->register_property_type(type, property::property_count<Type>());
            }

            template <typename T>
            bool hasProperty(T type) { return this->has(property::type_from_enum<T>(), type); }

            template <typename T, typename std::enable_if<std::is_enum<T>::value, int>::type = 0>
            PropertyWrapper operator[](T type) {
                return this->find(property::type_from_enum<T>(), type);
            }

            PropertyWrapper operator[](const property::PropertyDescription& type) {
                return this->find(type.type_property, type.property_index);
            }

            PropertyWrapper operator[](const property::PropertyDescription* type) {
                return this->find(type->type_property, type->property_index);
            }

            void registerNotifyHandler(const PropertyNotifyFn &fn){
                this->notifyFunctions.push_back(fn);
            }

            void triggerAllModified(){
                for(auto& prop : this->all_properties())
                    if(prop.isModified())
                        for(auto& elm : notifyFunctions)
                            elm(prop);
            }

            void toggleSave(bool flag) { this->save = flag; }
            bool isSaveEnabled(){ return this->save; }


            template <typename T, typename std::enable_if<std::is_enum<T>::value, int>::type = 0>
            PropertyWrapper find(T type) {
                return this->find(property::type_from_enum<T>(), type);
            }

            PropertyWrapper find(property::PropertyType type, int index);
            bool has(property::PropertyType type, int index);

            template <typename T, typename std::enable_if<std::is_enum<T>::value, int>::type = 0>
            bool has(T type) { return this->has(property::type_from_enum<T>(), (int) type); }
        private:
            bool register_property_type(property::PropertyType /* type */, size_t /* length */);

            bool save{true};
            std::vector<std::function<void(PropertyWrapper&)>> notifyFunctions{};

            size_t properties_count{0};
            std::vector<std::shared_ptr<PropertyBundle>> properties;
    };

    // PropertyManager is an alias for Properties
    using PropertyManager = Properties;
};

//DEFINE_TRANSFORMS(ts::property::PropertyType, uint8_t);
DEFINE_CONVERTER_ENUM(ts::property::PropertyType, uint8_t);
DEFINE_VARIABLE_TRANSFORM_ENUM(ts::property::PropertyType, uint8_t);