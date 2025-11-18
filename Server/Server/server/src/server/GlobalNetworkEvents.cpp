//
// Created by WolverinDEV on 15/04/2021.
//

#include "GlobalNetworkEvents.h"
#include <log/LogUtils.h>
#include <misc/threads.h>

using namespace ts::server;

namespace ts::server {
    struct NetworkEventLoopUseList {
        std::vector<NetworkEventLoop::EventLoopId> used_event_loops{};
    };
}

NetworkEventLoop::NetworkEventLoop(size_t event_loop_size) : event_loop_size{event_loop_size} { }
NetworkEventLoop::~NetworkEventLoop() {
    this->shutdown();
}

bool NetworkEventLoop::initialize() {
    std::lock_guard lock{this->mutex};
    while(this->event_loops.size() < this->event_loop_size) {
        auto event_loop = new EventLoop{this->event_loop_id_index++};
        event_loop->event_base = event_base_new();
        if(!event_loop->event_base) {
            logError(LOG_GENERAL, "Failed to allocate new event base.");
            delete event_loop;
            return false;
        }

        event_loop->dispatcher = std::thread{NetworkEventLoop::event_loop_dispatch, event_loop};
        threads::name(event_loop->dispatcher, "network loop #" + std::to_string(event_loop->loop_id));
        this->event_loops.push_back(event_loop);
    }

    return true;
}

void NetworkEventLoop::shutdown() {
    std::unique_lock lock{this->mutex};
    auto event_loops_ = std::move(this->event_loops);
    lock.unlock();

    for(const auto& loop : event_loops_) {
        event_base_loopexit(loop->event_base, nullptr);
    }

    for(const auto& loop : event_loops_) {
        if(!threads::timed_join(loop->dispatcher, std::chrono::seconds{15})) {
            /* This will cause a memory corruption since the memory we're freeing will still be accessed */
            logCritical(LOG_GENERAL, "Failed to join event loop {}. Detaching thread.", loop->loop_id);
            loop->dispatcher.detach();
        }

        event_base_free(loop->event_base);
        delete loop;
    }
}

void NetworkEventLoop::free_use_list(NetworkEventLoopUseList *list) {
    delete list;
}

event* NetworkEventLoop::allocate_event(int fd, short events, event_callback_fn callback, void *callback_data, NetworkEventLoopUseList **use_list) {
    if(use_list && !*use_list) {
        *use_list = new NetworkEventLoopUseList{};
    }

    std::lock_guard lock{this->mutex};
    EventLoop* event_loop{nullptr};

    size_t try_count{0};
    while(try_count < this->event_loops.size()) {
        event_loop = this->event_loops[this->event_loop_index % this->event_loops.size()];

        if(!use_list) {
            /* we have our event loop */
            break;
        }

        auto& used_loops = (*use_list)->used_event_loops;
        if(std::find(used_loops.begin(), used_loops.end(), event_loop->loop_id) == used_loops.end()) {
            /* we can use this event loop */
            break;
        }

        try_count++;
    }

    if(try_count >= this->event_loops.size()) {
        /* We've no event loop to put the event in */
        return nullptr;
    }
    assert(event_loop);

    auto event = event_new(event_loop->event_base, fd, events, callback, callback_data);
    if(!event) {
        /* failed to allocate the new event */
        return nullptr;
    }

    this->event_loop_index++;
    if(use_list) {
        (*use_list)->used_event_loops.push_back(event_loop->loop_id);
    }

    return event;
}

void NetworkEventLoop::event_loop_dispatch(EventLoop *event_loop) {
    debugMessage(LOG_GENERAL, "Network event loop {} started.", event_loop->loop_id);
    auto result = event_base_loop(event_loop->event_base, EVLOOP_NO_EXIT_ON_EMPTY);
    if(result < 0) {
        logError(LOG_GENERAL, "Network event loop exited due to an error.");
    } else if(result == 0) {
        debugMessage(LOG_GENERAL, "Network event loop {} exited.", event_loop->loop_id);
    } else if(result > 0) {
        logError(LOG_GENERAL, "Network event loop exited because of no pending events. This should not happen!");
    }
}