#pragma once

#include <atomic>
#include <thread>

#ifdef WIN32
    #define always_inline __forceinline
#else
    #define always_inline inline __attribute__((__always_inline__))
#endif

class spin_mutex {
        std::atomic_bool locked{false};
    public:
        always_inline void lock() {
            while (locked.exchange(true, std::memory_order_acquire))
                this->wait_until_release();
        }


        always_inline void wait_until_release() const {
            uint8_t round = 0;
            while (locked.load(std::memory_order_relaxed)) {
                //Yield when we're using this lock for a longer time, which we usually not doing
                if(round++ % 8 == 0) {
                    std::this_thread::yield();
                }
            }
        }

        always_inline bool try_lock() {
            return !locked.exchange(true, std::memory_order_acquire);
        }

        always_inline void unlock() {
            locked.store(false, std::memory_order_release);
        }
};

#undef always_inline