
//include <iterator> /* Required for breakpad */
//#include <client/linux/handler/exception_handler.h>
#include <iostream>
#include <misc/strobf.h>
#include <CXXTerminal/QuickTerminal.h>
#include <event2/thread.h>
#include <log/LogUtils.h>
#include <ThreadPool/Timer.h>

#include "src/Configuration.h"
#include "src/VirtualServer.h"
#include "src/InstanceHandler.h"
#include "src/server/QueryServer.h"
#include "src/terminal/CommandHandler.h"
#include "src/client/InternalClient.h"
#include "src/SignalHandler.h"

#include <dlfcn.h>
#include <sys/time.h>
#include <sys/resource.h>

using namespace std;
using namespace std::chrono;

#define BUILD_CREATE_TABLE(tblName, types) "CREATE TABLE IF NOT EXISTS `" tblName "` (" types ")"

#define CREATE_TABLE(table, types)   \
result = sql::command(sqlData, BUILD_CREATE_TABLE(table, types)).execute();\
if(!result){\
    logger::logger(0)->critical("Could not setup sql tables. Command '{}' returns {}", BUILD_CREATE_TABLE(table, types), result.fmtStr());\
    goto stopApp;\
}

bool mainThreadActive = true;
bool mainThreadDone = false;


ts::server::InstanceHandler* serverInstance = nullptr;

extern void testTomMath();

#ifndef FUCK_CLION
    #define DB_NAME_BEG "TeaData"
    #define DB_NAME_END ".sqlite"
    #define DB_NAME DB_NAME_BEG DB_NAME_END
#else
    #define DB_NAME "TeaData.sqlite"
#endif

#include "src/client/music/internal_provider/channel_replay/ChannelProvider.h"
#include <codecvt>
#include <src/rtc/lib.h>
#include <src/terminal/PipedTerminal.h>

class CLIParser {
    public:
        CLIParser (int &argc, char **argv){
            for (int i = 1; i < argc; i++)
                this->tokens.emplace_back(argv[i]);
        }

        std::deque<std::string> getCmdOptions(const std::string &option) const {
            std::deque<std::string> result;

            auto itr = this->tokens.begin();
            while(true) {
                itr = std::find(itr, this->tokens.end(), option);
                if (itr != this->tokens.end() && ++itr != this->tokens.end()){
                    result.push_back(*itr);
                    itr++;
                } else break;
            }
            return result;
        }

        std::deque<std::string> getCmdOptionsBegins(const std::string &option) const {
            std::deque<std::string> result;

            for(const auto& token : this->tokens)
                if(token.find(option) == 0)
                    result.push_back(token);

            return result;
        }

        const std::string& get_option(const std::string &option) const {
            auto itr =  std::find(this->tokens.begin(), this->tokens.end(), option);
            if (itr != this->tokens.end() && ++itr != this->tokens.end()){
                return *itr;
            }
            static const std::string empty_string;
            return empty_string;
        }

        bool cmdOptionExists(const std::string &option) const{
            return std::find(this->tokens.begin(), this->tokens.end(), option) != this->tokens.end();
        }
    private:
        std::vector <std::string> tokens;
};

/* addr is where the exception identifier is stored
   id is the exception identifier.  */
void __raise_exception (void **addr, void *id);

#define T(address) \
std::cout << "Testing: " << address << " => "; \
{\
    sockaddr_storage storage;\
    net::resolve_address(address, storage);\
    std::cout << manager.contains(storage) << std::endl;\
}

