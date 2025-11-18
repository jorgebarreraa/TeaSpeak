//
// Created by WolverinDEV on 30/07/2020.
//

#include "music.h"

using namespace ts::server::snapshots;

bool music_bot_parser::parse(std::string &error, music_bot_entry &entry, size_t &index) {
    auto data = this->command[index];

    entry.properties.register_property_type<property::ClientProperties>();

    size_t entry_index{0};
    std::string_view key{};
    std::string value{};
    while(data.next_entry(entry_index, key, value)) {
        if(key == "bot_id") {
            char* end_ptr{nullptr};
            entry.database_id = strtoull(value.c_str(), &end_ptr, 10);
            if (*end_ptr) {
                error = "failed to parse bot database id at character " + std::to_string(data.key_command_character_index(key) + key.length());
                return false;
            }
        } else if(key == "bot_unique_id") {
            entry.unique_id = value;
        } else if(key == "bot_owner_id") {
            char* end_ptr{nullptr};
            entry.bot_owner_id = strtoull(value.c_str(), &end_ptr, 10);
            if (*end_ptr) {
                error = "failed to parse bot owner database id at character " + std::to_string(data.key_command_character_index(key) + key.length());
                return false;
            }
        } else if(key == "end_bots") {
            continue;
        } else {
            const auto& property = property::find<property::ClientProperties>(key);
            if(property.is_undefined()) {
                //TODO: Issue a warning
                continue;
            }

            //TODO: Validate value
            entry.properties[property] = value;
        }
    }

    if(entry.database_id == 0) {
        error = "missing database id at entry " + std::to_string(data.command_character_index());
        return false;
    } else if(entry.unique_id.empty()) {
        error = "missing unique id at entry " + std::to_string(data.command_character_index());
        return false;
    }

    return true;
}

bool playlist_parser::parse(std::string &error, playlist_entry &entry, size_t &index) {
    auto data = this->command[index];

    entry.properties.register_property_type<property::PlaylistProperties>();

    size_t entry_index{0};
    std::string_view key{};
    std::string value{};
    while(data.next_entry(entry_index, key, value)) {
        if(key == "playlist_id") {
            char* end_ptr{nullptr};
            entry.playlist_id = strtoull(value.c_str(), &end_ptr, 10);
            if (*end_ptr) {
                error = "failed to parse playlist id at character " + std::to_string(data.key_command_character_index(key) + key.length());
                return false;
            }
        } else if(key == "end_playlist") {
            continue;
        } else {
            const auto& property = property::find<property::PlaylistProperties>(key);
            if(property.is_undefined()) {
                //TODO: Issue a warning
                continue;
            }

            //TODO: Validate value
            entry.properties[property] = value;
        }
    }

    if(entry.playlist_id == 0) {
        error = "missing playlist id at entry " + std::to_string(data.command_character_index());
        return false;
    }

    return true;
}

bool playlist_song_parser::parse(std::string &error, playlist_song_entry &entry, size_t &index) {
    auto data = this->command[index];

    entry.song_id = data.value_as<SongId>("song_id");
    entry.order_id = data.value_as<SongId>("song_order");
    entry.invoker_id = data.value_as<ClientDbId>("song_invoker");

    entry.url = data.value("song_url");
    entry.loader = data.value("song_url_loader");
    entry.loaded = data.value_as<bool>("song_loaded");

    entry.metadata = data.value("song_metadata");

    return true;
}