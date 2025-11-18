//
// Created by WolverinDEV on 30/07/2020.
//
#pragma once

#include "./snapshot.h"

namespace ts::server::snapshots {
    struct snapshot_data {
        snapshots::type type{snapshots::type::UNKNOWN};
        version_t version{-1};

        snapshots::server_entry parsed_server{};

        std::vector<snapshots::channel_entry> parsed_channels{};
        std::vector<snapshots::client_entry> parsed_clients{};

        std::vector<snapshots::group_entry> parsed_server_groups{};
        snapshots::group_relations parsed_server_group_relations{};

        std::vector<snapshots::group_entry> parsed_channel_groups{};
        snapshots::group_relations parsed_channel_group_relations{};

        std::deque<snapshots::permissions_flat_entry> client_permissions{};
        std::deque<snapshots::permissions_flat_entry> channel_permissions{};
        std::deque<snapshots::permissions_flat_entry> client_channel_permissions{};

        std::deque<snapshots::music_bot_entry> music_bots{};
        std::deque<snapshots::playlist_entry> playlists{};
        std::deque<snapshots::playlist_song_entry> playlist_songs{};
    };

    struct snapshot_mappings {
        ServerId new_server_id{};

        std::map<ClientDbId, ClientDbId> client_id{};
        std::map<ChannelId, ChannelId> channel_id{};
        std::map<ChannelId, ChannelId> channel_group_id{};
        std::map<ChannelId, ChannelId> server_group_id{};
        std::map<PlaylistId, PlaylistId> playlist_id{};
    };

    struct snapshot_deploy_timings {
        std::chrono::system_clock::time_point initial_timestamp{};
        std::chrono::system_clock::time_point decode_timestamp{}; /* end decode */
        std::chrono::system_clock::time_point parse_timestamp{}; /* end parse */
        std::chrono::system_clock::time_point deploy_timestamp{}; /* else deploy */
        std::chrono::system_clock::time_point startup_timestamp{}; /* end startup */
    };

    struct snapshot_deploy {
        snapshot_deploy_timings timings{};
        snapshot_mappings mappings{};
        snapshot_data data{};
    };
}
