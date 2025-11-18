#pragma once

#include <string>
#include <netinet/in.h>
#include <misc/net.h>
#include <mutex>

#include <event.h>

#include <Definitions.h>
#include <lookup/ipv4.h>

namespace ts::connection {
    class VoiceClientConnection;
}

namespace ts::server::server::udp {
    class Server;
    class Socket {
        public:
            Socket(Server* /* server handle */, ServerId /* server id */, const sockaddr_storage& /* address */);

            void start();
            void stop();

            [[nodiscard]] inline bool is_active() const { return this->file_descriptor > 0; }
            [[nodiscard]] inline std::string address_string() const { return net::to_string(address); }
            [[nodiscard]] inline uint16_t address_port() const { return net::port(address); }

        private:
            struct EVLoopEntry {
                Socket* socket;

                event* event_read{nullptr};
                event* event_write{nullptr};
            };

            Server* server;

            ServerId server_id;
            sockaddr_storage address;

            int file_descriptor{0};

            std::deque<EVLoopEntry*> event_loop_entries{};

            std::mutex clients_lock{};
            std::deque<std::shared_ptr<connection::VoiceClientConnection>> clients{};
            std::vector<std::shared_ptr<connection::VoiceClientConnection>> client_map_by_id{};
            lookup::ip_v4<connection::VoiceClientConnection> clients_by_ipv4{};
            lookup::ip_v4<connection::VoiceClientConnection> clients_by_ipv6{};

            spin_mutex client_write_lock{};
            connection::VoiceClientConnection* client_write_head{nullptr};
            connection::VoiceClientConnection** client_write_tail{&client_write_head};

            static void callback_event_read(int, short, void*);
            static void callback_event_write(int, short, void*);
    };

    struct ServerEventLoops {
        struct event_base* event_base{nullptr};
        std::thread dispatch_thread{};
    };

    class Server {
        public:

        private:
    };
}