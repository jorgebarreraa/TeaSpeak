#pragma once

#include <mutex>
#include <memory>
#include <vector>
#include <string>
#include <thread>
#include <condition_variable>

namespace ts {
    namespace event {
        class EventExecutor;

        class EventEntry {
                friend class EventExecutor;
            public:
                virtual void event_execute(const std::chrono::system_clock::time_point& /* scheduled timestamp */) = 0;

            private:
                void* _event_ptr = nullptr;
        };

        template <typename class_t>
        class ProxiedEventEntry : public event::EventEntry {
            public:
                using callback_t = void(class_t::*)(const std::chrono::system_clock::time_point &);
                using static_callback_t = void(*)(class_t *, const std::chrono::system_clock::time_point &);

                ProxiedEventEntry(const std::shared_ptr<class_t>& _instance, callback_t callback) : instance(_instance), callback(callback) { }

                std::weak_ptr<class_t> instance;
                callback_t callback;

                void event_execute(const std::chrono::system_clock::time_point &point) override {
                    auto _instance = this->instance.lock();
                    if(!_instance)
                        return;

                    auto callback_ptr = (void**) &this->callback;
                    (*(static_callback_t*) callback_ptr)(&*_instance, point);
                }
        };

        class EventExecutor {
            public:
                explicit EventExecutor(std::string  /* thread prefix */);
                virtual ~EventExecutor();

                bool initialize(int /* num threads */);
                bool schedule(const std::shared_ptr<EventEntry>& /* entry */);
                bool cancel(const std::shared_ptr<EventEntry>& /* entry */); /* Note: Will not cancel already running executes */
                void shutdown();

                inline const std::string& thread_prefix() const { return this->_thread_prefix; }

                void threads(int /* num threads */);
                inline int threads() const { return this->target_threads; }
            private:
                struct LinkedEntry {
                    LinkedEntry* previous;
                    LinkedEntry* next;

                    std::chrono::system_clock::time_point scheduled;
                    std::weak_ptr<EventEntry> entry;
                };

                static void _executor(EventExecutor*);
                void _spawn_executor(std::unique_lock<std::mutex>&);
                void _shutdown(std::unique_lock<std::mutex>&);
                void _reset_events(std::unique_lock<std::mutex>&);

#ifndef WIN32
                void _reassign_thread_names(std::unique_lock<std::mutex>&);
#endif

                bool should_shutdown = true;
                bool should_adjust = false; /* thread adjustments */
                int target_threads = 0;

                std::vector<std::thread> _threads;
                std::mutex lock;
                std::condition_variable condition;

                LinkedEntry* head = nullptr;
                LinkedEntry* tail = nullptr;

                std::string _thread_prefix;
        };
    }
}