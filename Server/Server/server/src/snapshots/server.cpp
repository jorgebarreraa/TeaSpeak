//
// Created by WolverinDEV on 11/04/2020.
//

#include "server.h"

using namespace ts::server::snapshots;

bool server_parser::parse(std::string &error, server_entry &result, size_t &offset) {
    auto data = this->command.bulk(offset++);
    if(!data.has_key("end_virtualserver")) {
        error = "missing virtual server end token at character " + std::to_string(data.command_character_index());
        return false;
    }

    result.properties.register_property_type<property::VirtualServerProperties>();

    size_t entry_index{0};
    std::string_view key{};
    std::string value{};
    while(data.next_entry(entry_index, key, value)) {
        if(key == "end_virtualserver" ||
        key == property::describe(property::VIRTUALSERVER_PORT).name ||
        key == property::describe(property::VIRTUALSERVER_HOST).name ||
        key == property::describe(property::VIRTUALSERVER_WEB_PORT).name ||
        key == property::describe(property::VIRTUALSERVER_WEB_HOST).name ||
        key == property::describe(property::VIRTUALSERVER_VERSION).name ||
        key == property::describe(property::VIRTUALSERVER_PLATFORM).name)
            continue;

        const auto& property = property::find<property::VirtualServerProperties>(key);
        if(property.is_undefined()) {
            //TODO: Issue a warning
            continue;
        }

        //TODO: Validate value?
        result.properties[property] = value;
    }

    return true;
}