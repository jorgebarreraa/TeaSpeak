//
// Created by WolverinDEV on 11/04/2020.
//

#include "channel.h"

using namespace ts::server::snapshots;

bool channel_parser::parse(std::string &error, channel_entry &channel, size_t &offset) {
    auto data = this->command.bulk(offset++);
    channel.properties.register_property_type<property::ChannelProperties>();

    std::optional<ChannelId> channel_id{};
    std::optional<ChannelId> parent_channel_id{};

    size_t entry_index{0};
    std::string_view key{};
    std::string value{};
    while(data.next_entry(entry_index, key, value)) {
        if(key == "begin_channels")
            continue;
        else if(key == "channel_id") {
            char* end_ptr{nullptr};
            channel_id = strtoull(value.c_str(), &end_ptr, 10);
            if (*end_ptr) {
                error = "failed to parse channel id at character " + std::to_string(data.key_command_character_index(key) + key.length());
                return false;
            }
        } else if(key == "channel_pid") {
            char* end_ptr{nullptr};
            parent_channel_id = strtoull(value.c_str(), &end_ptr, 10);
            if (*end_ptr) {
                error = "failed to parse channel parent id at character " + std::to_string(data.key_command_character_index(key) + key.length());
                return false;
            }
        } else {
            const auto& property = property::find<property::ChannelProperties>(key);
            if(property.is_undefined()) {
                //TODO: Issue a warning
                continue;
            }

            //TODO: Validate value
            channel.properties[property] = value;
        }
    }

    if(!channel_id.has_value()) {
        error = "channel entry at character index " + std::to_string(data.command_character_index()) + " misses a channel id";
        return false;
    }

    if(!parent_channel_id.has_value()) {
        error = "channel entry at character index " + std::to_string(data.command_character_index()) + " misses a channel parent id";
        return false;
    }
    channel.properties[property::CHANNEL_ID] = *channel_id;
    channel.properties[property::CHANNEL_PID] = *parent_channel_id;
    return true;
}