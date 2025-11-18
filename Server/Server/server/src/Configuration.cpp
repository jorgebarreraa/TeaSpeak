#include <utility>

#include <log/LogUtils.h>
#include "Configuration.h"
#include "build.h"
#include "../../license/shared/include/license/license.h"
#include <yaml-cpp/yaml.h>
#include <fstream>
#include <deque>
#include <log/LogUtils.h>
#include <regex>
#include <src/geo/GeoLocation.h>
#include <misc/strobf.h>

#define _stringify(x) #x
#define stringify(x) _stringify(x)

using namespace std;
using namespace std::chrono;
using namespace ts;
using namespace ts::config;

//Variable define
std::string config::database::url;
std::string config::database::sqlite::locking_mode;
std::string config::database::sqlite::journal_mode;
std::string config::database::sqlite::sync_mode;

std::shared_ptr<license::License> config::license;
std::shared_ptr<license::License> config::license_original;
bool config::experimental_31 = false;
std::string config::permission_mapping_file;

bool ts::config::binding::enforce_default_voice_host = false;
std::string ts::config::binding::DefaultVoiceHost;
std::string ts::config::binding::DefaultWebHost;
std::string ts::config::binding::DefaultQueryHost;
std::string ts::config::binding::DefaultFileHost;
uint16_t ts::config::binding::DefaultQueryPort;
uint16_t ts::config::binding::DefaultFilePort;

std::string config::server::DefaultServerVersion;
std::string config::server::DefaultServerPlatform;
LicenseType config::server::DefaultServerLicense;
bool config::server::strict_ut8_mode;
bool config::server::show_invisible_clients_as_online;
bool config::server::disable_ip_saving;
bool config::server::default_music_bot;
/*
 * namespace limits {
            extern size_t poke_message_length;
            extern size_t talk_power_request_message_length;
        }
 */
size_t config::server::limits::poke_message_length;
size_t config::server::limits::talk_power_request_message_length;
size_t config::server::limits::afk_message_length;

ssize_t config::server::max_virtual_server;
bool config::server::badges::allow_badges;
bool config::server::badges::allow_overwolf;
bool config::server::authentication::name;

bool config::server::clients::teamspeak;
std::string config::server::clients::extra_welcome_message_teamspeak;
std::string config::server::clients::teamspeak_not_allowed_message;
config::server::clients::WelcomeMessageType config::server::clients::extra_welcome_message_type_teamspeak;

bool config::server::clients::teaweb;
std::string config::server::clients::extra_welcome_message_teaweb;
std::string config::server::clients::teaweb_not_allowed_message;
config::server::clients::WelcomeMessageType config::server::clients::extra_welcome_message_type_teaweb;

std::string config::server::clients::extra_welcome_message_teaspeak;
config::server::clients::WelcomeMessageType config::server::clients::extra_welcome_message_type_teaspeak;

bool config::server::clients::ignore_max_clone_permissions;

uint16_t config::voice::default_voice_port;
size_t config::voice::DefaultPuzzlePrecomputeSize;
bool config::server::delete_missing_icon_permissions;
bool config::server::delete_old_bans;
int config::voice::RsaPuzzleLevel;
bool config::voice::warn_on_permission_editor;
bool config::voice::enforce_coocie_handshake;
int config::voice::connectLimit;
int config::voice::clientConnectLimit;
bool config::voice::notifyMuted;
bool config::voice::suppress_myts_warnings;
bool config::voice::allow_session_reinitialize;

std::string config::query::motd;
std::string config::query::newlineCharacter;
size_t config::query::max_line_buffer;
int config::query::sslMode;
std::string config::query::ssl::certFile;
std::string config::query::ssl::keyFile;

std::string config::messages::applicationCrashed;
std::string config::messages::applicationStopped;
std::string config::messages::serverStopped;

std::string config::messages::idle_time_exceeded;
std::string config::messages::mute_notify_message;
std::string config::messages::unmute_notify_message;

std::string config::messages::kick_invalid_badges;
std::string config::messages::kick_invalid_command;
std::string config::messages::kick_invalid_hardware_id;
std::string config::messages::kick_vpn;

std::string config::messages::shutdown::scheduled;
std::string config::messages::shutdown::interval;
std::string config::messages::shutdown::now;
std::string config::messages::shutdown::canceled;
std::vector<std::pair<std::chrono::seconds, std::string>> config::messages::shutdown::intervals;

std::string config::messages::music::song_announcement;

std::string config::messages::timeout::packet_resend_failed;
std::string config::messages::timeout::connection_reinitialized;

size_t config::threads::ticking;
size_t config::threads::command_execute;
size_t config::threads::network_events;
size_t config::threads::voice::events_per_server;
size_t config::threads::music::execute_limit;
size_t config::threads::music::execute_per_bot;
std::string config::messages::teamspeak_permission_editor;

bool config::web::activated;
std::deque<std::tuple<std::string, std::string, std::string>> config::web::ssl::certificates;
uint16_t config::web::webrtc_port_max;
uint16_t config::web::webrtc_port_min;
bool config::web::stun_enabled;
std::string config::web::stun_host;
uint16_t config::web::stun_port;
bool config::web::enable_upnp;
bool config::web::udp_enabled;
bool config::web::tcp_enabled;

size_t config::log::vs_size;
std::string config::log::path;
spdlog::level::level_enum config::log::terminalLevel;
spdlog::level::level_enum config::log::logfileLevel;
bool config::log::logfileColored;

std::string config::geo::countryFlag;
std::string config::geo::mappingFile;
bool config::geo::staticFlag;
geoloc::ProviderType config::geo::type;

bool config::geo::vpn_block;
std::string config::geo::vpn_file;

std::string config::crash_path = ".";

bool config::music::enabled;
std::string config::music::command_prefix;

//Parse stuff
#define CREATE_IF_NOT_EXISTS 0b00000001
#define PREMIUM_ONLY         0b00000010
#define FLAG_REQUIRE         0b00000100
#define FLAG_RELOADABLE      0b00001000

#define COMMENT(path, comment) commentMapping[path].emplace_back(comment)
#define WARN_SENSITIVE(path) COMMENT(path, "Do NOT TOUCH unless you're 100% sure!")

class ConfigParseError : public exception {
    public:
        ConfigParseError(shared_ptr<EntryBinding> entry, std::string message) : binding(move(entry)), message(std::move(message)) { }

        const char *what() const noexcept {
            return message.c_str();
        }

        shared_ptr<EntryBinding> entry() const { return this->binding; }
    private:
        shared_ptr<EntryBinding> binding;
        std::string message;
};

class PathNodeError : public exception {
    public:
        PathNodeError(std::string path, std::string message) : _message(std::move(message)), _path(std::move(path)) { }

        const char *what() const noexcept {
            return _message.c_str();
        }

        const std::string path() const { return this->_path; }
        const std::string message() const { return this->_message; }
    private:
        std::string _path;
        std::string _message;
};

std::string escapeJsonString(const std::string& input) {
    std::ostringstream ss;
    for (auto iter = input.cbegin(); iter != input.cend(); iter++) {
        //C++98/03:
        //for (std::string::const_iterator iter = input.begin(); iter != input.end(); iter++) {
        switch (*iter) {
            case '\\': ss << "\\\\"; break;
            case '"': ss << "\\\""; break;
            case '/': ss << "\\/"; break;
            case '\b': ss << "\\b"; break;
            case '\f': ss << "\\f"; break;
            case '\n': ss << "\\n"; break;
            case '\r': ss << "\\r"; break;
            case '\t': ss << "\\t"; break;
            default: ss << *iter; break;
        }
    }
    return ss.str();
}

std::string escapeHeaderString(const std::string& input) {
    std::ostringstream ss;
    for (auto iter = input.cbegin(); iter != input.cend(); iter++) {
        switch (*iter) {
            case '\b': ss << "\\b"; break;
            case '\f': ss << "\\f"; break;
            case '\n': ss << "\\n"; break;
            case '\r': ss << "\\r"; break;
            case '\t': ss << "\\t"; break;
            case '\"': ss << "\\\""; break;
            default: ss << *iter; break;
        }
    }
    return ss.str();
}

std::string deescapeJsonString(const std::string& input) {
    std::ostringstream ss;
    for (auto iter = input.cbegin(); iter != input.cend(); iter++) {
        if(*iter == '\\'){
            if(iter++ != input.cend()) return ss.str();

            switch (*++iter) {
                case 't': ss << "\t"; break;
                case 'n': ss << "\n"; break;
                case 'r': ss << "\r"; break;
                case 'b': ss << "\b"; break;
                case 'f': ss << "\f"; break;
                case '/': ss << "/"; break;
                case '\\': ss << "\\\\"; break;
                default: ss << *iter; break;
            }
        } else ss << *iter;

    }
    return ss.str();
}

static bool saveConfig = false;

std::vector<std::string> split(const std::string &text, char sep) {
    std::vector<std::string> tokens;
    std::size_t start = 0, end = 0;
    while ((end = text.find(sep, start)) != std::string::npos) {
        tokens.push_back(text.substr(start, end - start));
        start = end + 1;
    }
    if(!text.substr(start).empty())
        tokens.push_back(text.substr(start));
    return tokens;
}

//We need to keep the root nodes in memory
vector<YAML::Node> resolveNode(const YAML::Node &root,const string& path, bool override_old = false){
    vector<YAML::Node> result;
    result.push_back(root);

    auto entries = split(path, '.');
    for(auto it = entries.begin(); it != entries.end(); it++) {
        auto& back = result.back();
        if(back.IsMap() || it == entries.end() || back.IsNull())
            result.push_back(back[*it]);
        else if(!back.IsDefined() || override_old)
            result.push_back((back = YAML::Node(YAML::NodeType::Map))[*it]);
        else
            throw PathNodeError(path, "Node '" + *it + "' isnt a sequence");
    }
    return result;
}

void remapValue(YAML::Node& node, const string &oldPath, const string &newPath){
    auto old = resolveNode(node, oldPath);
    if(!old.back()) return;

    auto _new = resolveNode(node, newPath, true);
    _new.back() = YAML::Clone(old.back());

    if(old.size() > 1) {
        auto oldNode = old[old.size() - 1];
        oldNode = YAML::Null;
        if(old[old.size() - 2].remove(oldNode)) logError(LOG_GENERAL, "Could not remove old config entry");
    }
}

