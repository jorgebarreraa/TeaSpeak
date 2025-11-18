#include "./CommandHandler.h"

#include <csignal>
#include <log/LogUtils.h>
#include <misc/time.h>
#include <misc/memtracker.h>
#include <sql/sqlite/SqliteSQL.h>
#include <sys/resource.h>
#include <protocol/buffers.h>

#include "../SignalHandler.h"
#include "../client/ConnectedClient.h"
#include "../InstanceHandler.h"
#include "../ShutdownHelper.h"
#include "../server/QueryServer.h"
#include "../groups/GroupManager.h"

#ifdef HAVE_JEMALLOC
    #include <jemalloc/jemalloc.h>
#endif

using namespace std;
using namespace std::chrono;
using namespace ts;
using namespace ts::server;

extern ts::server::InstanceHandler* serverInstance;

//Keep this log message displayed
#define logError(...) logErrorFmt(true, 0, ##__VA_ARGS__)
#define logMessage(...) logMessageFmt(true, 0, ##__VA_ARGS__)

#define _STRINGIFY(x) #x
#define STRINGIFY(x) _STRINGIFY(x)

namespace terminal::chandler {
    bool handleCommand(CommandHandle& command){
        TerminalCommand cmd{};

        size_t index = 0;
        do {
            size_t next = command.command.find(' ', index);
            auto elm = command.command.substr(index, next - index);

            if(index == 0){
                cmd.command = elm;
                std::transform(elm.begin(), elm.end(), elm.begin(), ::tolower);
                cmd.lcommand = elm;
            } else {
                cmd.arguments.emplace_back("", elm);
                std::transform(elm.begin(), elm.end(), elm.begin(), ::tolower);
                cmd.larguments.push_back(elm);
            }
            index = next + 1; //if no next than next = ~0 and if we add 1 then next is 0
        } while(index != 0);
        cmd.line = command.command;

        if(cmd.lcommand == "help")
            return handleCommandHelp(command, cmd);
        else if(cmd.lcommand == "end" || cmd.lcommand == "shutdown")
            return handleCommandEnd(command, cmd);
        else if(cmd.lcommand == "info")
            return handleCommandInfo(command, cmd);
        else if(cmd.lcommand == "chat")
            return handleCommandChat(command, cmd);
        else if(cmd.lcommand == "permgrant")
            return handleCommandPermGrant(command, cmd);
        else if(cmd.lcommand == "dummycrash" || cmd.lcommand == "dummy_crash")
            return handleCommandDummyCrash(command, cmd);
        else if(cmd.lcommand == "dummyfdflood" || cmd.lcommand == "dummy_fdflood")
            return handleCommandDummyFdFlood(command, cmd);
        else if(cmd.lcommand == "meminfo")
            return handleCommandMemInfo(command, cmd);
        else if(cmd.lcommand == "spoken")
            return handleCommandSpoken(command, cmd);
        else if(cmd.lcommand == "passwd")
            return handleCommandPasswd(command, cmd);
        else if(cmd.lcommand == "memflush")
            return handleCommandMemFlush(command, cmd);
        else if(cmd.lcommand == "statsreset")
            return handleCommandStatsReset(command, cmd);
        else if(cmd.lcommand == "reload")
            return handleCommandReload(command, cmd);
        else if(cmd.lcommand == "taskinfo")
            return handleCommandTaskInfo(command, cmd);
        else {
            logWarning(LOG_INSTANCE, "Missing terminal command {} ({})", cmd.command, cmd.line);
            command.response.emplace_back("unknown command");
            return false;
        }
    }

    bool handleCommandDummyCrash(CommandHandle& /* handle */, TerminalCommand& arguments) {
        if(!arguments.arguments.empty()) {
            if(arguments.larguments[0] == "raise") {
                raise(SIGABRT);
                return true;
            } else if(arguments.larguments[0] == "assert") { //dummycrash assert
                assert(false);
                return true;
            } else if(arguments.larguments[0] == "exception") {
                throw std::bad_exception();
            }
        }

        *(int*)(nullptr) = 0;
        return true;
    }

    bool handleCommandHelp(CommandHandle& handle, TerminalCommand& args) {
        handle.response.emplace_back("Available commands:");
        handle.response.emplace_back("  - end | shutdown");
        handle.response.emplace_back("  - reload config");
        handle.response.emplace_back("  - chat");
        handle.response.emplace_back("  - info");
        handle.response.emplace_back("  - permgrant");
        handle.response.emplace_back("  - passwd");
        handle.response.emplace_back("  - dummy_crash");
        handle.response.emplace_back("  - memflush");
        handle.response.emplace_back("  - meminfo");
        return true;
    }

