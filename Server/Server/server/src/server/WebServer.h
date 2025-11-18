#pragma once

#include <ThreadPool/Mutex.h>
#include <ThreadPool/Thread.h>
#include <event.h>
#include <misc/net.h>

namespace ts {
    namespace server {
        class VirtualServer;
        class WebClient;

        class WebControlServer {
                friend class WebClient;
            public:
                struct Binding {
                    sockaddr_storage address{};
                    int file_descriptor = 0;
                    ::event* event_accept = nullptr;

                    inline std::string as_string() { return net::to_string(address, true); }
                };

                explicit WebControlServer(const std::shared_ptr<VirtualServer>&);
                ~WebControlServer();

                bool start(const std::deque<std::shared_ptr<Binding>>& /* bindings */, std::string& /* error */);
                inline bool running(){ return _running; }
                void stop();

                std::shared_ptr<VirtualServer> getTS(){ return this->handle; }

                std::deque<std::shared_ptr<WebClient>> connectedClients(){
                    threads::MutexLock l(this->clientLock);
                    return this->clients;
                }

                [[nodiscard]] std::optional<std::string> external_binding();
            private:
                std::shared_ptr<VirtualServer> handle = nullptr;

                threads::Mutex clientLock;
                std::deque<std::shared_ptr<WebClient>> clients;

                bool _running = false;
                std::deque<std::shared_ptr<Binding>> bindings;

                std::mutex server_reserve_fd_lock{};
                int server_reserve_fd{-1}; /* -1 = unset | 0 = in use | > 0 ready to use */

                //IO stuff
                std::chrono::system_clock::time_point accept_event_deleted;
            private:
                static void on_client_receive(int, short, void *);
                void unregisterConnection(const std::shared_ptr<WebClient>&);
        };
    }
}