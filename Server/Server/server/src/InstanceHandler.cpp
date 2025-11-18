#define XMALLOC undefined_malloc /* fix jemalloc and tomcrypt */
#define XCALLOC undefined_calloc
#define XFREE undefined_free
#define XREALLOC undefined_realloc

#include <log/LogUtils.h>
#include "InstanceHandler.h"
#include "src/client/InternalClient.h"
#include "src/server/QueryServer.h"
#include "src/manager/PermissionNameMapper.h"
#include "./FileServerHandler.h"
#include "./server/GlobalNetworkEvents.h"
#include <ThreadPool/Timer.h>
#include "ShutdownHelper.h"
#include <sys/utsname.h>
#include <misc/digest.h>
#include <misc/base64.h>
#include <misc/hex.h>
#include <misc/rnd.h>
#include <protocol/buffers.h>
#include "./groups/GroupManager.h"

#ifndef _POSIX_SOURCE
    #define _POSIX_SOURCE
#endif
#include <unistd.h>
#include "./manager/ActionLogger.h"
#include "./client/shared/ServerCommandExecutor.h"

#undef _POSIX_SOURCE

using namespace std;
using namespace std::chrono;
using namespace ts;
using namespace ts::server;

extern bool mainThreadActive;
InstanceHandler::InstanceHandler(SqlDataManager *sql) : sql(sql) {
    serverInstance = this;

    this->general_task_executor_ = std::make_shared<task_executor>(ts::config::threads::ticking, "instance tick ");
    this->general_task_executor_->set_exception_handler([](const std::string& task_name, const std::exception_ptr& exception) {
        std::string message{};
        try {
            std::rethrow_exception(exception);
        } catch (const std::exception& ex) {
            message = "std::exception::what() -> " + std::string{ex.what()};
        } catch(...) {
            message = "unknown exception";
        }

        logCritical(LOG_INSTANCE, "Instance task executor received exception: {}", message);
    });

    this->statistics = make_shared<stats::ConnectionStatistics>(nullptr);

    std::string error_message{};
    this->license_service_ = std::make_shared<license::LicenseService>();
    if(!this->license_service_->initialize(error_message)) {
        logCritical(LOG_INSTANCE, strobf("Failed to the license service: {}").string(), error_message);
    }
    this->dbHelper = new DatabaseHelper(this->getSql());

    this->action_logger_ = std::make_unique<log::ActionLogger>();
    if(!this->action_logger_->initialize(error_message)) {
        logCritical(LOG_INSTANCE, "Failed to initialize instance action logs: {}", error_message);
        logCritical(LOG_INSTANCE, "Action log has been disabled.");
        this->action_logger_->finalize();
    }

    this->_properties = std::make_shared<PropertyManager>();
    this->_properties->register_property_type<property::InstanceProperties>();
    this->properties()[property::SERVERINSTANCE_FILETRANSFER_PORT] = ts::config::binding::DefaultFilePort;
    this->properties()[property::SERVERINSTANCE_FILETRANSFER_HOST] = ts::config::binding::DefaultFileHost;
    this->properties()[property::SERVERINSTANCE_QUERY_PORT] = ts::config::binding::DefaultQueryPort;
    this->properties()[property::SERVERINSTANCE_QUERY_HOST] = ts::config::binding::DefaultQueryHost;


    auto result = sql::command(this->getSql(), "SELECT * FROM `properties` WHERE `id` = :id AND `type` = :type AND `serverId` = :serverId", variable{":id", 0}, variable{":serverId", 0}, variable{":type", property::PropertyType::PROP_TYPE_INSTANCE})
            .query([](InstanceHandler *instance, int length, char **values, char **columns) {
                string key, value;
                for (int index = 0; index < length; index++) {
                    if (strcmp(columns[index], "key") == 0) {
                        key = values[index];
                    } else if (strcmp(columns[index], "value") == 0) {
                        value = values[index] == nullptr ? "" : values[index];
                    }
                }

                const auto &info = property::find<property::InstanceProperties>(key);
                if(info == property::SERVERINSTANCE_UNDEFINED) {
                    logError(0, "Got an unknown instance property " + key);
                    return 0;
                }
                auto prop = instance->properties()[info];
                prop = value;
                prop.setDbReference(true);
                prop.setModified(false);
                return 0;
            }, this);
    if (!result) cerr << result << endl;
    this->_properties->registerNotifyHandler([&](Property &prop) {
        if ((prop.type().flags & property::FLAG_SAVE) == 0) {
            prop.setModified(false);
            return;
        }

        string sqlQuery;
        if (prop.hasDbReference())
            sqlQuery = "UPDATE `properties` SET `value` = :value WHERE `serverId` = :sid AND `type` = :type AND `id` = :id AND `key` = :key";
        else {
            prop.setDbReference(true);
            sqlQuery = "INSERT INTO `properties` (`serverId`, `type`, `id`, `key`, `value`) VALUES (:sid, :type, :id, :key, :value)";
        }

        sql::command(this->getSql(), sqlQuery, variable{":sid", 0}, variable{":type", property::PropertyType::PROP_TYPE_INSTANCE}, variable{":id", 0}, variable{":key", prop.type().name}, variable{":value", prop.value()})
                .executeLater().waitAndGetLater(LOG_SQL_CMD, sql::result{1, "future failed"});
    });
    this->properties()[property::SERVERINSTANCE_DATABASE_VERSION] = this->sql->get_database_version();
    this->properties()[property::SERVERINSTANCE_PERMISSIONS_VERSION] = this->sql->get_permissions_version();


    this->globalServerAdmin = std::make_shared<ts::server::InternalClient>(this->getSql(), nullptr, "serveradmin", true);
    this->globalServerAdmin->initialize_weak_reference(this->globalServerAdmin);
    ts::server::DatabaseHelper::assignDatabaseId(this->getSql(), 0, globalServerAdmin);

    this->_musicRoot = std::make_shared<InternalClient>(this->getSql(), nullptr, "Music Manager", false);
    dynamic_pointer_cast<InternalClient>(this->_musicRoot)->initialize_weak_reference(this->_musicRoot);

    {
        using GroupLoadResult = groups::GroupLoadResult;

        this->group_manager_ = std::make_shared<groups::GroupManager>(this->getSql(), 0, nullptr);
        if(!this->group_manager_->initialize(this->group_manager_, error_message)) {
            logCritical(LOG_INSTANCE, "Failed to initialize instance group manager: {}", error_message);
            mainThreadActive = false;
            return;
        }

        bool initialize_groups{false};
        switch(this->group_manager_->server_groups()->load_data(true)) {
            case GroupLoadResult::SUCCESS:
                break;
            case GroupLoadResult::NO_GROUPS:
                initialize_groups = true;
                break;

            case GroupLoadResult::DATABASE_ERROR:
                logCritical(LOG_INSTANCE, "Failed to load instance server groups (Database error)");
                mainThreadActive = false;
                return;
        }

        switch(this->group_manager_->channel_groups()->load_data(true)) {
            case GroupLoadResult::SUCCESS:
                break;
            case GroupLoadResult::NO_GROUPS:
                initialize_groups = true;
                break;

            case GroupLoadResult::DATABASE_ERROR:
                logCritical(LOG_INSTANCE, "Failed to load instance channel groups (Database error)");
                mainThreadActive = false;
                return;
        }

        if(!this->group_manager_->assignments().load_data(error_message)) {
            logCritical(LOG_INSTANCE, "Failed to load instance group assignments: {}", error_message);
            mainThreadActive = false;
            return;
        }

        if (initialize_groups) {
            if(!this->setupDefaultGroups()){
                logCritical(LOG_INSTANCE, "Could not setup server instance! Stopping...");
                mainThreadActive = false;
                return;
            }
        }

        this->validate_default_groups();
    }

    {
        this->default_tree = make_shared<ServerChannelTree>(nullptr, this->getSql());
        this->default_tree->loadChannelsFromDatabase();

        this->default_tree->deleteSemiPermanentChannels();
        if(this->default_tree->channel_count() == 0){
            logMessage(LOG_GENERAL, "Generating default tree");

            std::shared_ptr<BasicChannel>  ch;
            ch = this->default_tree->createChannel(0, 0, "[cspacer01]┏╋━━━━━━◥◣◆◢◤━━━━━━╋┓");
            ch = this->default_tree->createChannel(0, ch->channelId(), "[cspacer02] TeaSpeak Server");
            ch = this->default_tree->createChannel(0, ch->channelId(), "[cspacer03]┗╋━━━━━━◥◣◆◢◤━━━━━━╋┛");
            ch = this->default_tree->createChannel(0, ch->channelId(), "[cspacer04]Default Channel");
            this->default_tree->setDefaultChannel(ch);

            this->properties()[property::SERVERINSTANCE_UNIQUE_ID] = ""; /* we def got a new instance */
        }
        if(this->default_tree->channel_count() == 4) {
            auto default_channel = this->default_tree->findChannel("[cspacer04]Default Channel", nullptr);
            if(default_channel) {
                auto ch = this->default_tree->createChannel(0, default_channel->channelId(), "[cspacer05]Administrator Room");
                ch->permissions()->set_permission(permission::i_channel_needed_view_power, {75, 0}, permission::v2::set_value, permission::v2::do_nothing, false, false);
                this->save_channel_permissions();
            }
        }
        if(!this->default_tree->getDefaultChannel()) {
            this->default_tree->setDefaultChannel(this->default_tree->findChannel("[cspacer04]Default Channel", nullptr));
        }
        if(!this->default_tree->getDefaultChannel()) {
            this->default_tree->setDefaultChannel(*this->default_tree->channels().begin());
        }
        assert(this->default_tree->getDefaultChannel());
    }

    {
        this->default_server_properties = serverInstance->databaseHelper()->loadServerProperties(nullptr);
    }


    if(this->properties()[property::SERVERINSTANCE_MONTHLY_TIMESTAMP].as_or<int64_t>(0) == 0) {
        debugMessage(LOG_INSTANCE, "Setting up monthly reset timestamp!");
        this->properties()[property::SERVERINSTANCE_MONTHLY_TIMESTAMP] = duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
    }

    this->banMgr = new BanManager(this->getSql());
    this->banMgr->loadBans();
}

