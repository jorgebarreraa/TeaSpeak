#pragma once

#include <thread>
#include <string>
#include <string_view>

namespace threads {
    extern bool name(std::thread& /* thread */, const std::string_view& /* name */);
    extern std::string name(std::thread& /* thread */);

    /*
     * This function will not throw an error if the thread has already been joined.
     * It returns true if join succeeded, false on any error (like thread has already be joined)
     */
    extern bool save_join(std::thread& /* thread */, bool /* ignore resource deadlock */ = false);

    extern bool timed_join(std::thread& /* thread */, const std::chrono::nanoseconds& /* timeout */);

    template <typename Clock>
    inline bool timed_join(std::thread& thread, const std::chrono::time_point<Clock>& timeout) {
        auto now = Clock::now();
        if(now > timeout)
            timeout = now;
        return timed_join(thread, timeout - now);
    }
}
