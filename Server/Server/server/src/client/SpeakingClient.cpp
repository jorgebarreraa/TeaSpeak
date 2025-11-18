#include <memory>

#include <PermissionManager.h>
#include <misc/endianness.h>
#include <log/LogUtils.h>
#include <ThreadPool/Timer.h>
#include <regex>
#include <src/build.h>
#include <Properties.h>
#include <src/client/command_handler/helpers.h>
#include <misc/utf8.h>
#include "src/channel/ClientChannelView.h"
#include "SpeakingClient.h"
#include "src/InstanceHandler.h"
#include "StringVariable.h"
#include "misc/timer.h"
#include "../manager/ActionLogger.h"
#include "./voice/VoiceClient.h"
#include "../rtc/imports.h"
#include "../groups/GroupManager.h"
#include "../PermissionCalculator.h"

using namespace std::chrono;
using namespace ts;
using namespace ts::server;
using namespace ts::protocol;

SpeakingClient::SpeakingClient(sql::SqlManager *a, const std::shared_ptr<VirtualServer> &b) : ConnectedClient{a, b}, whisper_handler_{this} {
    speak_begin = std::chrono::system_clock::now();
    speak_last_packet = std::chrono::system_clock::now();
};

SpeakingClient::~SpeakingClient() {
    if(auto server{this->server}; this->rtc_client_id > 0 && server) {
        server->rtc_server().destroy_client(this->rtc_client_id);
    }
}

bool SpeakingClient::shouldReceiveVoice(const std::shared_ptr<ConnectedClient> &sender) {
    //if(this->properties()[property::CLIENT_AWAY].as<bool>()) return false;
    if(!this->properties()[property::CLIENT_OUTPUT_HARDWARE].as_or<bool>(true)) return false;
    if(this->properties()[property::CLIENT_OUTPUT_MUTED].as_or<bool>(false)) return false;

    {
        shared_lock client_lock(this->channel_tree_mutex);
        for(const auto& entry : this->mutedClients)
            if(entry.lock() == sender)
                return false;
    }
    return true;
}

bool SpeakingClient::shouldReceiveVoiceWhisper(const std::shared_ptr<ConnectedClient> &sender) {
    if(!this->shouldReceiveVoice(sender))
        return false;

    return permission::v2::permission_granted(this->cpmerission_needed_whisper_power, sender->cpmerission_whisper_power);
}

bool SpeakingClient::should_handle_voice_packet(size_t) {
    auto current_channel = this->currentChannel;
    if(!current_channel) { return false; }
    if(!this->allowedToTalk) { return false; }
    this->updateSpeak(false, system_clock::now());
    this->resetIdleTime();

    return true;
}

inline bool update_whisper_error(std::chrono::system_clock::time_point& last) {
    auto now = std::chrono::system_clock::now();
    if(last + std::chrono::milliseconds{500} < now) {
        last = now;
        return true;
    }
    return false;
}


auto regex_wildcard = std::regex(".*");

#define S(x) #x
#define HWID_REGEX(name, pattern)                           \
auto regex_hwid_ ##name = []() noexcept {                   \
    try {                                                   \
        return std::regex(pattern);                         \
    } catch (std::exception& ex) {                          \
        logError(0, "Failed to parse regex for " S(name));  \
    }                                                       \
    return regex_wildcard;                                  \
}();

HWID_REGEX(windows, "^[a-z0-9]{32},[a-z0-9]{32}$");
HWID_REGEX(unix, "^[a-z0-9]{32}$");
HWID_REGEX(android, "^[a-z0-9]{16}$");
HWID_REGEX(ios, "^[A-Z0-9]{8}-[A-Z0-9]{4}-[A-Z0-9]{4}-[A-Z0-9]{4}-[A-Z0-9]{12}$");