InstanceHandler::~InstanceHandler() {
    delete this->banMgr;
    delete this->dbHelper;

    group_manager_ = nullptr;
    globalServerAdmin = nullptr;
    _musicRoot = nullptr;

    statistics = nullptr;
}

inline string strip(std::string message) {
    while(!message.empty()) {
        if(message[0] == ' ')
            message = message.substr(1);
        else if(message[message.length() - 1] == ' ')
            message = message.substr(0, message.length() - 1);
        else break;
    }
    return message;
}

inline vector<string> split_hosts(const std::string& message, char delimiter) {
    vector<string> result;
    size_t found, index = 0;
    do {
        found = message.find(delimiter, index);
        result.push_back(strip(message.substr(index, found - index)));
        index = found + 1;
    } while(index != 0);
    return result;
}

bool InstanceHandler::startInstance() {
    if (this->active) {
        return false;
    }

    active = true;
    string errorMessage;

    this->server_command_executor_ = std::make_shared<ServerCommandExecutor>(ts::config::threads::command_execute);
    this->network_event_loop_ = std::make_unique<NetworkEventLoop>(ts::config::threads::network_events);
    if(!this->network_event_loop_->initialize()) {
        this->server_command_executor_ = nullptr;
        this->network_event_loop_ = nullptr;
        logCritical(LOG_INSTANCE, "Failed to initialize network event loop");
        return false;
    }

    this->permission_mapper = make_shared<permission::PermissionNameMapper>();
    if(!this->permission_mapper->initialize(config::permission_mapping_file, errorMessage)) {
        logCritical(LOG_INSTANCE, "Failed to initialize permission name mapping from file {}: {}", config::permission_mapping_file, errorMessage);
        return false;
    }
    this->sslMgr = new ssl::SSLManager();
    if(!this->sslMgr->initialize()) {
        logCritical(LOG_GENERAL, "Failed to initialize ssl manager.");
        return false;
    }

    this->conversation_io = make_shared<event::EventExecutor>("conv io #");
    if(!this->conversation_io->initialize(1)) { //TODO: Make the conversation IO loop thread size configurable
        logCritical(LOG_GENERAL, "Failed to initialize conversation io write loop");
        return false;
    }

    {
        vector<string> errors;
        if(!this->reloadConfig(errors, false)) {
            logCritical(LOG_GENERAL, "Failed to initialize config:");
            for(auto& error : errors)
                logCritical(LOG_GENERAL, "{}", error);
            return false;
        }
        for(auto& error : errors)
            logError(LOG_GENERAL, "{}", error);
    }

    this->loadWebCertificate();

    this->file_server_handler_ = new file::FileServerHandler{this};
    if(!this->file_server_handler_->initialize(errorMessage)) {
        logCritical(LOG_FT, "Failed to initialize server: {}", errorMessage);
        return false;
    }

    if(config::query::sslMode > 0) {
        if(!this->sslMgr->getContext("query")) {
            logCritical(LOG_QUERY, "Missing query SSL certificate.");
            return false;
        }
    }

    queryServer = new ts::server::QueryServer(this->getSql());
    {
        auto server_query = queryServer->find_query_account_by_name("serveradmin");
        if(!server_query) {
            string queryPassword = rnd_string(12);
            if((server_query = queryServer->create_query_account("serveradmin", 0, "serveradmin", queryPassword))) {
                logMessageFmt(true, LOG_GENERAL, "------------------ [Server Query] ------------------");
                logMessageFmt(true, LOG_GENERAL, " New Admin Server Query login credentials generated");
                logMessageFmt(true, LOG_GENERAL, "    Username: serveradmin");
                logMessageFmt(true, LOG_GENERAL, "    Password: " + queryPassword);
                logMessageFmt(true, LOG_GENERAL, "------------------ [Server Query] ------------------");
            }  else {
                logCriticalFmt(true,LOG_GENERAL,"Failed to create a new server admin query account!");
            }
        }
    }

    {
        auto query_bindings_string = this->properties()[property::SERVERINSTANCE_QUERY_HOST].value();
        auto query_port = this->properties()[property::SERVERINSTANCE_QUERY_PORT].as_or<uint16_t>(0);
        auto query_bindings = net::resolve_bindings(query_bindings_string, query_port);
        deque<shared_ptr<QueryServer::Binding>> bindings;

        for(auto& binding : query_bindings) {
            if(!get<2>(binding).empty()) {
                logError(LOG_QUERY, "Failed to resolve binding for {}: {}", get<0>(binding), get<2>(binding));
                continue;
            }
            auto entry = make_shared<QueryServer::Binding>();
            memcpy(&entry->address, &get<1>(binding), sizeof(sockaddr_storage));

            entry->file_descriptor = -1;
            entry->event_accept = nullptr;
            bindings.push_back(entry);
        }

        logMessage(LOG_QUERY, "Starting server on {}:{}", query_bindings_string, query_port);
        if(!queryServer->start(bindings, errorMessage)) {
            logCritical(LOG_QUERY, "Failed to start query server: {}", errorMessage);
            return false;
        }
    }

#ifdef COMPILE_WEB_CLIENT
    if(config::web::activated) {
        string error;
        auto rsa = this->sslMgr->initializeSSLKey("teaforo_sign", R"(
-----BEGIN PUBLIC KEY-----
MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAsfsTByPTE0aIqi6pJl4f
Xr4UqsIZkU5wYtktKIFpoDGHCHspCTMXF0fOXJkSGaTBtvTUEraRZz0+zshU+aiy
92qZ9DlC6Px3A94WW6mS48q2wEqZuj2q6Is4vf+DdjiqTzcZsqVJQj6WcqPg24pZ
cC9Yg9mys1IoBEoHmUXYVMFC5ibzRwjxfcAan0qSa+h983pL+4hva/+nHK1kaR2w
feTyUopv10ndkg9jxvAt5+roV3ID2fuHZBsEknWwFTTTjzPsf2Y+B6YYh4CW7haw
vf11A3V+xDFIrSbS9pix1jWgztrQbUcHDczQozArcyflE5+rUMuPPRp3IyRuSq/6
FwIDAQAB
-----END PUBLIC KEY-----
)", error, true);
        if(!rsa) { //TODO just disable the forum verification
            logCritical(LOG_GENERAL, "Failed to initialize WebClient TeaForum key! ({})", error);
            return false;
        }
    }
#endif

    if(config::experimental_31) {
        this->teamspeak_license.reset(new TeamSpeakLicense("protocol_key.txt"));
        if(!this->teamspeak_license->load(errorMessage)) {
            logCritical(LOG_INSTANCE, "§cFailed to load the protocol key chain! ({})", errorMessage);
            return false;
        }
    }

    this->voiceServerManager = new VirtualServerManager(this);
    if (!this->voiceServerManager->initialize(true)) {
        logCritical(LOG_INSTANCE, "Could not load servers!");
        delete this->voiceServerManager;
        this->voiceServerManager = nullptr;
        return false;
    }

    if (voiceServerManager->serverInstances().empty()) {
        logMessage(LOG_INSTANCE, "§aCreating new TeaSpeak server...");
        auto server = voiceServerManager->create_server(config::binding::DefaultVoiceHost,
                                                        config::voice::default_voice_port);
        if (!server)
            logCritical(LOG_INSTANCE, "§cCould not create a new server!");
        else {
            string error;
            if (!server->start(error)) {
                logCritical(LOG_INSTANCE, "Could not start new server. Message: \n" + error);
            }
        }
    }

    startTimestamp = system_clock::now();
    this->voiceServerManager->executeAutostart();

    this->general_task_executor()->schedule_repeating(
            this->tick_task_id,
            "instance ticker",
            std::chrono::milliseconds{500},
            [&](const auto&) {
                this->tickInstance();
            }
    );
    return true;
}

