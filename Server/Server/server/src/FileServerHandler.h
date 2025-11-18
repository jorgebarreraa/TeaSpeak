#pragma once

#include <string>
#include <files/FileServer.h>

#include "./InstanceHandler.h"

namespace ts::server::file {
    class FileServerHandler {
        public:
            explicit FileServerHandler(InstanceHandler*);

            bool initialize(std::string& /* error */);
            void finalize();
        private:
            InstanceHandler* instance_;

            void callback_transfer_registered(const std::shared_ptr<transfer::Transfer>&);
            void callback_transfer_started(const std::shared_ptr<transfer::Transfer>&);
            void callback_transfer_finished(const std::shared_ptr<transfer::Transfer>&);

            void callback_transfer_aborted(const std::shared_ptr<transfer::Transfer>&, const transfer::TransferStatistics&, const transfer::TransferError&);
            void callback_transfer_statistics(const std::shared_ptr<transfer::Transfer>&, const transfer::TransferStatistics&);
    };
}