command_result SpeakingClient::applyClientInitParameters(Command &cmd) {
    for(const auto& key : cmd[0].keys()) {
        if(key == "return_code") {
            /* That's ok */
            continue;
        } else if(key == "client_nickname") {
            auto name_length = utf8::count_characters(cmd[key].string());
            if (name_length < 3) {
                return command_result{error::parameter_invalid, "client_nickname"};
            }

            if (name_length > 30) {
                return command_result{error::parameter_invalid, "client_nickname"};
            }

            /* The unique of the name will be checked when registering the client at the target server */
            this->properties()[property::CLIENT_NICKNAME] = cmd[key].string();
        } else if(key == "client_nickname_phonetic") {
            auto name = cmd["client_nickname_phonetic"].string();
            auto name_length = utf8::count_characters(name);
            if (name_length < 0 || name_length > 30) {
                return command_result{error::parameter_invalid, "client_nickname_phonetic"};
            }
            this->properties()[property::CLIENT_NICKNAME_PHONETIC] = name;
        } else if(key == "client_version" || key == "client_platform") {
            auto value = cmd[key].string();
            if(value.length() > 512) {
                /* The web client uses the full browser string which might be a bit longer */
                return command_result{error::client_hacked};
            }

            for(auto& character : value) {
                if(!isascii(character)) {
                    logWarning(this->getServerId(), "{} Tried to join within an invalid supplied '{}' ({})", CLIENT_STR_LOG_PREFIX, key,cmd[key].string());
                    return command_result{error::client_hacked};
                }
            }

            if(key == "client_version") {
                this->properties()[property::CLIENT_VERSION] = value;
            } else {
                this->properties()[property::CLIENT_PLATFORM] = value;
            }
        } else if(key == "client_version_sign") {
            /* We currently don't check this parameter nor we need it later. Don't store it. */
        } else if(key == "hwid") {
            auto value = cmd[key].string();
            if(value.length() > 255) {
                return command_result{error::parameter_invalid, "hwid"};
            }
            this->properties()[property::CLIENT_HARDWARE_ID] = value;
        } else if(key == "client_input_muted") {
            this->properties()[property::CLIENT_INPUT_MUTED] = cmd[key].as<bool>();
        } else if(key == "client_input_hardware") {
            this->properties()[property::CLIENT_INPUT_HARDWARE] = cmd[key].as<bool>();
        } else if(key == "client_output_hardware") {
            this->properties()[property::CLIENT_OUTPUT_HARDWARE] = cmd[key].as<bool>();
        } else if(key == "client_output_muted") {
            this->properties()[property::CLIENT_OUTPUT_MUTED] = cmd[key].as<bool>();
        } else if(key == "client_default_channel") {
            auto value = cmd[key].string();
            if(value.length() > 512) {
                return command_result{error::parameter_invalid, "client_default_channel"};
            }
            this->properties()[property::CLIENT_DEFAULT_CHANNEL] = value;
        } else if(key == "client_default_channel_password") {
            auto value = cmd[key].string();
            if(value.length() > 255) {
                return command_result{error::parameter_invalid, "client_default_channel_password"};
            }
            this->properties()[property::CLIENT_DEFAULT_CHANNEL_PASSWORD] = cmd[key].string();
        } else if(key == "client_away") {
            this->properties()[property::CLIENT_AWAY] = cmd[key].as<bool>();
        } else if(key == "client_away_message") {
            auto value = cmd[key].string();
            if(value.length() > ts::config::server::limits::afk_message_length) {
                return command_result{error::parameter_invalid, "client_away_message"};
            }
            this->properties()[property::CLIENT_AWAY_MESSAGE] = value;
        } else if(key == "client_badges") {
            auto value = cmd[key].string();
            if(value.length() > 400) {
                return command_result{error::parameter_invalid, "client_badges"};
            }
            this->properties()[property::CLIENT_BADGES] = value;
        } else if(key == "client_meta_data") {
            auto value = cmd[key].string();
            if(value.length() > 65536) {
                return command_result{error::parameter_invalid, "client_meta_data"};
            }
            this->properties()[property::CLIENT_META_DATA] = value;
        } else if(key == "client_key_offset") {
            /* We don't really care about this value */
        } else if(key == "client_server_password") {
            /* We don't need to store the password. */
        } else if(key == "client_default_token") {
            auto value = cmd[key].string();
            if(value.length() > 255) {
                return command_result{error::parameter_invalid, "client_default_token"};
            }
            this->properties()[property::CLIENT_DEFAULT_TOKEN] = cmd[key].string();
        } else if(key == "client_myteamspeak_id" || key == "myTeamspeakId") {
            auto value = cmd[key].string();
            if(value.length() > 255) {
                return command_result{error::parameter_invalid, "client_myteamspeak_id"};
            }
            this->properties()[property::CLIENT_MYTEAMSPEAK_ID] = cmd[key].string();
        } else if(key == "acTime" || key == "userPubKey" || key == "authSign" || key == "pubSign" || key == "pubSignCert") {
            /* Used for the MyTeamSpeak services. We don't store them. */
        } else if(key == "client_integrations") {
            /* TS3 specific parameters. Ignore these. Length is also just guessed */
#if 0
            auto value = cmd[key].string();
            if(value.length() > 255) {
                return command_result{error::parameter_invalid, "client_integrations"};
            }
            this->properties()[property::CLIENT_INTEGRATIONS] = cmd[key].string();
#endif
        } else if(key == "client_active_integrations_info") {
            /* TS3 specific parameters. Ignore these. Length is also just guessed */
#if 0
            auto value = cmd[key].string();
            if(value.length() > 255) {
                return command_result{error::parameter_invalid, "client_active_integrations_info"};
            }
            this->properties()[property::CLIENT_ACTIVE_INTEGRATIONS_INFO] = cmd[key].string();
#endif
        } else if(key == "client_browser_engine") {
            /* Currently not really used but passed by the web client */
        } else {
            debugMessage(this->getServerId(), "{} Received unknown clientinit parameter {}. Ignoring it.", this->getLoggingPrefix(), key);
        }
    }

    return ts::command_result{error::ok};
}

command_result SpeakingClient::resolveClientInitBan() {
    auto active_ban = this->resolveActiveBan(this->getPeerIp());
    if(!active_ban) {
        return ts::command_result{error::ok};
    }

    logMessage(this->getServerId(), "{} Disconnecting while init because of ban record. Record id {} at server {}",
               CLIENT_STR_LOG_PREFIX,
               active_ban->banId,
               active_ban->serverId);
    serverInstance->banManager()->trigger_ban(active_ban, this->getServerId(), this->getUid(), this->getHardwareId(), this->getDisplayName(), this->getPeerIp());

    string fullReason = string() + "You are banned " + (active_ban->serverId == 0 ? "globally" : "from this server") + ". Reason: \"" + active_ban->reason + "\". Ban expires ";

    string time;
    if(active_ban->until.time_since_epoch().count() != 0) {
        time += "in ";
        auto seconds = chrono::ceil<chrono::seconds>(active_ban->until - chrono::system_clock::now()).count();
        tm p{};
        memset(&p, 0, sizeof(p));

        while(seconds >= 365 * 24 * 60 * 60){
            p.tm_year++;
            seconds -= 365 * 24 * 60 * 60;
        }

        while(seconds >= 24 * 60 * 60){
            p.tm_yday++;
            seconds -= 24 * 60 * 60;
        }

        while(seconds >= 60 * 60){
            p.tm_hour++;
            seconds -= 60 * 60;
        }

        while(seconds >= 60){
            p.tm_min++;
            seconds -= 60;
        }
        p.tm_sec = (int) seconds;

        if(p.tm_year > 0) {
            time += to_string(p.tm_year) + " years, ";
        }

        if(p.tm_yday > 0) {
            time += to_string(p.tm_yday) + " days, ";
        }

        if(p.tm_hour > 0) {
            time += to_string(p.tm_hour) + " hours, ";
        }

        if(p.tm_min > 0) {
            time += to_string(p.tm_min) + " minutes, ";
        }

        if(p.tm_sec > 0) {
            time += to_string(p.tm_sec) + " seconds, ";
        }

        if(time.empty()) time = "now, ";
        time = time.substr(0, time.length() - 2);
    } else time = "never";
    fullReason += time + "!";

    return command_result{error::server_connect_banned, fullReason};
}

