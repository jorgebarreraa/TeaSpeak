//
// Created by WolverinDEV on 15/04/2021.
//

#pragma once

#include <mutex>
#include <thread>
#include <vector>
#include <event.h>

namespace ts::server {
    struct NetworkEventLoopUseList;
    class NetworkEventLoop {
        public:
            typedef uint32_t EventLoopId;
            explicit NetworkEventLoop(size_t /* thread pool size */);
            ~NetworkEventLoop();

            [[nodiscard]] bool initialize();
            void shutdown();

            [[nodiscard]] inline size_t loop_count() const { return this->event_loop_size; }

            /**
             * Allocate a new event on the network event loop.
             * @param fd
             * @param events
             * @param callback
             * @param callback_arg
             * @return `nullptr` if an error occurred and an even otherwise
             */
            [[nodiscard]] struct event* allocate_event(
                    evutil_socket_t /* fd */,
                    short /* events */,
                    event_callback_fn /* callback */,
                    void */* callback_arg */,
                    NetworkEventLoopUseList** /* containing all loops the event has already been bound to */
            );

            void free_use_list(NetworkEventLoopUseList* /* use list */);
        private:
            struct EventLoop {
                const EventLoopId loop_id;
                struct event_base* event_base{nullptr};
                std::thread dispatcher{};
            };

            size_t event_loop_size;

            std::mutex mutex{};
            EventLoopId event_loop_id_index{1};
            size_t event_loop_index{0};
            std::vector<EventLoop*> event_loops{};

            static void event_loop_dispatch(EventLoop*);
    };
}