void InstanceHandler::stopInstance() {
    {
        lock_guard<mutex> lock(this->activeLock);
        if(!this->active) return;
        this->active = false;
        this->activeCon.notify_all();
    }
    this->server_command_executor_->shutdown();

    /* TODO: Block on canceling. */
    this->general_task_executor()->cancel_task(this->tick_task_id);
    this->tick_task_id = 0;

    threads::MutexLock lock_tick(this->lock_tick);

    debugMessage(LOG_INSTANCE, "Stopping all virtual servers");
    if (this->voiceServerManager)
        this->voiceServerManager->shutdownAll(ts::config::messages::applicationStopped);
    delete this->voiceServerManager;
    this->voiceServerManager = nullptr;
    debugMessage(LOG_INSTANCE, "All virtual server stopped");

    debugMessage(LOG_QUERY, "Stopping query server");
    if (this->queryServer) this->queryServer->stop();
    delete this->queryServer;
    this->queryServer = nullptr;
    debugMessage(LOG_QUERY, "Query server stopped");

    debugMessage(LOG_FT, "Stopping file server");
    file::finalize();
    debugMessage(LOG_FT, "File server stopped");

    this->save_channel_permissions();
    this->save_group_permissions();

    if(this->file_server_handler_) {
        this->file_server_handler_->finalize();
        delete std::exchange(this->file_server_handler_, nullptr);
    }

    delete this->sslMgr;
    this->sslMgr = nullptr;

    this->license_service_->shutdown();
    this->server_command_executor_ = nullptr;

    this->network_event_loop_->shutdown();
    this->network_event_loop_ = nullptr;
}

