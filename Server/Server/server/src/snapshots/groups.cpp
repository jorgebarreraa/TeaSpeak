//
// Created by WolverinDEV on 11/04/2020.
//

#include "groups.h"

using namespace ts::server::snapshots;

bool group_parser::parse(std::string &error, group_entry &group, size_t &offset) {
    auto group_data = this->command.bulk(offset);
    bool key_found;

    {
        auto value_string = group_data.value(this->id_key, key_found);
        if(!key_found) {
            error = "missing id for group entry at character " + std::to_string(group_data.command_character_index());
            return false;
        }

        char* end_ptr{nullptr};
        group.group_id = strtoll(value_string.c_str(), &end_ptr, 10);
        if (*end_ptr) {
            error = "unparsable id for group entry at character " + std::to_string(group_data.key_command_character_index(this->id_key) + this->id_key.length());
            return false;
        }
    }

    group.name = group_data.value("name", key_found);
    if(!key_found) {
        error = "missing name for group entry at character " + std::to_string(group_data.command_character_index());
        return false;
    }

    return this->pparser.parse(error, group.permissions, offset);
}

bool relation_parser::parse(std::string &error, group_relations &result, size_t &offset) {
    auto relation_end = this->command.next_bulk_containing("end_relations", offset);
    if(!relation_end.has_value()) {
        error = "missing end relations token";
        return false;
    }

    bool key_found;
    while(offset < *relation_end) {
        ChannelId channel_id;
        auto begin_bulk = this->command.bulk(offset);
        if(begin_bulk.has_key("iid")) {
            channel_id = begin_bulk.value_as<ChannelId>("iid");
        } else {
            /* may be the case for the global assignments (iid may be not send in the first few generations) */
            channel_id = 0;
        }

        auto& relations = result[channel_id];
        auto next_iid = this->command.next_bulk_containing("iid", offset + 1);

        size_t end_index;
        if(next_iid.has_value() && *next_iid < relation_end) {
            end_index = *next_iid;
            relations.reserve(*next_iid - offset);
        } else {
            end_index = *relation_end;
            relations.reserve(*relation_end - offset);
        }

        while(offset < end_index) {
            auto relation_data = this->command.bulk(offset++);
            auto& relation = relations.emplace_back();

            {
                auto value_string = relation_data.value("cldbid", key_found);
                if(!key_found) {
                    error = "missing client id for group relation entry at character " + std::to_string(relation_data.command_character_index());
                    return false;
                }

                char* end_ptr{nullptr};
                relation.client_id = strtoll(value_string.c_str(), &end_ptr, 10);
                if (*end_ptr) {
                    error = "unparsable client id for group relation entry at character " + std::to_string(relation_data.key_command_character_index("cldbid") + 4);
                    return false;
                }
            }


            {
                auto value_string = relation_data.value("gid", key_found);
                if(!key_found) {
                    error = "missing group id for group relation entry at character " + std::to_string(relation_data.command_character_index());
                    return false;
                }

                char* end_ptr{nullptr};
                relation.group_id = strtoll(value_string.c_str(), &end_ptr, 10);
                if (*end_ptr) {
                    error = "unparsable group id for group relation entry at character " + std::to_string(relation_data.key_command_character_index("gid") + 3);
                    return false;
                }
            }
        }
    }
    return true;
}