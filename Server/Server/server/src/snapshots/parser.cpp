//
// Created by WolverinDEV on 30/07/2020.
//

#include <Definitions.h>
#include <log/LogUtils.h>
#include <misc/base64.h>
#include <misc/digest.h>

#include <zstd.h>

#include "./snapshot.h"
#include "./server.h"
#include "./client.h"
#include "./channel.h"
#include "./music.h"
#include "./groups.h"
#include "./snapshot_data.h"

using namespace ts;
using namespace ts::server;

inline bool verify_hash(std::string &error, ServerId server_id, const command_parser &data, size_t hash_offset, const std::string_view& expected_hash) {
    if(!expected_hash.empty() && !data.has_switch("ignorehash")) {
        auto payload = data.payload_view(hash_offset);
        if(auto switches{payload.find(" -")}; switches != std::string::npos) {
            debugMessage(server_id, "Truncating the snapshot payload after index {} (First switch).", switches);
            payload = payload.substr(0, switches);
        }

        auto calculated_hash = base64::encode(digest::sha1(payload));

        /* debugMessage(server_id, "SHA1 for playload: {}", payload); */
        debugMessage(server_id, "Expected hash: {}, Calculated hash: {}", expected_hash, calculated_hash);
        if(expected_hash != calculated_hash) {
            error = "invalid snapshot hash.\nExpected: " + std::string{expected_hash} + "\nCalculated: " + calculated_hash;
            return false;
        }
    }
    return true;
}

bool snapshots::parse_snapshot(snapshot_data &result, std::string &error, ServerId server_id, const command_parser &data) {
    if(data.bulk(0).has_key("version")) {
        return snapshots::parse_snapshot_ts3(result, error, server_id, data);
    } else if(data.bulk(0).has_key("snapshot_version") || (data.bulk_count() > 1 && data.bulk(1).has_key("snapshot_version"))) {
        /* teaspeak snapshot */
        return snapshots::parse_snapshot_teaspeak(result, error, server_id, data);
    } else {
        /* old TS3 snapshot format */
        return snapshots::parse_snapshot_ts3(result, error, server_id, data);
    }
}

inline std::unique_ptr<char, void(*)(void*)> decompress_snapshot(std::string& error, size_t& decompressed_length, const std::string& data) {
    std::unique_ptr<char, void(*)(void*)> result{nullptr, ::free};

    auto uncompressed_length = ZSTD_getFrameContentSize(data.data(), data.length());
    if(uncompressed_length == ZSTD_CONTENTSIZE_UNKNOWN) {
        error = "unknown uncompressed size";
        return result;
    } else if(uncompressed_length == ZSTD_CONTENTSIZE_ERROR) {
        error = "failed to calculate uncompressed size";
        return result;
    } else if(ZSTD_isError(uncompressed_length)) {
        error = "failed to calculate uncompressed size: " + std::string{ZSTD_getErrorName(uncompressed_length)};
        return result;
    } else if(uncompressed_length > 512 * 1024 * 1028) {
        error = "uncompressed size exceeds half a gigabyte!";
        return result;
    }

    result.reset((char*) ::malloc(uncompressed_length));
    if(!result) {
        error = "failed to allocate decompress buffer";
        return result;
    }

    decompressed_length = ZSTD_decompress(&*result, uncompressed_length, data.data(), data.length());
    if(ZSTD_isError(decompressed_length)) {
        error = "decompress error occurred: " + std::string{ZSTD_getErrorName(decompressed_length)};
        result = nullptr;
        return result;
    }

    return result;
}