command_result SpeakingClient::handleCommandClientInit(Command& cmd) {
    TIMING_START(timings);

    /* Firstly check if the client tries to join flood */
    {
        lock_guard lock{this->server->join_attempts_lock};
        auto client_address = this->getPeerIp();
        auto& client_join_attempts = this->server->join_attempts[client_address];
        auto& general_join_attempts = this->server->join_attempts["_"];

        if(config::voice::clientConnectLimit > 0 && client_join_attempts + 1 > config::voice::clientConnectLimit) {
            return command_result{error::client_join_rate_limit_reached};
        }

        if(config::voice::connectLimit > 0 && general_join_attempts + 1 > config::voice::connectLimit) {
            return command_result{error::server_join_rate_limit_reached};
        }

        client_join_attempts++;
        general_join_attempts++;
    }

    /* Check if we've enabled the modal quit modal. If so just deny the join in general */
    {
        auto host_message_mode = this->server->properties()[property::VIRTUALSERVER_HOSTMESSAGE_MODE].as_or<int>(0);
        auto quit_message = this->server->properties()[property::VIRTUALSERVER_HOSTMESSAGE].value();

        if(host_message_mode == 3) {
            return ts::command_result{error::server_modal_quit, quit_message};
        }
    }

    TIMING_STEP(timings, "join limit check");

    if(!DatabaseHelper::assignDatabaseId(this->server->getSql(), this->server->getServerId(), this->ref())) {
        return command_result{error::vs_critical, "Could not assign database id!"};
    }

    {
        auto result{this->applyClientInitParameters(cmd)};
        if(result.has_error()) {
            return result;
        }
        result.release_data();
    }

    TIMING_STEP(timings, "state load (db) ");

    ClientPermissionCalculator permission_calculator{this, nullptr};

    /* Check if the target client ip address has been denied */
    if(geoloc::provider_vpn && !permission_calculator.permission_granted(permission::b_client_ignore_vpn, 1)) {
        auto provider = this->isAddressV4() ? geoloc::provider_vpn->resolveInfoV4(this->getPeerIp(), true) : geoloc::provider_vpn->resolveInfoV6(this->getPeerIp(), true);
        if(provider) {
            auto message = strvar::transform(ts::config::messages::kick_vpn, strvar::StringValue{"provider.name", provider->name}, strvar::StringValue{"provider.website", provider->side});
            return command_result{error::server_connect_banned, message};
        }

        TIMING_STEP(timings, "VPN block test  ");
    }

    /*
     * Check if the supplied client hardware id is correct (only applies for TeamSpeak 3 clients)
     * This has been created due to Bluescrems request but I think we might want to drop this.
     */
    if(this->getType() == ClientType::CLIENT_TEAMSPEAK) {
        if(permission_calculator.permission_granted(permission::b_client_enforce_valid_hwid, 1)) {
            auto hardware_id = this->properties()[property::CLIENT_HARDWARE_ID].value();
            if(
                    !std::regex_match(hardware_id, regex_hwid_windows) &&
                    !std::regex_match(hardware_id, regex_hwid_unix) &&
                    !std::regex_match(hardware_id, regex_hwid_android) &&
                    !std::regex_match(hardware_id, regex_hwid_ios)
            ) {
                return command_result{error::parameter_invalid, config::messages::kick_invalid_hardware_id};
            }
        }
        TIMING_STEP(timings, "hwid check      ");
    }

    /* Check if the client has supplied a valid password or permissions to ignore it */
    if(!this->server->verifyServerPassword(cmd["client_server_password"].string(), true)) {
        if(!permission_calculator.permission_granted(permission::b_virtualserver_join_ignore_password, 1)) {
            return command_result{error::server_invalid_password};
        }
    }
    TIMING_STEP(timings, "server password ");

    /* Maximal client connected clones */
    if(!config::server::clients::ignore_max_clone_permissions) {
        size_t clones_hardware_id{0};
        size_t clones_unique_id{0};
        size_t clones_ip{0};

        auto own_hardware_id = this->getHardwareId();
        auto own_unique_id = this->getUid();
        auto own_ip = this->getPeerIp();

        this->server->forEachClient([&](const shared_ptr<ConnectedClient>& client) {
            if(&*client == this) {
                return;
            }

            switch(client->getType()) {
                case ClientType::CLIENT_TEASPEAK:
                case ClientType::CLIENT_TEAMSPEAK:
                case ClientType::CLIENT_WEB:
                    break;

                case ClientType::CLIENT_MUSIC:
                case ClientType::CLIENT_INTERNAL:
                case ClientType::CLIENT_QUERY:
                    return;

                case ClientType::MAX:
                default:
                    assert(false);
                    return;
            }

            if(client->getUid() == this->getUid()) {
                clones_unique_id++;
            }

            if(client->getPeerIp() == this->getPeerIp()) {
                clones_ip++;
            }

            if(client->getHardwareId() == own_hardware_id) {
                clones_hardware_id++;
            }
        });

        if(clones_unique_id) {
            auto max_clones = permission_calculator.calculate_permission(permission::i_client_max_clones_uid);
            if(max_clones.has_value && !permission::v2::permission_granted(clones_unique_id, max_clones)) {
                return command_result{error:: client_too_many_clones_connected, "too many clones connected (unique id)"};
            }
        }

        if(clones_ip) {
            auto max_clones = permission_calculator.calculate_permission(permission::i_client_max_clones_ip);
            if(max_clones.has_value && !permission::v2::permission_granted(clones_ip, max_clones)) {
                return command_result{error:: client_too_many_clones_connected, "too many clones connected (ip)"};
            }
        }

        /* If we have no hardware id don't count any clones */
        if(clones_hardware_id && !own_hardware_id.empty()) {
            auto max_clones = permission_calculator.calculate_permission(permission::i_client_max_clones_hwid);
            if(max_clones.has_value && !permission::v2::permission_granted(clones_hardware_id, max_clones)) {
                return command_result{error:: client_too_many_clones_connected, "too many clones connected (hardware id)"};
            }
        }
    }
    TIMING_STEP(timings, "max client clone");

    /* Resolve active bans and deny the connect */
    if(!permission_calculator.permission_granted(permission::b_client_ignore_bans, 1)) {
        auto result{this->resolveClientInitBan()};
        if(result.has_error()) {
            return result;
        }
        result.release_data();
    }
    TIMING_STEP(timings, "active ban test ");

    /* Check if the server might be full */
    {
        size_t online_client_count{0};
        for(const auto &cl : this->server->getClients()) {
            switch(cl->getType()) {
                case ClientType::CLIENT_TEASPEAK:
                case ClientType::CLIENT_TEAMSPEAK:
                case ClientType::CLIENT_WEB:
                    switch(cl->connectionState()) {
                        case ConnectionState::CONNECTED:
                        case ConnectionState::INIT_HIGH:
                            online_client_count++;
                            break;

                        case ConnectionState::INIT_LOW:
                        case ConnectionState::DISCONNECTING:
                        case ConnectionState::DISCONNECTED:
                            break;

                        case ConnectionState::UNKNWON:
                        default:
                            assert(false);
                            continue;
                    }
                    break;

                case ClientType::CLIENT_MUSIC:
                case ClientType::CLIENT_INTERNAL:
                case ClientType::CLIENT_QUERY:
                    break;

                case ClientType::MAX:
                default:
                    assert(false);
                    break;
            }
        }

        auto server_client_limit = this->server->properties()[property::VIRTUALSERVER_MAXCLIENTS].as_or<size_t>(0);
        auto server_reserved_slots = this->server->properties()[property::VIRTUALSERVER_RESERVED_SLOTS].as_or<size_t>(0);

        auto normal_slots = server_reserved_slots >= server_client_limit ? 0 : server_client_limit - server_reserved_slots;
        if(normal_slots <= online_client_count) {
            if(server_client_limit <= online_client_count || !permission_calculator.permission_granted(permission::b_client_use_reserved_slot, 1)) {
                return command_result{error::server_maxclients_reached};
            }
        }
    }
    TIMING_STEP(timings, "server slot test");


    /* Update the last connected and total connected statistics and send a permission list update if the server has been updated */
    {
        auto old_last_connected = this->properties()[property::CLIENT_LASTCONNECTED].as_or<int64_t>(0);
        this->properties()[property::CLIENT_LASTCONNECTED] = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        this->properties()[property::CLIENT_TOTALCONNECTIONS].increment_by<uint64_t>(1);

        auto time_point = system_clock::time_point() + seconds(old_last_connected);
        if(time_point < build::version()->timestamp) {
            logMessage(this->getServerId(), "{} Client may cached a old permission list (Server is newer than the client's last join)", CLIENT_STR_LOG_PREFIX);
            Command _dummy("dummy_permissionslist");
            this->handleCommandPermissionList(_dummy);
        }
    }

    /* Update the clients IP address within the database */
    serverInstance->databaseHelper()->updateClientIpAddress(this->getServerId(), this->getClientDatabaseId(), this->getLoggingPeerIp());
    TIMING_STEP(timings, "con stats update");

    this->processJoin();
    TIMING_STEP(timings, "join client     ");

    debugMessage(this->getServerId(), "{} Client init timings: {}", CLIENT_STR_LOG_PREFIX, TIMING_FINISH(timings));
    return command_result{error::ok};
}

