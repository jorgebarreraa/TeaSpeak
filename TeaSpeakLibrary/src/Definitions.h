#pragma once

#include "Variable.h"
#include "converters/converter.h"

#define tsclient std::shared_ptr<ts::server::ConnectedClient>
#define tsclientmusic std::shared_ptr<ts::server::MusicClient>

namespace ts {
    typedef uint16_t ServerId;
    typedef int32_t OptionalServerId;
    constexpr auto EmptyServerId = (OptionalServerId) -1;

    typedef uint16_t VirtualServerId;
    typedef uint64_t ClientDbId;
    typedef uint16_t ClientId;
    typedef std::string ClientUid;
    typedef uint64_t ChannelId;
    typedef uint32_t GroupId;
    typedef uint16_t FloodPoints;
    typedef uint64_t BanId;
    typedef uint32_t LetterId;
    typedef uint32_t SongId;
    typedef uint32_t IconId;
    typedef uint32_t PlaylistId;

    //0 = No License | 1 = Hosting | 2 = Offline license | 3 = Non-profit license | 4 = Unknown | 5 = Unknown
    //Everything over 5 is no lt (Client >= 1.3.6)
    enum LicenseType: uint8_t {
        _LicenseType_MIN,
        LICENSE_NONE = _LicenseType_MIN,    /* ServerLicenseType::SERVER_LICENSE_NONE */
        LICENSE_HOSTING,                    /* ServerLicenseType::SERVER_LICENSE_ATHP */
        LICENSE_OFFLINE,                    /* ServerLicenseType::SERVER_LICENSE_OFFLINE */
        LICENSE_NPL,                        /* ServerLicenseType::SERVER_LICENSE_NPL */
        LICENSE_UNKNOWN,                    /* ServerLicenseType::SERVER_LICENSE_SDK */
        LICENSE_PLACEHOLDER,                /* ServerLicenseType::SERVER_LICENSE_SDKOFFLINE */
        LICENSE_AUTOMATIC_SERVER,
        LICENSE_AUTOMATIC_INSTANCE,
        _LicenseType_MAX = LICENSE_AUTOMATIC_INSTANCE,
    };
    /*
        enum ServerLicenseType : uint8_t {
            SERVER_LICENSE_NONE,
            SERVER_LICENSE_OFFLINE,
            SERVER_LICENSE_SDK,
            SERVER_LICENSE_SDKOFFLINE,
            SERVER_LICENSE_NPL,
            SERVER_LICENSE_ATHP,
            SERVER_LICENSE_AAL,
            SERVER_LICENSE_DEFAULT,
        };
     */

    enum PluginTargetMode : uint8_t {
        PLUGINCMD_CURRENT_CHANNEL = 0,                //send plugincmd to all clients in current channel
        PLUGINCMD_SERVER,                             //send plugincmd to all clients on server
        PLUGINCMD_CLIENT,                             //send plugincmd to all given manager ids
        PLUGINCMD_SUBSCRIBED_CLIENTS,                  //send plugincmd to all subscribed clients in current channel

        PLUGINCMD_SEND_COMMAND = 0xF0
    };

    enum ViewReasonId: uint8_t {
        VREASON_USER_ACTION = 0,
        VREASON_MOVED = 1,
        VREASON_SYSTEM = 2,
        VREASON_TIMEOUT = 3,
        VREASON_CHANNEL_KICK = 4,
        VREASON_SERVER_KICK = 5,
        VREASON_BAN = 6,
        VREASON_SERVER_STOPPED = 7,
        VREASON_SERVER_LEFT = 8,
        VREASON_CHANNEL_UPDATED = 9,
        VREASON_EDITED = 10,
        VREASON_SERVER_SHUTDOWN = 11
    };

    struct ViewReasonSystemT { };
    static constexpr auto ViewReasonSystem = ViewReasonSystemT{};

    struct ViewReasonServerLeftT { };
    static constexpr auto ViewReasonServerLeft = ViewReasonServerLeftT{};

    enum ChatMessageMode : uint8_t {
        TEXTMODE_PRIVATE = 1,
        TEXTMODE_CHANNEL = 2,
        TEXTMODE_SERVER = 3
    };