void build_comments(map<string,deque<string>>& map, const std::deque<std::shared_ptr<EntryBinding>>& bindings) {
    for(const auto& entry : bindings) {
        for(const auto& message : entry->description) {
            if(message.first.empty()) {
                for(const auto& m : message.second)
                    map[entry->key].push_back(m);
            } else {
                map[entry->key].push_back(message.first + ":");
                for(const auto& m : message.second)
                    map[entry->key].push_back("  " + m);
            }
        }

        if(entry->value_description)
            map[entry->key].push_back(entry->value_description());
    }
}

void read_bindings(YAML::Node& root, const std::deque<std::shared_ptr<EntryBinding>>& bindings, uint8_t required_flags = 0) {
    for(const auto& entry : bindings) {
        if(entry->bounded_by == 2) continue;
        if(required_flags > 0 && (entry->flags & required_flags) == 0) continue;

        auto nodes = resolveNode(root, entry->key);
        assert(!nodes.empty());
        assert(entry->read_config);

        if((entry->flags & PREMIUM_ONLY) > 0 && !config::license->isPremium()) {
            const auto default_value = entry->default_value();
            if(nodes.back().IsNull() || !nodes.back().IsDefined())
                entry->set_default(nodes.back());

            for(const auto& e : entry->default_value())
                entry->read_argument(e);
            continue;
        }

        entry->read_config(nodes.back());
    }
}

inline string apply_comments(stringstream &in, map<string, deque<string>>& comments);
std::deque<std::shared_ptr<EntryBinding>> create_local_bindings(int& version, std::string& license);

#define CURRENT_CONFIG_VERSION 16
static std::string _config_path;
vector<string> config::parseConfig(const std::string& path) {
    _config_path = path;

    vector<string> errors;
    saveConfig = false;

    ifstream cfgStream(path);
    YAML::Node config;
    try {
        config = YAML::Load(cfgStream);
    } catch (const YAML::ParserException& ex){
        errors.push_back("Could not load config file: " + ex.msg + " @" + to_string(ex.mark.line) + ":" + to_string(ex.mark.column));
        return errors;
    }
    cfgStream.close();

    std::map<std::string, std::deque<std::string>> comments;
    try {
        int config_version;
        string teaspeak_license;
        {
            auto bindings = create_local_bindings(config_version, teaspeak_license);
            read_bindings(config, bindings);
            build_comments(comments, bindings);
        }
        if(config_version > CURRENT_CONFIG_VERSION) {
            errors.emplace_back("Given config version is higher that currently supported config version!");
            errors.push_back("Decrease the version by hand to " + to_string(CURRENT_CONFIG_VERSION));
            errors.emplace_back("Attention: Decreasing the version could may lead to data loss!");
            return errors;
        }
        {
            if(config_version != CURRENT_CONFIG_VERSION) {
                logMessage(LOG_GENERAL, "You're using an outdated config.");
                logMessage(LOG_GENERAL, "Updating config");

                switch (config_version){
                    case 1:
                        remapValue(config, "voice.rsa_puzzle_precompute_size", "voice.rsa.puzzle_pool_size");
                    case 2:
                        remapValue(config, "messages.voice.app_stopped",    "messages.application.stop");
                        remapValue(config, "messages.voice.app_crashed",    "messages.application.crash");
                        remapValue(config, "messages.voice.server_stopped", "messages.voice.server_stop");
                    case 3:
                        remapValue(config, "voice.kick_invalid_packet", "voice.protocol.kick_invalid_packet");
                    case 4:
                        remapValue(config, "messages.voice.default_country", "voice.fallback_country");
                    case 6:
                        config["geolocation"] = YAML::Node(YAML::NodeType::Map);
                        remapValue(config, "voice.fallback_country", "geolocation.fallback_country");
                        remapValue(config, "voice.force_fallback_country.", "geolocation.force_fallback_country");
                    case 7:
                        remapValue(config, "web.ssh.certificate", "web.ssl.certificate");
                        remapValue(config, "web.ssh.privatekey", "web.ssl.privatekey");
                    case 8:
                        remapValue(config, "voice.rsa.puzzle_level", "voice.handshake.puzzle_level");
                    case 9:
                        if(config["general"]["dbFile"].IsDefined())
                            config["general"]["dbFile"] = "sqlite://" + config["general"]["dbFile"].as<string>();
                        remapValue(config, "general.dbFile", "general.database_url");
                    case 10:
                        remapValue(config, "messages.mute.kick_invalid.hardware_id", "messages.kick_invalid.hardware_id");
                        remapValue(config, "messages.mute.kick_invalid.command", "messages.kick_invalid.command");
                        remapValue(config, "messages.mute.kick_invalid.badges", "messages.kick_invalid.badges");
                        remapValue(config, "messages.level", "log.terminal_level");
                    case 11:
                        remapValue(config, "general.database_url", "general.database.url");
                    case 12:
                    {
                        auto nodes_certificate = YAML::Clone(resolveNode(config, "web.ssl.certificate").back()); //We'll clone here because we're overriding it later
                        auto nodes_key = resolveNode(config, "web.ssl.privatekey").back();

                        if(nodes_certificate.IsDefined() && nodes_key.IsDefined()) {
                            auto node_certificates_default = resolveNode(config, "web.ssl.certificate.default", true);
                            node_certificates_default.back() = YAML::Node(YAML::NodeType::Map);
                            node_certificates_default.back()["certificate"] = nodes_certificate;
                            node_certificates_default.back()["private_key"] = nodes_key;
                        }

                        nodes_key = YAML::Node(YAML::NodeType::Undefined);
                    }
                    case 13:
                    {
                        auto nodes_key = resolveNode(config, "binding.query.host").back();
                        if(nodes_key.IsDefined() && nodes_key.as<string>() == "0.0.0.0")
                            nodes_key = nodes_key.as<string>() + ",[::]";
                        auto node_ft = resolveNode(config, "binding.file.host").back();
                        if(node_ft.IsDefined() && node_ft.as<string>() == "0.0.0.0")
                            node_ft = node_ft.as<string>() + ",[::]";
                    }
                    case 14:
                    {
                        {
                            auto nodes_key = resolveNode(config, "threads.voice.execute_limit").back();
                            if(nodes_key.IsDefined() && nodes_key.as<uint32_t>() == 10)
                                nodes_key = "5";
                        }
                        {
                            auto nodes_key = resolveNode(config, "threads.voice.io_limit").back();
                            if(nodes_key.IsDefined() && nodes_key.as<uint32_t>() == 10)
                                nodes_key = "5";
                        }
                        {
                            auto nodes_key = resolveNode(config, "threads.web.io_loops").back();
                            if(nodes_key.IsDefined() && nodes_key.as<uint32_t>() == 4)
                                nodes_key = "2";
                        }
                        {
                            auto nodes_key = resolveNode(config, "threads.music.execute_limit").back();
                            if(nodes_key.IsDefined() && nodes_key.as<uint32_t>() == 15)
                                nodes_key = "2";
                        }
                    }
                    case 15: {
                        auto nodes_key = resolveNode(config, "web.webrtc.stun.ip").back();
                        if(nodes_key.IsDefined() && nodes_key.as<std::string>() == "127.0.0.1") {
                            nodes_key.reset();
                            resolveNode(config, "web.webrtc.stun.enabled").back() = "1";
                        }
                    }
                    default:
                        break;
                }
                config["version"] = CURRENT_CONFIG_VERSION;
                config_version = CURRENT_CONFIG_VERSION;
            }
        }

        //License parsing
        license_parsing:
        {
            string err;
            if(teaspeak_license.empty() || teaspeak_license == "none") {
                //Due to an implementation mistake every default license looks like this:
                //AQA7CN26AAAAAMoLy9BIR2S9HyMeqBx7ZMUUc1rFXkt5YztwZCQRngncqtSs8hsQrzszzeNQSEcVXLtvIhm6m331OgjdugAAAADKbA25Oxab9/MK0xK3iZ8mUg+YkaZQkYS2Bj4Kcq2WFTCynv+sIfeYaei+VV8f/AJvxJDUfADJoSoGX85BIT3cTV+usRs3Adx0Ix/Wi5G4roL8Ypl0p8lYwELLGEVyEQf21rYMWOtVkQk2yoGuQikgLxr6p9sShKmAoES1lbHr8Xk=
                //teaspeak_license = license::createLocalLicence(license::LicenseType::DEMO, system_clock::time_point(), "TeaSpeak");
                teaspeak_license = strobf("AQA7CN26AAAAAMoLy9BIR2S9HyMeqBx7ZMUUc1rFXkt5YztwZCQRngncqtSs8hsQrzszzeNQSEcVXLtvIhm6m331OgjdugAAAADKbA25Oxab9/MK0xK3iZ8mUg+YkaZQkYS2Bj4Kcq2WFTCynv+sIfeYaei+VV8f/AJvxJDUfADJoSoGX85BIT3cTV+usRs3Adx0Ix/Wi5G4roL8Ypl0p8lYwELLGEVyEQf21rYMWOtVkQk2yoGuQikgLxr6p9sShKmAoES1lbHr8Xk=").string();
                config::license = license::readLocalLicence(teaspeak_license, err);
            } else {
                config::license_original = license::readLocalLicence(teaspeak_license, err);
                config::license = config::license_original;
            }

            if(!config::license){
                logErrorFmt(true, LOG_GENERAL, strobf("The given license could not be parsed!").string());
                logErrorFmt(true, LOG_GENERAL, strobf("Falling back to the default license.").string());
                teaspeak_license = "none";
                goto license_parsing;
            }

            if(!config::license->isValid()) {
                if(config::license->data.type == license::LicenseType::INVALID) {
                    errors.emplace_back(strobf("Give license isn't valid!").string());
                    return errors;
                }

                logErrorFmt(true, LOG_GENERAL, strobf("The given license isn't valid!").string());
                logErrorFmt(true, LOG_GENERAL, strobf("Falling back to the default license.").string());
                teaspeak_license = "none";

                goto license_parsing;
            }
        }

        {
            auto bindings = create_bindings();
            read_bindings(config, bindings);
            build_comments(comments, bindings);
        }

        auto currentVersion = config::server::default_version();
        if(currentVersion != config::server::DefaultServerVersion) {
            auto ref = config::server::DefaultServerVersion;
            try {
                auto pattern = strobf("TeaSpeak\\s+").string() + build::pattern();
                static std::regex const matcher(pattern);
                if (std::regex_match(ref, matcher)) {
                    logMessage(LOG_GENERAL, "Updating displayed TeaSpeak server version in config from {} to {}", ref, currentVersion);
                    config["server"]["version"] = currentVersion;
                    config::server::DefaultServerVersion = currentVersion;
                } else {
                    debugMessage(LOG_GENERAL, "Config version string does not matches default pattern. Do no updating it (Pattern: {}, Value: {})", pattern, ref);
                }
            } catch(std::exception& e){
                logError(LOG_GENERAL, "Could not update displayed version ({})", e.what());
            }
        }

        stringstream off;
        off << config;

        ofstream foff(path);
        foff << apply_comments(off, comments) << endl;
        foff.close();
    } catch(const YAML::Exception& ex) {
        errors.push_back("Could not read config: " + ex.msg + " @" + to_string(ex.mark.line) + ":" + to_string(ex.mark.column));
        return errors;
    } catch(const ConfigParseError& ex) {
        errors.push_back("Failed to parse config entry \"" + ex.entry()->key + "\": " + ex.what());
        return errors;
    } catch(const PathNodeError& ex) {
        errors.push_back("Expected sequence for path " + ex.path() + ": " + ex.message());
        return errors;
    }
    return errors;
}

