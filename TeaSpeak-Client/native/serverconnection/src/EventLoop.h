#pragma once

#include <mutex>
#include <memory>
#include <vector>
#include <string>
#include <thread>
#include <condition_variable>

namespace tc::event {
    class EventExecutor;
    class EventEntry {
            friend class EventExecutor;
        public:
            EventEntry() = default;
            virtual ~EventEntry() = default;

            virtual void event_execute(const std::chrono::system_clock::time_point& /* scheduled timestamp */) = 0;
            virtual void event_execute_dropped(const std::chrono::system_clock::time_point& /* scheduled timestamp */) {}

            std::unique_lock<std::timed_mutex> execute_lock(bool force) {
                if(force) {
                    return std::unique_lock<std::timed_mutex>(this->_execute_mutex);
                } else {
                    auto lock = std::unique_lock<std::timed_mutex>(this->_execute_mutex, std::defer_lock);
                    if(this->execute_lock_timeout.count() > 0) {
                        (void) lock.try_lock_for(this->execute_lock_timeout);
                    } else {
                        (void) lock.try_lock();
                    }
                    return lock;
                }
            }

            [[nodiscard]] inline bool single_thread_executed() const { return this->_single_thread; }
            inline void single_thread_executed(bool value) { this->_single_thread = value; }

        protected:
            std::chrono::nanoseconds execute_lock_timeout{0};
        private:
            void* _event_ptr = nullptr;
            bool _single_thread = true; /* if its set to true there might are some dropped executes! */
            std::timed_mutex _execute_mutex;
    };

    class EventExecutor {
        public:
            explicit EventExecutor(const std::string& /* thread prefix */);
            virtual ~EventExecutor();

            bool initialize(int /* num threads */);
            bool schedule(const std::shared_ptr<EventEntry>& /* entry */);
            bool cancel(const std::shared_ptr<EventEntry>& /* entry */); /* Note: Will not cancel already running executes */
            void shutdown();
        private:
            struct LinkedEntry {
                LinkedEntry* previous;
                LinkedEntry* next;

                std::chrono::system_clock::time_point scheduled;
                std::weak_ptr<EventEntry> entry;
            };
            static void _executor(EventExecutor*);
            void _shutdown(std::unique_lock<std::mutex>&);
            void _reset_events(std::unique_lock<std::mutex>&);

            bool should_shutdown = true;

            std::vector<std::thread> threads;
            std::mutex lock;
            std::condition_variable condition;

            LinkedEntry* head = nullptr;
            LinkedEntry* tail = nullptr;

            std::string thread_prefix;
    };
}