#include <algorithm>
#include <log/LogUtils.h>
#include "VirtualServerManager.h"
#include "src/server/VoiceServer.h"
#include "src/client/query/QueryClient.h"
#include "InstanceHandler.h"
#include "src/client/ConnectedClient.h"
#include <ThreadPool/ThreadHelper.h>
#include <files/FileServer.h>
#include <set>

using namespace std;
using namespace std::chrono;
using namespace ts::server;

VirtualServerManager::VirtualServerManager(InstanceHandler* handle) : handle(handle) {
    this->puzzles = new udp::PuzzleManager{};
}

VirtualServerManager::~VirtualServerManager() {
    this->state = State::STOPPED;
    {
        threads::MutexLock lock(this->instanceLock);
        this->instances.clear();
    }

    {
        this->acknowledge.condition.notify_all();
        if(!threads::timed_join(this->acknowledge.executor,std::chrono::seconds{2})) {
            logCritical(LOG_GENERAL, "Failed to shutdown packet resend thread.");
            this->acknowledge.executor.detach();
        }
    }

    delete this->puzzles;
    this->puzzles = nullptr;
}

bool VirtualServerManager::initialize(bool autostart) {
    this->state = State::STARTING;
    logMessage(LOG_INSTANCE, "Generating server puzzles...");
    auto start = system_clock::now();
    if(!this->puzzles->precompute_puzzles(config::voice::DefaultPuzzlePrecomputeSize))
        logCritical(LOG_INSTANCE, "Failed to precompute RSA puzzles");
    logMessage(LOG_INSTANCE, "Puzzles generated! Time required: " + to_string(duration_cast<milliseconds>(system_clock::now() - start).count()) + "ms");

    size_t serverCount = 0;
    sql::command(this->handle->getSql(), "SELECT COUNT(`serverId`) FROM `servers`").query([](size_t& ptr, int, char** v, char**) { ptr = stoll(v[0]); return 0; }, serverCount);

    {
        logMessage(LOG_INSTANCE, "Loading startup cache (This may take a while)");
        auto beg = system_clock::now();
        this->handle->databaseHelper()->loadStartupCache();
        auto end = system_clock::now();
        logMessage(LOG_INSTANCE, "Required {}ms to preload the startup cache. Cache needs {}mb",
            duration_cast<milliseconds>(end - beg).count(),
            this->handle->databaseHelper()->cacheBinarySize() / 1024 / 1024
        );
    }

    auto beg = system_clock::now();
    size_t server_count = 0;
    sql::command(this->handle->getSql(), "SELECT `serverId`, `host`, `port` FROM `servers`").query([&](VirtualServerManager* mgr, int length, std::string* values, std::string* columns){
        ServerId id = 0;
        std::string host;
        uint16_t port = 0;

        for(int index = 0; index < length; index++) {
            try {
                if(columns[index] == "serverId")
                    id = static_cast<ServerId>(stoll(values[index]));
                else if(columns[index] == "host")
                    host = values[index];
                else if(columns[index] == "port")
                    port = static_cast<uint16_t>(stoul(values[index]));
            } catch(std::exception& ex) {
                logError(LOG_INSTANCE, "Failed to parse virtual server from database. Failed to parse field {} with value {}: {}", columns[index], values[index], ex.what());
                return 0;
            }
        }


        if(id == 0) {
            logError(LOG_INSTANCE, "Failed to load virtual server from database. Server id is zero!");
            return 0;
        } else if(id == 0xFFFF) {
            /* snapshot server */
            return 0;
        }

        if(host.empty()) {
            logWarning(id, "The loaded host is empty. Using default one (from the config.yml)");
            host = config::binding::DefaultVoiceHost;
        }

        if(port == 0) {
            logError(LOG_INSTANCE, "Failed to load virtual server from database. Server port is zero!");
            return 0;
        }


        auto server = make_shared<VirtualServer>(id, this->handle->getSql());
        server->self = server;
        if(!server->initialize(true)) {
            //FIXME error handling
        }
        server->properties()[property::VIRTUALSERVER_HOST] = host;
        server->properties()[property::VIRTUALSERVER_PORT] = port;

        {
            threads::MutexLock l(this->instanceLock);
            this->instances.push_back(server);
        }

        if(autostart && server->properties()[property::VIRTUALSERVER_AUTOSTART].as_or<bool>(false)) {
            logMessage(server->getServerId(), "Starting server");
            string msg;
            try {
                if(!server->start(msg))
                    logError(server->getServerId(), "Failed to start server.\n   Message: " + msg);
            } catch (const std::exception& ex) {
                logError(server->getServerId(), "Could not start server! Got an active exception. Message {}", ex.what());
            }
        }
        if(id > 0)
            this->handle->databaseHelper()->clearStartupCache(id);

        server_count++;
        return 0;
    }, this);
    auto time = duration_cast<milliseconds>(system_clock::now() - beg).count();
    logMessage(LOG_INSTANCE, "Loaded {} servers within {}ms. Server/sec: {:2f}",
             server_count,
             time,
             (float) server_count / (time / 1024 == 0 ? 1 : time / 1024)
    );
    this->handle->databaseHelper()->clearStartupCache(0);

    {
        this->acknowledge.executor = std::thread([&]{
            system_clock::time_point next_execute = system_clock::now() + milliseconds(500);
            while(this->state == State::STARTED || this->state == State::STARTING) {
                unique_lock<mutex> lock(this->acknowledge.lock);
                this->acknowledge.condition.wait_until(lock, next_execute, [&](){ return this->state != State::STARTED && this->state != State::STARTING; });

                auto now = system_clock::now();
                next_execute = now + milliseconds(500);
                for(const auto& server : this->serverInstances()) {
                    auto vserver = server->getVoiceServer(); //Read this only once
                    if(vserver)
                        vserver->execute_resend(now, next_execute);
                }
            }
            return 0;
        });
    }

    this->state = State::STARTED;
    return true;
}

