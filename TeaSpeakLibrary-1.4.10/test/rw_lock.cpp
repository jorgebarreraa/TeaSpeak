//
// Created by WolverinDEV on 03/03/2020.
//

#include "src/lock/rw_mutex.h"
#include <atomic>
#include <vector>
#include <thread>
#include <iostream>
#include <functional>
#include <algorithm>

namespace helper {
    std::mutex threads_lock{};
    size_t running_threads{0};
    std::vector<std::thread> thread_handles{};
    bool sync_pending{false};

    inline void sync_threads() {
        static std::condition_variable sync_cv;
        static size_t synced_threads{0};

        auto timeout = std::chrono::system_clock::now() + std::chrono::seconds{5};
        std::unique_lock lock{threads_lock};
        if(++synced_threads == running_threads) {
            sync_cv.notify_all();
            synced_threads = 0;
            sync_pending = false;
            return;
        }

        sync_pending = true;
        if(sync_cv.wait_until(lock, timeout) == std::cv_status::timeout) {
            std::cerr << "failed to sync threads" << std::endl;
            abort();
        }
    }

    template <typename... args_t>
    inline void create_thread(args_t&&... arguments) {
        auto callback = std::bind(arguments...);

        std::lock_guard tlock{threads_lock};
        thread_handles.emplace_back([callback]{
            callback();

            std::lock_guard tlock{threads_lock};
            auto it = std::find_if(thread_handles.begin(), thread_handles.end(), [](const auto& thread) {
                return thread.get_id() == std::this_thread::get_id();
            });
            if(it == thread_handles.end())
                return; /* thread will be joined via await_all_threads */

            if(sync_pending) {
                std::cerr << "thread terminated while sync was pending" << std::endl;
                abort();
            }

            running_threads--;
            it->detach();
            thread_handles.erase(it);
        });
        running_threads++;
    }

    inline void await_all_threads() {
        std::unique_lock thread_lock{threads_lock};
        while(!thread_handles.empty()) {
            auto thread = std::move(thread_handles.back());
            thread_handles.pop_back();
            thread_lock.unlock();
            thread.join();
            thread_lock.lock();

            if(sync_pending) {
                std::cerr << "thread terminated while sync was pending" << std::endl;
                abort();
            }
            running_threads--;
        }

    }
}

void testSharedLockLock() {
    ts::rw_mutex lock{};

    lock.lock_shared();
    lock.lock_shared();

    lock.unlock_shared();
    lock.lock_shared();
    lock.unlock_shared();
    lock.unlock_shared();
}

void testExclusiveLock() {
    ts::rw_mutex lock{};

    size_t read_locked{0}, write_locked{0};
    for(int i = 0; i < 10; i++) {
        if(rand() % 2 == 0) {
            write_locked++;
            lock.lock();
            lock.unlock();
        } else {
            read_locked++;
            lock.lock_shared();
            lock.unlock_shared();
        }
    }

    /* to ensure that the read and write function has been tested more than once */
    assert(read_locked > 1);
    assert(write_locked > 1);
}

void testRW_Multithread() {
    ts::rw_mutex lock{};

    for(int i = 0; i < 20; i++) {
        const auto do_sync = i < 5;

        std::atomic<bool> write_locked{false};
        helper::create_thread([&]{
            lock.lock();
            write_locked = true;
            if(do_sync) helper::sync_threads(); /* sync point 1 */
            std::this_thread::sleep_for(std::chrono::seconds{1});
            write_locked = false;
            lock.unlock();
        });

        helper::create_thread([&]{
            if(do_sync) helper::sync_threads(); /* sync point 1 */
            lock.lock_shared();
            std::cout << "shared locked" << std::endl;
            if(write_locked) {
                std::cerr << "Shared lock succeeded, but thread has been write locked!" << std::endl;
                return;
            }
            std::this_thread::sleep_for(std::chrono::seconds{1});
            std::cout << "unlocking shared lock" << std::endl;
            lock.unlock_shared();
        });
        helper::await_all_threads();
        std::cout << "Loop " << i << " passed" << std::endl;
    }
}

void testLockUpgrade() {
    std::atomic<bool> lock_released{true};
    std::atomic<bool> lock_shared{false};
    ts::rw_mutex lock{};

    lock.lock_shared();
    std::cout << "[main] Locked shared lock" << std::endl;
    lock_released = false;
    lock_shared = true;

    helper::create_thread([&]{
        std::cout << "Awaiting exclusive lock" << std::endl;
        lock.lock();
        std::cout << "Received exclusive lock" << std::endl;
        if(!lock_released) {
            std::cerr << "Acquired write lock even thou is hasn't been released!" << std::endl;
            abort();
        }

        lock.unlock();
    });
    std::this_thread::sleep_for(std::chrono::milliseconds {100});

    if(auto err = lock.upgrade_lock(); err) {
        std::cerr << "Failed to upgrade lock: " << err << std::endl;
        abort();
    }
    std::cout << "[main] Upgraded shared lock" << std::endl;
    lock_shared = false;

    helper::create_thread([&]{
        std::cout << "Awaiting shared lock" << std::endl;
        lock.lock_shared();
        std::cout << "Received shared lock" << std::endl;
        if(!lock_shared) {
            std::cerr << "Acquired write lock even thou is hasn't been lock in shared mode!" << std::endl;
            abort();
        }

        lock.unlock_shared();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds {100});
    lock_shared = true;
    std::cout << "[main] Downgrading exclusive lock" << std::endl;
    lock.downgrade_lock();

    std::this_thread::sleep_for(std::chrono::milliseconds {10});
    lock_released = true;
    std::cout << "[main] Releasing shared lock" << std::endl;
    lock.unlock_shared();

    helper::await_all_threads();
}

void testLockUpgradeRelease() {
    {
        ts::rw_mutex lock{};
        lock.lock_shared();
        (void) lock.upgrade_lock();
        lock.unlock();
    }
    {
        ts::rw_mutex lock{};
        lock.lock_shared();
        (void) lock.upgrade_lock();
        lock.downgrade_lock();
        lock.unlock_shared();
    }
}

void testLockGuard() {
    ts::rw_mutex  mutex{};
    ts::rwshared_lock lock{mutex};
    bool lock_allowed{false};

    if(auto err = lock.auto_lock_exclusive(); err) {
        std::cerr << "Failed to upgrade lock: " << err << std::endl;
        abort();
    }
    assert(lock.exclusive_locked());

    helper::create_thread([&]{
        std::cout << "Awaiting exclusive lock" << std::endl;
        mutex.lock();
        std::cout << "Received exclusive lock" << std::endl;
        if(!lock_allowed) {
            std::cerr << "Acquired write lock even thou is hasn't been released!" << std::endl;
            abort();
        }

        mutex.unlock();
    });
    std::this_thread::sleep_for(std::chrono::milliseconds {100});

    lock.auto_lock_shared();

    std::this_thread::sleep_for(std::chrono::milliseconds {100});
    lock_allowed = true;
    lock.auto_unlock();
    helper::await_all_threads();

    lock.lock_exclusive();
}

int main() {
    { /* get rid of the unused warnings */
        ts::rw_unsafe_mutex mutex_a{};
        ts::rw_safe_mutex mutex_b{};
    }

    testSharedLockLock();
    testExclusiveLock();
    //testRW_Multithread();
    //TODO: Test lock order
    testLockUpgrade();
    testLockUpgradeRelease();
    testLockGuard();
}