#define CONFIG_NAME "config.yml"
const char *malloc_conf = ""; //retain:false"; //,dirty_decay_ms:0";
int main(int argc, char** argv) {
#ifdef HAVE_JEMALLOC
    (void*) malloc_conf;
#endif

#if 0
    {
        //ts::property::list<ts::property::InstanceProperties>()
        std::cout << "| Name | Type | Flags | Default Value | Description |  \n";
        std::cout << "|:-- | -- | -- | -- |:-- |  \n";
        for(const auto& property : ts::property::list<ts::property::VirtualServerProperties>()) {
            std::cout << "| `" << property->name << "` | ";

            switch(property->type_value) {
                case ts::property::TYPE_STRING:
                    std::cout << "String | ";
                    break;

                case ts::property::TYPE_BOOL:
                    std::cout << "Boolean | ";
                    break;

                case ts::property::TYPE_SIGNED_NUMBER:
                    std::cout << "Signed number | ";
                    break;

                case ts::property::TYPE_UNSIGNED_NUMBER:
                    std::cout << "Unsigned number | ";
                    break;

                case ts::property::TYPE_FLOAT:
                    std::cout << "Float | ";
                    break;

                default:
                    std::cout << "Unknown | ";
                    break;
            }

            std::string flags{};
            if(property->flags & ts::property::FLAG_INTERNAL) { flags += ", internal"; }
            if(property->flags & ts::property::FLAG_GLOBAL) { flags += ", global"; }

            if(property->flags & ts::property::FLAG_SNAPSHOT) { flags += ", snapshot"; }
            if(property->flags & ts::property::FLAG_SAVE) { flags += ", saved"; }
            if(property->flags & ts::property::FLAG_SAVE_MUSIC) { flags += ", saved (music)"; }

            if(property->flags & ts::property::FLAG_NEW) { flags += ", new"; }
            if(property->flags & ts::property::FLAG_SERVER_VARIABLE) { flags += ", server variable"; }
            if(property->flags & ts::property::FLAG_SERVER_VIEW) { flags += ", server view variable"; }

            if(property->flags & ts::property::FLAG_CLIENT_VARIABLE) { flags += ", client variable"; }
            if(property->flags & ts::property::FLAG_CLIENT_VIEW) { flags += ", client view variable"; }
            if(property->flags & ts::property::FLAG_CLIENT_INFO) { flags += ", client info variable"; }

            if(property->flags & ts::property::FLAG_CHANNEL_VARIABLE) { flags += ", channel variable"; }
            if(property->flags & ts::property::FLAG_CHANNEL_VIEW) { flags += ", channel view variable"; }

            // FLAG_GROUP_VIEW = FLAG_CHANNEL_VIEW << 1UL,
            if(property->flags & ts::property::FLAG_INSTANCE_VARIABLE) { flags += ", instance variable"; }
            if(property->flags & ts::property::FLAG_PLAYLIST_VARIABLE) { flags += ", playlist variable"; }
            if(property->flags & ts::property::FLAG_USER_EDITABLE) { flags += ", editable"; }

            if(!flags.empty()) {
                std::cout << flags.substr(2);
            }
            std::cout << "| ";
            if(property->default_value.empty()) {
                std::cout << "empty";
            } else {
                std::cout << "`" << property->default_value << "` ";
            }
            std::cout << "| ";
            std::cout << "No description ";
            std::cout << "| ";
            std::cout << "  \n";
        }
        return 0;
    }
#endif

    CLIParser arguments(argc, argv);
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();
    ts::permission::setup_permission_resolve();

    {
        auto evthread_use_pthreads_result = evthread_use_pthreads();
        assert(evthread_use_pthreads_result == 0);
        (void) evthread_use_pthreads_result;
    }

    if(!arguments.cmdOptionExists("--no-terminal")) {
        terminal::install();
        if(!terminal::active()) { cerr << "could not setup terminal!" << endl; return -1; }
    }

    if(arguments.cmdOptionExists("--help") || arguments.cmdOptionExists("-h")) {
        #define HELP_FMT "  {} {} | {}"
        logMessageFmt(true, LOG_GENERAL, "Available command line parameters:");
        logMessageFmt(true, LOG_GENERAL, HELP_FMT, "-h", "--help", "Shows this page");
        logMessageFmt(true, LOG_GENERAL, HELP_FMT, "-q", "--set_query_password", "Changed the server admin query password");
        logMessageFmt(true, LOG_GENERAL, HELP_FMT, "-P<property>=<value>", "--property:<property>=<value>", "Override a config value manual");
        logMessageFmt(true, LOG_GENERAL, HELP_FMT, "-l", "--property-list", "List all available properties");
        terminal::uninstall();
        return 0;
    }
    if(arguments.cmdOptionExists("--property-list") || arguments.cmdOptionExists("-l")) {
        logMessageFmt(true, LOG_GENERAL, "Available properties:");
        auto properties = ts::config::create_bindings();
        for(const auto& property : properties) {
            logMessageFmt(true, LOG_GENERAL, "  " + property->key);
            for(const auto& entry : property->description) {
                if(entry.first.empty()) {
                    for(const auto& line : entry.second)
                        logMessageFmt(true, LOG_GENERAL, "    " + line);
                } else {
                    logMessageFmt(true, LOG_GENERAL, "    " + entry.first + ":");
                    for(const auto& line : entry.second)
                        logMessageFmt(true, LOG_GENERAL, "      " + line);
                }
            }
            logMessageFmt(true, LOG_GENERAL, "    " + property->value_description());
        }
        return 0;
    }

    if(!arguments.cmdOptionExists("--valgrind")) {
        ts::syssignal::setup();
    }
    ts::syssignal::setup_threads();

    map<string, string> override_settings;
    {
        auto short_override = arguments.getCmdOptionsBegins("-P");
        for(const auto& entry : short_override) {
            if(entry.length() < 2) continue;
            auto ei = entry.find('=');
            if(ei == string::npos || ei == 2) {
                logErrorFmt(true, LOG_GENERAL, "Invalid command line parameter. (\"" + entry + "\")");
                return 1;
            }

            auto key = entry.substr(2, ei - 2);
            auto value = entry.substr(ei + 1);
            override_settings[key] = value;
        }
    }
    {
        auto short_override = arguments.getCmdOptionsBegins("--property:");
        for(const auto& entry : short_override) {
            if(entry.length() < 11) continue;
            auto ei = entry.find('=');
            if(ei == string::npos || ei == 11) {
                logErrorFmt(true, LOG_GENERAL, "Invalid command line parameter. (\"" + entry + "\")");
                return 1;
            }

            auto key = entry.substr(11, ei - 11);
            auto value = entry.substr(ei + 1);
            override_settings[key] = value;
        }
    }

    {
        auto bindings = ts::config::create_bindings();
        for(const auto& setting : bindings) {
            for(auto it = override_settings.begin(); it != override_settings.end(); it++) {
                if(it->first == setting->key) {
                    try {
                        setting->read_argument(it->second);
                    } catch(const std::exception& ex) {
                        logErrorFmt(true, LOG_GENERAL, "Failed to apply value for given property '" + it->first + "': " + ex.what());
                    }
                    override_settings.erase(it);
                    break;
                }
            }
        }
        for(const auto& entry : override_settings) {
            logMessageFmt(true, LOG_GENERAL, "Missing property " + entry.first + ". Value unused!");
        }
    }
    /*
    {
        auto a = malloc(10);     // 0xa04010
        auto b = malloc(10);     // 0xa04030
        auto c = malloc(10);     // 0xa04050

        free(a);
        free(b);  // To bypass "double free or corruption (fasttop)" check
        free(a);  // Double Free !!

        auto d = malloc(10);     // 0xa04010
        auto e = malloc(10);     // 0xa04030
        auto f = malloc(10);     // 0xa04010   - Same as 'd' !
    }
    */
    /*
    std::string error;
    if(!interaction::waitForAttach(error)){
        cerr << "Rsult: " << error << endl;
    }

    while(interaction::memoryInfo()){
        usleep(1 * 1000 * 1000);
        logMessage("Current instances: " + to_string(interaction::memoryInfo()->instanceCount) + "/" + to_string(interaction::memoryInfo()->maxInstances));
    }

    interaction::removeMemoryHook();
     if(true) return 0;
     */

    {
        //http://git.mcgalaxy.de/WolverinDEV/tomcrypt/blob/develop/src/misc/crypt/crypt_inits.c#L40-86
        std::string descriptors = "LTGE";
        bool crypt_init = false;
        for(const auto& c : descriptors) {
            if((crypt_init |= crypt_mp_init(&c))) {
                break;
            }
        }

        if(!crypt_init) {
            logCritical(LOG_GENERAL, "Could not initialise libtomcrypt mp descriptors!");
            return 1;
        }
        if(register_prng(&sprng_desc) == -1) {
            logCritical(LOG_GENERAL, "could not setup prng");
            return EXIT_FAILURE;
        }
        if (register_cipher(&rijndael_desc) == -1) {
            logCritical(LOG_GENERAL, "could not setup rijndael");
            return EXIT_FAILURE;
        }
        testTomMath();
    }

    ts::server::SqlDataManager* sql = nullptr;
    std::string errorMessage;
    shared_ptr<logger::LoggerConfig> logConfig = nullptr;
    std::string line;

    logMessageFmt(true, LOG_GENERAL, "Loading configuration");
    auto cfgErrors = ts::config::parseConfig(CONFIG_NAME);
    if(!cfgErrors.empty()){
        logErrorFmt(true, LOG_GENERAL, "Could not load configuration. Errors: (" + to_string(cfgErrors.size()) + ")");
        for(const auto& entry : cfgErrors)
            logErrorFmt(true, LOG_GENERAL, " - {}", entry);
        logErrorFmt(true, LOG_GENERAL, "Stopping server...");
        goto stopApp;
    }

    logMessage(LOG_GENERAL, "Setting up logging");
    logConfig = make_shared<logger::LoggerConfig>();
    logConfig->logfileLevel = (spdlog::level::level_enum) ts::config::log::logfileLevel;
    logConfig->terminalLevel = (spdlog::level::level_enum) ts::config::log::terminalLevel;
    logConfig->file_colored = ts::config::log::logfileColored;
    logConfig->logPath = ts::config::log::path;
    logConfig->vs_group_size = ts::config::log::vs_size;
    logConfig->sync = !terminal::instance();

    logger::setup(logConfig);
    threads::timer::function_log = [](const std::string& message, bool debug) {
        auto msg = message.find('\n') == std::string::npos ? message : message.substr(0, message.find('\n'));
        if(debug)
            debugMessage(LOG_GENERAL, msg);
        else
            logWarning(LOG_GENERAL, msg);
    };

    logger::updateLogLevels();
    logMessage(LOG_GENERAL, "Starting TeaSpeak-Server v{}", build::version()->string(true));

    {
        debugMessage(LOG_GENERAL, "Initializing RTP library version {}", ts::rtc::version());

        std::string error;
        if(!ts::rtc::initialize(error)) {
            logCritical(LOG_GENERAL, "Failed to initialize RTC library: {}", error);
            return EXIT_FAILURE;
        }
    }

    if(ts::config::license_original && ts::config::license_original->data.type != license::LicenseType::DEMO){
        logMessageFmt(true, LOG_GENERAL, strobf("[]---------------------------------------------------------[]").string());
        logMessageFmt(true, LOG_GENERAL, strobf("  §aThank you for buying the TeaSpeak-§lPremium-§aSoftware!  ").string());
        logMessageFmt(true, LOG_GENERAL, strobf("  §aLicense information:").string());
        logMessageFmt(true, LOG_GENERAL, strobf("     §aLicense owner  : §e").string() + ts::config::license_original->owner());
        logMessageFmt(true, LOG_GENERAL, strobf("     §aLicense type   : §e").string() + license::LicenseTypeNames[ts::config::license_original->data.type]);

        if(ts::config::license_original->end().time_since_epoch().count() == 0){
            logMessageFmt(true, LOG_GENERAL, strobf("     §aLicense expires: §enever").string());
        } else {
            char timeBuffer[32];
            time_t t = duration_cast<seconds>(ts::config::license_original->end().time_since_epoch()).count();
            tm* stime = localtime(&t);
            strftime(timeBuffer, 32, "%c", stime);
            logMessageFmt(true, LOG_GENERAL, strobf("     §aLicense expires: §e").string() + string(timeBuffer));
        }
        logMessageFmt(true, LOG_GENERAL, string() + strobf("     §aLicense valid  : ").string() + (ts::config::license_original->isValid() ? strobf("§ayes").string() : strobf("§cno").string()));
        logMessageFmt(true, LOG_GENERAL, strobf("[]---------------------------------------------------------[]").string());
    }

    {
        rlimit rlimit{0, 0};
        //forum.teaspeak.de/index.php?threads/2570/
        constexpr auto seek_help_message = "For more help visit the forum and read this thread (https://forum.teaspeak.de/index.php?threads/2570/).";
        if(getrlimit(RLIMIT_NOFILE, &rlimit) != 0) {
            //prlimit -n4096 -p pid_of_process
            logWarningFmt(true, LOG_INSTANCE, "Failed to get open file rlimit ({}). Please ensure its over 16384.", strerror(errno));
            logWarningFmt(true, LOG_INSTANCE, seek_help_message);
        } else {
            const auto original = rlimit.rlim_cur;
            rlimit.rlim_cur = std::max(rlimit.rlim_cur, std::min(rlimit.rlim_max, (rlim_t) 16384));
            if(original != rlimit.rlim_cur) {
                if(setrlimit(RLIMIT_NOFILE, &rlimit) != 0) {
                    logErrorFmt(true, LOG_INSTANCE, "Failed to set open file rlimit to {} ({}). Please ensure its over 16384.", rlimit.rlim_cur, strerror(errno));
                    logWarningFmt(true, LOG_INSTANCE, seek_help_message);
                    goto rlimit_updates;
                }
            }
            if(rlimit.rlim_cur < 16384) {
                logWarningFmt(true, LOG_INSTANCE, "Open file rlimit is bellow 16384 ({}). Please increase the system file descriptor limits.", rlimit.rlim_cur);
                logWarningFmt(true, LOG_INSTANCE, seek_help_message);
            }
        }
        rlimit_updates:;
    }

    logMessage(LOG_GENERAL, "Starting music providers");

    if(terminal::instance()) terminal::instance()->setPrompt("§aStarting server. §7[§aloading music§7]");
    if(ts::config::music::enabled && !arguments.cmdOptionExists("--no-providers")) {
        ::music::manager::loadProviders("providers");
        ::music::manager::register_provider(::music::provider::ChannelProvider::create_provider());
    }

    if(terminal::instance()) terminal::instance()->setPrompt("§aStarting server. §7[§aloading geoloc§7]");

    if(!ts::config::geo::staticFlag) {
        if(ts::config::geo::type == geoloc::PROVIDER_SOFTWARE77)
            geoloc::provider = new geoloc::Software77Provider(ts::config::geo::mappingFile);
        else if(ts::config::geo::type == geoloc::PROVIDER_IP2LOCATION)
            geoloc::provider = new geoloc::IP2LocationProvider(ts::config::geo::mappingFile);
        else {
            logCritical(LOG_GENERAL,"Invalid geo resolver type!");
        }
        if(geoloc::provider && !geoloc::provider->load(errorMessage)) {
            logCritical(LOG_GENERAL,"Could not setup geoloc! Fallback to default flag!");
            logCritical(LOG_GENERAL,"Message: {}", errorMessage);
            geoloc::provider = nullptr;
            errorMessage = "";
        }
    }
    if(ts::config::geo::vpn_block) {
        geoloc::provider_vpn = new geoloc::IPCatBlocker(ts::config::geo::vpn_file);

        if(geoloc::provider_vpn && !geoloc::provider_vpn->load(errorMessage)) {
            logCritical(LOG_GENERAL,"Could not setup vpn detector!");
            logCritical(LOG_GENERAL,"Message: {}", errorMessage);
            geoloc::provider_vpn = nullptr;
            errorMessage = "";
        }
    }
    if(terminal::instance()) terminal::instance()->setPrompt("§aStarting server. §7[§aloading sql§7]");

    sql = new ts::server::SqlDataManager();
    if(!sql->initialize(errorMessage)) {
        logCriticalFmt(true, LOG_GENERAL, "Could not initialize SQL!");
        if(errorMessage.find("database is locked") != string::npos) {
            logCriticalFmt(true, LOG_GENERAL, "----------------------------[ ATTENTION ]----------------------------");
            logCriticalFmt(true, LOG_GENERAL, "{:^69}", "Your database is already in use!");
            logCriticalFmt(true, LOG_GENERAL, "{:^69}", "Stop the other instance first!");
            logCriticalFmt(true, LOG_GENERAL, "----------------------------[ ATTENTION ]----------------------------");
        } else {
            logCriticalFmt(true, LOG_GENERAL, errorMessage);
        }
        goto stopApp;
    }

    if(terminal::instance()) terminal::instance()->setPrompt("§aStarting server. §7[§astarting instance§7]");

    serverInstance = new ts::server::InstanceHandler(sql); //if error than mainThreadActive = false
    if(!mainThreadActive || !serverInstance->startInstance())
        goto stopApp;

    if(arguments.cmdOptionExists("-q") || arguments.cmdOptionExists("--set_query_password")) {
        auto password = arguments.cmdOptionExists("-q") ? arguments.get_option("-q") : arguments.get_option("--set_query_password");
        if(!password.empty()) {
            logMessageFmt(true, LOG_GENERAL, "Updating server admin query password to \"{}\"", password);
            auto account = serverInstance->getQueryServer()->find_query_account_by_name("serveradmin");
            if(!account) {
                logErrorFmt(true, LOG_GENERAL, "Failed to update server admin query password! Login does not exists!");
            } else {
                if(!serverInstance->getQueryServer()->change_query_password(account, password)) {
                    logErrorFmt(true, LOG_GENERAL, "Failed to update server admin query password! (Internal error)");
                }
            }
        }
    }

    terminal::initialize_pipe(arguments.get_option("--pipe-path"));
    if(terminal::instance()) terminal::instance()->setPrompt("§7> §f");
    while(mainThreadActive) {
        usleep(5 * 1000);

        if(terminal::instance()) {
            if(terminal::instance()->linesAvailable() > 0){
                while(!(line = terminal::instance()->readLine("§7> §f")).empty())
                    threads::Thread(THREAD_DETACHED, [line]{
                        terminal::chandler::CommandHandle handle{};
                        handle.command = line;

                        if(!terminal::chandler::handleCommand(handle)) {
                            for(const auto& response : handle.response)
                                logErrorFmt(true, LOG_GENERAL, "{}", response);
                        } else {
                            for(const auto& response : handle.response)
                                logMessageFmt(true, LOG_GENERAL, "{}", response);
                        }
                    });
            }
        }
    }

    terminal::finalize_pipe();

    stopApp:
    logMessageFmt(true, LOG_GENERAL, "Stopping application");
    ::music::manager::finalizeProviders();

    if(serverInstance)
        serverInstance->stopInstance();
    delete serverInstance;
    serverInstance = nullptr;

    ts::music::MusicBotManager::shutdown();
    if(sql)
        sql->finalize();
    delete sql;
    logMessageFmt(true, LOG_GENERAL, "Application suspend successful!");

    logger::uninstall();
    if(terminal::active())
        terminal::uninstall();
    mainThreadDone = true;
    return 0;
}