    bool handleCommandEnd(CommandHandle& handle, TerminalCommand& arguments){
        if(arguments.arguments.empty()) {
            handle.response.emplace_back("Invalid argument count!");
            handle.response.emplace_back("Usage: shutdown <now|<number>[h|m|s]:...> <reason>");
            handle.response.emplace_back("Example: shutdown info | Displays info about the current scheduled shutdown");
            handle.response.emplace_back("Example: shutdown cancel | Cancels the currently scheduled shutdown");
            handle.response.emplace_back("Example: shutdown now Server shutdown | The server instance will shutdown instantly");
            handle.response.emplace_back("Example: shutdown 1h:30m Server shutdown | The server instance will shutdown in 1h and 30 min");
            handle.response.emplace_back("Example: shutdown 1h:1m:1s Server shutdown | The server instance will shutdown in 1h and 1 min and 1 second");
            return false;
        }

        nanoseconds period{};
        if(arguments.larguments[0] == "info") {
            auto task = ts::server::scheduledShutdown();
            if(!task) {
                handle.response.emplace_back("It isn't a shutdown scheduled!");
            } else {
                auto time = system_clock::to_time_t(task->time_point);
                handle.response.emplace_back("You scheduled a shutdown task at " + string(ctime(&time)));
            }
            return true;
        } else if(arguments.larguments[0] == "cancel") {
            auto task = ts::server::scheduledShutdown();
            if(!task) {
                handle.response.emplace_back("The isn't a shutdown scheduled!");
            } else {
                ts::server::cancelShutdown(true);
                handle.response.emplace_back("Shutdown task canceled!");
            }
            return true;
        } else if(arguments.larguments[0] != "now") {
            string error;
            period = period::parse(arguments.larguments[0], error);
            if(!error.empty()) {
                handle.response.emplace_back("Invalid period: " + error);
                return false;
            }
        }

        std::string reason = ts::config::messages::applicationStopped;
        if(arguments.arguments.size() > 1) {
            reason = "";
            for(auto it = arguments.arguments.begin() + 1; it != arguments.arguments.end(); it++)
                reason += it->string() + (it + 1 != arguments.arguments.end() ? " " : "");
        }

        if(period.count() == 0) {
            handle.response.emplace_back("Stopping instance");
            ts::server::shutdownInstance(reason);
        } else {
            auto time = system_clock::to_time_t(system_clock::now() + period);
            handle.response.emplace_back("Scheduled shutdown at " + string(ctime(&time)) + "");
            ts::server::scheduleShutdown(system_clock::now() + period, reason);
        }
        return true;
    }

    bool handleCommandInfo(CommandHandle& /* handle */, TerminalCommand& cmd){
        return false;
    }

    bool handleCommandChat(CommandHandle& handle, TerminalCommand& cmd){
        if(cmd.arguments.size() < 3){
            handle.response.emplace_back("Invalid usage!");
            handle.response.emplace_back("§e/chat <serverId> <mode{server=3|channel=2|manager=1}> <targetId> <message...>");
            return false;
        }

        ServerId sid = cmd.arguments[0];
        auto server = sid == 0 ? nullptr : serverInstance->getVoiceServerManager()->findServerById(sid);
        if(sid != 0 && !server) {
            handle.response.emplace_back("Could not resolve target server.");
            return false;
        }

        ts::ChatMessageMode mode = cmd.arguments[1];
        if(sid == 0 && mode != ChatMessageMode::TEXTMODE_SERVER){
            handle.response.emplace_back("Invalid mode/serverId");
            return false;
        }
        debugMessage(LOG_GENERAL,"Chat message mode " + to_string(mode));

        std::string message;
        int index = 3;
        while(index < cmd.arguments.size()){
            message += " " + cmd.arguments[index++].as<std::string>();
        }

        if(message.empty()){
            handle.response.emplace_back("Invalid message!");
            return false;
        }
        message = message.substr(1);

        switch (mode){
            case ChatMessageMode::TEXTMODE_SERVER:
                if(server){
                    server->broadcastMessage(server->getServerRoot(), message);
                } else {
                    for(auto srv : serverInstance->getVoiceServerManager()->serverInstances())
                        if(srv->running())
                            srv->broadcastMessage(srv->getServerRoot(), message);
                }
                break;
            case ChatMessageMode::TEXTMODE_CHANNEL:
                {
                    auto channel = server->getChannelTree()->findChannel(cmd.arguments[2].as<ChannelId>());
                    if(!channel){
                        handle.response.emplace_back("Could not resole target channel!");
                        return false;
                    }
                    for(const auto &cl : server->getClientsByChannel(channel))
                        cl->notifyTextMessage(ChatMessageMode::TEXTMODE_CHANNEL, server->getServerRoot(), cl->getClientId(), 0, system_clock::now(), message);
                }
                break;
            case ChatMessageMode::TEXTMODE_PRIVATE:
                {
                    ConnectedLockedClient<ConnectedClient> client{server->find_client_by_id(cmd.arguments[2].as<ClientId>())};
                    if(!client){
                        handle.response.emplace_back("Cloud not find manager from clid");
                        return false;
                    }

                    client->notifyTextMessage(ChatMessageMode::TEXTMODE_CHANNEL, server->getServerRoot(), client->getClientId(), 0, system_clock::now(), message);
                }
                break;
            default:
                handle.response.emplace_back("Invalid chat message mode!");
                return false;
        }

        handle.response.emplace_back("Chat message successfully send!");
        return true;
    }