void SpeakingClient::processJoin() {
    TIMING_START(timings);
    this->server->registerClient(this->ref());
    {
        if(this->rtc_client_id) {
            /* in case of client reconnect */
            this->server->rtc_server().destroy_client(this->rtc_client_id);
        }

        std::string error{};
        this->rtc_client_id = this->server->rtc_server().create_client(dynamic_pointer_cast<SpeakingClient>(this->ref()));

        if(auto voice_client{dynamic_cast<VoiceClient*>(this)}; voice_client) {
            if(!this->server->rtc_server().initialize_native_connection(error, this->rtc_client_id)) {
                logCritical(this->getServerId(), "{} Native connection setup failed: {}", CLIENT_STR_LOG_PREFIX, error);
            }
        }
        if(this->getType() == ClientType::CLIENT_WEB || this->getType() == ClientType::CLIENT_TEASPEAK) {
            if(!this->server->rtc_server().initialize_rtc_connection(error, this->rtc_client_id)) {
                logCritical(this->getServerId(), "{} RTC connection setup failed: {}", CLIENT_STR_LOG_PREFIX, error);
            } else {
                this->rtc_session_pending_describe = true;
            }
        }
    }

    TIMING_STEP(timings, "server reg ");

    this->properties()[property::CLIENT_COUNTRY] = config::geo::countryFlag;
    if(geoloc::provider) {
        auto loc = this->isAddressV4() ? geoloc::provider->resolveInfoV4(this->getPeerIp(), false) : geoloc::provider->resolveInfoV6(this->getPeerIp(), false);
        if(loc) {
            debugMessage(this->getServerId(), "{} Resolved ip address to {}/{}", this->getLoggingPrefix(), loc->name, loc->identifier);
            this->properties()[property::CLIENT_COUNTRY] = loc->identifier;
        } else {
            debugMessage(this->getServerId(), "{} Could not resolve IP info for {}", this->getLoggingPrefix(), this->getLoggingPeerIp());
        }
    }

    TIMING_STEP(timings, "ip 2 loc as"); //IP to location assignment
    this->sendServerInit();
    debugMessage(this->getServerId(), "Client id: " + to_string(this->getClientId()));
    TIMING_STEP(timings, "notify sini");

    if(auto token{this->properties()[property::CLIENT_DEFAULT_TOKEN].value()}; !token.empty()){
        auto token_info = this->server->getTokenManager().load_token(token, true);
        if(token_info) {
            if(token_info->is_expired()) {
                debugMessage(this->getServerId(), "{} Client tried to use an expired token {}", this->getLoginName(), token);
            } else if(token_info->use_limit_reached()) {
                debugMessage(this->getServerId(), "{} Client tried to use an token which reached the use limit {}", this->getLoginName(), token);
            } else {
                debugMessage(this->getServerId(), "{} Client used token {}", this->getLoginName(), token);
                this->server->getTokenManager().log_token_use(token_info->id);
                this->useToken(token_info->id);
            }
        } else {
            debugMessage(this->getServerId(), "{} Client tried to use an unknown token {}", token);
        }

        /* Clear out the value. We don't need the default token any more */
        this->properties()[property::CLIENT_DEFAULT_TOKEN] = "";
    }
    TIMING_STEP(timings, "token use  ");


    if(!this->server->assignDefaultChannel(this->ref(), false)) {
        auto result = command_result{error::vs_critical, "Could not assign default channel!"};
        this->notifyError(result);
        result.release_data();
        this->close_connection(system_clock::now() + seconds(1));
        return;
    }
    TIMING_STEP(timings, "assign chan");

    this->sendChannelList(true);
    this->state = ConnectionState::CONNECTED;
    TIMING_STEP(timings, "send chan t");

    /* trick the join method */
    auto channel = this->currentChannel;
    this->currentChannel = nullptr;
    {
        /* enforce an update of these properties */
        this->properties()[property::CLIENT_CHANNEL_GROUP_INHERITED_CHANNEL_ID] = "0";
        this->properties()[property::CLIENT_CHANNEL_GROUP_ID] = "0";
        this->properties()[property::CLIENT_TALK_POWER] = "0";

        unique_lock server_channel_lock(this->server->channel_tree_mutex);
        this->server->client_move(this->ref(), channel, nullptr, "", ViewReasonId::VREASON_USER_ACTION, false, server_channel_lock);
        if(this->getType() != ClientType::CLIENT_TEAMSPEAK) {
            std::lock_guard own_channel_lock{this->channel_tree_mutex};
            this->subscribeChannel({this->currentChannel}, false, true); /* su "improve" the TS3 clients join speed we send the channel clients a bit later, when the TS3 client gets his own client variables */
        }
    }
    TIMING_STEP(timings, "join move  ");

    this->properties()->triggerAllModified();
    {
        std::optional<ts::command_builder> generated_notify{};
        this->notifyServerGroupList(generated_notify, true);
    }
    {
        std::optional<ts::command_builder> generated_notify{};
        this->notifyChannelGroupList(generated_notify, true);
    }
    TIMING_STEP(timings, "notify grou");
    logMessage(this->getServerId(), "Voice client {}/{} ({}) from {} joined.",
            this->getClientDatabaseId(),
            this->getUid(),
            this->getDisplayName(),
            this->getLoggingPeerIp() + ":" + to_string(this->getPeerPort())
    );

    this->connectTimestamp = chrono::system_clock::now();
    this->idleTimestamp = chrono::system_clock::now();

    TIMING_STEP(timings, "welcome msg");
    {
        std::string message{};
        config::server::clients::WelcomeMessageType type{config::server::clients::WELCOME_MESSAGE_TYPE_NONE};
        if(this->getType() == ClientType::CLIENT_TEASPEAK) {
            message = config::server::clients::extra_welcome_message_teaspeak;
            type = config::server::clients::extra_welcome_message_type_teaspeak;
        } else if(this->getType() == ClientType::CLIENT_TEAMSPEAK) {
            message = config::server::clients::extra_welcome_message_teamspeak;
            type = config::server::clients::extra_welcome_message_type_teamspeak;
        } else if(this->getType() == ClientType::CLIENT_WEB) {
            message = config::server::clients::extra_welcome_message_teaweb;
            type = config::server::clients::extra_welcome_message_type_teaweb;
        }

        if(type == config::server::clients::WELCOME_MESSAGE_TYPE_POKE) {
            this->notifyClientPoke(this->server->serverRoot, message);
        } else if(type == config::server::clients::WELCOME_MESSAGE_TYPE_CHAT) {
            this->notifyTextMessage(ChatMessageMode::TEXTMODE_SERVER, this->server->serverRoot, 0, 0, std::chrono::system_clock::now(), message);
        }
    }
    debugMessage(this->getServerId(), "{} Client join timings: {}", CLIENT_STR_LOG_PREFIX, TIMING_FINISH(timings));

    this->join_whitelisted_channel.clear();
    serverInstance->action_logger()->client_channel_logger.log_client_join(this->getServerId(), this->ref(), this->getChannelId(), this->currentChannel->name());
}

