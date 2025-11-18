#pragma once

#include <Definitions.h>
#include <Properties.h>
#include <chrono>
#include <deque>
#include "./snapshot.h"

namespace ts::server::snapshots {
    struct channel_entry {
        PropertyManager properties{};
    };

    class channel_parser : public parser<channel_entry> {
        public:
            channel_parser(type type_, version_t version, const command_parser& command) : parser{type_, version, command} {}

            bool parse(
                    std::string& /* error */,
                    channel_entry& /* result */,
                    size_t& /* offset */) override;
    };
}