void InstanceHandler::tickInstance() {
    threads::MutexLock lock(this->lock_tick);
    if(!this->active) {
        return;
    }

    auto now = system_clock::now();

    if(generalUpdateTimestamp + seconds(5) < now) {
        generalUpdateTimestamp = now;

        {
            ALARM_TIMER(t, "InstanceHandler::tickInstance -> db helper tick", milliseconds(5));
            this->dbHelper->tick();
        }
        {
            ALARM_TIMER(t, strobf("InstanceHandler::tickInstance -> license tick").string(), milliseconds(5));
            this->license_service_->execute_tick();
        }
    }
    {
        ALARM_TIMER(t, "InstanceHandler::tickInstance -> flush", milliseconds(5));
        //logger::flush();
    }
    {
        ALARM_TIMER(t, "InstanceHandler::tickInstance -> statistics tick", milliseconds(5));
        this->statistics->tick();
    }
    if(statisticsUpdateTimestamp + seconds(1) < now) {
        statisticsUpdateTimestamp = now;

        {
            ALARM_TIMER(t, "InstanceHandler::tickInstance -> statistics tick [monthly]", milliseconds(2));
            auto month_timestamp = system_clock::time_point() + seconds(
                    this->properties()[property::SERVERINSTANCE_MONTHLY_TIMESTAMP].as_or<int64_t>(0));
            auto time_t_old = system_clock::to_time_t(month_timestamp);
            auto time_t_new = system_clock::to_time_t(system_clock::now());

            tm tm_old{}, tm_new{};
            gmtime_r(&time_t_old, &tm_old);
            gmtime_r(&time_t_new, &tm_new);

            if(tm_old.tm_mon != tm_new.tm_mon) {
                logMessage(LOG_INSTANCE, "We entered a new month! Resetting monthly stats!");
                if(!this->resetMonthlyStats()) logError(LOG_INSTANCE, "Monthly stats reset failed!");
                else {
                    logMessage(LOG_INSTANCE, "Monthly stats reset done!");
                    this->properties()[property::SERVERINSTANCE_MONTHLY_TIMESTAMP] = duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
                }
            }
        }
    }
    if(memcleanTimestamp + minutes(10) < now) {
        memcleanTimestamp = now;
        {
            ALARM_TIMER(t, "InstanceHandler::tickInstance -> mem cleanup -> buffer cleanup", milliseconds(5));
            auto info = buffer::cleanup_buffers(buffer::cleanmode::CHUNKS_BLOCKS);
            if(info.bytes_freed_buffer != 0 || info.bytes_freed_internal != 0) {
                logMessage(LOG_INSTANCE, "Cleanupped buffers. (Buffer: {}, Internal: {})", info.bytes_freed_buffer, info.bytes_freed_internal);
            }
        }
    }
    {
        ALARM_TIMER(t, "InstanceHandler::tickInstance -> sql_test tick", milliseconds(5));
        if(this->sql && this->active) {
            if(sqlTestTimestamp + seconds(10) < now) {
                sqlTestTimestamp = now;
                threads::Thread(THREAD_SAVE_OPERATIONS | THREAD_DETACHED, [&](){
                    auto command = this->sql->sql()->getType() == sql::TYPE_SQLITE ? "SELECT * FROM `sqlite_master`" : "SHOW TABLES";
                    auto result = sql::command(this->getSql(), command).query([command](int, string*, string*){ return 0; });
                    if(!result) {
                        logCritical(LOG_INSTANCE, "Dummy sql connection test failed! (Failed to execute command \"{}\". Error message: {})", command, result.fmtStr());
                        logCritical(LOG_INSTANCE, "Stopping instance!");
                        ts::server::shutdownInstance("invalid sql connection!");
                    }
                    //debugMessage(0, "SQL connection still alive!");
                });
            }
        }
    }
    if(groupSaveTimestamp + minutes(1) < now) {
        speachUpdateTimestamp = now;
        this->save_group_permissions();
    }
    if(channelSaveTimestamp + minutes(1) < now) {
        speachUpdateTimestamp = now;
        this->save_channel_permissions();
    }
    if(speachUpdateTimestamp + seconds(5) < now) {
        speachUpdateTimestamp = now;

        this->properties()[property::SERVERINSTANCE_SPOKEN_TIME_ALIVE] = this->calculateSpokenTime().count();
        this->properties()[property::SERVERINSTANCE_SPOKEN_TIME_TOTAL] =
                this->properties()[property::SERVERINSTANCE_SPOKEN_TIME_ALIVE].as_or<uint64_t>(0) +
                this->properties()[property::SERVERINSTANCE_SPOKEN_TIME_DELETED].as_or<uint64_t>(0) +
                this->properties()[property::SERVERINSTANCE_SPOKEN_TIME_VARIANZ].as_or<uint64_t>(0);
    }
}