    namespace server {
        enum ClientType {
            CLIENT_TEAMSPEAK,
            CLIENT_QUERY,
            CLIENT_INTERNAL,
            CLIENT_WEB,
            CLIENT_MUSIC,
            CLIENT_TEASPEAK,
            MAX,

            UNKNOWN = 0xFF
        };

        enum class ClientState : uint8_t {
            UNKNWON,

            INITIALIZING,
            CONNECTED,
            DISCONNECTED
        };

        enum class ConnectionState : uint8_t {
            UNKNWON,
            INIT_LOW,
            INIT_HIGH,
            CONNECTED,
            DISCONNECTING,
            DISCONNECTED
        };
    }

    enum QueryEventGroup : int {
        QEVENTGROUP_MIN                 = 0,
        QEVENTGROUP_SERVER              = 0,
        QEVENTGROUP_CLIENT_MISC         = 1,
        QEVENTGROUP_CLIENT_GROUPS       = 2,
        QEVENTGROUP_CLIENT_VIEW         = 3,
        QEVENTGROUP_CHAT                = 4,
        QEVENTGROUP_CHANNEL             = 5,
        QEVENTGROUP_MUSIC               = 6,
        QEVENTGROUP_MAX                 = 8
    };

    enum QueryEventSpecifier {
        QEVENTSPECIFIER_MIN                             = 0,
        QEVENTSPECIFIER_SERVER_EDIT                     = 0,

        QEVENTSPECIFIER_CLIENT_MISC_POKE                = 0,
        QEVENTSPECIFIER_CLIENT_MISC_UPDATE              = 1,
        QEVENTSPECIFIER_CLIENT_MISC_COMMAND             = 2,

        QEVENTSPECIFIER_CLIENT_GROUPS_ADD               = 0,
        QEVENTSPECIFIER_CLIENT_GROUPS_REMOVE            = 1,
        QEVENTSPECIFIER_CLIENT_GROUPS_CHANNEL_CHANGED   = 2,

        QEVENTSPECIFIER_CLIENT_VIEW_JOIN                = 0,
        QEVENTSPECIFIER_CLIENT_VIEW_SWITCH              = 1,
        QEVENTSPECIFIER_CLIENT_VIEW_LEAVE               = 2,

        QEVENTSPECIFIER_CHAT_COMPOSING                  = 0,
        QEVENTSPECIFIER_CHAT_MESSAGE_SERVER             = 1,
        QEVENTSPECIFIER_CHAT_MESSAGE_CHANNEL            = 2,
        QEVENTSPECIFIER_CHAT_MESSAGE_PRIVATE            = 3,
        QEVENTSPECIFIER_CHAT_CLOSED                     = 4,

        QEVENTSPECIFIER_CHANNEL_CREATE                  = 0,
        QEVENTSPECIFIER_CHANNEL_MOVE                    = 1,
        QEVENTSPECIFIER_CHANNEL_EDIT                    = 2,
        QEVENTSPECIFIER_CHANNEL_DESC_EDIT               = 3,
        QEVENTSPECIFIER_CHANNEL_PASSWORD_EDIT           = 4,
        QEVENTSPECIFIER_CHANNEL_DELETED                 = 5,

        QEVENTSPECIFIER_MUSIC_QUEUE                     = 0,
        QEVENTSPECIFIER_MUSIC_PLAYER                    = 1,

        QEVENTSPECIFIER_MAX                             = 6,
    };
}


#define DEFINE_TRANSFORMS(a, b)             \
    DEFINE_CONVERTER_ENUM(a, b);            \
    DEFINE_VARIABLE_TRANSFORM_ENUM(a, b);

DEFINE_TRANSFORMS(ts::server::ClientState, uint8_t);
DEFINE_TRANSFORMS(ts::server::ClientType, uint8_t);
DEFINE_TRANSFORMS(ts::server::ConnectionState, uint8_t);
DEFINE_TRANSFORMS(ts::LicenseType, uint8_t);
DEFINE_TRANSFORMS(ts::PluginTargetMode, uint8_t);
DEFINE_TRANSFORMS(ts::ViewReasonId, uint8_t);
DEFINE_TRANSFORMS(ts::ChatMessageMode, uint8_t);

#ifdef WIN32
#define ts_always_inline __forceinline
#else
#define ts_always_inline inline __attribute__((__always_inline__))
#endif