shared_ptr<VirtualServer> VirtualServerManager::findServerById(ServerId sid) {
    for(auto server : this->serverInstances())
        if(server->getServerId() == sid)
            return server;
    return nullptr;
}

shared_ptr<VirtualServer> VirtualServerManager::findServerByPort(uint16_t port) {
    for(const auto& server : this->serverInstances()){
        if(server->properties()[property::VIRTUALSERVER_PORT] == port) {
            return server;
        }
    }
    return nullptr;
}

uint16_t VirtualServerManager::next_available_port(const std::string& host_string) {
    auto instances_ = this->serverInstances();
    std::set<uint16_t> unallowed_ports{};

    for(const auto& instance : instances_) {
        unallowed_ports.insert(instance->properties()[property::VIRTUALSERVER_PORT].as_or<uint16_t>(0));

        auto vserver = instance->getVoiceServer();
        if(instance->running() && vserver) {
            for(const auto& socket : vserver->getSockets()) {
                unallowed_ports.insert(net::port(socket->address()));
            }
        }
    }
    auto bindings = net::resolve_bindings(host_string, 0);

    uint16_t port = config::voice::default_voice_port;
    while(true) {
        if(port < 1024) goto next_port;

        for(auto& p : unallowed_ports) {
            if(p == port) {
                goto next_port;
            }
        }

        for(auto& binding : bindings) {
            if(!std::get<2>(binding).empty()) continue; /* error on that */
            auto& baddress = std::get<1>(binding);
            auto& raw_port = baddress.ss_family == AF_INET ? ((sockaddr_in*) &baddress)->sin_port : ((sockaddr_in6*) &baddress)->sin6_port;
            raw_port = htons(port);

            switch (net::address_available(baddress, net::binding_type::TCP)) {
                case net::binding_result::ADDRESS_USED:
                    goto next_port;

                case net::binding_result::ADDRESS_FREE:
                case net::binding_result::INTERNAL_ERROR:
                default:
                    break; /* if we've an internal error we ignore it */
            }
            switch (net::address_available(baddress, net::binding_type::UDP)) {
                case net::binding_result::ADDRESS_USED:
                    goto next_port;

                case net::binding_result::ADDRESS_FREE:
                case net::binding_result::INTERNAL_ERROR:
                default:
                    break; /* if we've an internal error we ignore it */
            }
        }
        break;

        next_port:
        port++;
    }
    return port;
}

ts::ServerId VirtualServerManager::next_available_server_id(bool& success) {
    auto server_id_base = this->handle->properties()[property::SERVERINSTANCE_VIRTUAL_SERVER_ID_INDEX].as_or<ServerId>(0);
    /* ensure we're not using 0xFFFF (This is the snapshot server) */
    if(server_id_base > 65530) {
        success = false;
        return 0;
    }
    ServerId serverId = server_id_base != 0 ? server_id_base : (ServerId) 1;

    auto instances = this->serverInstances();
    vector<ServerId> used_ids;
    used_ids.reserve(instances.size());

    for(const auto& server : instances)
        used_ids.push_back(server->getServerId());

    std::stable_sort(used_ids.begin(), used_ids.end(), [](ServerId a, ServerId b) { return b > a; });
    while(true) {
        auto it = used_ids.begin();
        while(it != used_ids.end() && *it < serverId)
            it++;

        if(it == used_ids.end() || *it != serverId) {
            break;
        } else {
            used_ids.erase(used_ids.begin(), it + 1);
            serverId++;
        }
    }

    /* increase counter */
    if(server_id_base != 0)
        this->handle->properties()[property::SERVERINSTANCE_VIRTUAL_SERVER_ID_INDEX] = serverId;

    success = true;
    return serverId;
}