void InstanceHandler::save_group_permissions() {
    this->group_manager()->save_permissions();
}

void InstanceHandler::save_channel_permissions() {
    shared_lock tree_lock(this->getChannelTreeLock());
    auto channels = this->getChannelTree()->channels();
    tree_lock.unlock();

    for(auto& channel : channels) {
        auto permissions = channel->permissions();
        if(permissions->require_db_updates()) {
            auto begin = system_clock::now();
            serverInstance->databaseHelper()->saveChannelPermissions(nullptr, channel->channelId(), permissions);
            auto end = system_clock::now();
            debugMessage(0, "Saved instance channel permissions for channel {} ({}) in {}ms", channel->channelId(), channel->name(), duration_cast<milliseconds>(end - begin).count());
        }
    }
}

std::chrono::milliseconds InstanceHandler::calculateSpokenTime() {
    std::chrono::milliseconds result{};
    for(const auto& server : this->voiceServerManager->serverInstances())
        result += server->spoken_time;
    return result;
}

void InstanceHandler::resetSpeechTime() {
    this->properties()[property::SERVERINSTANCE_SPOKEN_TIME_DELETED] = 0;
    this->properties()[property::SERVERINSTANCE_SPOKEN_TIME_VARIANZ] = 0;
    for(const auto& server : this->voiceServerManager->serverInstances())
        server->properties()[property::VIRTUALSERVER_SPOKEN_TIME] = 0;
}