bool snapshots::parse_snapshot_teaspeak(snapshot_data &result, std::string &error, ts::ServerId server_id, const ts::command_parser &data) {
    snapshots::version_t version;
    if(data.bulk(0).has_key("snapshot_version")) {
        version = data.bulk(0).value_as<snapshots::version_t>("snapshot_version");
    } else {
        version = data.bulk(1).value_as<snapshots::version_t>("snapshot_version");
    }

    if(version < 1) {
        error = "snapshot version too old";
        return false;
    } else if(version == 2) {
        auto hash = data.bulk(0).value("hash");

        if(!verify_hash(error, server_id, data, 1, hash))
            return false;

        /* the actual snapshot begins at index 2 */
        return snapshots::parse_snapshot_raw(result, error, server_id, data, 2, type::TEASPEAK, version);
    } else if(version > 2) {
        auto compressed_data{base64::decode(data.value("data"))};

        size_t decompressed_length;
        auto decompressed = decompress_snapshot(error, decompressed_length, compressed_data);
        if(!decompressed) {
            return false;
        }

        std::string_view decompressed_data{&*decompressed, decompressed_length};
        ts::command_parser decompressed_parser{decompressed_data};
        if(!decompressed_parser.parse(false)) {
            error = "failed to parse snapshot payload";
            return false;
        }

        return snapshots::parse_snapshot_raw(result, error, server_id, decompressed_parser, 0, type::TEASPEAK, version);
    } else if(version > 3) {
        error = "snapshot version is too new";
        return false;
    } else {
        error = "invalid snapshot version";
        return false;
    }
}

bool snapshots::parse_snapshot_ts3(snapshot_data &result, std::string &error, ts::ServerId server_id, const ts::command_parser &data) {
    snapshots::version_t version{0};
    if(data.bulk(0).has_key("version"))
        version = data.bulk(0).value_as<snapshots::version_t>("version");

    if(data.bulk(0).has_key("salt")) {
        error = "TeaSpeak dosn't support encrypted snapshots yet";
        return false;
    }

    if(version == 0) {
        auto hash = data.bulk(0).value("hash");
        if(!verify_hash(error, server_id, data, 1, hash))
            return false;

        return snapshots::parse_snapshot_raw(result, error, server_id, data, 1, type::TEAMSPEAK, version);
    } else if(version == 1) {
        error = "version 1 is an invalid version";
        return false;
    } else if(version >= 2 && version <= 3) {
        std::string compressed_data;
        if(version == 2) {
            compressed_data = base64::decode(data.payload_view(1));
        } else {
            compressed_data = base64::decode(data.value("data"));
        }

        size_t decompressed_length;
        auto decompressed = decompress_snapshot(error, decompressed_length, compressed_data);
        if(!decompressed) {
            return false;
        }

        std::string_view decompressed_data{&*decompressed, decompressed_length};
        ts::command_parser decompressed_parser{decompressed_data};
        if(!decompressed_parser.parse(false)) {
            error = "failed to parse snapshot payload";
            return false;
        }

        return snapshots::parse_snapshot_raw(result, error, server_id, decompressed_parser, 0, type::TEAMSPEAK, version);
    } else {
        error = "snapshots with version 1-3 are currently supported";
        return false;
    }
}