void SpeakingClient::processLeave() {
    auto ownLock = this->ref();
    auto server = this->getServer();


    auto channel = this->currentChannel;
    if(server) {
        if(this->rtc_client_id) {
            server->rtc_server().destroy_client(this->rtc_client_id);
            this->rtc_client_id = 0;
        }

        logMessage(this->getServerId(), "Voice client {}/{} ({}) from {} left.", this->getClientDatabaseId(), this->getUid(), this->getDisplayName(), this->getLoggingPeerIp() + ":" + to_string(this->getPeerPort()));
        {
            std::unique_lock server_channel_lock(server->channel_tree_mutex);
            server->unregisterClient(ownLock, "disconnected", server_channel_lock);
        }
        server->music_manager_->cleanup_client_bots(this->getClientDatabaseId());
        //ref_server = nullptr; Removed caused nullptr exceptions
    }
}

void SpeakingClient::triggerVoiceEnd() {
    this->properties()[property::CLIENT_FLAG_TALKING] = false;
}

void SpeakingClient::updateSpeak(bool only_update, const std::chrono::system_clock::time_point &now) {
    std::lock_guard speak_lock{this->speak_mutex};

    if(this->speak_last_packet + this->speak_accuracy < now) {
        if(this->speak_last_packet > this->speak_begin) {
            if(!this->properties()[property::CLIENT_FLAG_TALKING].as_or<bool>(false)) {
                this->properties()[property::CLIENT_FLAG_TALKING] = true;
            }

            this->speak_time += duration_cast<milliseconds>(this->speak_last_packet - this->speak_begin);
        } else {
            if(this->properties()[property::CLIENT_FLAG_TALKING].as_or<bool>(false)) {
                this->properties()[property::CLIENT_FLAG_TALKING] = false;
            }
        }

        this->speak_begin = now;
        this->speak_last_packet = now;
    }

    if(!only_update) {
        this->speak_last_packet = now;
    }
}