    bool handleCommandPermGrant(CommandHandle& handle, TerminalCommand& cmd) {
        if(cmd.arguments.size() != 4) {
            handle.response.emplace_back("Invalid arguments!");
            handle.response.emplace_back("Arguments: <ServerId> <GroupId> <Permission Name> <Grant>");
            return false;
        }

        if(cmd.larguments[0].find_first_not_of("0123456789") != std::string::npos) {
            handle.response.emplace_back("Invalid server id! (Given number isn't numeric!)");
            return false;
        }
        if(cmd.larguments[1].find_first_not_of("0123456789") != std::string::npos) {
            handle.response.emplace_back("Invalid group id! (Given number isn't numeric!)");
            return false;
        }
        if(cmd.larguments[3].find_first_not_of("-0123456789") != std::string::npos) {
            handle.response.emplace_back("Invalid grant number! (Given number isn't numeric!)");
            return false;
        }

        permission::PermissionValue grant;
        ServerId serverId;
        GroupId groupId;

        try {
            serverId = cmd.arguments[0];
            groupId = cmd.arguments[1];
            grant = cmd.arguments[3];
        } catch(const std::exception& ex){
            handle.response.emplace_back("Could not parse given numbers");
            return false;
        }

        auto server = serverInstance->getVoiceServerManager()->findServerById(serverId);
        if(!server) {
            handle.response.emplace_back("Could not resolve server!");
            return false;
        }

        auto group = server->group_manager()->server_groups()->find_group(groups::GroupCalculateMode::GLOBAL, groupId);
        if(!group) {
            handle.response.emplace_back("Could not resolve server group!");
            return false;
        }

        auto perm = permission::resolvePermissionData(cmd.larguments[2]);
        if(perm->type == permission::unknown) {
            handle.response.emplace_back("Could not resolve permission!");
            return false;
        }
        group->permissions()->set_permission(perm->type, {0, grant}, permission::v2::do_nothing, permission::v2::set_value);
        handle.response.emplace_back("§aSuccessfully updated grant permissions.");
        return true;
    }