bool snapshots::parse_snapshot_raw(snapshot_data &snapshot_data, std::string &error, ServerId server_id, const command_parser &command,
                                   size_t command_offset, snapshots::type type, snapshots::version_t version) {
    /* all snapshots start with the virtual server properties */
    {
        snapshots::server_parser parser{type, version, command};
        if(!parser.parse(error, snapshot_data.parsed_server, command_offset))
            return false;
    }

    /* afterwards all channels */
    {
        snapshots::channel_parser parser{type, version, command};
        auto data = command.bulk(command_offset);
        if(!data.has_key("begin_channels")) {
            error = "missing begin channels token at " + std::to_string(data.command_character_index());
            return false;
        }

        auto end_bulk = command.next_bulk_containing("end_channels", command_offset);
        if(!end_bulk.has_value()) {
            error = "missing end channels token";
            return false;
        } else if(*end_bulk == command_offset) {
            error = "snapshot contains no channels";
            return false;
        }
        snapshot_data.parsed_channels.reserve(*end_bulk - command_offset);

        while(!command.bulk(command_offset).has_key("end_channels")) {
            auto& entry = snapshot_data.parsed_channels.emplace_back();
            if(!parser.parse(error, entry, command_offset))
                return false;
        }
        command_offset++; /* the "end_channels" token */
    }

    /* after channels all clients */
    {
        snapshots::client_parser parser{type, version, command};
        auto data = command.bulk(command_offset);
        if(!data.has_key("begin_clients")) {
            error = "missing begin clients token at " + std::to_string(data.command_character_index());
            return false;
        }

        auto end_bulk = command.next_bulk_containing("end_clients", command_offset);
        if(!end_bulk.has_value()) {
            error = "missing end clients token";
            return false;
        }
        snapshot_data.parsed_channels.reserve(*end_bulk - command_offset);

        while(!command.bulk(command_offset).has_key("end_clients")) {
            auto& entry = snapshot_data.parsed_clients.emplace_back();
            if(!parser.parse(error, entry, command_offset))
                return false;
        }
        command_offset++; /* the "end_clients" token */
    }

    if(type == snapshots::type::TEASPEAK && version >= 2) {

        bool music_bots_parsed{false},
                playlist_parsed{false},
                playlist_songs_parsed{false};

        if(!command.bulk(command_offset).has_key("begin_music")) {
            error = "missing begin music key";
            return false;
        }

        while(!command.bulk(command_offset).has_key("end_music")) {
            if(command.bulk(command_offset).has_key("begin_bots")) {
                if(std::exchange(music_bots_parsed, true)) {
                    error = "duplicated music bot list";
                    return false;
                }

                snapshots::music_bot_parser parser{type, version, command};
                while(!command.bulk(command_offset).has_key("end_bots")) {
                    auto& bot = snapshot_data.music_bots.emplace_back();

                    if(!parser.parse(error, bot, command_offset))
                        return false;

                    command_offset++;
                }
            } else if(command.bulk(command_offset).has_key("begin_playlist")) {
                if(std::exchange(playlist_parsed, true)) {
                    error = "duplicated playlist list";
                    return false;
                }

                snapshots::playlist_parser parser{type, version, command};
                while(!command.bulk(command_offset).has_key("end_playlist")) {
                    auto& playlist = snapshot_data.playlists.emplace_back();

                    if(!parser.parse(error, playlist, command_offset))
                        return false;

                    command_offset++;
                }
            } else if(command.bulk(command_offset).has_key("begin_playlist_songs")) {
                if(std::exchange(playlist_songs_parsed, true)) {
                    error = "duplicated playlist songs list";
                    return false;
                }

                snapshots::playlist_song_parser parser{type, version, command};

                PlaylistId current_playlist_id{0};
                while(!command.bulk(command_offset).has_key("end_playlist_songs")) {
                    if(snapshot_data.playlist_songs.empty() || command.bulk(command_offset).has_key("song_playlist_id"))
                        current_playlist_id = command.bulk(command_offset).value_as<PlaylistId>("song_playlist_id");

                    auto& playlist_song = snapshot_data.playlist_songs.emplace_back();
                    playlist_song.playlist_id = current_playlist_id;

                    if(!parser.parse(error, playlist_song, command_offset))
                        return false;

                    command_offset++;
                }
            } else {
                __asm__("nop");
            }
            command_offset++;
        }
        command_offset++;

        /* check if everything has been parsed */
        if(!music_bots_parsed) {
            error = "missing music bots";
            return false;
        }

        if(!playlist_parsed) {
            error = "missing playlists";
            return false;
        }

        if(!playlist_songs_parsed) {
            error = "missing playlist songs";
            return false;
        }
    }

    /* permissions */
    {
        bool server_groups_parsed{false},
                channel_groups_parsed{false},
                client_permissions_parsed{false},
                channel_permissions_parsed{false},
                client_channel_permissions_parsed{false};

        if(!command.bulk(command_offset++).has_key("begin_permissions")) {
            error = "missing begin permissions key";
            return false;
        }

        snapshots::relation_parser relation_parser{type, version, command};
        while(!command.bulk(command_offset).has_key("end_permissions")) {
            if(command.bulk(command_offset).has_key("server_groups")) {
                if(std::exchange(server_groups_parsed, true)) {
                    error = "duplicated server group list";
                    return false;
                }

                snapshots::group_parser group_parser{type, version, command, "id", permission::teamspeak::GroupType::SERVER};

                /* parse all groups */
                while(!command.bulk(command_offset).has_key("end_groups")){
                    auto& group = snapshot_data.parsed_server_groups.emplace_back();
                    if(!group_parser.parse(error, group, command_offset)) /* will consume the end group token */
                        return false;
                    command_offset++; /* for the "end_group" token */
                }
                command_offset++; /* for the "end_groups" token */

                /* parse relations */
                if(!relation_parser.parse(error, snapshot_data.parsed_server_group_relations, command_offset))
                    return false;
                command_offset++; /* for the "end_relations" token */

                if(snapshot_data.parsed_server_group_relations.size() > 1) {
                    error = "all group relations should be for channel id 0 but received more than one different channel.";
                    return false;
                } else if(!snapshot_data.parsed_server_group_relations.empty() && snapshot_data.parsed_server_group_relations.begin()->first != 0) {
                    error = "all group relations should be for channel id 0 but received it for " + std::to_string(snapshot_data.parsed_server_group_relations.begin()->first);
                    return false;
                }
            } else if(command.bulk(command_offset).has_key("channel_groups")) {
                if(std::exchange(channel_groups_parsed, true)) {
                    error = "duplicated channel group list";
                    return false;
                }

                snapshots::group_parser group_parser{type, version, command, "id", permission::teamspeak::GroupType::CHANNEL};

                /* parse all groups */
                while(!command.bulk(command_offset).has_key("end_groups")){
                    auto& group = snapshot_data.parsed_channel_groups.emplace_back();
                    if(!group_parser.parse(error, group, command_offset))
                        return false;
                    command_offset++; /* for the "end_group" token */
                }
                command_offset++; /* for the "end_groups" token */

                /* parse relations */
                if(!relation_parser.parse(error, snapshot_data.parsed_channel_group_relations, command_offset))
                    return false;
                command_offset++; /* for the "end_relations" token */
            } else if(command.bulk(command_offset).has_key("client_flat")) {
                /* client permissions */
                if(std::exchange(client_permissions_parsed, true)) {
                    error = "duplicated client permissions list";
                    return false;
                }

                snapshots::flat_parser flat_parser{type, version, command, permission::teamspeak::GroupType::CLIENT};
                if(!flat_parser.parse(error, snapshot_data.client_permissions, command_offset))
                    return false;
                command_offset++; /* for the "end_flat" token */
            } else if(command.bulk(command_offset).has_key("channel_flat")) {
                /* channel permissions */
                if(std::exchange(channel_permissions_parsed, true)) {
                    error = "duplicated channel permissions list";
                    return false;
                }

                snapshots::flat_parser flat_parser{type, version, command, permission::teamspeak::GroupType::CHANNEL};
                if(!flat_parser.parse(error, snapshot_data.channel_permissions, command_offset))
                    return false;

                command_offset++; /* for the "end_flat" token */
            } else if(command.bulk(command_offset).has_key("channel_client_flat")) {
                /* channel client permissions */
                if(std::exchange(client_channel_permissions_parsed, true)) {
                    error = "duplicated client channel permissions list";
                    return false;
                }

                snapshots::flat_parser flat_parser{type, version, command, permission::teamspeak::GroupType::CLIENT};
                if(!flat_parser.parse(error, snapshot_data.client_channel_permissions, command_offset))
                    return false;

                command_offset++; /* for the "end_flat" token */
            } else {
                command_offset++;
            }
        }

        /* check if everything has been parsed */
        {
            if(!server_groups_parsed) {
                error = "missing server groups";
                return false;
            }

            if(!channel_groups_parsed) {
                error = "missing channel groups";
                return false;
            }

            if(!client_permissions_parsed) {
                error = "missing client permissions";
                return false;
            }

            if(!channel_permissions_parsed) {
                error = "missing channel permissions";
                return false;
            }

            if(!client_channel_permissions_parsed) {
                error = "missing client channel permissions";
                return false;
            }
        }
    }

    debugMessage(server_id, "Parsed snapshot containing {} channels, {} server groups, {} channel groups, {} clients, {} music bots and {} playlists.",
        snapshot_data.parsed_channels.size(),
        snapshot_data.parsed_server_groups.size(),
        snapshot_data.parsed_channel_groups.size(),
        snapshot_data.parsed_clients.size(),
        snapshot_data.music_bots.size(),
        snapshot_data.playlists.size()
    );

    return true;
}