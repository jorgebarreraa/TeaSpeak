#pragma once

#include <string>
#include <vector>
#include <Definitions.h>
#ifdef byte
    #undef byte
#endif
#include <spdlog/common.h>
#include <misc/strobf.h>
#include "geo/GeoLocation.h"
#include "../../license/shared/include/license/license.h"
#include "build.h"

namespace YAML {
    class Node;
}
namespace ts::config {
    struct EntryBinding {
        std::string key;
        std::map<std::string, std::deque<std::string>> description;
        uint8_t flags = 0;

        int type = 0; /* 0 = unbound | 1 = string | 2 = number | 3 = boolean | 4 = user defined */
        int bounded_by = 0; /* 0 = unbound | 1 = config | 2 = command line */

        std::function<std::deque<std::string>()> default_value;
        std::function<std::string()> value_description;
        std::function<void(YAML::Node&)> set_default;
        std::function<void(YAML::Node&)> read_config;
        std::function<void(const std::string&)> read_argument;
    };

    extern bool update_license(std::string& /* error */, const std::string& /* new license */);
    extern std::vector<std::string> parseConfig(const std::string& /* path */);
    extern std::vector<std::string> reload();
    extern std::deque<std::shared_ptr<EntryBinding>> create_bindings();

    namespace database {
        extern std::string url;
        namespace sqlite {
            extern std::string journal_mode;
            extern std::string locking_mode;
            extern std::string sync_mode;
        }
    }

    extern std::shared_ptr<license::License> license;
    extern std::shared_ptr<license::License> license_original;

    extern bool experimental_31;
    extern std::string permission_mapping_file;

    namespace binding {
        extern bool enforce_default_voice_host;
        extern std::string DefaultVoiceHost;
        extern std::string DefaultWebHost;
        extern std::string DefaultQueryHost;
        extern std::string DefaultFileHost;

        extern uint16_t DefaultQueryPort;
        extern uint16_t DefaultFilePort;
    }

    namespace server {
        extern std::string DefaultServerVersion;
        extern std::string DefaultServerPlatform;

        extern bool delete_old_bans;
        extern bool delete_missing_icon_permissions;

        extern LicenseType DefaultServerLicense;

        extern bool strict_ut8_mode;

        extern bool show_invisible_clients_as_online;
        extern bool disable_ip_saving;
        extern bool default_music_bot;

        namespace limits {
            extern size_t poke_message_length;
            extern size_t talk_power_request_message_length;
            extern size_t afk_message_length;
        }

        namespace badges {
            extern bool allow_overwolf;
            extern bool allow_badges;
        }

        namespace authentication {
            extern bool name;
        }

        namespace clients {
            enum WelcomeMessageType {
                WELCOME_MESSAGE_TYPE_MIN,
                WELCOME_MESSAGE_TYPE_NONE = WELCOME_MESSAGE_TYPE_MIN,
                WELCOME_MESSAGE_TYPE_CHAT,
                WELCOME_MESSAGE_TYPE_POKE,
                WELCOME_MESSAGE_TYPE_MAX
            };

            extern bool teamspeak;
            extern std::string teamspeak_not_allowed_message;
            extern std::string extra_welcome_message_teamspeak;
            extern WelcomeMessageType extra_welcome_message_type_teamspeak;

            extern std::string extra_welcome_message_teaspeak;
            extern WelcomeMessageType extra_welcome_message_type_teaspeak;

            extern bool teaweb;
            extern std::string teaweb_not_allowed_message;
            extern std::string extra_welcome_message_teaweb;
            extern WelcomeMessageType extra_welcome_message_type_teaweb;

            extern bool ignore_max_clone_permissions;
        }

        extern ssize_t max_virtual_server;

        __attribute__((always_inline)) inline std::string default_version() { return strobf("TeaSpeak ").string() + build::version()->string(true); }
        __attribute__((always_inline)) inline bool check_server_version_with_license() {
            return default_version() == DefaultServerVersion || (license->isPremium() && license->isValid());
        }
    }

    namespace voice {
        extern size_t DefaultPuzzlePrecomputeSize;
        extern int RsaPuzzleLevel;
        extern bool enforce_coocie_handshake;


        extern bool notifyMuted;
        extern int connectLimit;
        extern int clientConnectLimit;

        extern bool suppress_myts_warnings;
        extern uint16_t default_voice_port;

        extern bool warn_on_permission_editor;
        extern bool allow_session_reinitialize;
    }

    namespace geo {
        extern std::string countryFlag;
        extern bool staticFlag;

        extern std::string mappingFile;
        extern geoloc::ProviderType type;

        extern bool vpn_block;
        extern std::string vpn_file;
    }

    namespace query {
        extern std::string motd;
        extern std::string newlineCharacter;
        extern size_t max_line_buffer;

        extern int sslMode;
        namespace ssl {
            extern std::string keyFile;
            extern std::string certFile;
        }
    }

    namespace music {
        extern bool enabled;
        extern std::string command_prefix;
    }

    namespace messages {
        extern std::string serverStopped;
        extern std::string applicationStopped;
        extern std::string applicationCrashed;

        extern std::string idle_time_exceeded;

        extern std::string mute_notify_message;
        extern std::string unmute_notify_message;

        extern std::string kick_invalid_hardware_id;
        extern std::string kick_invalid_badges;
        extern std::string kick_invalid_command;

        extern std::string kick_vpn;

        extern std::string teamspeak_permission_editor;

        namespace shutdown {
            extern std::string scheduled;
            extern std::string interval;
            extern std::string now;
            extern std::string canceled;

            extern std::vector<std::pair<std::chrono::seconds, std::string>> intervals;
        }

        namespace music {
            extern std::string song_announcement;
        }

        namespace timeout {
            extern std::string packet_resend_failed;
            extern std::string connection_reinitialized;
        }
    }

    namespace web {
        extern bool activated;

        namespace ssl {
            /* servername; private key file; public key file*/
            extern std::deque<std::tuple<std::string, std::string, std::string>> certificates;
        }

        extern uint16_t webrtc_port_min;
        extern uint16_t webrtc_port_max;

        extern bool enable_upnp;

        extern bool stun_enabled;
        extern std::string stun_host;
        extern uint16_t stun_port;

        extern bool tcp_enabled;
        extern bool udp_enabled;
    }

    namespace threads {
        extern size_t ticking;
        extern size_t command_execute;
        extern size_t network_events;

        namespace voice {
            extern size_t events_per_server;
        }

        namespace music {
            extern size_t execute_per_bot;
            extern size_t execute_limit;
        }
    }

    namespace log {
        extern std::string path;
        extern size_t vs_size;
        extern spdlog::level::level_enum logfileLevel;
        extern bool logfileColored;
        extern spdlog::level::level_enum terminalLevel;
    }

    extern std::string crash_path;
}