std::vector<std::string> config::reload() {
    std::vector<std::string> errors;
    saveConfig = false;

    ifstream cfgStream(_config_path);
    YAML::Node config;
    try {
        config = YAML::Load(cfgStream);
    } catch (const YAML::ParserException& ex){
        errors.emplace_back("Could not load config file: " + ex.msg + " @" + to_string(ex.mark.line) + ":" + to_string(ex.mark.column));
        return errors;
    }

    try {
        int config_version;
        string teaspeak_license;
        {
            auto bindings = create_local_bindings(config_version, teaspeak_license);
            read_bindings(config, bindings, 0);
        }
        if(config_version != CURRENT_CONFIG_VERSION) {
            errors.emplace_back("Given config version is no equal to the initial one!");
            return errors;
        }

        auto bindings = create_bindings();
        read_bindings(config, bindings, FLAG_RELOADABLE);


        const auto& logConfig = logger::currentConfig();
        if(logConfig) {
            logConfig->logfileLevel = (spdlog::level::level_enum) ts::config::log::logfileLevel;
            logConfig->terminalLevel = (spdlog::level::level_enum) ts::config::log::terminalLevel;
            logger::updateLogLevels();
        }
    } catch(const YAML::Exception& ex) {
        errors.emplace_back("Could not read config: " + ex.msg + " @" + to_string(ex.mark.line) + ":" + to_string(ex.mark.column));
        return errors;
    } catch(const ConfigParseError& ex) {
        errors.emplace_back("Failed to parse config entry \"" + ex.entry()->key + "\": " + ex.what());
        return errors;
    } catch(const PathNodeError& ex) {
        errors.emplace_back("Expected sequence for path " + ex.path() + ": " + ex.message());
        return errors;
    }

    return errors;
}

bool config::update_license(std::string &error, const std::string &new_license) {
    std::vector<std::string> lines{};

    {
        lines.reserve(1024);

        std::ifstream icfg_stream{_config_path};
        if(!icfg_stream) {
            error = "failed to open config file";
            return false;
        }

        std::string line{};
        while(std::getline(icfg_stream, line))
            lines.push_back(line);

        icfg_stream.close();
    }

    bool license_found{false};
    for(auto& line : lines) {
        if(!line.starts_with("  license:")) continue;

        line = "  license: \"" + new_license + "\"";
        license_found = true;
        break;
    }
    if(!license_found) {
        error = "missing license config key";
        return false;
    }

    {
        std::ofstream ocfg_stream{_config_path};
        if(!ocfg_stream) {
            error = "failed to write to config file";
            return false;
        }

        for(const auto& line : lines)
            ocfg_stream << line << "\n";
        ocfg_stream << std::flush;
        ocfg_stream.close();
    }
    return true;
}

void bind_string_description(const shared_ptr<EntryBinding>& _entry, std::string& target, const std::string& default_value) {
    _entry->default_value = [default_value]() -> std::deque<std::string> { return { default_value }; };
    _entry->value_description = [] { return "The value must be a string"; };
}

void bind_string_parse(const shared_ptr<EntryBinding>& _entry, std::string& target, const std::string& default_value) {
    weak_ptr weak_entry = _entry;

    _entry->set_default = [weak_entry, default_value](YAML::Node& node) {
        auto entry = weak_entry.lock();
        if(!entry) throw ConfigParseError(nullptr, "Entry handle got deleted!");

        node = default_value;
    };

    _entry->read_config = [weak_entry, default_value, &target](YAML::Node& node) {
        auto entry = weak_entry.lock();
        if(!entry) throw ConfigParseError(nullptr, "Entry handle got deleted!");

        if(!node.IsDefined() || node.IsNull()) {
            if((entry->flags & FLAG_REQUIRE) > 0)
                throw ConfigParseError(entry, "missing required setting");
            else
                entry->set_default(node);
        }
        try {
            target = node.as<string>();
            entry->bounded_by = 1;
        } catch (const YAML::BadConversion& e) {
            throw ConfigParseError(entry, "Invalid node content. Requested was a string!");
        }
    };

    _entry->read_argument = [weak_entry, &target](const std::string& value) {
        auto entry = weak_entry.lock();
        if(!entry) throw ConfigParseError(nullptr, "Entry handle got deleted!");
        entry->bounded_by = 2;

        target = value;
    };
}

template<typename T>
inline std::string type_name() {
    int status;
    std::string tname = typeid(T).name();
    char *demangled_name = abi::__cxa_demangle(tname.c_str(), NULL, NULL, &status);
    if(status == 0) {
        tname = demangled_name;
        std::free(demangled_name);
    }
    return tname;
}

template <typename type_t>
static typename std::enable_if<std::is_unsigned<type_t>::value, type_t>::type integral_parse(const std::string& value) {
    try {
        auto result = std::stoull(value);
        if(result < numeric_limits<type_t>::min()) throw std::out_of_range("");
        if(result > numeric_limits<type_t>::max()) throw std::out_of_range("");
        return (type_t) result;
    } catch(std::out_of_range& ex) {
        throw YAML::BadConversion(YAML::Mark::null_mark());
    } catch(std::invalid_argument& ex) {
        throw YAML::BadConversion(YAML::Mark::null_mark());
    }
}

template <typename type_t>
static typename std::enable_if<!std::is_unsigned<type_t>::value, type_t>::type integral_parse(const std::string& value) {
    try {
        auto result = std::stoll(value);
        if(result < numeric_limits<type_t>::min()) throw std::out_of_range("");
        if(result > numeric_limits<type_t>::max()) throw std::out_of_range("");
        return (type_t) result;
    } catch(std::out_of_range& ex) {
        throw YAML::BadConversion(YAML::Mark::null_mark());
    } catch(std::invalid_argument& ex) {
        throw YAML::BadConversion(YAML::Mark::null_mark());
    }
}

template <typename type_t>
static typename std::enable_if<sizeof(type_t) == 1, uint16_t>::type enum_number_cast(type_t value) { return (uint16_t) value; }
template <typename type_t>
static typename std::enable_if<sizeof(type_t) == 2, uint16_t>::type enum_number_cast(type_t value) { return (uint16_t) value; }
template <typename type_t>
static typename std::enable_if<sizeof(type_t) == 4, uint32_t>::type enum_number_cast(type_t value) { return (uint32_t) value; }
template <typename type_t>
static typename std::enable_if<sizeof(type_t) == 8, uint64_t>::type enum_number_cast(type_t value) { return (uint64_t) value; }

static map<string, string> integral_mapping = {
        {"bool", "boolean"},
        {"unsigned char", "positive numeric value"},
        {"unsigned short", "positive numeric value"},
        {"unsigned int", "positive numeric value"},
        {"unsigned long short", "positive numeric value"},
        {"char", "numeric value"},
        {"short", "numeric value"},
        {"int", "numeric value"},
        {"long short", "numeric value"},
};

template <typename type_t, typename std::enable_if<std::is_integral<type_t>::value || std::is_enum<type_t>::value, int>::type = 0>
void bind_integral_description(const shared_ptr<EntryBinding>& _entry, type_t& target, type_t default_value, type_t min_value, type_t max_value) {
    _entry->default_value = [default_value]() -> std::deque<std::string> { return { to_string(default_value) }; };
    _entry->value_description = [min_value, max_value] {
        auto type_name = ::type_name<decltype(enum_number_cast<type_t>((type_t) 0))>();
        return "The value must be a " + (integral_mapping.count(type_name) > 0 ? integral_mapping[type_name] : type_name) + " between " + to_string(min_value) + " and " + to_string(max_value);
    };
}