void SpeakingClient::tick_server(const std::chrono::system_clock::time_point &time) {
    ConnectedClient::tick_server(time);
    ALARM_TIMER(A1, "SpeakingClient::tick", milliseconds(2));
    this->updateSpeak(true, time);

    if(this->state == ConnectionState::CONNECTED) {
        if(this->max_idle_time.has_value) {
            auto max_idle = this->max_idle_time.value;
            if(max_idle > 0 && this->idleTimestamp.time_since_epoch().count() > 0 && duration_cast<seconds>(time - this->idleTimestamp).count() > max_idle) {
                this->server->notify_client_kick(this->ref(), this->server->getServerRoot(), ts::config::messages::idle_time_exceeded, nullptr);
                this->close_connection(system_clock::now() + seconds(1));
            }
        }

    }
}

void SpeakingClient::updateChannelClientProperties(bool channel_lock, bool notify) {
    ConnectedClient::updateChannelClientProperties(channel_lock, notify);
    this->max_idle_time = this->calculate_permission(permission::i_client_max_idletime, this->currentChannel ? this->currentChannel->channelId() : 0);
}

command_result SpeakingClient::handleCommand(Command &command) {
    if(this->connectionState() == ConnectionState::INIT_HIGH) {
        if(this->handshake.state == HandshakeState::BEGIN || this->handshake.state == HandshakeState::IDENTITY_PROOF) {
            command_result result;
            if(command.command() == "handshakebegin") {
                result.reset(this->handleCommandHandshakeBegin(command));
            } else if(command.command() == "handshakeindentityproof") {
                result.reset(this->handleCommandHandshakeIdentityProof(command));
            } else {
                result.reset(command_result{error::client_not_logged_in});
            }

            if(result.has_error()) {
                this->postCommandHandler.push_back([&]{
                    this->close_connection(system_clock::now() + seconds(1));
                });
            }
            return result;
        }
    } else if(this->connectionState() == ConnectionState::CONNECTED) {
        if(command.command() == "rtcsessiondescribe") {
            return this->handleCommandRtcSessionDescribe(command);
        } else if(command.command() == "rtcicecandidate") {
            return this->handleCommandRtcIceCandidate(command);
        } else if(command.command() == "rtcsessionreset") {
            return this->handleCommandRtcSessionReset(command);
        } else if(command.command() == "broadcastaudio") {
            return this->handleCommandBroadcastAudio(command);
        } else if(command.command() == "broadcastvideo") {
            return this->handleCommandBroadcastVideo(command);
        } else if(command.command() == "broadcastvideojoin") {
            return this->handleCommandBroadcastVideoJoin(command);
        } else if(command.command() == "broadcastvideoleave") {
            return this->handleCommandBroadcastVideoLeave(command);
        } else if(command.command() == "broadcastvideoconfig") {
            return this->handleCommandBroadcastVideoConfig(command);
        } else if(command.command() == "broadcastvideoconfigure") {
            return this->handleCommandBroadcastVideoConfigure(command);
        }
    }
    return ConnectedClient::handleCommand(command);
}

command_result SpeakingClient::handleCommandRtcSessionDescribe(Command &command) {
    CMD_REQ_SERVER;
    if(this->rtc_session_pending_describe) {
        this->rtc_session_pending_describe = false;
    } else {
        CMD_CHK_AND_INC_FLOOD_POINTS(15);
    }

    uint32_t mode;
    if(command["mode"].string() == "offer") {
        mode = 1;
    } else if(command["mode"].string() == "answer") {
        mode = 2;
    } else {
        return command_result{error::parameter_invalid, "mode"};
    }

    std::string error{};
    if(!this->server->rtc_server().apply_remote_description(error, this->rtc_client_id, mode, command["sdp"])) {
        return command_result{error::vs_critical, error};
    }

    if(mode == 1) {
        std::string result{};
        if(!this->server->rtc_server().generate_local_description(this->rtc_client_id, result)) {
            return command_result{error::vs_critical, result};
        } else {
            ts::command_builder notify{"notifyrtcsessiondescription"};
            notify.put_unchecked(0, "mode", "answer");
            notify.put_unchecked(0, "sdp", result);
            this->sendCommand(notify);
        }
    }

    return command_result{error::ok};
}

command_result SpeakingClient::handleCommandRtcSessionReset(Command &command) {
    CMD_REQ_SERVER;
    CMD_CHK_AND_INC_FLOOD_POINTS(15);

    this->server->rtc_server().reset_rtp_session(this->rtc_client_id);
    if(this->getType() == ClientType::CLIENT_TEASPEAK) {
        /* registering the broadcast again since rtp session reset resets the broadcasts as well */
        this->server->rtc_server().start_broadcast_audio(this->rtc_client_id, 1);
    }
    return command_result{error::ok};
}

command_result SpeakingClient::handleCommandRtcIceCandidate(Command &command) {
    CMD_REQ_SERVER;

    std::string error;
    if(command[0].has("candidate")) {
        auto candidate = command["candidate"].string();
        if(!this->server->rtc_server().add_ice_candidate(error, this->rtc_client_id, command["media_line"], candidate)) {
            return command_result{error::vs_critical, error};
        }
    } else {
        this->server->rtc_server().ice_candidates_finished(this->rtc_client_id);
    }
    return command_result{error::ok};
}

