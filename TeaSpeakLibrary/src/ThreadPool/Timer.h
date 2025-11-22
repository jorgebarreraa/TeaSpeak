#pragma once

// Legacy compatibility header for ThreadPool/Timer.h
// This file is included but not actively used in the codebase
// The actual task scheduling functionality is provided by misc/task_executor.h

#include <chrono>
#include <functional>
#include <string>

namespace ThreadPool {
    // Stub namespace for legacy compatibility
    // No actual implementation needed as task_executor is used instead
}

namespace threads {
    namespace timer {
        // Function log callback for timer/thread debugging
        // Signature: void(const std::string& message, bool debug)
        inline std::function<void(const std::string&, bool)> function_log = [](const std::string&, bool) {
            // Default no-op implementation
        };
    }
}