ServerSlotUsageReport VirtualServerManager::instanceSlotUsageReport() {
    ServerSlotUsageReport result{};
    for(const auto& server : this->serverInstances()) {
        if(!server->running()) {
            continue;
        }

        result += server->onlineStats();
    }
    return result;
}

size_t VirtualServerManager::runningServers() {
    size_t res = 0;
    for(const auto& sr : this->serverInstances())
        if(sr->running()) res++;
    return res;
}

shared_ptr<VirtualServer> VirtualServerManager::create_server(std::string hosts, uint16_t port) {
    bool sid_success = false;

    ServerId serverId = this->next_available_server_id(sid_success);
    if(!sid_success)
        return nullptr;

    this->delete_server_in_db(serverId, false); /* just to ensure */
    sql::command(this->handle->getSql(), "INSERT INTO `servers` (`serverId`, `host`, `port`) VALUES (:sid, :host, :port)", variable{":sid", serverId}, variable{":host", hosts}, variable{":port", port}).executeLater().waitAndGetLater(LOG_SQL_CMD, {1, "future failed"});

    auto prop_copy = sql::command(this->handle->getSql(), "INSERT INTO `properties` (`serverId`, `type`, `id`, `key`, `value`) SELECT :target_sid AS `serverId`, `type`, `id`, `key`, `value` FROM `properties` WHERE `type` = :type AND `id` = 0 AND `serverId` = 0;",
                 variable{":target_sid", serverId},
                 variable{":type", property::PROP_TYPE_SERVER}).execute();
    if(!prop_copy)
        logCritical(LOG_GENERAL, "Failed to copy default server properties: {}", prop_copy.fmtStr());

    auto server = make_shared<VirtualServer>(serverId, this->handle->getSql());
    server->self = server;
    if(!server->initialize(true)) {
        //FIXME error handling
    }
    server->properties()[property::VIRTUALSERVER_HOST] = hosts;
    server->properties()[property::VIRTUALSERVER_PORT] = port;
    if(config::server::default_music_bot) {
        auto bot = server->music_manager_->createBot(0);
        if(!bot) {
            logCritical(server->getServerId(), "Failed to create default music bot!");
        }
    }
    {
        threads::MutexLock l(this->instanceLock);
        this->instances.push_back(server);
    }
    return server;
}

bool VirtualServerManager::deleteServer(shared_ptr<VirtualServer> server) {
    {
        threads::MutexLock l(this->instanceLock);
        bool found = false;
        for(const auto& s : this->instances)
            if(s == server) {
                found = true;
                break;
            }
        if(!found) return false;
        this->instances.erase(std::remove_if(this->instances.begin(), this->instances.end(), [&](const shared_ptr<VirtualServer>& s) {
            return s == server;
        }), this->instances.end());
    }

    if(server->getState() != ServerState::OFFLINE)
        server->stop("server deleted", true);
    for(const auto& cl : server->getClients()) { //start disconnecting
        if(cl->getType() == CLIENT_TEAMSPEAK || cl->getType() == CLIENT_TEASPEAK || cl->getType() == CLIENT_WEB) {
            cl->close_connection(chrono::system_clock::now());
        } else if(cl->getType() == CLIENT_QUERY){
            auto qc = dynamic_pointer_cast<QueryClient>(cl);
            qc->disconnect_from_virtual_server("server delete");
        } else if(cl->getType() == CLIENT_MUSIC) {
            cl->disconnect("");
            cl->currentChannel = nullptr;
        } else if(cl->getType() == CLIENT_INTERNAL) {

        } else {
        }
    }
    {
        for(const shared_ptr<ConnectedClient>& client : server->getClients()) {
            if(client && client->getType() == ClientType::CLIENT_QUERY) {
                lock_guard lock(client->command_lock);
                client->server = nullptr;

                client->loadDataForCurrentServer();
            }
        }
    }
    {
        std::unique_lock state_lock{server->state_mutex};
        server->state = ServerState::DELETING;
    }

    this->handle->properties()[property::SERVERINSTANCE_SPOKEN_TIME_DELETED].increment_by(server->properties()[property::VIRTUALSERVER_SPOKEN_TIME].as_or<uint64_t>(0));
    this->delete_server_in_db(server->serverId, false);
    this->handle->databaseHelper()->handleServerDelete(server->serverId);

    file::server()->unregister_server(server->getServerId(), true);
    return true;
}