ts::command_result parse_broadcast_options(ParameterBulk &cmd, VideoBroadcastOptions& options, bool requires_all) {
    if(cmd.has("broadcast_bitrate_max")) {
        options.update_mask |= VideoBroadcastOptions::kOptionBitrate;
        options.bitrate = cmd["broadcast_bitrate_max"];
    } else if(requires_all) {
        return ts::command_result{error::parameter_missing, "broadcast_bitrate_max"};
    }

    if(cmd.has("broadcast_keyframe_interval")) {
        options.update_mask |= VideoBroadcastOptions::kOptionKeyframeInterval;
        options.keyframe_interval = cmd["broadcast_keyframe_interval"];
    } else if(requires_all) {
        return ts::command_result{error::parameter_missing, "broadcast_keyframe_interval"};
    }

    return ts::command_result{error::ok};
}

void simplify_broadcast_options(const VideoBroadcastOptions& current_options, VideoBroadcastOptions& target_options) {
    if(target_options.bitrate == current_options.bitrate) {
        target_options.update_mask &= ~VideoBroadcastOptions::kOptionBitrate;
    }

    if(target_options.keyframe_interval == current_options.keyframe_interval) {
        target_options.update_mask &= ~VideoBroadcastOptions::kOptionKeyframeInterval;
    }
}

/**
 * Test if the client has permissions to use the target broadcast options
 * @param client
 * @param channel_id
 * @param options
 * @return
 */
ts::command_result test_broadcast_options(SpeakingClient& client, ChannelId channel_id, const VideoBroadcastOptions& options) {
    if(options.update_mask & VideoBroadcastOptions::kOptionBitrate) {
        auto required_value = options.bitrate == 0 ? -1 : (permission::PermissionValue) (options.bitrate / 1000);
        if(!permission::v2::permission_granted(required_value, client.calculate_permission(permission::i_video_max_kbps, channel_id))) {
            return ts::command_result{permission::i_video_max_kbps};
        }
    }

    return ts::command_result{error::ok};
}

inline command_result broadcast_start_result_to_command_result(rtc::BroadcastStartResult broadcast_result) {
    switch(broadcast_result) {
        case rtc::BroadcastStartResult::Success:
            return ts::command_result{error::ok};
        case rtc::BroadcastStartResult::InvalidBroadcastType:
            return ts::command_result{error::parameter_invalid, "type"};
        case rtc::BroadcastStartResult::InvalidStreamId:
            return ts::command_result{error::rtc_missing_target_channel};
        case rtc::BroadcastStartResult::ClientHasNoChannel:
            return ts::command_result{error::vs_critical, "no channel"};
        case rtc::BroadcastStartResult::InvalidClient:
            return ts::command_result{error::vs_critical, "invalid client"};
        case rtc::BroadcastStartResult::UnknownError:
        default:
            return ts::command_result{error::vs_critical, "unknown error"};
    }
}

command_result SpeakingClient::handleCommandBroadcastAudio(Command &command) {
    CMD_REQ_SERVER;
    CMD_CHK_AND_INC_FLOOD_POINTS(5);

    auto ssrc = command[0].has("ssrc") ? command["ssrc"].as<uint32_t>() : (uint32_t) 0;
    auto broadcast_result = this->server->rtc_server().start_broadcast_audio(this->rtc_client_id, ssrc);
    return broadcast_start_result_to_command_result(broadcast_result);
}

command_result SpeakingClient::handleCommandBroadcastVideo(Command &command) {
    CMD_REQ_SERVER;
    CMD_CHK_AND_INC_FLOOD_POINTS(15);

    auto ssrc = command[0].has("ssrc") ? command["ssrc"].as<uint32_t>() : (uint32_t) 0;
    auto type = (rtc::VideoBroadcastType) command["type"].as<uint8_t>();

    switch(type) {
        case rtc::VideoBroadcastType::Screen:
            if(!permission::v2::permission_granted(1, this->calculate_permission(permission::b_video_screen, this->getChannelId()))) {
                return ts::command_result{permission::b_video_screen};
            }

            break;


        case rtc::VideoBroadcastType::Camera:
            if(!permission::v2::permission_granted(1, this->calculate_permission(permission::b_video_camera, this->getChannelId()))) {
                return ts::command_result{permission::b_video_camera};
            }

            break;

        default:
            return ts::command_result{error::parameter_invalid, "type"};
    }

    VideoBroadcastOptions options;
    memset(&options, 0, sizeof(options));
    if(ssrc != 0) {
        ts::command_result result;

        result.reset(parse_broadcast_options(command[0], options, true));
        if(result.has_error()) {
            return result;
        }

        result.reset(test_broadcast_options(*this, this->getChannelId(), options));
        if(result.has_error()) {
            return result;
        }

        result.release_data();
    }

    auto broadcast_result = this->server->rtc_server().start_broadcast_video(this->rtc_client_id, type, ssrc, &options);
    return broadcast_start_result_to_command_result(broadcast_result);
}