    //meminfo basic
    //memflush buffer
    //memflush alloc
    bool handleCommandMemFlush(CommandHandle& handle, TerminalCommand& cmd) {
        if(cmd.arguments.size() > 0) {
            if(cmd.larguments[0] == "db") {
                if(serverInstance->getSql()->getType() != sql::TYPE_SQLITE) {
                    handle.response.emplace_back("This command just works when you use sqlite!");
                    return false;
                }
                logMessage("Memory used by SQLite:");
                logMessage("  Currently used:  {0:>6}kb ({0:>9} bytes)", sqlite3_memory_used() / 1024);
                logMessage("  Max used:        {0:>6}kb ({0:>9} bytes)", sqlite3_memory_highwater(true) / 1024);
                logMessage("  Freed:           {0:>6}kb ({0:>9} bytes)", sqlite3_db_release_memory(((sql::sqlite::SqliteManager*) serverInstance->getSql())->getDatabase()) / 1024);
                logMessage("  Used after free: {0:>6}kb ({0:>9} bytes)", sqlite3_memory_used() / 1024);
                sqlite3_memory_highwater(true); //Reset the watermark
                return true;
            } else if(cmd.larguments[0] == "buffer") {
                auto info = buffer::cleanup_buffers(buffer::cleanmode::CHUNKS_BLOCKS);
                logMessage("Cleaned up {} bytes ({} bytes internal)", info.bytes_freed_internal + info.bytes_freed_buffer,info.bytes_freed_internal);
                return true;
            } else if(cmd.larguments[0] == "alloc") {
#ifdef  HAVE_JEMALLOC
                size_t
                    old_retained, old_active, old_allocated,
                    new_retained, new_active, new_allocated,
                    size_size_t;
                mallctl("stats.retained", &old_retained, &(size_size_t = sizeof(size_t)), nullptr, 0);
                mallctl("stats.allocated", &old_allocated, &(size_size_t = sizeof(size_t)), nullptr, 0);
                mallctl("stats.active", &old_active, &(size_size_t = sizeof(size_t)), nullptr, 0);

                auto begin = system_clock::now();
                mallctl("arena." STRINGIFY(MALLCTL_ARENAS_ALL) ".decay", nullptr, nullptr, nullptr, 0);
                mallctl("arena." STRINGIFY(MALLCTL_ARENAS_ALL) ".purge", nullptr, nullptr, nullptr, 0);
                auto end = system_clock::now();

                { /* refresh everything */
                    uint64_t epoch = static_cast<uint64_t>(system_clock::now().time_since_epoch().count());
                    mallctl("epoch", nullptr, nullptr, &epoch, sizeof(int16_t));
                }
                mallctl("stats.retained", &new_retained, &(size_size_t = sizeof(size_t)), nullptr, 0);
                mallctl("stats.allocated", &new_allocated, &(size_size_t = sizeof(size_t)), nullptr, 0);
                mallctl("stats.active", &new_active, &(size_size_t = sizeof(size_t)), nullptr, 0);

                logMessage("Cleaned up allocated internals successfully within {}us", duration_cast<microseconds>(end - begin).count());
                logMessage("    Allocated: {0:>9} => {0:>9} bytes", old_allocated, new_allocated);
                logMessage("    Retained : {0:>9} => {0:>9} bytes", old_retained, new_retained);
                logMessage("    Active   : {0:>9} => {0:>9} bytes", old_active, new_active);
#else
                logError("Jemalloc extension has not been compiled!");
#endif
                return true;
            }
        }
        logMessage("Invalid argument count. Possible: [db|buffer|alloc]");
        return true;
    }

    void process_mem_usage(double& vm_usage, double& resident_set)
    {
        vm_usage     = 0.0;
        resident_set = 0.0;

        // the two fields we want
        unsigned long vsize;
        long rss;
        {
            std::string ignore;
            std::ifstream ifs("/proc/self/stat", std::ios_base::in);
            ifs >> ignore >> ignore >> ignore >> ignore >> ignore >> ignore >> ignore >> ignore >> ignore >> ignore
                >> ignore >> ignore >> ignore >> ignore >> ignore >> ignore >> ignore >> ignore >> ignore >> ignore
                >> ignore >> ignore >> vsize >> rss;
        }

        long page_size_kb = sysconf(_SC_PAGE_SIZE) / 1024; // in case x86-64 is configured to use 2MB pages
        vm_usage = vsize / 1024.0;
        resident_set = rss * page_size_kb;
    }

    //meminfo track
    bool handleCommandMemInfo(CommandHandle& /* handle */, TerminalCommand& cmd){
        bool flag_base = false, flag_malloc = false, flag_track = false, flag_buffer = false;

        if(cmd.arguments.size() > 0) {
            if(cmd.larguments[0] == "basic")
                flag_base = true;
            else if(cmd.larguments[0] == "malloc")
                flag_malloc = true;
            else if(cmd.larguments[0] == "track")
                flag_track = true;
            else if(cmd.larguments[0] == "buffers")
                flag_buffer = true;
        } else {
            flag_base = flag_malloc = flag_track = flag_buffer = true;
        }

        if(flag_base) {
            double vm, rss;
            process_mem_usage(vm, rss);
            logMessage("Used memory: {} (VM: {})", rss, vm);
        }
        if(flag_malloc) {
            stringstream ss;
#ifdef HAVE_JEMALLOC
            malloc_stats_print([](void* data, const char* buffer) {
                auto _ss = (stringstream*) data;
                *_ss << buffer;
            }, &ss, nullptr);
#else
            ss << "Jemalloc is not present!";
#endif
            logMessage(ss.str());
        }
        if(flag_track)
            memtrack::statistics();
        if(flag_buffer) {
            auto info = buffer::buffer_memory();
            logMessage("Allocated memory: {}kb", ceil((info.bytes_internal + info.bytes_buffer) / 1024));
            logMessage("  Internal: {}kb", ceil((info.bytes_internal) / 1024));
            logMessage("  Buffers : {}kb", ceil((info.bytes_buffer) / 1024));
            logMessage("  Buffers Used: {}kb", ceil((info.bytes_buffer_used) / 1024));
        }
        return true;
    }

