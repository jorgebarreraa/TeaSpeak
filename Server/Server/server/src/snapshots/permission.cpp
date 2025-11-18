//
// Created by WolverinDEV on 11/04/2020.
//

#include "permission.h"

using namespace ts::server::snapshots;

permission_parser::permission_parser(ts::server::snapshots::type type_, ts::server::snapshots::version_t version,
                                     const ts::command_parser &command, permission_parser_options options) : parser{type_, version, command}, options{std::move(options)} {
    if(type_ == type::TEAMSPEAK) {
        this->parser_impl = &permission_parser::parse_entry_teamspeak_v0;
    } else if(type_ == type::TEASPEAK) {
        if(version >= 1) {
            this->parser_impl = &permission_parser::parse_entry_teaspeak_v1;
        } else {
            /* TeaSpeak has no snapshot version 0. 0 implies a TeamSpeak snapshot */
            assert(false);
        }
    } else {
        assert(false);
    }
}

bool permission_parser::parse(
        std::string &error,
        std::vector<permission_entry> &result,
        size_t &offset) {

    size_t end_offset{(size_t) -1};

    {
        size_t end_begin_offset{offset + (this->options.ignore_delimiter_at_index_0 ? 1 : 0)};
        for(const auto& token : this->options.delimiter) {
            auto index = this->command.next_bulk_containing(token, end_begin_offset);
            if(index.has_value() && *index < end_offset)
                end_offset = *index;
        }

        if(end_offset == (size_t) -1) {
            error = "missing end token";
            return false;
        }
    }

    if(end_offset == offset) {
        /* no entries at all */
        return true;
    }
    result.reserve((end_offset - offset) * 2); /* reserve some extra space because we might import permissions */

    assert(this->type_ == type::TEAMSPEAK || this->type_ == type::TEASPEAK);

    while(offset < end_offset) {
        if(!(this->*(this->parser_impl))(error, result, this->command[offset]))
            return false;

        offset++;
    }

    return true;
}

bool permission_parser::parse_entry_teamspeak_v0(std::string &error, std::vector<permission_entry> &result,
                                              const ts::command_bulk &data) {
    bool key_found;
    auto original_name = data.value("permid", key_found);
    if(!key_found) {
        error = "missing id for permission entry at character " + std::to_string(data.command_character_index());
        return false;
    }

    permission::PermissionValue value;
    {
        auto value_string = data.value("permvalue", key_found);
        if(!key_found) {
            error = "missing value for permission entry at character " + std::to_string(data.command_character_index());
            return false;
        }

        char* end_ptr{nullptr};
        value = strtoll(value_string.c_str(), &end_ptr, 10);
        if (*end_ptr) {
            error = "unparsable permission value at index " + std::to_string(data.key_command_character_index("permvalue") + 9);
            return false;
        }
    }

    auto flag_skip = data.value("permskip", key_found) == "1";
    if(!key_found) {
        error = "missing skip flag for permission entry at character " + std::to_string(data.command_character_index());
        return false;
    }

    auto flag_negate = data.value("permnegated", key_found) == "1";
    if(!key_found) {
        error = "missing skip flag for permission entry at character " + std::to_string(data.command_character_index());
        return false;
    }

    for(const auto& mapped : permission::teamspeak::map_key(original_name, this->options.target_permission_type)) {
        auto type = permission::resolvePermissionData(mapped);
        if(type == permission::PermissionTypeEntry::unknown)
            continue;

        permission_entry* entry{nullptr};
        for(auto& e : result) {
            if(e.type == type) {
                entry = &e;
                break;
            }
        }

        if(!entry) {
            entry = &result.emplace_back();
            entry->type = type;
        }

        if(mapped == type->grant_name) {
            entry->granted = {value, true};
        } else {
            entry->value = {value, true};
            entry->flag_negate = flag_negate;
            entry->flag_skip = flag_skip;
        }
    }
    return true;
}

bool permission_parser::parse_entry_teaspeak_v1(std::string &error, std::vector<permission_entry> &result,
                                                const ts::command_bulk &data) {
    bool key_found;
    auto permission_name = data.value("perm", key_found);
    if(!key_found) {
        error = "missing id for permission entry at character " + std::to_string(data.command_character_index());
        return false;
    }

    auto flag_skip = data.value("flag_skip", key_found) == "1";
    if(!key_found) {
        error = "missing skip flag for permission entry at character " + std::to_string(data.command_character_index());
        return false;
    }

    auto flag_negated = data.value("flag_negated", key_found) == "1";
    if(!key_found) {
        error = "missing negate flag for permission entry at character " + std::to_string(data.command_character_index());
        return false;
    }

    permission::PermissionValue value;
    {
        auto value_string = data.value("value", key_found);
        if(!key_found) {
            error = "missing value for permission entry at character " + std::to_string(data.command_character_index());
            return false;
        }

        char* end_ptr{nullptr};
        value = strtoll(value_string.c_str(), &end_ptr, 10);
        if (*end_ptr) {
            error = "unparsable permission value at index " + std::to_string(data.key_command_character_index("value") + 5);
            return false;
        }
    }

    permission::PermissionValue granted;
    {
        auto value_string = data.value("grant", key_found);
        if(!key_found) {
            error = "missing grant for permission entry at character " + std::to_string(data.command_character_index());
            return false;
        }

        char* end_ptr{nullptr};
        granted = strtoll(value_string.c_str(), &end_ptr, 10);
        if (*end_ptr) {
            error = "unparsable permission granted value at index " + std::to_string(data.key_command_character_index("grant") + 5);
            return false;
        }
    }

    auto type = permission::resolvePermissionData(permission_name);
    if(type == permission::PermissionTypeEntry::unknown)
        return true; /* we just drop unknown permissions */ //TODO: Log this drop

    auto& entry = result.emplace_back();
    entry.type = type;
    entry.flag_skip = flag_skip;
    entry.flag_negate = flag_negated;
    entry.value = {value, value != permNotGranted};
    entry.granted = {granted, granted != permNotGranted};
    return true;
}

