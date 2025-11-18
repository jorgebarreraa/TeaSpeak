#pragma once

#include "Configuration.h"

namespace ts {
    namespace server {
        extern void shutdownInstance(const std::string& reason = ts::config::messages::applicationStopped);
        extern bool isShuttingDown();

        struct ShutdownData {
            std::string reason;
            std::chrono::system_clock::time_point time_point;
            bool active;

            std::thread shutdown_thread{};
            std::mutex shutdownMutex;
            std::condition_variable shutdownNotify;
        };

        extern bool scheduleShutdown(const std::chrono::system_clock::time_point&, const std::string& = ts::config::messages::applicationStopped);
        extern std::shared_ptr<ShutdownData> scheduledShutdown();
        extern void cancelShutdown(bool notify = true);
    }
}