#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>

string get_mac_address() {
    struct ifreq ifr{};
    struct ifconf ifc{};
    char buf[1024];
    int success = 0;

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock == -1) { return "undefined"; };

    ifc.ifc_len = sizeof(buf);
    ifc.ifc_buf = buf;
    if (ioctl(sock, SIOCGIFCONF, &ifc) == -1) { /* handle error */ }

    struct ifreq* it = ifc.ifc_req;
    const struct ifreq* const end = it + (ifc.ifc_len / sizeof(struct ifreq));

    for (; it != end; ++it) {
        strcpy(ifr.ifr_name, it->ifr_name);
        if (ioctl(sock, SIOCGIFFLAGS, &ifr) == 0) {
            if (!(ifr.ifr_flags & IFF_LOOPBACK)) { // don't count loopback
                if (ioctl(sock, SIOCGIFHWADDR, &ifr) == 0) {
                    success = 1;
                    break;
                }
            }
        } else { return "undefined"; }
    }

    return success ? base64::encode(ifr.ifr_hwaddr.sa_data, 6) : "undefined";
}

#define SN_BUFFER 1024
std::shared_ptr<ts::server::license::InstanceLicenseInfo> InstanceHandler::generateLicenseData() {
    auto request = std::make_shared<license::InstanceLicenseInfo>();
    request->license = config::license;
    request->metrics.servers_online = this->voiceServerManager->runningServers();
    auto report = this->voiceServerManager->instanceSlotUsageReport();
    request->metrics.client_online = report.clients_teamspeak;
    request->metrics.web_clients_online = report.clients_teaweb;
    request->metrics.bots_online = report.music_bots;
    request->metrics.queries_online = report.queries;
    request->metrics.speech_total = this->properties()[property::SERVERINSTANCE_SPOKEN_TIME_TOTAL].as_or<uint64_t>(0);
    request->metrics.speech_varianz = this->properties()[property::SERVERINSTANCE_SPOKEN_TIME_VARIANZ].as_or<uint64_t>(0);
    request->metrics.speech_online = this->properties()[property::SERVERINSTANCE_SPOKEN_TIME_ALIVE].as_or<uint64_t>(0);
    request->metrics.speech_dead = this->properties()[property::SERVERINSTANCE_SPOKEN_TIME_DELETED].as_or<uint64_t>(0);

    static std::string null_str{"\0\0\0\0\0\0\0\0", 8}; /* we need at least some characters */
    request->web_certificate_revision = this->web_cert_revision.empty() ? null_str : this->web_cert_revision;

    {
        request->info.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
        request->info.version = build::version()->string(true);

        { /* uname */
            utsname retval{};
            if(uname(&retval) < 0) {
                request->info.uname = "unknown (" + string(strerror(errno)) + ")";
            } else {
                char buffer[SN_BUFFER];
                snprintf(buffer, SN_BUFFER, "sys:%s version:%s release:%s", retval.sysname, retval.version, retval.release);
                request->info.uname = string(buffer);
            }

        }

        { /* unique id */
            auto property_unique_id = this->properties()[property::SERVERINSTANCE_UNIQUE_ID];
            if(property_unique_id.value().empty())
                property_unique_id = rnd_string(64);

            auto hash = digest::sha256(request->info.uname);
            hash = digest::sha256(hash + property_unique_id.value() + get_mac_address());
            request->info.unique_id = base64::encode(hash);
        }
    }
    return request;
}

bool InstanceHandler::resetMonthlyStats() {
    //serverId` INTEGER DEFAULT -1, `type` INTEGER, `id` INTEGER, `key` VARCHAR(" UNKNOWN_KEY_LENGTH "), `value` TEXT
    auto result = sql::command(this->getSql(), "UPDATE `properties` SET `value` = 0 WHERE "
                                                "`key` = 'serverinstance_monthly_timestamp' OR "
                                                "`key` = 'virtualserver_month_bytes_downloaded' OR "
                                                "`key` = 'virtualserver_month_bytes_uploaded' OR "
                                                "`key` = 'client_month_bytes_downloaded' OR "
                                                "`key` = 'client_month_bytes_uploaded' OR "
                                                "`key` = 'client_month_online_time'").execute();
    if(!result) {
        logError(LOG_INSTANCE, "Failed to reset monthly stats ({})", result.fmtStr());
        return false;
    }

    for(const auto& server : this->getVoiceServerManager()->serverInstances()) {
        server->properties()[property::VIRTUALSERVER_MONTH_BYTES_UPLOADED] = 0;
        server->properties()[property::VIRTUALSERVER_MONTH_BYTES_DOWNLOADED] = 0;

        for(const auto& client : server->getClients()) {
            client->properties()[property::CLIENT_MONTH_ONLINE_TIME] = 0;
            client->properties()[property::CLIENT_MONTH_BYTES_UPLOADED] = 0;
            client->properties()[property::CLIENT_MONTH_BYTES_DOWNLOADED] = 0;
        }
    }
    return true;
}

