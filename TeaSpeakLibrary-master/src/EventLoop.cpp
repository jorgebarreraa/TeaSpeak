#include <thread>
#include <utility>
#include <vector>
#include <condition_variable>
#include <cassert>
#include <algorithm>
#include "./log/LogUtils.h"
#include "./misc/sassert.h"
#include "./EventLoop.h"

using namespace std;
using namespace ts::event;

EventExecutor::EventExecutor(std::string  thread_prefix) : _thread_prefix{std::move(thread_prefix)} {}
EventExecutor::~EventExecutor() {
    unique_lock lock(this->lock);
    this->_shutdown(lock);
    this->_reset_events(lock);
}

void EventExecutor::threads(int threads) {
    unique_lock lock(this->lock);

    if(this->target_threads == threads)
        return;

    this->target_threads = threads;
    while(this->_threads.size() < threads)
        this->_spawn_executor(lock);

    if(this->_threads.size() > threads) {
        this->should_adjust = true;
        this->condition.notify_all();
    }

#ifndef WIN32
    this->_reassign_thread_names(lock);
#endif
}

bool EventExecutor::initialize(int threads) {
    unique_lock lock(this->lock);
    this->_shutdown(lock);
    if(!lock.owns_lock())
        lock.lock();

    this->should_shutdown = false;
    while(threads-- > 0)
        _spawn_executor(lock);

#ifndef WIN32
    this->_reassign_thread_names(lock);
#endif
    return true;
}

void EventExecutor::_spawn_executor(std::unique_lock<std::mutex>& lock) {
    if(!lock.owns_lock())
        lock.lock();

    this->_threads.emplace_back(&EventExecutor::_executor, this);
}

void EventExecutor::shutdown() {
    unique_lock lock(this->lock);
    this->_shutdown(lock);
}

bool EventExecutor::schedule(const std::shared_ptr<ts::event::EventEntry> &entry) {
    unique_lock lock(this->lock);
    if(!entry || entry->_event_ptr)
        return true; /* already scheduled */

    auto linked_entry = new LinkedEntry{};
    linked_entry->entry = entry;
    linked_entry->next = nullptr;
    linked_entry->previous = this->tail;
    linked_entry->scheduled = std::chrono::system_clock::now();

    if(this->tail) {
        this->tail->next = linked_entry;
        this->tail = linked_entry;
    } else {
        this->head = linked_entry;
        this->tail = linked_entry;
    }
    this->condition.notify_one();

    entry->_event_ptr = linked_entry;
    return true;
}

bool EventExecutor::cancel(const std::shared_ptr<ts::event::EventEntry> &entry) {
    unique_lock lock(this->lock);
    if(!entry || !entry->_event_ptr)
        return false;

    auto linked_entry = (LinkedEntry*) entry->_event_ptr;
    entry->_event_ptr = nullptr;

    this->head = linked_entry->next;
    if(this->head) {
        assert(linked_entry == this->head->previous);
        this->head->previous = nullptr;
    } else {
        assert(linked_entry == this->tail);
        this->tail = nullptr;
    }

    delete linked_entry;
    return true;
}

void EventExecutor::_shutdown(std::unique_lock<std::mutex> &lock) {
    if(!lock.owns_lock())
        lock.lock();

    this->should_shutdown = true;
    this->condition.notify_all();
    lock.unlock();
    for(auto& thread : this->_threads)
        if(thread.joinable()) {
            try {
                thread.join(); /* TODO: Timeout? */
            } catch(std::system_error& ex) {
                if(ex.code() != errc::invalid_argument) /* thread is not joinable anymore :) */
                    throw;
            }
        }
    lock.lock();
    this->should_shutdown = false;
}

void EventExecutor::_reset_events(std::unique_lock<std::mutex> &lock) {
    if(!lock.owns_lock())
        lock.lock();

    auto entry = this->head;
    while(entry) {
        auto next = entry->next;
        delete entry;
        entry = next;
    }
    this->head = nullptr;
    this->tail = nullptr;
}

#ifndef WIN32
void EventExecutor::_reassign_thread_names(std::unique_lock<std::mutex> &lock) {
    if(!lock.owns_lock())
        lock.lock();

    size_t index = 1;
    for(auto& thread : this->_threads) {
        auto handle = thread.native_handle();
        auto name = this->_thread_prefix + to_string(index++);
        pthread_setname_np(handle, name.c_str());
    }
}
#endif

void EventExecutor::_executor(ts::event::EventExecutor *loop) {
    while(true) {
        sassert(std::addressof(loop->lock) != nullptr);

        unique_lock lock(loop->lock);
        loop->condition.wait(lock, [&] {
            return loop->should_shutdown || loop->should_adjust || loop->head != nullptr;
        });
        if(loop->should_shutdown)
            break;

        if(loop->should_adjust) {
            const auto current_threads = loop->_threads.size();
            if(current_threads > loop->target_threads) {
                /* terminate this loop */

                auto thread_id = std::this_thread::get_id();
                auto index = std::find_if(loop->_threads.begin(), loop->_threads.end(), [&](const std::thread& thread) { return thread_id == thread.get_id(); });
                if(index == loop->_threads.end()) {
                    /* TODO: Log error */
                } else {
                    (*index).detach(); /* lets detach ourselfs before we delete the handle */
                    loop->_threads.erase(index);
                }
                loop->should_adjust = ((current_threads - 1) != loop->target_threads);
                return; /* we're out now! */
            } else {
                loop->should_adjust = false;
            }

#ifndef WIN32
            if(!loop->should_adjust)
                loop->_reassign_thread_names(lock);
#endif
        }

        if(!loop->head)
            continue;

        auto linked_entry = loop->head;
        loop->head = linked_entry->next;
        if(loop->head) {
            sassert(linked_entry == loop->head->previous);
            loop->head->previous = nullptr;
        } else {
            sassert(linked_entry == loop->tail);
            loop->tail = nullptr;
        }

        auto event_handler = linked_entry->entry.lock();
        if(!event_handler) {
            /* event handler passed away while waiting for beeing executed */
            delete linked_entry;
            continue;
        }

        sassert(event_handler->_event_ptr == linked_entry);
        event_handler->_event_ptr = nullptr;
        lock.unlock();

        event_handler->event_execute(linked_entry->scheduled);
        delete linked_entry;
    }
}