    bool handleCommandSpoken(CommandHandle& /* handle */, TerminalCommand& cmd) {
        //TODO print spoken statistics
        return false;
    }

    bool handleCommandPasswd(CommandHandle& handle, TerminalCommand& cmd) {
        if(cmd.arguments.size() != 2) {
            handle.response.emplace_back("Invalid usage: passwd <new_password> <repeated>");
            return false;
        }
        if(cmd.arguments[0].string() != cmd.arguments[1].string()) {
            handle.response.emplace_back("Passwords does not match!");
            return false;
        }

        auto serveradmin = serverInstance->getQueryServer()->find_query_account_by_name("serveradmin");
        if(!serveradmin) {
            auto password = "";
            logErrorFmt(true, 0, "Creating a new serveradmin query login!");
            if(!(serveradmin = serverInstance->getQueryServer()->create_query_account("serveradmin", 0, "serveradmin", password))) {
                handle.response.emplace_back("Could not create serveradmin account!");
                return false;
            }
        }

        serverInstance->getQueryServer()->change_query_password(serveradmin, cmd.arguments[0]);
        handle.response.emplace_back("Server admin successfully changed!");
        return true;
    }


    extern bool handleCommandStatsReset(CommandHandle& handle, TerminalCommand& cmd) {
        serverInstance->properties()[property::SERVERINSTANCE_MONTHLY_TIMESTAMP] = 0;
        handle.response.emplace_back("Monthly statistics will be reset");
        return true;
    }

    deque<int> fd_leaks;
    bool handleCommandDummyFdFlood(CommandHandle& /* handle */, TerminalCommand& cmd) {
        size_t value;
        if(cmd.arguments.size() < 1) {
            value = 1024;

            rlimit rlimit{0, 0};
            getrlimit(RLIMIT_NOFILE, &rlimit);
            logMessage("RLimit: {}/{}", rlimit.rlim_cur, rlimit.rlim_max);
            //setrlimit(7, &limit);
        } else if(cmd.larguments[0] == "clear") {
            logMessage("Clearup leaks");
            for(auto& fd : fd_leaks)
                close(fd);
            fd_leaks.clear();
            return true;
        } else {
            value = cmd.arguments[0].as<size_t>();
        }


        logMessage("Leaking {} file descriptors", value);
        size_t index = 0;
        while(index < value) {
            auto fd = dup(1);
            if(fd < 0)
                logMessage("Failed to create a file descriptor {} | {}", errno, strerror(errno));
            else
                fd_leaks.push_back(fd);

            index++;
        }
        return true;
    }


    bool handleCommandReload(CommandHandle& handle, TerminalCommand& cmd) {
        if(cmd.larguments.empty() || cmd.larguments[0] != "config") {
            handle.response.emplace_back("Invalid target. Available:");
            handle.response.emplace_back(" - config");
            return false;
        }

        vector<string> error;
        if(!serverInstance->reloadConfig(error, true)) {
            handle.response.emplace_back("Failed to reload instance ({}):", error.size());
            for(auto& msg : error)
                handle.response.emplace_back(" - " + msg);
        } else if(!error.empty()) {
            handle.response.emplace_back("Reloaded successfully. Messages:");
            for(auto& msg : error)
                handle.response.emplace_back(" - " + msg);
        } else {
            handle.response.emplace_back("Reloaded successfully.");
        }

        return true;
    }


    extern bool handleCommandTaskInfo(CommandHandle& handle, TerminalCommand& cmd) {
        serverInstance->general_task_executor()->print_statistics([&](const std::string& message) {
            handle.response.push_back(message);
        }, cmd.arguments.size() >= 1 && cmd.larguments[0] == "full");
        return true;
    }
}