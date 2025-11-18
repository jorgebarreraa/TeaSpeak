#pragma once

#include "ConnectedClient.h"

namespace ts::server {
    class VirtualServer;
    class InternalClient : public ConnectedClient {
        public:
            InternalClient(sql::SqlManager*, const std::shared_ptr<VirtualServer>&, std::string, bool);
            ~InternalClient();

            void initialize_weak_reference(const std::shared_ptr<ConnectedClient> &client) override {
                ConnectedClient::initialize_weak_reference(client);
            }

            void sendCommand(const ts::Command &command, bool low) override;
            void sendCommand(const ts::command_builder &command, bool low) override;
            bool close_connection(const std::chrono::system_clock::time_point& timeout = std::chrono::system_clock::time_point()) override;
            bool disconnect(const std::string &reason) override;
        protected:
            void tick_server(const std::chrono::system_clock::time_point &time) override;
    };
}