void VirtualServerManager::executeAutostart() {
    threads::MutexLock l(this->instanceLock);
    auto lastStart = system_clock::time_point();
    for(const auto& server : this->instances){
        if(!server->running() && server->properties()[property::VIRTUALSERVER_AUTOSTART].as_or<bool>(false)){
            threads::self::sleep_until(lastStart + milliseconds(10)); //Don't start all server at the same point (otherwise all servers would tick at the same moment)
            lastStart = system_clock::now();
            logMessage(server->getServerId(), "Starting server");
            string msg;
            try {
                if(!server->start(msg))
                    logError(server->getServerId(), "Failed to start server.\n   Message:{}", msg);
            } catch (const std::exception& ex) {
                logError(server->getServerId(), "Could not start server! Got an active exception. Message {}", ex.what());
            }
        }
    }
}

void VirtualServerManager::shutdownAll(const std::string& msg) {
    for(const auto &server : this->serverInstances())
        server->preStop(msg);
    for(const auto &server : this->serverInstances()){
        if(server->running()) server->stop(msg, true);
    }
}

void VirtualServerManager::delete_server_in_db(ts::ServerId server_id, bool data_only) {
#define execute_delete(statement) \
result = sql::command(this->handle->getSql(), statement, variable{":sid", server_id}).execute(); \
if(!result) { \
    logWarning(LOG_INSTANCE, "Failed to execute SQL command {}: {}", statement, result.fmtStr()); \
    result = sql::result{}; \
}

    sql::result result{};

    if(!data_only) {
        execute_delete("DELETE FROM `servers` WHERE `serverId` = :sid");
    }

    execute_delete("DELETE FROM `tokens` WHERE `serverId` = :sid");
    execute_delete("DELETE FROM `properties` WHERE `serverId` = :sid");
    execute_delete("DELETE FROM `permissions` WHERE `serverId` = :sid");
    execute_delete("DELETE FROM `channels` WHERE `serverId` = :sid");
    execute_delete("DELETE FROM `bannedClients` WHERE `serverId` = :sid");
    execute_delete("DELETE FROM `groups` WHERE `serverId` = :sid");
    execute_delete("DELETE FROM `assignedGroups` WHERE `serverId` = :sid");
    execute_delete("DELETE FROM `complains` WHERE `serverId` = :sid");
    execute_delete("DELETE FROM `letters` WHERE `serverId` = :sid");

    execute_delete("DELETE FROM `musicbots` WHERE `serverId` = :sid");

    execute_delete("DELETE FROM `playlists` WHERE `serverId` = :sid");
    execute_delete("DELETE FROM `playlist_songs` WHERE `serverId` = :sid");

    execute_delete("DELETE FROM `conversations` WHERE `server_id` = :sid");
    execute_delete("DELETE FROM `conversation_blocks` WHERE `server_id` = :sid");
    execute_delete("DELETE FROM `ban_trigger` WHERE `server_id` = :sid");
    execute_delete("DELETE FROM `clients_server` WHERE `server_id` = :sid");
}

#define execute_change(table, column) \
result = sql::command(this->handle->getSql(), "UPDATE `" table "` SET `" column "` = :nsid WHERE `" column "` = :osid;", \
                        variable{":osid", old_id}, variable{":nsid", new_id}).execute(); \
if(!result) { \
    logWarning(LOG_INSTANCE, "Failed to execute server id change on table {} (column {}): {}", table, column, result.fmtStr()); \
    result = sql::result{}; \
}

void VirtualServerManager::change_server_id_in_db(ts::ServerId old_id, ts::ServerId new_id) {
    sql::result result{};


    execute_change("tokens", "serverId");
    execute_change("properties", "serverId");
    execute_change("permissions", "serverId");
    execute_change("channels", "serverId");
    execute_change("bannedClients", "serverId");
    execute_change("groups", "serverId");
    execute_change("assignedGroups", "serverId");
    execute_change("complains", "serverId");
    execute_change("letters", "serverId");
    execute_change("musicbots", "serverId");
    execute_change("playlists", "serverId");
    execute_change("playlist_songs", "serverId");

    execute_change("conversations", "server_id");
    execute_change("conversation_blocks", "server_id");
    execute_change("ban_trigger", "server_id");
    execute_change("clients_server", "server_id");

    execute_change("servers", "serverId");
}