/* Fix for Virtuzzo 7 where sometimes the pthread create fails! */

typedef int (*pthread_create_t)(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine) (void *), void *arg);
pthread_create_t original_pthread_create{nullptr};
int	pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine) (void *), void *arg) {
    if(!original_pthread_create) {
        original_pthread_create = (pthread_create_t) dlsym(RTLD_NEXT, "pthread_create");
        if(!original_pthread_create) {
            std::cerr << "[CRITICAL] Missing original pthread_create function. Aborting execution!" << std::endl;
            std::abort();
        }
    }

    int result, attempt{0}, sleep{5};
    while((result = original_pthread_create(thread, attr, start_routine, arg)) != 0 && errno == EAGAIN) {
        if(attempt > 55) {
            std::cerr << "[CRITICAL] pthread_create(...) cause EAGAIN for the last 50 attempts (~4.7seconds)! Aborting application execution!" << std::endl;
            std::abort();
        } else if(attempt > 5) {
            /* let some other threads do work */
            pthread_yield();
        } else if(attempt == 0) {
            std::string message{"[CRITICAL] Failed to spawn thread (Resource temporarily unavailable). Trying to recover."};
            std::cerr << message << std::endl;
        }

        //std::string message{"[CRITICAL] pthread_create(...) cause EAGAIN! Trying again in " + std::to_string(sleep) + "usec (Attempt: " + std::to_string(attempt) + ")"};
        //std::cerr << message << std::endl;
        usleep(sleep);
        attempt++;
        sleep = (int) (sleep * 1.25);
    }
    if(attempt > 0) {
        std::string message{"[CRITICAL] Successfully recovered from pthread_create() EAGAIN error. Took " + std::to_string(attempt) + " attempts."};
        std::cerr << message << std::endl;
    }
    return result;
}