#pragma once

#include <Definitions.h>
#include <chrono>
#include <deque>
#include <utility>
#include "./snapshot.h"
#include "./permission.h"

namespace ts::server::snapshots {
    struct group_entry {
        GroupId group_id;
        std::string name;

        std::vector<permission_entry> permissions{};
    };

    struct group_relation {
        ClientDbId client_id;
        GroupId group_id;
    };

    typedef std::map<ChannelId, std::vector<group_relation>> group_relations;

    class group_parser : public parser<group_entry> {
        public:
            group_parser(type type_, version_t version, const command_parser& command, std::string id_key, permission::teamspeak::GroupType target_permission_type)
                : parser{type_, version, command}, id_key{std::move(id_key)}, pparser{type_, version, command, {
                    target_permission_type,
                    {"end_group"},
                    false
                }} {}

            bool parse(
                    std::string& /* error */,
                    group_entry& /* result */,
                    size_t& /* offset */) override;

        private:
            std::string id_key{};

            permission_parser pparser;
    };

    class relation_parser : public parser<group_relations> {
        public:
            relation_parser(type type_, version_t version, const command_parser& command) : parser{type_, version, command} {}

            bool parse(
                    std::string& /* error */,
                    group_relations& /* result */,
                    size_t& /* offset */) override;
    };
}