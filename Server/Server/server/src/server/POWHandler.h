#pragma once

#include <mutex>
#include <netinet/in.h>
#include <pipes/buffer.h>
#include <src/server/PrecomputedPuzzles.h>
#include <Definitions.h>
#include "VoiceServer.h"
#include "src/VirtualServer.h"


namespace ts::server {
    class VoiceServerSocket;
    class POWHandler {
        public:
            enum LowHandshakeState : uint8_t {
                COOKIE_GET,
                COOKIE_SET,
                PUZZLE_GET,
                PUZZLE_SET,
                PUZZLE_SOLVE,
                PUZZLE_RESET,
                COMPLETED,
                COMMAND_RESET = 127,
                UNSET = 0xFB
            };

            struct Client {
                std::shared_ptr<VoiceServerSocket> socket;
                sockaddr_storage address;
                udp::pktinfo_storage address_info;

                std::timed_mutex handle_lock;
                std::chrono::system_clock::time_point last_packet;
                LowHandshakeState state = LowHandshakeState::COOKIE_GET;

                uint8_t client_control_data[4]{0};
                uint8_t server_control_data[16]{0};
                uint8_t server_data[100];

                uint32_t client_version;

                std::shared_ptr<udp::Puzzle> rsa_challenge;
            };

            explicit POWHandler(VoiceServer* /* server */);

            void handle_datagram(const std::shared_ptr<VoiceServerSocket>& /* socket */, const sockaddr_storage& /* address */, msghdr& /* info */, const pipes::buffer_view& /* buffer */);
            void execute_tick();
        private:
            inline ServerId get_server_id() {
                return this->server->get_server()->getServerId();
            }
            VoiceServer* server;

            std::mutex pending_clients_lock;
            std::deque<std::shared_ptr<Client>> pending_clients;

            void delete_client(const std::shared_ptr<Client>& /* client */);

            void handle_cookie_get(const std::shared_ptr<Client>& /* client */, const pipes::buffer_view& /* buffer */);
            void handle_puzzle_get(const std::shared_ptr<Client>& /* client */, const pipes::buffer_view& /* buffer */);
            void handle_puzzle_solve(const std::shared_ptr<Client>& /* client */, const pipes::buffer_view& /* buffer */);
            std::shared_ptr<VoiceClient> register_verified_client(const std::shared_ptr<Client>& /* client */);

            void send_data(const std::shared_ptr<Client> &client /* client */, const pipes::buffer_view &buffer /* buffer */);
            void reset_client(const std::shared_ptr<Client> &client /* client */);
    };
}