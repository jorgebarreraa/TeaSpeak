#pragma once

#include <files/FileServer.h>
#include <deque>
#include <utility>
#include <thread>
#include <shared_mutex>
#include <sys/socket.h>
#include <pipes/ws.h>
#include <pipes/ssl.h>
#include <misc/net.h>
#include <misc/spin_mutex.h>
#include <random>
#include <misc/memtracker.h>
#include "./NetTools.h"

namespace ts::server::file {
    namespace filesystem { class LocalFileSystem; }
    namespace transfer { class LocalFileTransfer; }

    class LocalVirtualFileServer : public VirtualFileServer {
        public:
            explicit LocalVirtualFileServer(ServerId server_id, std::string unique_id) : VirtualFileServer{server_id, std::move(unique_id)} {}

            void max_networking_upload_bandwidth(int64_t value) override;
            void max_networking_download_bandwidth(int64_t value) override;

            networking::NetworkThrottle upload_throttle{};
            networking::NetworkThrottle download_throttle{};
    };

    class LocalFileProvider : public AbstractFileServer {
        public:
            LocalFileProvider();
            virtual ~LocalFileProvider();

            [[nodiscard]] bool initialize(std::string& /* error */);
            void finalize();

            [[nodiscard]] std::string file_base_path() const override;

            filesystem::AbstractProvider &file_system() override;
            transfer::AbstractProvider &file_transfer() override;


            std::shared_ptr<VirtualFileServer> register_server(ServerId /* server id */) override;
            void unregister_server(ServerId /* server id */, bool /* delete server */) override;
        private:
            filesystem::LocalFileSystem* file_system_;
            transfer::LocalFileTransfer* file_transfer_;
    };
}