command_result SpeakingClient::handleCommandBroadcastVideoJoin(Command &cmd) {
    CMD_REQ_SERVER;

    auto broadcast_type = (rtc::VideoBroadcastType) cmd["bt"].as<uint8_t>();
    auto broadcast_id = cmd["bid"].as<uint32_t>();

    if(broadcast_id != this->rtc_client_id) {
        CMD_CHK_AND_INC_FLOOD_POINTS(25);

        /* the broadcast is is actually the rtc client id of the broadcaster */
        uint32_t camera_streams, screen_streams;
        if(!this->server->rtc_server().client_video_stream_count(this->rtc_client_id, &camera_streams, &screen_streams)) {
            return ts::command_result{error::vs_critical, "failed to count client streams"};
        }

        auto permission_max_streams = this->calculate_permission(permission::i_video_max_streams, this->getChannelId());
        if(permission_max_streams.has_value) {
            if(!permission::v2::permission_granted(camera_streams + screen_streams, permission_max_streams)) {
                return ts::command_result{permission::i_video_max_streams};
            }
        }

        switch(broadcast_type) {
            case rtc::VideoBroadcastType::Camera: {
                const auto permission_max_camera_streams = this->calculate_permission(permission::i_video_max_camera_streams, this->getChannelId());
                if(permission_max_camera_streams.has_value) {
                    if(!permission::v2::permission_granted(camera_streams, permission_max_camera_streams)) {
                        return ts::command_result{permission::i_video_max_camera_streams};
                    }
                }
                break;
            }

            case rtc::VideoBroadcastType::Screen: {
                const auto permission_max_screen_streams = this->calculate_permission(permission::i_video_max_camera_streams, this->getChannelId());
                if(permission_max_screen_streams.has_value) {
                    if(!permission::v2::permission_granted(screen_streams, permission_max_screen_streams)) {
                        return ts::command_result{permission::i_video_max_screen_streams};
                    }
                }
                break;
            }

            default:
                return ts::command_result{error::broadcast_invalid_type};
        }
    } else {
        CMD_CHK_AND_INC_FLOOD_POINTS(5);
        /* The client is free to join his own broadcast */
    }

    using VideoBroadcastJoinResult = rtc::VideoBroadcastJoinResult;
    switch(this->server->rtc_server().join_video_broadcast(this->rtc_client_id, broadcast_id, broadcast_type)) {
        case VideoBroadcastJoinResult::Success:
            return ts::command_result{error::ok};

        case VideoBroadcastJoinResult::InvalidBroadcast:
            return ts::command_result{error::broadcast_invalid_id};

        case VideoBroadcastJoinResult::InvalidBroadcastType:
            return ts::command_result{error::broadcast_invalid_type};

        case VideoBroadcastJoinResult::InvalidClient:
            return ts::command_result{error::client_invalid_id};

        case VideoBroadcastJoinResult::UnknownError:
        default:
            return ts::command_result{error::vs_critical};
    }
}

command_result SpeakingClient::handleCommandBroadcastVideoLeave(Command &cmd) {
    CMD_REQ_SERVER;

    auto broadcast_type = (rtc::VideoBroadcastType) cmd["bt"].as<uint8_t>();
    auto broadcast_id = cmd["bid"].as<uint32_t>();

    this->server->rtc_server().leave_video_broadcast(this->rtc_client_id, broadcast_id, broadcast_type);
    return ts::command_result{error::ok};
}

command_result SpeakingClient::handleCommandBroadcastVideoConfig(Command &cmd) {
    CMD_REQ_SERVER;

    auto broadcast_type = (rtc::VideoBroadcastType) cmd["bt"].as<uint8_t>();

    VideoBroadcastOptions options;
    auto result = this->server->rtc_server().client_broadcast_video_config(this->rtc_client_id, broadcast_type, &options);
    switch(result) {
        case rtc::VideoBroadcastConfigureResult::Success:
            break;

        case rtc::VideoBroadcastConfigureResult::InvalidBroadcast:
            return ts::command_result{error::broadcast_invalid_id};

        case rtc::VideoBroadcastConfigureResult::InvalidBroadcastType:
            return ts::command_result{error::broadcast_invalid_type};

        case rtc::VideoBroadcastConfigureResult::InvalidClient:
            return ts::command_result{error::client_invalid_id};

        case rtc::VideoBroadcastConfigureResult::UnknownError:
        default:
            return ts::command_result{error::vs_critical};
    }

    ts::command_builder notify{this->notify_response_command("notifybroadcastvideoconfig")};
    notify.put_unchecked(0, "bt", (uint8_t) broadcast_type);
    notify.put_unchecked(0, "broadcast_keyframe_interval", options.keyframe_interval);
    notify.put_unchecked(0, "broadcast_bitrate_max", options.bitrate);
    this->sendCommand(notify);

    return ts::command_result{error::ok};
}

inline command_result broadcast_config_result_to_command_result(rtc::VideoBroadcastConfigureResult result) {
    switch(result) {
        case rtc::VideoBroadcastConfigureResult::Success:
            return ts::command_result{error::ok};

        case rtc::VideoBroadcastConfigureResult::InvalidBroadcast:
            return ts::command_result{error::broadcast_invalid_id};

        case rtc::VideoBroadcastConfigureResult::InvalidBroadcastType:
            return ts::command_result{error::broadcast_invalid_type};

        case rtc::VideoBroadcastConfigureResult::InvalidClient:
            return ts::command_result{error::client_invalid_id};

        case rtc::VideoBroadcastConfigureResult::UnknownError:
        default:
            return ts::command_result{error::vs_critical};
    }
}

command_result SpeakingClient::handleCommandBroadcastVideoConfigure(Command &cmd) {
    CMD_REQ_SERVER;

    auto broadcast_type = (rtc::VideoBroadcastType) cmd["bt"].as<uint8_t>();

    VideoBroadcastOptions current_options;
    auto query_result = this->server->rtc_server().client_broadcast_video_config(this->rtc_client_id, broadcast_type, &current_options);
    if(query_result != rtc::VideoBroadcastConfigureResult::Success) {
        return broadcast_config_result_to_command_result(query_result);
    }

    VideoBroadcastOptions options;
    memset(&options, 0, sizeof(options));
    {
        ts::command_result result;

        result.reset(parse_broadcast_options(cmd[0], options, false));
        if(result.has_error()) {
            return result;
        }

        simplify_broadcast_options(current_options, options);
        result.reset(test_broadcast_options(*this, this->getChannelId(), options));
        if(result.has_error()) {
            return result;
        }

        result.release_data();
    }

    auto result = this->server->rtc_server().client_broadcast_video_configure(this->rtc_client_id, broadcast_type, &options);
    return broadcast_config_result_to_command_result(result);
}