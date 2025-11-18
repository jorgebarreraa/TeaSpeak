#pragma once

#include <Definitions.h>
#include <chrono>
#include <deque>
#include "./snapshot.h"

namespace ts::server::snapshots {
    struct client_entry {
        ClientDbId database_id;
        std::string unique_id;
        std::string nickname;
        std::string description;

        std::chrono::system_clock::time_point timestamp_created;
        std::chrono::system_clock::time_point timestamp_last_connected;
        size_t client_total_connections;
    };

    class client_parser : public parser<client_entry> {
        public:
            client_parser(type type_, version_t version, const command_parser& command) : parser{type_, version, command} {}

            bool parse(
                    std::string& /* error */,
                    client_entry& /* result */,
                    size_t& /* offset */) override;
    };

    class client_writer : public writer<client_entry> {
        public:
            client_writer(type type_, version_t version, command_builder& command) : writer{type_, version, command} {}

            bool write(std::string &, size_t &, const client_entry &) override;
    };
}