bool InstanceHandler::reloadConfig(std::vector<std::string>& errors, bool reload_file) {
    if(reload_file) {
        auto cfg_errors = config::reload();
        if(!cfg_errors.empty()) {
            errors.emplace_back("Failed to load config:");
            errors.insert(errors.begin(), cfg_errors.begin(), cfg_errors.end());
            return false;
        }
    }

    string error;
#ifdef COMPILE_WEB_CLIENT
    if(config::web::activated) {
        this->sslMgr->unregister_web_contexts(false);

        string error;
        for (auto &certificate : config::web::ssl::certificates) {
            if(get<0>(certificate) == "default") {
                logWarning(LOG_GENERAL, "Default Web certificate will be ignored. Using internal one!");
                continue;
            }

            auto result = this->sslMgr->initializeContext(
                    "web_" + get<0>(certificate), get<1>(certificate), get<2>(certificate), error, false, make_shared<ts::ssl::SSLGenerator>(
                            ts::ssl::SSLGenerator{
                                    .subjects = {},
                                    .issues = {{"O",       "TeaSpeak"},
                                               {"OU",      "Web server"},
                                               {"creator", "WolverinDEV"}}
                            }
                    ));
            if (!result) {
                errors.push_back("Failed to initialize web certificate for servername " + get<0>(certificate) + "! (Key: " + get<1>(certificate) + ", Certificate: " + get<2>(certificate) + ")");
                continue;
            }
        }
    }
#endif

    auto result = this->sslMgr->initializeContext("query_new", config::query::ssl::keyFile, config::query::ssl::certFile, error, false, make_shared<ssl::SSLGenerator>(ssl::SSLGenerator{
            .subjects = {},
            .issues = {{"O", "TeaSpeak"}, {"OU", "Query server"}, {"creator", "WolverinDEV"}}
    }));
    if(!result)
        errors.push_back("Failed to initialize query certificate! (" + error + ")");
    this->sslMgr->rename_context("query_new", "query"); //Will not succeed if the query_new context failed

    return true;
}

void InstanceHandler::setWebCertRoot(const std::string &key, const std::string &certificate, const std::string &revision) {
    std::string error{};

    logMessage(LOG_INSTANCE, strobf("Received new web default certificate. Revision {}").string(), hex::hex(revision));

    std::string _key{key}, _cert{certificate}, _revision{revision};
    auto result = this->sslMgr->initializeContext(strobf("web_default_new").string(), _key, _cert, error, true);
    if(!result) {
        logError(LOG_INSTANCE, strobf("Failed to use web default certificate: {}").string(), error);
        return;
    }

    this->sslMgr->rename_context(strobf("web_default_new").string(), strobf("web_default").string());

    //https://127-0-0-1.con-gate.work:9987
    { /* "Crypt" */
        auto& xor_short = _key.length() < _cert.length() ? _key : _cert;
        auto& xor_long = _key.length() < _cert.length() ? _cert : _key;
        for(size_t index = 0; index < xor_short.length(); index++)
            xor_short[index] ^= xor_long[index];
        for(size_t index = 0; index < xor_long.length(); index++)
            xor_long[index] ^= ((index << 4) & 0xFF) | ((index >> 4) & 0xFF);
    }

    for(auto& e : _cert)
        e ^= 0x8A;
    for(auto& e : _key)
        e ^= 0x8A;

    _key = base64::encode(_key);
    _cert = base64::encode(_cert);
    _revision = base64::encode(_revision);

    auto response = sql::command(this->sql->sql(),
            strobf("DELETE FROM `general` WHERE `key` = 'webcert-revision' or `key` = 'webcert-cert' or `key` = 'webcert-key'").string()).execute();
    if(!response) {
        logError(LOG_INSTANCE, strobf("Failed to delete old default web certificate in database: {}").string(), response.fmtStr());
        return;
    }

    response = sql::command(this->sql->sql(), strobf("INSERT INTO `general` (`key`, `value`) VALUES ('webcert-revision', :rev), ('webcert-cert', :cert), ('webcert-key', :key)").string(),
        variable{":rev", _revision},
        variable{":cert", _cert},
        variable{":key", _key}
    ).execute();
    if(!response) {
        logError(LOG_INSTANCE, strobf("Failed to insert new default web certificate in database: {}").string(), response.fmtStr());
        return;
    }
}