template <typename type_t, typename std::enable_if<std::is_integral<type_t>::value || std::is_enum<type_t>::value, int>::type = 0>
void bind_integral_parse(const shared_ptr<EntryBinding>& _entry, type_t& target, type_t default_value, type_t min_value, type_t max_value) {
    weak_ptr weak_entry = _entry;

    _entry->set_default = [weak_entry, default_value](YAML::Node& node) {
        auto entry = weak_entry.lock();
        if(!entry) throw ConfigParseError(nullptr, "Entry handle got deleted!");

        node = enum_number_cast(default_value);
    };

    _entry->read_config = [weak_entry, default_value, &target, min_value, max_value](YAML::Node& node) {
        auto entry = weak_entry.lock();
        if(!entry) throw ConfigParseError(nullptr, "Entry handle got deleted!");

        if(!node.IsDefined() || node.IsNull()) {
            if((entry->flags & FLAG_REQUIRE) > 0)
                throw ConfigParseError(entry, "missing required setting");
            else
                entry->set_default(node);
        }
        try {
            auto str = node.as<string>();
            auto value = (type_t) node.as<decltype(enum_number_cast<type_t>((type_t) 0))>();
            if(value < min_value) throw ConfigParseError(entry, "Invalid value (" + to_string(value) + "). Received value substeps boundary (" + to_string(min_value) + ")");
            if(value > max_value) throw ConfigParseError(entry, "Invalid value (" + to_string(value) + "). Received value exceeds boundary (" + to_string(max_value) + ")");

            target = value;
            entry->bounded_by = 1;
        } catch (const YAML::BadConversion& e) {
            throw ConfigParseError(entry, "Invalid node content. Requested was a integral of type " + type_name<type_t>() + "!");
        }
    };

    _entry->read_argument = [weak_entry, &target, min_value, max_value](const std::string& string) {
        auto entry = weak_entry.lock();
        if(!entry) throw ConfigParseError(nullptr, "Entry handle got deleted!");
        entry->bounded_by = 2;

        try {
            auto value = (type_t) integral_parse<decltype(enum_number_cast<type_t>((type_t) 0))>(string);
            if(value < min_value) throw ConfigParseError(entry, "Invalid value (" + to_string(value) + "). Received value substeps boundary (" + to_string(min_value) + ")");
            if(value > max_value) throw ConfigParseError(entry, "Invalid value (" + to_string(value) + "). Received value exceeds boundary (" + to_string(max_value) + ")");

            target = value;
            entry->bounded_by = 2;
        } catch (const YAML::BadConversion& e) {
            throw ConfigParseError(entry, "Invalid node content. Requested was a integral of type " + type_name<type_t>() + "!");
        }
    };
}

void bind_vector_description(const shared_ptr<EntryBinding>& _entry, deque<string>&, const deque<string>& default_value) {
    _entry->default_value = [default_value]() -> std::deque<std::string> { return default_value; };
    _entry->value_description = [] { return "The value must be a sequence"; };
}

void bind_vector_parse(const shared_ptr<EntryBinding>& _entry, deque<string>& target, const deque<string>& default_value) {
    weak_ptr weak_entry = _entry;


    _entry->set_default = [weak_entry, default_value](YAML::Node& node) {
        auto entry = weak_entry.lock();
        if(!entry) throw ConfigParseError(nullptr, "Entry handle got deleted!");

        node = YAML::Node(YAML::NodeType::Sequence);
        for(const auto& entry : default_value)
            node.push_back(entry);
    };

    _entry->read_config = [weak_entry, default_value, &target](YAML::Node& node) {
        auto entry = weak_entry.lock();
        if(!entry) throw ConfigParseError(nullptr, "Entry handle got deleted!");

        if(!node.IsDefined() || node.IsNull()) {
            if((entry->flags & FLAG_REQUIRE) > 0)
                throw ConfigParseError(entry, "missing required setting");
            else {
                entry->set_default(node);
            }
        }
        if(!node.IsSequence())
            throw ConfigParseError(entry, "node requires to be a sequence");

        try {
            target.clear();
            for(const auto& element : node) {
                target.push_back(element.as<string>());
            }
        } catch (const YAML::BadConversion& e) {
            throw ConfigParseError(entry, "Invalid node sequence content");
        }
    };

    _entry->read_argument = [weak_entry, &target](const std::string& string) {
        auto entry = weak_entry.lock();
        if(!entry) throw ConfigParseError(nullptr, "Entry handle got deleted!");
        if(entry->bounded_by != 2)
            target.clear();
        entry->bounded_by = 2;

        target.push_back(string);
    };
}

struct GroupStackEntry {
    GroupStackEntry(deque<string>& list, string path) : list(list), path(move(path)) {
        list.push_back(this->path);
    }
    ~GroupStackEntry() {
        assert(list.back() == this->path);
        list.pop_back();
    }

    string path;
    deque<string>& list;
};

inline std::string join_path(const deque<string>& stack, const std::string& entry) {
    stringstream ss;
    for(const auto& e : stack)
        ss << e << ".";
    ss << entry;
    return ss.str();
}

#define STR(x) #x
#define BIND_GROUP(name) GroupStackEntry group_ ##name(group_stack, STR(name));

#define CREATE_BINDING(name, _flags) \
    auto binding = make_shared<EntryBinding>(); \
    binding->key = join_path(group_stack, name); \
    binding->flags = (_flags); \
    result.push_back(binding)

#define BIND_STRING(target, default) \
    bind_string_parse(binding, target, default); \
    bind_string_description(binding, target, default)

#define BIND_VECTOR(target, default) \
    bind_vector_parse(binding, target, default); \
    bind_vector_description(binding, target, default)

#define BIND_INTEGRAL(target, default, min, max) \
    bind_integral_parse<typename std::remove_reference<decltype(target)>::type>( \
            binding, \
            target, \
            (typename std::remove_reference<decltype(target)>::type) default, \
            (typename std::remove_reference<decltype(target)>::type) min, \
            (typename std::remove_reference<decltype(target)>::type) max \
    ); \
    bind_integral_description<typename std::remove_reference<decltype(target)>::type>( \
            binding, \
            target, \
            (typename std::remove_reference<decltype(target)>::type) default, \
            (typename std::remove_reference<decltype(target)>::type) min, \
            (typename std::remove_reference<decltype(target)>::type) max \
    )

#define BIND_BOOL(target, default) BIND_INTEGRAL(target, default, false, true)