permission_writer::permission_writer(type type_, version_t version,
                                     ts::command_builder &command, permission::teamspeak::GroupType target_permission_type) : command{command}, type_{type_}, version_{version}, target_permission_type_{target_permission_type} {
    if(type_ == type::TEAMSPEAK) {
        this->write_impl = &permission_writer::write_entry_teamspeak_v0;
    } else if(type_ == type::TEASPEAK) {
        if(version >= 1) {
            this->write_impl = &permission_writer::write_entry_teaspeak_v1;
        } else {
            /* TeaSpeak has no snapshot version 0. 0 implies a TeamSpeak snapshot */
            assert(false);
        }
    } else {
        assert(false);
    }
}

bool permission_writer::write(std::string &error, size_t &offset, const std::deque<permission_entry> &entries) {
    this->command.reserve_bulks(entries.size() * 2);
    for(auto& entry : entries)
        if(!this->write_entry(error, offset, entry))
            return false;
    return true;
}

bool permission_writer::write_entry(std::string &error, size_t &offset, const ts::server::snapshots::permission_entry &entry) {
    return (this->*(this->write_impl))(error, offset, entry);
}

bool permission_writer::write_entry_teamspeak_v0(std::string &error, size_t& offset,
                                                 const ts::server::snapshots::permission_entry &entry) {
    if(entry.value.has_value) {
        for(const auto& name : permission::teamspeak::unmap_key(entry.type->name, this->target_permission_type_)) {
            auto bulk = this->command.bulk(offset++);
            bulk.put_unchecked("permid", name);
            bulk.put_unchecked("permvalue", entry.value.value);
            bulk.put_unchecked("permskip", entry.flag_skip);
            bulk.put_unchecked("permnegated", entry.flag_negate);
        }
    }

    if(entry.granted.has_value) {
        for(const auto& name : permission::teamspeak::unmap_key(entry.type->grant_name, this->target_permission_type_)) {
            auto bulk = this->command.bulk(offset++);
            bulk.put_unchecked("permid", name);
            bulk.put_unchecked("permvalue", entry.granted.value);
            bulk.put_unchecked("permskip", "0");
            bulk.put_unchecked("permnegated", "0");
        }
    }
    return true;
}

bool permission_writer::write_entry_teaspeak_v1(std::string &error, size_t &offset,
                                                const ts::server::snapshots::permission_entry &entry) {
    if(!entry.value.has_value && !entry.granted.has_value)
        return true; /* should not happen, but we skip that here */

    auto bulk = this->command.bulk(offset++);
    bulk.put_unchecked("perm", entry.type->name);
    bulk.put_unchecked("value", entry.value.has_value ? entry.value.value : permNotGranted);
    bulk.put_unchecked("grant", entry.granted.has_value ? entry.granted.value : permNotGranted);
    bulk.put_unchecked("flag_skip", entry.flag_skip);
    bulk.put_unchecked("flag_negated", entry.flag_negate);
    return true;
}

bool flat_parser::parse(std::string &error, std::deque<permissions_flat_entry> &result, size_t &offset) {
    auto flat_end = this->command.next_bulk_containing("end_flat", offset);
    if(!flat_end.has_value()) {
        error = "missing flat end for " + std::to_string(this->command.bulk(offset).command_character_index());
        return false;
    }

    bool key_found;
    while(offset < *flat_end) {
        auto flat_data = this->command.bulk(offset);
        auto& flat_entry = result.emplace_back();

        /* id1 */
        {
            auto value_string = flat_data.value("id1", key_found);
            if(!key_found) {
                error = "missing id1 for flat entry at character " + std::to_string(flat_data.command_character_index());
                return false;
            }

            char* end_ptr{nullptr};
            flat_entry.id1 = strtoll(value_string.c_str(), &end_ptr, 10);
            if (*end_ptr) {
                error = "unparsable id1 for flat entry at character " + std::to_string(flat_data.key_command_character_index("id1") + 3);
                return false;
            }
        }

        /* id2 */
        {
            auto value_string = flat_data.value("id2", key_found);
            if(!key_found) {
                error = "missing id2 for flat entry at character " + std::to_string(flat_data.command_character_index());
                return false;
            }

            char* end_ptr{nullptr};
            flat_entry.id2 = strtoll(value_string.c_str(), &end_ptr, 10);
            if (*end_ptr) {
                error = "unparsable id2 for flat entry at character " + std::to_string(flat_data.key_command_character_index("id2") + 3);
                return false;
            }
        }

        if(!this->pparser.parse(error, flat_entry.permissions, offset))
            return false;
    }
    return true;
}