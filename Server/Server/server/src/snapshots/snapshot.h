#pragma once

#include <cstdint>
#include <Definitions.h>
#include <query/command3.h>

namespace ts::server::snapshots {
    enum struct type {
        TEAMSPEAK,
        TEASPEAK,

        UNKNOWN
    };

    typedef int32_t version_t;
    constexpr version_t unknown_version{-1};

    template <typename result_t>
    class parser {
        public:
            parser(type type_, version_t version, const command_parser& command) :
                command{command}, type_{type_}, version_{version} {}

            virtual bool parse(
                    std::string& /* error */,
                    result_t& /* result */,
                    size_t& /* offset */) = 0;
        protected:
            const command_parser& command;
            const type type_;
            const version_t version_;
    };

    template <typename entry_t>
    class writer {
        public:
            writer(type type_, version_t version, command_builder& command) :
                    command{command}, type_{type_}, version_{version} {}

            virtual bool write(
                    std::string& /* error */,
                    size_t& /* offset */,
                    const entry_t& /* entry */) = 0;
        protected:
            command_builder& command;
            const type type_;
            const version_t version_;
    };

    struct snapshot_data;

    extern bool parse_snapshot(snapshot_data& /* result */, std::string& /* error */, ServerId /* target server id */, const ts::command_parser& /* command */);

    extern bool parse_snapshot_ts3(snapshot_data& /* result */, std::string& /* error */, ServerId /* target server id */, const command_parser& /* source */);
    extern bool parse_snapshot_teaspeak(snapshot_data& /* result */, std::string& /* error */, ServerId /* target server id */, const command_parser& /* source */);

    extern bool parse_snapshot_raw(snapshot_data& /* result */, std::string& /* error */, ServerId /* target server id */, const command_parser& /* source */, size_t /* offset */, snapshots::type /* type */, snapshots::version_t /* version */);

}