#define ADD_DESCRIPTION(desc, ...) \
    for(const auto& entry : {desc, ##__VA_ARGS__}) \
        binding->description["Description"].emplace_back(entry)

#define ADD_NOTE(desc, ...) \
    for(const auto& entry : {desc, ##__VA_ARGS__}) \
        binding->description["Notes"].emplace_back(entry)

#define ADD_NOTE_RELOADABLE() \
    binding->description["Notes"].emplace_back("This option could be reloaded while the instance is running.")

#define ADD_WARN(desc, ...) \
    for(const auto& entry : {desc, ##__VA_ARGS__}) \
        binding->description["Warning"].emplace_back(entry)

#define ADD_SENSITIVE() ADD_WARN("Do NOT TOUCH unless you're 100% sure!")

std::deque<std::shared_ptr<EntryBinding>> create_local_bindings(int& version, std::string& license) {
    deque<shared_ptr<EntryBinding>> result;
    deque<string> group_stack;

    {
        CREATE_BINDING("version", 0);
        BIND_INTEGRAL(version, CURRENT_CONFIG_VERSION, 0, 10000000000);
        ADD_DESCRIPTION("The current config version");
        ADD_WARN("This is an auto-generated id!", "Modification could cause data loss!");
    }
    {
        CREATE_BINDING("general.license", 0);
        BIND_STRING(license, "none");
        ADD_DESCRIPTION("Insert here your TeaSpeak license code (if you have one)");
    }

    return result;
}

static std::deque<std::shared_ptr<EntryBinding>> _create_bindings;
std::deque<std::shared_ptr<EntryBinding>> config::create_bindings() {
    if(!_create_bindings.empty()) return _create_bindings;

    deque<shared_ptr<EntryBinding>> result;
    deque<string> group_stack;

    {
        BIND_GROUP(general);

        //CREATE_BINDING("database_url", 0); old
        {
            BIND_GROUP(database);
            {
                CREATE_BINDING("url", 0);
                BIND_STRING(config::database::url, "sqlite://TeaData.sqlite");
                ADD_DESCRIPTION("Available urls:");
                ADD_DESCRIPTION("  sqlite://[file]");
                ADD_DESCRIPTION("  mysql://[host][:port]/[database][?propertyName1=propertyValue1[&propertyName2=propertyValue2]...]");
                ADD_DESCRIPTION("");
                ADD_DESCRIPTION("More info about about the mysql url could be found here: https://dev.mysql.com/doc/connector-j/5.1/en/connector-j-reference-configuration-properties.html");
                ADD_DESCRIPTION("There's also a new property called 'connections', which describes how many connections and queries could be executed synchronously");
                ADD_DESCRIPTION("MySQL example: mysql://localhost:3306/teaspeak?userName=root&password=mysecretpassword&connections=4");
                ADD_DESCRIPTION("Attention: If you're using MySQL you need at least 3 connections!");
            }

            {
                BIND_GROUP(sqlite);
                {
                    CREATE_BINDING("locking_mode", 0);
                    BIND_STRING(config::database::sqlite::locking_mode, "EXCLUSIVE");
                    ADD_DESCRIPTION("Sqlite database locking mode.");
                    ADD_DESCRIPTION("Set it to nothing (\"\") to use the default driver setting");
                    ADD_DESCRIPTION("More information could be found here: https://www.sqlite.org/lockingv3.html");
                }
                {
                    CREATE_BINDING("sync_mode", 0);
                    BIND_STRING(config::database::sqlite::sync_mode, "NORMAL");
                    ADD_DESCRIPTION("Sqlite database synchronous mode.");
                    ADD_DESCRIPTION("Set it to nothing (\"\") to use the default driver setting");
                    ADD_DESCRIPTION("More information could be found here: https://www.sqlite.org/pragma.html#pragma_synchronous");
                }
                {
                    CREATE_BINDING("journal_mode", 0);
                    BIND_STRING(config::database::sqlite::journal_mode, "WAL");
                    ADD_DESCRIPTION("Sqlite database journal  mode.");
                    ADD_DESCRIPTION("Set it to nothing (\"\") to use the default driver setting");
                    ADD_DESCRIPTION("More information could be found here: https://www.sqlite.org/pragma.html#pragma_journal_mode");
                }
            }
        }
        {
            CREATE_BINDING("crash_path", 0);
            BIND_STRING(config::crash_path, "crash_dumps/");
            ADD_DESCRIPTION("Define the folder where the crash dump files will be moved, when the server crashes");
        }
        {
            CREATE_BINDING("command_prefix", 0);
            BIND_STRING(config::music::command_prefix, ".");
            ADD_DESCRIPTION("The default channel chat command prefix");
        }
        {
            CREATE_BINDING("permission_mapping", 0);
            BIND_STRING(config::permission_mapping_file, "resources/permission_mapping.txt");
            ADD_DESCRIPTION("Mapping for the permission names");
            ADD_SENSITIVE();
        }
    }
    {
        BIND_GROUP(log)
        {
            CREATE_BINDING("level", FLAG_RELOADABLE);
            BIND_INTEGRAL(config::log::logfileLevel, spdlog::level::debug, spdlog::level::trace, spdlog::level::off);
            ADD_NOTE_RELOADABLE();
            ADD_DESCRIPTION("The log level within the log files");
            ADD_DESCRIPTION("Available types:");
            ADD_DESCRIPTION("  0: Trace");
            ADD_DESCRIPTION("  1: Debug");
            ADD_DESCRIPTION("  2: Info");
            ADD_DESCRIPTION("  3: Warn");
            ADD_DESCRIPTION("  4: Error");
            ADD_DESCRIPTION("  5: Critical");
            ADD_DESCRIPTION("  6: Off");
        }
        {
            CREATE_BINDING("terminal_level", FLAG_RELOADABLE);
            BIND_INTEGRAL(config::log::terminalLevel, spdlog::level::info, spdlog::level::trace, spdlog::level::off);
            ADD_NOTE_RELOADABLE();
            ADD_DESCRIPTION("The log level within the TeaSpeak server terminal");
            ADD_DESCRIPTION("Available types:");
            ADD_DESCRIPTION("  0: Trace");
            ADD_DESCRIPTION("  1: Debug");
            ADD_DESCRIPTION("  2: Info");
            ADD_DESCRIPTION("  3: Warn");
            ADD_DESCRIPTION("  4: Error");
            ADD_DESCRIPTION("  5: Critical");
            ADD_DESCRIPTION("  6: Off");
        }
        {
            CREATE_BINDING("colored", 0);
            BIND_BOOL(config::log::logfileColored, false);
            ADD_DESCRIPTION("Disable/enable ascii codes within the log file");
        }
        {
            CREATE_BINDING("vs_size", 0);
            BIND_INTEGRAL(config::log::vs_size, 0, 0, numeric_limits<ServerId>::max());
            ADD_DESCRIPTION("Virtual server log chunk size");
        }
        {
            CREATE_BINDING("path", 0);
            BIND_STRING(config::log::path, "logs/log_${time}(%Y-%m-%d_%H:%M:%S)_${group}.log");
            ADD_DESCRIPTION("The log file path");
        }
    }
    {
        BIND_GROUP(binding);
        {
            BIND_GROUP(voice);
            {
                CREATE_BINDING("default_host", 0);
                BIND_STRING(config::binding::DefaultVoiceHost, "0.0.0.0,::");
                ADD_NOTE("Multibinding supported here! Host delimiter is \",\"");
            }
            {
                CREATE_BINDING("enforce", 0);
                BIND_BOOL(config::binding::enforce_default_voice_host, false);
                ADD_NOTE("Enforce the default host for every virtual server. Ignoring the server specific host");
            }
        }
        {
            BIND_GROUP(web);
            {
                CREATE_BINDING("default_host", 0);
                BIND_STRING(config::binding::DefaultWebHost, "0.0.0.0,[::]");
                ADD_NOTE("Multibinding supported here! Host delimiter is \",\"");
            }
        }
        {
            BIND_GROUP(query);
            {
                CREATE_BINDING("port", 0);
                BIND_INTEGRAL(config::binding::DefaultQueryPort, 10101, 1, 65535);
            }
            {
                CREATE_BINDING("host", 0);
                BIND_STRING(config::binding::DefaultQueryHost, "0.0.0.0,[::]");
                ADD_NOTE("Multibinding supported here! Host delimiter is \",\"");
            }
        }
        {
            BIND_GROUP(file);
            {
                CREATE_BINDING("port", 0);
                BIND_INTEGRAL(config::binding::DefaultFilePort, 30303, 1, 65535);
            }
            {
                CREATE_BINDING("host", 0);
                BIND_STRING(config::binding::DefaultFileHost, "0.0.0.0,[::]");
                ADD_NOTE("Multibinding supported here! Host delimiter is \",\"");
            }
        }
    }
    {
        BIND_GROUP(query);
        {
            CREATE_BINDING("nl_char", FLAG_RELOADABLE);
            BIND_STRING(config::query::newlineCharacter, "\n");
            ADD_DESCRIPTION("Change the query newline character");
            ADD_NOTE("NOTE: TS3 - Compatible bots may require \"\n\r\"");
        }
        {
            CREATE_BINDING("max_line_buffer", FLAG_RELOADABLE);
            BIND_INTEGRAL(config::query::max_line_buffer, 1024 * 1024, 1024 * 8, 1024 * 1024 * 512);
            ADD_DESCRIPTION("Max number of characters one query command could contain.");
        }
        {
            CREATE_BINDING("motd", FLAG_RELOADABLE);
            BIND_STRING(config::query::motd, "TeaSpeak\nWelcome on the TeaSpeak ServerQuery interface.\n");
            ADD_DESCRIPTION("The query welcome message");

            ADD_NOTE("If not like TeamSpeak then some applications may not recognize the Query");
            ADD_NOTE("Default TeamSpeak 3 MOTD:");
            ADD_NOTE("TS3\n\rWelcome to the TeamSpeak 3 ServerQuery interface, type \"help\" for a list of commands and \"help <command>\" for information on a specific command.\n\r");
            ADD_NOTE("NOTE: Sometimes you have to append one \r\n more!");
        }
        {
            CREATE_BINDING("enableSSL", 0);
            BIND_INTEGRAL(config::query::sslMode, 2, 0, 2);
            ADD_DESCRIPTION("Enable/disable SSL for query");
            ADD_DESCRIPTION("Available modes:");
            ADD_DESCRIPTION("  0: Disabled");
            ADD_DESCRIPTION("  1: Enabled (Enforced encryption)");
            ADD_DESCRIPTION("  2: Hybrid (Prefer encryption but fallback when it isn't available)");
        }
        {
            BIND_GROUP(ssl);
            {
                CREATE_BINDING("certificate", FLAG_RELOADABLE);
                BIND_STRING(config::query::ssl::certFile, "certs/query_certificate.pem");
                ADD_DESCRIPTION("The SSL certificate for the query client");
            }
            {
                CREATE_BINDING("privatekey", FLAG_RELOADABLE);
                BIND_STRING(config::query::ssl::keyFile, "certs/query_privatekey.pem");
                ADD_DESCRIPTION("The SSL private key for the query client (You have to export the key without a password!)");
            }
        }
    }
    {
        BIND_GROUP(voice)
        {
            CREATE_BINDING("default_port", 0);
            BIND_INTEGRAL(config::voice::default_voice_port, 9987, 1, 65535);
            ADD_DESCRIPTION("Change the default voice server port", "This also defines the start where the instance search for free server ports on a new server creation");
            ADD_NOTE("This setting only apply once, when you create a new instance.",
                    "Once applied the default server port would not be changed!",
                    "The start point for the server creation still apply.");
        }
        {
            CREATE_BINDING("notifymute", FLAG_RELOADABLE);
            BIND_BOOL(config::voice::notifyMuted, false);
            ADD_DESCRIPTION("Enable/disable the mute notify");
        }
        {
            CREATE_BINDING("suppress_myts_warnings", FLAG_RELOADABLE);
            BIND_BOOL(config::voice::suppress_myts_warnings, true);
            ADD_DESCRIPTION("Suppress the MyTS integration warnings");
        }
        {
            CREATE_BINDING("allow_session_reinitialize", FLAG_RELOADABLE);
            BIND_BOOL(config::voice::allow_session_reinitialize, true);
            ADD_DESCRIPTION("Enable/disable fast session reinitialisation.");
            ADD_SENSITIVE();
        }
        {
            CREATE_BINDING("rsa.puzzle_pool_size", 0);
            BIND_INTEGRAL(config::voice::DefaultPuzzlePrecomputeSize, 128, 1, 65536);
            ADD_DESCRIPTION("The amount of precomputed puzzles");
            ADD_SENSITIVE();
        }
        {
            BIND_GROUP(handshake);
            {
                CREATE_BINDING("puzzle_level", 0);
                BIND_INTEGRAL(config::voice::RsaPuzzleLevel, 1000, 512, 1048576);
                ADD_DESCRIPTION("The puzzle level. (A higher number will result a longer calculation time for the manager RSA puzzle)");
                ADD_SENSITIVE();
            }
            {
                CREATE_BINDING("enforce_cookie", 0);
                BIND_BOOL(config::voice::enforce_coocie_handshake, true);
                ADD_DESCRIPTION("Enforces the cookie exchange (Low level protection against distributed denial of service attacks (DDOS attacks))");
                ADD_NOTE("This option is highly recommended!");
                ADD_SENSITIVE();
            }
            {
                CREATE_BINDING("warn_on_permission_editor", 0);
                BIND_BOOL(config::voice::warn_on_permission_editor, true);
                ADD_DESCRIPTION("Enables/disabled the warning popup for the TeamSpeak 3 permission editor.");
                ADD_NOTE("This option is highly recommended!");
            }
        }
        {
            CREATE_BINDING("connect_limit", FLAG_RELOADABLE);
            BIND_INTEGRAL(config::voice::connectLimit, 10, 0, 1024);
            ADD_DESCRIPTION("Maximum amount of join attempts per second.");
            ADD_NOTE("A value of zero means unlimited");
        }
        {
            CREATE_BINDING("client_connect_limit", FLAG_RELOADABLE);
            BIND_INTEGRAL(config::voice::clientConnectLimit, 3, 0, 1024);
            ADD_DESCRIPTION("Maximum amount of join attempts per second per ip.");
            ADD_NOTE("A value of zero means unlimited");
        }
        {
            CREATE_BINDING("protocol.experimental_31", FLAG_RELOADABLE);
            BIND_BOOL(config::experimental_31, false);
            ADD_DESCRIPTION("Enables the newer and safer protocol based on TeamSpeak's documented standard");
            ADD_NOTE("An invalid protocol chain could lead clients to calculate a wrong shared secret result");
            ADD_NOTE("This may cause a connection setup fail and the client will be unable to connect!");
        }
    }
    {
        BIND_GROUP(server)
        {
            CREATE_BINDING("platform", PREMIUM_ONLY | FLAG_RELOADABLE);
            BIND_STRING(config::server::DefaultServerPlatform, build::platform());
            ADD_DESCRIPTION("The displayed platform to the client");
            ADD_NOTE("This option is only for the premium version.");
            ADD_NOTE_RELOADABLE();
        }
        {
            CREATE_BINDING("version", PREMIUM_ONLY | FLAG_RELOADABLE);
            BIND_STRING(config::server::DefaultServerVersion, strobf("TeaSpeak ").string() + build::version()->string(true));
            ADD_DESCRIPTION("The displayed version to the client");
            ADD_NOTE("This option is only for the premium version.");
            ADD_NOTE_RELOADABLE();
        }
        {
            CREATE_BINDING("licence", PREMIUM_ONLY | FLAG_RELOADABLE);
            BIND_INTEGRAL(config::server::DefaultServerLicense, LicenseType::LICENSE_AUTOMATIC_SERVER, LicenseType::_LicenseType_MIN, LicenseType::_LicenseType_MAX);
            ADD_DESCRIPTION("The displayed licence type to every TeaSpeak 3 Client");
            ADD_DESCRIPTION("Available types:");
            ADD_DESCRIPTION("  0: No Licence");
            ADD_DESCRIPTION("  1: Authorised TeaSpeak Host Provider License (ATHP)");
            ADD_DESCRIPTION("  2: Offline/Lan Licence");
            ADD_DESCRIPTION("  3: Non-Profit License (NPL)");
            ADD_DESCRIPTION("  4: Unknown Licence");
            ADD_DESCRIPTION("  5: ~placeholder~");
            ADD_DESCRIPTION("  6: Auto-License (Server based)");
            ADD_DESCRIPTION("  7: Auto-License (Instance based)");
            ADD_NOTE("This option just work for non 3.2 clients!");
            ADD_NOTE("This option is only for the premium version.");
            ADD_NOTE_RELOADABLE();
        }
        {
            CREATE_BINDING("delete_old_bans", 0);
            BIND_BOOL(config::server::delete_old_bans, true);
            ADD_DESCRIPTION("Enable/disable the deletion of old bans within the database");
        }
#if 0
        {
            CREATE_BINDING("delete_missing_icon_permissions", 0);
            BIND_BOOL(config::server::delete_missing_icon_permissions, true);
            ADD_DESCRIPTION("Enable/disable the deletion of invalid icon id permissions");
        }
#endif
        {
            CREATE_BINDING("strict_ut8_mode", FLAG_RELOADABLE);
            BIND_BOOL(config::server::strict_ut8_mode, false);
            ADD_DESCRIPTION("If enabled an error will be throws on invalid UTF-8 characters within the protocol (Query & Client).");
            ADD_DESCRIPTION("Else the property pair will be dropped silently!");
        }
        {
            CREATE_BINDING("show_invisible_clients", FLAG_RELOADABLE);
            BIND_BOOL(config::server::show_invisible_clients_as_online, true);
            ADD_DESCRIPTION("Allow anybody to send text messages to clients which are in invisible channels");
        }
        {
            CREATE_BINDING("disable_ip_saving", 0);
            BIND_BOOL(config::server::disable_ip_saving, false);
            ADD_DESCRIPTION("Disable the saving of IP addresses within the database.");
        }
        {
            CREATE_BINDING("default_music_bot", FLAG_RELOADABLE);
            BIND_BOOL(config::server::default_music_bot, true);
            ADD_DESCRIPTION("Add by default a new music bot to each created virtual server.");
        }
        {
            CREATE_BINDING("max_virtual_servers",  FLAG_RELOADABLE);
            BIND_INTEGRAL(config::server::max_virtual_server, 16, -1, 999999);
            ADD_DESCRIPTION("Set the limit for maximal virtual servers. -1 means unlimited.");
            ADD_NOTE_RELOADABLE();
        }
        {
            BIND_GROUP(limits);

            {
                CREATE_BINDING("poke_message_length",  FLAG_RELOADABLE);
                BIND_INTEGRAL(config::server::limits::poke_message_length, 1024, 1, 262144);
                ADD_NOTE_RELOADABLE();
            }

            {
                CREATE_BINDING("talk_power_request_message_length",  FLAG_RELOADABLE);
                BIND_INTEGRAL(config::server::limits::talk_power_request_message_length, 50, 1, 262144);
                ADD_NOTE_RELOADABLE();
            }

            {
                CREATE_BINDING("afk_message_length",  FLAG_RELOADABLE);
                BIND_INTEGRAL(config::server::limits::afk_message_length, 50, 1, 262144);
                ADD_NOTE_RELOADABLE();
            }
        }
        {
            /*
            BIND_GROUP(badges);
            {
                CREATE_BINDING("badges", 0);
                BIND_BOOL(config::server::badges::allow_badges, true);
                ADD_DESCRIPTION("Allow or disallow TeamSpeak badges");
            }
            {
                CREATE_BINDING("overwolf", 0);
                BIND_BOOL(config::server::badges::allow_overwolf, true);
                ADD_DESCRIPTION("Allow or disallow the Overwolf badge");
            }
             */
        }
        {
            BIND_GROUP(authentication);
            {
                CREATE_BINDING("name", FLAG_RELOADABLE);
                BIND_BOOL(config::server::authentication::name, false);
                ADD_DESCRIPTION("Allow or disallow client authentication just by their name");
                ADD_NOTE_RELOADABLE();
            }
        }
        {
            using WelcomeMessageType = config::server::clients::WelcomeMessageType;
            BIND_GROUP(clients);

            /* TeamSpeak */
            {
                CREATE_BINDING("teamspeak", FLAG_RELOADABLE);
                BIND_BOOL(config::server::clients::teamspeak, true);
                ADD_DESCRIPTION("Allow/disallow the TeamSpeak 3 client to join the server.");
                ADD_NOTE_RELOADABLE();
            }
            {
                CREATE_BINDING("not_allowed_message", FLAG_RELOADABLE);
                BIND_STRING(config::server::clients::teamspeak_not_allowed_message, "");
                ADD_DESCRIPTION("Change the message, displayed when denying the access to the server");
                ADD_NOTE_RELOADABLE();
            }
            {
                CREATE_BINDING("teamspeak_message", 0); /* No reload flag else we could just manipulate the licensing thing */
                BIND_STRING(config::server::clients::extra_welcome_message_teamspeak, "");
                ADD_DESCRIPTION("Add an extra welcome message for TeamSpeak client users");
                /* Don't make the reloadable else you could just disable the outdated client server join message */
                //ADD_NOTE_RELOADABLE();
            }
            {
                CREATE_BINDING("teamspeak_message_type", 0); /* No reload flag else we could just manipulate the licensing thing */
                BIND_INTEGRAL(config::server::clients::extra_welcome_message_type_teamspeak, WelcomeMessageType::WELCOME_MESSAGE_TYPE_NONE, WelcomeMessageType::WELCOME_MESSAGE_TYPE_MIN, WelcomeMessageType::WELCOME_MESSAGE_TYPE_MAX);
                ADD_DESCRIPTION("The welcome message type modes");
                ADD_DESCRIPTION(std::to_string(WelcomeMessageType::WELCOME_MESSAGE_TYPE_NONE) + " - None, do nothing");
                ADD_DESCRIPTION(std::to_string(WelcomeMessageType::WELCOME_MESSAGE_TYPE_CHAT) + " - Message, sends this message before the server welcome message");
                ADD_DESCRIPTION(std::to_string(WelcomeMessageType::WELCOME_MESSAGE_TYPE_POKE) + " - Message, pokes the client with the message when he enters the server");
                /* Don't make the reloadable else you could just disable the outdated client server join message */
                //ADD_NOTE_RELOADABLE();
            }

            /* TeaSpeak */
            /*
            {
                CREATE_BINDING("teaspeak", FLAG_RELOADABLE);
                BIND_BOOL(config::server::clients::teaspeak, true);
                ADD_DESCRIPTION("Allow/disallow the TeaSpeak - Client to join the server.");
                ADD_NOTE_RELOADABLE();
            }
            */
            {
                CREATE_BINDING("teaspeak_message", FLAG_RELOADABLE);
                BIND_STRING(config::server::clients::extra_welcome_message_teaspeak, "");
                ADD_DESCRIPTION("Add an extra welcome message for the TeaSpeak - Client users");
                ADD_NOTE_RELOADABLE();
            }
            {
                CREATE_BINDING("teaspeak_message_type", FLAG_RELOADABLE);
                BIND_INTEGRAL(config::server::clients::extra_welcome_message_type_teaspeak, WelcomeMessageType::WELCOME_MESSAGE_TYPE_NONE, WelcomeMessageType::WELCOME_MESSAGE_TYPE_MIN, WelcomeMessageType::WELCOME_MESSAGE_TYPE_MAX);
                ADD_DESCRIPTION("The welcome message type modes");
                ADD_DESCRIPTION(std::to_string(WelcomeMessageType::WELCOME_MESSAGE_TYPE_NONE) + " - None, do nothing");
                ADD_DESCRIPTION(std::to_string(WelcomeMessageType::WELCOME_MESSAGE_TYPE_CHAT) + " - Message, sends this message before the server welcome message");
                ADD_DESCRIPTION(std::to_string(WelcomeMessageType::WELCOME_MESSAGE_TYPE_POKE) + " - Message, pokes the client with the message when he enters the server");
                ADD_NOTE_RELOADABLE();
            }


            /* TeaWeb */
            {
                CREATE_BINDING("teaweb", FLAG_RELOADABLE);
                BIND_BOOL(config::server::clients::teaweb, true);
                ADD_DESCRIPTION("Allow/disallow the TeaSpeak - Web client to join the server.");
                ADD_NOTE_RELOADABLE();
            }
            {
                CREATE_BINDING("not_allowed_message", FLAG_RELOADABLE);
                BIND_STRING(config::server::clients::teaweb_not_allowed_message, "");
                ADD_DESCRIPTION("Chaneg the message, displayed when denying the access to the server");
                ADD_NOTE_RELOADABLE();
            }
            {
                CREATE_BINDING("teaweb_message", FLAG_RELOADABLE);
                BIND_STRING(config::server::clients::extra_welcome_message_teaweb, "");
                ADD_DESCRIPTION("Add an extra welcome message for the TeaSpeak - Web client users");
                ADD_NOTE_RELOADABLE();
            }
            {
                CREATE_BINDING("teaweb_message_type", FLAG_RELOADABLE);
                BIND_INTEGRAL(config::server::clients::extra_welcome_message_type_teaweb, WelcomeMessageType::WELCOME_MESSAGE_TYPE_NONE, WelcomeMessageType::WELCOME_MESSAGE_TYPE_MIN, WelcomeMessageType::WELCOME_MESSAGE_TYPE_MAX);
                ADD_DESCRIPTION("The welcome message type modes");
                ADD_DESCRIPTION(std::to_string(WelcomeMessageType::WELCOME_MESSAGE_TYPE_NONE) + " - None, do nothing");
                ADD_DESCRIPTION(std::to_string(WelcomeMessageType::WELCOME_MESSAGE_TYPE_CHAT) + " - Message, sends this message before the server welcome message");
                ADD_DESCRIPTION(std::to_string(WelcomeMessageType::WELCOME_MESSAGE_TYPE_POKE) + " - Message, pokes the client with the message when he enters the server");
                ADD_NOTE_RELOADABLE();
            }

            {
                CREATE_BINDING("ignore_max_clone_permissions", FLAG_RELOADABLE);
                BIND_BOOL(config::server::clients::ignore_max_clone_permissions, false);
                ADD_DESCRIPTION("Allows you to disable the permission checks for i_client_max_clones_uid, i_client_max_clones_ip and i_client_max_clones_hwid");
                ADD_SENSITIVE();
                ADD_NOTE_RELOADABLE();
            }
        }
    }
    {
        BIND_GROUP(web);
        {
            CREATE_BINDING("enabled", 0);
            BIND_BOOL(config::web::activated, true);
            ADD_DESCRIPTION("Disable/enable the possibility to connect via the TeaSpeak web client");
            ADD_NOTE("If you've disabled this feature the TeaClient wound be able to join too.");
        }
        /* LibNice has been build without this
        {
            CREATE_BINDING("upnp", 0);
            BIND_BOOL(config::web::enable_upnp, false);
            ADD_DESCRIPTION("Disable/enable UPNP support");
            ADD_SENSITIVE();
        }
         */
        {
            BIND_GROUP(ssl)
            {
                CREATE_BINDING("certificate", FLAG_RELOADABLE);
                ADD_NOTE_RELOADABLE();
                binding->type = 4;

                /* no terminal handling */
                binding->read_argument = [](const std::string&) {
                    logError(LOG_GENERAL, "Failed to parse ssl certificate. Its only possible to configure them via config!");
                };
                binding->default_value = []() -> deque<string> { return {}; };

                //Unused :)
                binding->set_default = [](YAML::Node& node) {
                    auto default_node = node["default"];
                    default_node["certificate"] = "default_certificate.pem";
                    default_node["private_key"] = "default_privatekey.pem";
                };

                weak_ptr<EntryBinding> _binding = binding;
                binding->read_config = [_binding](YAML::Node& node) {
                    auto b = _binding.lock();
                    if(!b) return;
                    config::web::ssl::certificates.clear();

                    if(!node.IsDefined() || node.IsNull())
                        return;

                    for(auto it = node.begin(); it != node.end(); it++) {
                        auto node_cert = it->second["certificate"];
                        auto node_key = it->second["private_key"];
                        if(!node_cert.IsDefined()) {
                            logError(LOG_GENERAL, "Failed to parse web certificate. Missing \"certificate\" key.");
                            continue;
                        }
                        if(!node_key.IsDefined()) {
                            logError(LOG_GENERAL, "Failed to parse web certificate. Missing \"private_key\" key.");
                            continue;
                        }

                        config::web::ssl::certificates.emplace_back(it->first.as<string>(), node_key.as<string>(), node_cert.as<string>());
                    }
                };
            }
            /*
            {
                CREATE_BINDING("certificate", 0);
                BIND_STRING(config::web::ssl::certFile, "certs/default_certificate.pem");
                ADD_DESCRIPTION("The SSL certificate for the web client");
            }
            {
                CREATE_BINDING("privatekey", 0);
                BIND_STRING(config::web::ssl::keyFile, "certs/default_privatekey.pem");
                ADD_DESCRIPTION("The SSL private key for the web client (You have to export the key without a password!)");
            }
            */
        }

        {
            CREATE_BINDING("webrtc.port_min", FLAG_RELOADABLE);
            BIND_INTEGRAL(config::web::webrtc_port_min, 50000, 0, 65535);
            ADD_DESCRIPTION("Define the port range within the web client and TeaClient operates in");
            ADD_DESCRIPTION("A port of zero stands for no limit");
            ADD_NOTE("These ports must opened to use the voice bridge (Protocol: UDP)");
        }
        {
            CREATE_BINDING("webrtc.port_max", FLAG_RELOADABLE);
            BIND_INTEGRAL(config::web::webrtc_port_max, 56000, 0, 65535);
            ADD_DESCRIPTION("Define the port range within the web client and TeaClient operates in");
            ADD_DESCRIPTION("A port of zero stands for no limit");
            ADD_NOTE("These ports must opened to use the voice bridge (Protocol: UDP)");
        }
        {
            CREATE_BINDING("webrtc.stun.enabled", FLAG_RELOADABLE);
            BIND_INTEGRAL(config::web::stun_enabled, true, false, true);
            ADD_DESCRIPTION("Whatever to use a STUN server");
            ADD_NOTE_RELOADABLE();
        }
        {
            CREATE_BINDING("webrtc.stun.host", FLAG_RELOADABLE);
            BIND_STRING(config::web::stun_host, "stun.l.google.com");
            ADD_DESCRIPTION("The address of the stun server to use.");
            ADD_NOTE_RELOADABLE();
        }
        {
            CREATE_BINDING("webrtc.stun.port", FLAG_RELOADABLE);
            BIND_INTEGRAL(config::web::stun_port, 19302, 1, 0xFFFF);
            ADD_DESCRIPTION("Port of the stun server");
            ADD_NOTE_RELOADABLE();
        }
        {
            CREATE_BINDING("webrtc.udp", FLAG_RELOADABLE);
            BIND_INTEGRAL(config::web::udp_enabled, true, false, true);
            ADD_DESCRIPTION("Enable UDP for theweb client");
            ADD_NOTE_RELOADABLE();
        }
        {
            CREATE_BINDING("webrtc.tcp", FLAG_RELOADABLE);
            BIND_INTEGRAL(config::web::tcp_enabled, true, false, true);
            ADD_DESCRIPTION("Enable TCP for theweb client");
            ADD_NOTE_RELOADABLE();
        }
    }
    {
        BIND_GROUP(geolocation);
        {
            CREATE_BINDING("fallback_country", FLAG_RELOADABLE);
            BIND_STRING(config::geo::countryFlag, "DE");
            ADD_DESCRIPTION("The fallback country if lookup fails");
            ADD_NOTE_RELOADABLE();
        }
        {
            CREATE_BINDING("force_fallback_country", FLAG_RELOADABLE);
            BIND_BOOL(config::geo::staticFlag, false);
            ADD_DESCRIPTION("Enforce the default country and disable resolve");
            ADD_NOTE_RELOADABLE();
        }

        {
            CREATE_BINDING("mapping.file", 0);
            BIND_STRING(config::geo::mappingFile, "geoloc/IP2Location.CSV");
            ADD_DESCRIPTION("The mapping file for the given provider");
            ADD_DESCRIPTION("Default for IP2Location: geoloc/IP2Location.CSV");
            ADD_DESCRIPTION("Default for Software77: geoloc/IpToCountry.csv");
        }
        {
            CREATE_BINDING("mapping.type", 0);
            BIND_INTEGRAL(config::geo::type, geoloc::PROVIDER_IP2LOCATION, geoloc::PROVIDER_MIN, geoloc::PROVIDER_MAX);
            ADD_DESCRIPTION("The IP 2 location resolver");
            ADD_DESCRIPTION("0 = IP2Location");
            ADD_DESCRIPTION("1 = Software77");
        }
        {
            BIND_GROUP(vpn);
            {
                CREATE_BINDING("file", 0);
                BIND_STRING(config::geo::vpn_file, "geoloc/ipcat.csv");
                ADD_DESCRIPTION("The mapping file for vpn checker (https://github.com/client9/ipcat/blob/master/datacenters.csv)");
            }
            {
                CREATE_BINDING("enabled", 0);
                BIND_BOOL(config::geo::vpn_block,false);
                ADD_DESCRIPTION("Disable/enable the vpn detection");
            }
        }
    }
    {
        BIND_GROUP(music)
        {
            CREATE_BINDING("enabled", 0);
            BIND_BOOL(config::music::enabled, true);
            ADD_DESCRIPTION("Enable/disable the music bots");
        }
    }
    {
        BIND_GROUP(messages);
        {
            CREATE_BINDING("voice.server_stop", FLAG_RELOADABLE);
            BIND_STRING(config::messages::serverStopped, "Server stopped");
            ADD_NOTE_RELOADABLE();
        }
        {
            CREATE_BINDING("application.stop", FLAG_RELOADABLE);
            BIND_STRING(config::messages::applicationStopped, "Application stopped");
            ADD_NOTE_RELOADABLE();
        }
        {
            CREATE_BINDING("application.crash", FLAG_RELOADABLE);
            BIND_STRING(config::messages::applicationCrashed, "Application crashed");
            ADD_NOTE_RELOADABLE();
        }
        {
            CREATE_BINDING("idle_time", FLAG_RELOADABLE);
            BIND_STRING(config::messages::idle_time_exceeded, "Idle time exceeded");
            ADD_NOTE_RELOADABLE();
        }
        {
            CREATE_BINDING("teamspeak_permission_editor", FLAG_RELOADABLE);
            BIND_STRING(config::messages::teamspeak_permission_editor, "\n[b][COLOR=#aa0000]ATTENTION[/COLOR][/b]:\nIt seems like you're trying to edit the TeaSpeak permissions with the TeamSpeak 3 client!\nThis is [b]really[/b] buggy due a bug within the client you're using.\n\nWe recommand to [b]use the [url=https://web.teaspeak.de/]TeaSpeak-Web[/url][/b] client or the [b][url=https://teaspeak.de/]TeaSpeak client[/url][/b].\nYatQA is a good option as well.\n\nTo disable/edit this message please edit the config.yml\nPlease note: Permission bugs, which will be reported wound be accepted.");
            ADD_NOTE_RELOADABLE();
        }
        {
            BIND_GROUP(mute);
            {
                CREATE_BINDING("mute_message", FLAG_RELOADABLE);
                BIND_STRING(config::messages::mute_notify_message, "Hey!\nI muted you!");
                ADD_NOTE_RELOADABLE();
            }
            {
                CREATE_BINDING("unmute_message", FLAG_RELOADABLE);
                BIND_STRING(config::messages::unmute_notify_message, "Hey!\nI unmuted you!");
                ADD_NOTE_RELOADABLE();
            }
        }
        {
            BIND_GROUP(kick_invalid);
            {
                CREATE_BINDING("hardware_id", FLAG_RELOADABLE);
                BIND_STRING(config::messages::kick_invalid_hardware_id, "Invalid hardware id. Protocol hacked?");
                ADD_NOTE_RELOADABLE();
            }
            {
                CREATE_BINDING("command", FLAG_RELOADABLE);
                BIND_STRING(config::messages::kick_invalid_hardware_id, "Invalid command. Protocol hacked?");
                ADD_NOTE_RELOADABLE();
            }
            {
                CREATE_BINDING("badges", FLAG_RELOADABLE);
                BIND_STRING(config::messages::kick_invalid_hardware_id, "Invalid badges. Protocol hacked?");
                ADD_NOTE_RELOADABLE();
            }
        }
        {
            CREATE_BINDING("vpn.kick", FLAG_RELOADABLE);
            BIND_STRING(config::messages::kick_vpn, "Please disable your VPN! (Provider: ${provider.name})");
            ADD_DESCRIPTION("This is the kick/ban message when a client tries to connect with a vpn");

            ADD_DESCRIPTION("Variables are enabled. Available:");
            ADD_DESCRIPTION(" - provider.name => Contains the provider of the ip which has been flaged as vps");
            ADD_DESCRIPTION(" - provider.website => Contains the website provider of the ip which has been flaged as vps");

            ADD_NOTE_RELOADABLE();
        }
        {
            BIND_GROUP(shutdown);
            {
                CREATE_BINDING("scheduled", FLAG_RELOADABLE);
                BIND_STRING(config::messages::shutdown::scheduled, "[b][color=#DA9100]Scheduled shutdown at ${time}(%Y-%m-%d %H:%M:%S)[/color][/b]");
                ADD_NOTE_RELOADABLE();
            }
            {
                CREATE_BINDING("interval", FLAG_RELOADABLE);
                BIND_STRING(config::messages::shutdown::interval, "[b][color=red]Server instance shutting down in ${interval}[/color][/b]");
                ADD_DESCRIPTION("${interval} is defined via map in 'intervals'");
                ADD_NOTE_RELOADABLE();
            }
            {
                CREATE_BINDING("intervals", 0);
                binding->default_value = []() { return deque<string>{}; };
                binding->type = 4;

                weak_ptr weak_binding = binding;
                binding->read_config = [weak_binding](YAML::Node& node) {
                    auto bind = weak_binding.lock();
                    if(!bind) return;

                    if(node.IsNull() || !node.IsDefined() || !node.IsMap())
                        bind->set_default(node);

                    try {
                        auto intervals = node.as<map<size_t, string>>();
                        for(const auto& pair : intervals)
                            config::messages::shutdown::intervals.push_back({seconds(pair.first), pair.second});
                    } catch(YAML::Exception& ex) {
                        logError(LOG_GENERAL, "Failed to parse shutdown intervals! Exception: {}", ex.what());
                    }
                };

                binding->set_default = [weak_binding](YAML::Node& node) {
                    auto bind = weak_binding.lock();
                    if(!bind) return;

                    node = YAML::Node();

                    const static vector<pair<size_t, string>> intervals = {
                            {1, "1 second"},
                            {2, "2 seconds"},
                            {3, "3 seconds"},
                            {4, "4 seconds"},
                            {5, "5 seconds"},
                            {10, "10 seconds"},
                            {20, "20 seconds"},
                            {30, "30 seconds"},
                            {60, "1 minute"},
                            {2 * 60, "2 minutes"},
                            {3 * 60, "3 minutes"},
                            {5 * 60, "5 minutes"},
                            {10 * 60, "10 minutes"},
                            {20 * 60, "20 minutes"},
                            {30 * 60, "30 minutes"},
                            {60 * 60, "1 hour"},
                            {2 * 60 * 60, "2 hours"},
                    };

                    for(const auto& pair : intervals)
                        node[to_string(pair.first)] = pair.second;
                };
                ADD_DESCRIPTION("Add or delete intervals as you want");
            }
            {
                CREATE_BINDING("now", FLAG_RELOADABLE);
                BIND_STRING(config::messages::shutdown::now, "[b][color=red]Server instance shutting down in now[/color][/b]");
                ADD_NOTE_RELOADABLE();
            }
            {
                CREATE_BINDING("canceled", FLAG_RELOADABLE);
                BIND_STRING(config::messages::shutdown::canceled, "[b][color=green]Scheduled instance shutdown canceled![/color][/b]");
                ADD_NOTE_RELOADABLE();
            }
        }
        {
            BIND_GROUP(music);
            {
                CREATE_BINDING("song_announcement", FLAG_RELOADABLE);
                BIND_STRING(config::messages::music::song_announcement, "Now replaying ${title} (${url}) added by ${invoker}");
                ADD_DESCRIPTION("${title} title of the song");
                ADD_DESCRIPTION("${description} description of the song");
                ADD_DESCRIPTION("${url} url of the song");
                ADD_DESCRIPTION("${invoker} link to the song adder");
            }
        }
        {
            BIND_GROUP(timeout);
            {
                CREATE_BINDING("connection_reinitialized", FLAG_RELOADABLE);
                BIND_STRING(config::messages::timeout::connection_reinitialized, "Connection lost");
                ADD_NOTE_RELOADABLE();
            }
            {
                CREATE_BINDING("packet_resend_failed", FLAG_RELOADABLE);
                BIND_STRING(config::messages::timeout::packet_resend_failed, "Packet resend failed");
                ADD_NOTE_RELOADABLE();
            }
        }
    }
    {
        BIND_GROUP(threads);
        {
            CREATE_BINDING("ticking", 0);
            BIND_INTEGRAL(config::threads::ticking, 2, 1, 128);
            ADD_DESCRIPTION("Thread pool size for the ticking task of a VirtualServer");
            ADD_SENSITIVE();
        }
        {
            BIND_GROUP(music);
            {
                CREATE_BINDING("execute_limit", 0);
                BIND_INTEGRAL(config::threads::music::execute_limit, 5, 1, 1024);
                ADD_DESCRIPTION("Max number of threads for command handling on the instance");
                ADD_SENSITIVE();
            }
            {
                CREATE_BINDING("execute_per_bot", 0);
                BIND_INTEGRAL(config::threads::music::execute_per_bot, 1, 1, 128);
                ADD_DESCRIPTION("Threads per server for command executing");
                ADD_SENSITIVE();
            }
        }
        {
            CREATE_BINDING("command_execute", 0);
            BIND_INTEGRAL(config::threads::command_execute, 4, 1, 128);
            ADD_DESCRIPTION("Command executors");
            ADD_SENSITIVE();
        }
        {
            CREATE_BINDING("network_events", 0);
            BIND_INTEGRAL(config::threads::network_events, 4, 1, 128);
            ADD_DESCRIPTION("Network event loops");
            ADD_SENSITIVE();
        }
        {
            BIND_GROUP(voice)
            {
                CREATE_BINDING("events_per_server", 0);
                BIND_INTEGRAL(config::threads::voice::events_per_server, 4, 1, 128);
                ADD_DESCRIPTION("Kernel events per server socket");
                ADD_DESCRIPTION("This value is upper bound to threads.network_events and should not exceed it.");
                ADD_SENSITIVE();
            }
        }
    }
    return _create_bindings = result;
}


inline string apply_comments(stringstream &in, map<string, deque<string>>& comments) {
    stringstream out;
    stringstream lineBuffer;

    vector<string> tree;
    vector<int> deepness;

    char read;

    //The header
    for(const auto& comment : comments["header"])
        out << "#" << escapeHeaderString(comment) << endl;

    while(in){
        int deep = 0;

        do {
            read = static_cast<char>(in.get());
            if(read != ' ' && read != '\t') break;
            lineBuffer << read;
            deep++;
        } while(in);
        assert(read != ' ' && read != '\t');

        stringstream keyStream;
        stringstream pathStream;
        std::string key;
        std::string path;

        if(read == '-') { //We have a list entry
            lineBuffer << read;
            goto writeUntilNewLine;
        }

        do {
            lineBuffer << read;
            if(read == ':') break;
            keyStream << read;
            read = static_cast<char>(in.get());
        } while(in);
        assert(read == ':');

        key = keyStream.str();
        while (!deepness.empty() && deep <= deepness.back()) {
            deepness.pop_back();
            tree.pop_back();
        }
        deepness.push_back(deep);
        tree.push_back(key);

        for(const auto& entry : tree)
            pathStream << "." << entry;
        path = pathStream.str().substr(1);
        //cout << "having key " << key << " at deep " << deep << " - " << deepness.size() << " - " << path << endl;
        for(const auto& comment : comments[path]){
            for(int index = 0; index < deepness.back(); index++)
                out << " ";
            out << "#" << escapeHeaderString(comment) << endl;
        }
        writeUntilNewLine:
        out << lineBuffer.str();
        lineBuffer = stringstream();

        do {
            read = static_cast<char>(in.get());
            if(!in) break; //End of lstream reached :D
            out << read;
        } while(in && read != '\n');
    }
    assert(lineBuffer.str().empty());

    //The footer
    for(const auto& comment : comments["footer"])
        out << "#" << escapeJsonString(comment) << endl;

    return out.str();
}