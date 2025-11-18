#include <log/LogUtils.h>
#include <StringVariable.h>
#include <ThreadPool/ThreadHelper.h>
#include <src/terminal/PipedTerminal.h>
#include "ShutdownHelper.h"
#include "InstanceHandler.h"

using namespace std;
using namespace std::chrono;
using namespace ts;
using namespace ts::server;

extern bool mainThreadActive;

bool shuttingDown = false;
void ts::server::shutdownInstance(const std::string& message) {
    if(shuttingDown) return;
    shuttingDown = true;

    auto hangup_controller = std::thread([]{
        threads::self::sleep_for(chrono::seconds(30));
        logCriticalFmt(true, 0, "Could not shutdown server within 30 seconds! (Hangup!)");
        logCriticalFmt(true, 0, "Killing server!");

        terminal::finalize_pipe();
        auto force_kill = std::thread([]{
            threads::self::sleep_for(chrono::seconds(5));
            logCriticalFmt(true, 0, "Failed to exit normally!");
            logCriticalFmt(true, 0, "executing raise(SIGKILL);");
            raise(SIGKILL);
        });
        threads::name(force_kill, "force stopper");
        force_kill.detach();

        exit(2);
        //kill(0, SIGKILL);
    });
    threads::name(hangup_controller, "stop controller");
    hangup_controller.detach();

    logMessage(LOG_GENERAL, "Stopping all server instances!");
    if(serverInstance && serverInstance->getVoiceServerManager())
        serverInstance->getVoiceServerManager()->shutdownAll(message);

    mainThreadActive = false;
}

bool ts::server::isShuttingDown() {
    return shuttingDown;
}

std::shared_ptr<server::ShutdownData> currentShutdown = nullptr;
std::shared_ptr<server::ShutdownData> server::scheduledShutdown() { return currentShutdown; }

inline void broadcastMessage(const std::string& message) {
    if(!serverInstance || !serverInstance->getVoiceServerManager())
        return;

    for(const auto &server : serverInstance->getVoiceServerManager()->serverInstances()) {
        if(server->running()) {
            server->broadcastMessage(server->getServerRoot(), message);
        }
    }
}

void executeScheduledShutdown(const std::shared_ptr<ShutdownData>& data);
bool server::scheduleShutdown(const std::chrono::system_clock::time_point& time, const std::string& reason) {
    server::cancelShutdown(false); //Cancel old shutdown

    auto data = std::make_shared<ShutdownData>();
    data->active = true;
    data->time_point = time;
    data->reason = reason;

    data->shutdown_thread = std::thread{[data]{
        executeScheduledShutdown(data);
    }};
    threads::name(data->shutdown_thread, "Shutdown executor");
    currentShutdown = data;
    return true;
}

void server::cancelShutdown(bool notify) {
    if(!currentShutdown) return;
    if(notify && !config::messages::shutdown::canceled.empty()) {
        broadcastMessage(config::messages::shutdown::canceled);
    }

    auto current = server::scheduledShutdown();
    current->active = false;
    current->shutdownNotify.notify_all();
    if(!threads::save_join(current->shutdown_thread)) {
        logCritical(LOG_GENERAL, "Could not terminate shutdown thread!");
        current->shutdown_thread.detach();
    }
    currentShutdown = nullptr;
}

void executeScheduledShutdown(const shared_ptr<ShutdownData>& data) {
    std::time_t time_point = system_clock::to_time_t(data->time_point);
    {
        auto message = strvar::transform(config::messages::shutdown::scheduled, strvar::FunctionValue("time", (strvar::FunctionValue::FValueFNEasy) [&](std::deque<std::string> value)  {
                auto pattern = !value.empty() ? value[0] : "%Y-%m-%d_%H:%M:%S";

                tm* tm_info = localtime(&time_point);

                char timeBuffer[1024];
                if(strftime(timeBuffer, 1024, pattern.c_str(), tm_info) == 0) {
                    return string("string is longer than the buffer");
                }

                return string(timeBuffer);
        }));
        broadcastMessage(message);
    }

    while(data->time_point > system_clock::now()) {
        auto per = data->time_point - system_clock::now();
        pair<seconds, string> best_period = {seconds(0), ""};

        for(const auto& period : config::messages::shutdown::intervals) {
            if(period.first > per) continue;
            if(period.first > best_period.first) best_period = period;
        }

        {
            std::unique_lock<std::mutex> lock(data->shutdownMutex);
            data->shutdownNotify.wait_until(lock, data->time_point - best_period.first, [data](){ return !data->active; });
            if(!data->active) return;
        }

        if(best_period.first.count() == 0)
            broadcastMessage(config::messages::shutdown::now);
        else
            broadcastMessage(strvar::transform(config::messages::shutdown::interval, strvar::StringValue{"interval", best_period.second}));
    }

    ts::server::shutdownInstance(data->reason);
    //No need to delete own task
}