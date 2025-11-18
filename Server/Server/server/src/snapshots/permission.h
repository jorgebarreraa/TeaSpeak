#pragma once

#include <PermissionManager.h>
#include <query/command3.h>
#include "./snapshot.h"

namespace ts::server::snapshots {
    struct permission_entry {
        std::shared_ptr<permission::PermissionTypeEntry> type{nullptr};

        permission::v2::PermissionFlaggedValue value{0, false};
        permission::v2::PermissionFlaggedValue granted{0, false};

        bool flag_skip{false};
        bool flag_negate{false};
    };

    struct permission_parser_options {
        permission::teamspeak::GroupType target_permission_type;
        std::vector<std::string> delimiter;
        bool ignore_delimiter_at_index_0;
    };

    class permission_parser : public parser<std::vector<permission_entry>> {
        public:
            permission_parser(type type_, version_t version, const command_parser& command, permission_parser_options /* options */);

            bool parse(
                    std::string& /* error */,
                    std::vector<permission_entry>& /* result */,
                    size_t& /* offset */) override;
        private:
            typedef bool(permission_parser::*parse_impl_t)(std::string &error, std::vector<permission_entry> &result, const ts::command_bulk &data);

            const permission_parser_options options;
            parse_impl_t parser_impl;

            bool parse_entry_teamspeak_v0(
                    std::string& /* error */,
                    std::vector<permission_entry>& /* result */,
                    const command_bulk& /* entry */
            );

            bool parse_entry_teaspeak_v1(
                    std::string& /* error */,
                    std::vector<permission_entry>& /* result */,
                    const command_bulk& /* entry */
            );
    };

    class permission_writer {
        public:
            permission_writer(type type_, version_t version, command_builder& command, permission::teamspeak::GroupType target_permission_type);

            bool write(
                    std::string& /* error */,
                    size_t& /* offset */,
                    const std::deque<permission_entry>& /* permissions */);

            bool write_entry(
                    std::string& /* error */,
                    size_t& /* offset */,
                    const permission_entry& /* permissions */);
        private:
            typedef bool(permission_writer::*write_impl_t)(std::string &error, size_t& offset,const permission_entry& /* permissions */);

            command_builder& command;
            const type type_;
            const version_t version_;
            const permission::teamspeak::GroupType target_permission_type_;
            write_impl_t write_impl;

            bool write_entry_teamspeak_v0(
                    std::string &error,
                    size_t& offset,
                    const permission_entry& /* permissions */
            );

            bool write_entry_teaspeak_v1(
                    std::string &error,
                    size_t& offset,
                    const permission_entry& /* permissions */
            );
    };

    struct permissions_flat_entry {
        uint64_t id1;
        uint64_t id2;

        std::vector<permission_entry> permissions;
    };

    class flat_parser : public parser<std::deque<permissions_flat_entry>> {
        public:
            flat_parser(type type_, version_t version, const command_parser& command, permission::teamspeak::GroupType target_permission_type)
            : parser{type_, version, command}, pparser{type_, version, command, {
                        target_permission_type,
                        {"id1", "id2", "end_flat"}, /* only id1 should be enough, because if id2 changes id1 will be set as well but we just wan't to get sure */
                        true
                }} {}

            bool parse(
                    std::string& /* error */,
                    std::deque<permissions_flat_entry>& /* result */,
                    size_t& /* offset */) override;

        private:
            permission_parser pparser;
    };
}