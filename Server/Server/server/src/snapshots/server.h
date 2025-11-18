#pragma once

#include <Definitions.h>
#include <Properties.h>
#include <chrono>
#include <deque>
#include "./snapshot.h"

namespace ts::server::snapshots {
    struct server_entry {
        PropertyManager properties{};
    };

    class server_parser : public parser<server_entry> {
        public:
            server_parser(type type_, version_t version, const command_parser& command) : parser{type_, version, command} {}

            bool parse(
                    std::string & /* error */,
                    server_entry & /* result */,
                    size_t & /* offset */) override;
    };

    /*
    class server_writer : public writer<server_entry> {
        public:
            server_writer(type type_, version_t version, command_builder& command) : writer{type_, version, command} {}

            bool write(std::string &, size_t &, const server_entry &) override;
    };
     */
}