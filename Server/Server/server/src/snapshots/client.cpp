//
// Created by WolverinDEV on 11/04/2020.
//

#include "client.h"

using namespace ts::server::snapshots;

bool client_parser::parse(std::string &error, client_entry &client, size_t &offset) {
    bool key_found;
    auto data = this->command.bulk(offset++);

    {
        auto value_string = data.value("client_id", key_found);
        if(!key_found) {
            error = "missing id for client entry at character " + std::to_string(data.command_character_index());
            return false;
        }

        char* end_ptr{nullptr};
        client.database_id = strtoll(value_string.c_str(), &end_ptr, 10);
        if (*end_ptr) {
            error = "unparsable id for client entry at character " + std::to_string(data.key_command_character_index("client_id") + 9);
            return false;
        }
    }

    {
        auto value_string = data.value("client_created", key_found);
        if(!key_found) {
            error = "missing created timestamp for client entry at character " + std::to_string(data.command_character_index());
            return false;
        }

        char* end_ptr{nullptr};
        auto value = strtoll(value_string.c_str(), &end_ptr, 10);
        if (*end_ptr) {
            error = "unparsable created timestamp for client entry at character " + std::to_string(data.key_command_character_index("client_created") + 14);
            return false;
        }
        client.timestamp_created = std::chrono::system_clock::time_point{} + std::chrono::seconds{value};
    }

    /* optional */
    {
        auto value_string = data.value("client_lastconnected", key_found);
        if(key_found) {
            char* end_ptr{nullptr};
            auto value = strtoll(value_string.c_str(), &end_ptr, 10);
            if (*end_ptr) {
                error = "unparsable last connected timestamp for client entry at character " + std::to_string(data.key_command_character_index("client_lastconnected") + 20);
                return false;
            }
            client.timestamp_last_connected = std::chrono::system_clock::time_point{} + std::chrono::seconds{value};
        } else {
            client.timestamp_last_connected = std::chrono::system_clock::time_point{};
        }
    }

    /* optional */
    {
        auto value_string = data.value("client_totalconnections", key_found);
        if(key_found) {
            char* end_ptr{nullptr};
            client.client_total_connections = strtoll(value_string.c_str(), &end_ptr, 10);
            if (*end_ptr) {
                error = "unparsable total connection count for client entry at character " + std::to_string(data.key_command_character_index("client_totalconnections") + 23);
                return false;
            }
        } else {
            client.client_total_connections = 0;
        }
    }

    client.unique_id = data.value("client_unique_id", key_found);
    if(!key_found) {
        error = "missing unique id for client entry at character " + std::to_string(data.command_character_index());
        return false;
    }

    client.nickname = data.value("client_nickname", key_found);
    if(!key_found) {
        error = "missing nickname for client entry at character " + std::to_string(data.command_character_index());
        return false;
    }

    client.description = data.value("client_description", key_found);
    if(!key_found) {
        error = "missing description for client entry at character " + std::to_string(data.command_character_index());
        return false;
    }

    return true;
}

bool client_writer::write(std::string &error, size_t &offset, const client_entry &client) {
    auto data = this->command.bulk(offset++);
    data.put_unchecked("client_id", client.database_id);
    data.put_unchecked("client_unique_id", client.unique_id);
    data.put_unchecked("client_nickname", client.nickname);
    data.put_unchecked("client_description", client.description);
    data.put_unchecked("client_created", std::chrono::floor<std::chrono::seconds>(client.timestamp_created.time_since_epoch()).count());
    data.put_unchecked("client_lastconnected", std::chrono::floor<std::chrono::seconds>(client.timestamp_last_connected.time_since_epoch()).count());
    data.put_unchecked("client_totalconnections", client.client_total_connections);
    if(this->type_ == type::TEAMSPEAK)
        data.put_unchecked("client_unread_messages", "0");
    return true;
}