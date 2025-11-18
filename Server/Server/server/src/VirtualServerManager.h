#pragma once

#include <deque>
#include <EventLoop.h>
#include "src/server/PrecomputedPuzzles.h"
#include "VirtualServer.h"
#include <query/command3.h>
#include "snapshots/snapshot.h"

namespace ts::server {
    class InstanceHandler;

    class VirtualServerManager {
        public:
            enum State {
                STOPPED,
                STARTING,
                STARTED,
                STOPPING
            };

            enum struct SnapshotDeployResult {
                SUCCESS,

                REACHED_SOFTWARE_SERVER_LIMIT,
                REACHED_CONFIG_SERVER_LIMIT,
                REACHED_SERVER_ID_LIMIT,

                CUSTOM_ERROR /* error message set */
            };

            explicit VirtualServerManager(InstanceHandler*);
            ~VirtualServerManager();

            bool initialize(bool execute_autostart = true);

            std::shared_ptr<VirtualServer> create_server(std::string hosts, uint16_t port);
            bool deleteServer(std::shared_ptr<VirtualServer>);

            std::shared_ptr<VirtualServer> findServerById(ServerId);
            std::shared_ptr<VirtualServer> findServerByPort(uint16_t);
            uint16_t next_available_port(const std::string& /* host string */);
            ServerId next_available_server_id(bool& /* success */);

            std::deque<std::shared_ptr<VirtualServer>> serverInstances(){
                threads::MutexLock l(this->instanceLock);
                return instances;
            }

            ServerSlotUsageReport instanceSlotUsageReport();
            size_t runningServers();

            void executeAutostart();
            void shutdownAll(const std::string&);

            //Don't use shared_ptr references to keep sure that they be hold in memory
            bool createServerSnapshot(Command &cmd, std::shared_ptr<VirtualServer> server, int version, std::string &error);
            SnapshotDeployResult deploy_snapshot(std::string& /* error */, std::shared_ptr<VirtualServer>& /* target server */, const command_parser& /* source */);

            udp::PuzzleManager* rsaPuzzles() { return this->puzzles; }

            /* This must be recursive */
            threads::Mutex server_create_lock;

            State getState() { return this->state; }
        private:
            State state = State::STOPPED;
            InstanceHandler* handle;
            threads::Mutex instanceLock;
            std::deque<std::shared_ptr<VirtualServer>> instances;
            udp::PuzzleManager* puzzles{nullptr};

            struct {
                std::thread executor{};
                std::condition_variable condition;
                std::mutex lock;
            } acknowledge;

            void delete_server_in_db(ServerId /* server id */, bool /* data only */);
            void change_server_id_in_db(ServerId /* old id */, ServerId /* new id */);

            bool try_deploy_snapshot(std::string& /* error */, ServerId /* target server id */, ServerId /* logging server id */, const command_parser& /* source */);
    };
}