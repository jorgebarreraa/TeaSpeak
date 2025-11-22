#pragma once

// Legacy compatibility header for ThreadPool/Mutex.h
// Provides basic mutex compatibility

#include <mutex>

namespace ThreadPool {
    // Alias standard mutex types for compatibility
    using Mutex = std::mutex;
    using RecursiveMutex = std::recursive_mutex;
}