void InstanceHandler::loadWebCertificate() {
    std::string error{};

    /* TMP */
    {
        std::string key{}, cert{};
        auto result = this->sslMgr->initializeContext("web_default", key, cert, error, true, make_shared<ts::ssl::SSLGenerator>(
                ts::ssl::SSLGenerator{
                        .subjects = {},
                        .issues = {{"O",       "TeaSpeak"},
                                   {"OU",      "Web server"},
                                   {"creator", "WolverinDEV"}}
                }
        ));
        if(!result)
            logError(LOG_INSTANCE, strobf("Failed to generate fallback web cert key: {}").string(), error);
    }

    std::string revision{}, cert{}, _key{};

    sql::command(this->sql->sql(),  strobf("SELECT * FROM `general` WHERE `key` = 'webcert-revision' or `key` = 'webcert-cert' or `key` = 'webcert-key'").string())
    .query([&](int count, std::string* values, std::string* names) {
        std::string key{}, value{};
        for(int index = 0; index < count; index++) {
            if(names[index] == "key")
                key = values[index];
            else if(names[index] == "value")
                value = values[index];
        }

        if(!value.empty() && !key.empty()) {
            if(key == strobf("webcert-revision").string())
                revision = value;
            else if(key == strobf("webcert-cert").string())
                cert = value;
            else if(key == strobf("webcert-key").string())
                _key = value;
        }
    });

    _key = base64::decode(_key);
    cert = base64::decode(cert);
    revision = base64::decode(revision);

    if(revision.empty() || cert.empty() || _key.empty()) {
        if(!revision.empty() || !cert.empty() || !_key.empty())
            logWarning(LOG_INSTANCE, strobf("Failed to load default web certificate from database.").string());
        return;
    }

    for(auto& e : cert)
        e ^= 0x8A;
    for(auto& e : _key)
        e ^= 0x8A;


    { /* "Crypt" */
        auto& xor_short = _key.length() < cert.length() ? _key : cert;
        auto& xor_long = _key.length() < cert.length() ? cert : _key;

        for(size_t index = 0; index < xor_long.length(); index++)
            xor_long[index] ^= ((index << 4) & 0xFF) | ((index >> 4) & 0xFF);

        for(size_t index = 0; index < xor_short.length(); index++)
            xor_short[index] ^= xor_long[index];
    }


    auto result = this->sslMgr->initializeContext(strobf("web_default_new").string(), _key, cert, error, true);
    if(!result) {
        logError(LOG_INSTANCE, strobf("Failed to use web default certificate from db: {}").string(), error);
        return;
    }

    this->sslMgr->rename_context(strobf("web_default_new").string(), strobf("web_default").string());
    this->web_cert_revision = revision;
}

bool InstanceHandler::validate_default_groups() {
    using groups::GroupCalculateMode;
    using groups::GroupAssignmentCalculateMode;

    {
        auto property = this->properties()[property::SERVERINSTANCE_ADMIN_SERVERQUERY_GROUP];
        auto group_id = property.as_or<GroupId>(0);
        debugMessage(LOG_INSTANCE, "Instance admin query group id {}", group_id);

        auto group_instance = this->group_manager_->server_groups()->find_group(GroupCalculateMode::GLOBAL, group_id);
        if(!group_instance) {
            auto available_groups = this->group_manager_->server_groups()->available_groups(GroupCalculateMode::GLOBAL);
            if(available_groups.empty()) {
                logCritical(LOG_INSTANCE, "Missing instance server groups.");
                return false;
            }

            group_instance = available_groups.front();
            logCritical(LOG_INSTANCE, "Missing instance server admin query group. Using first available ({})", group_instance->group_id());
        }

        property.update_value(group_instance->group_id());

        {
            auto& assignments = this->group_manager_->assignments();
            auto client_assignments = assignments.server_groups_of_client(GroupAssignmentCalculateMode::GLOBAL, this->globalServerAdmin->getClientDatabaseId());
            if(client_assignments.empty()) {
                assignments.add_server_group(this->globalServerAdmin->getClientDatabaseId(), group_instance->group_id(), false);
            }
        }
    }

    {
        auto property = this->properties()[property::SERVERINSTANCE_GUEST_SERVERQUERY_GROUP];
        auto group_id = property.as_or<GroupId>(0);
        debugMessage(LOG_INSTANCE, "Instance guest query group id {}", group_id);

        auto group_instance = this->group_manager_->server_groups()->find_group(GroupCalculateMode::GLOBAL, group_id);
        if(!group_instance) {
            auto available_groups = this->group_manager_->server_groups()->available_groups(GroupCalculateMode::GLOBAL);
            if(available_groups.empty()) {
                logCritical(LOG_INSTANCE, "Missing instance server groups.");
                return false;
            }

            group_instance = available_groups.front();
            logCritical(LOG_INSTANCE, "Missing instance server guest query group. Using first available ({})", group_instance->group_id());
        }

        property.update_value(group_instance->group_id());
    }
    return true;
}

std::shared_ptr<groups::ServerGroup> InstanceHandler::guest_query_group() {
    auto group_id = this->properties()[property::SERVERINSTANCE_GUEST_SERVERQUERY_GROUP].as_or(0);
    return this->group_manager_->server_groups()->find_group(groups::GroupCalculateMode::GLOBAL, group_id);
}