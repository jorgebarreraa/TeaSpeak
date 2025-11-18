#pragma once

#include <Definitions.h>
#include <chrono>
#include <deque>
#include <Properties.h>
#include "./snapshot.h"

namespace ts::server::snapshots {
    struct music_bot_entry {
        ClientDbId database_id{0};

        std::string unique_id{};
        ClientDbId bot_owner_id{};

        PropertyManager properties{};
    };

    struct playlist_entry {
        PlaylistId playlist_id{0};
        PropertyManager properties{};
    };

    struct playlist_song_entry {
        PlaylistId playlist_id{0}; /* Attention: This will not be written into the snapshot directly! */
        SongId song_id{0};
        SongId order_id{0};

        ClientDbId invoker_id{0};

        std::string url{};
        std::string loader{};
        bool loaded{false};

        std::string metadata{};
    };

    class music_bot_parser : public parser<music_bot_entry> {
        public:
            music_bot_parser(type type_, version_t version, const command_parser &command) : parser{type_, version,
                                                                                                 command} {}

            bool parse(
                    std::string & /* error */,
                    music_bot_entry & /* result */,
                    size_t & /* offset */) override;
    };

    class playlist_parser : public parser<playlist_entry> {
        public:
            playlist_parser(type type_, version_t version, const command_parser &command) : parser{type_, version,
                                                                                                    command} {}

            bool parse(
                    std::string & /* error */,
                    playlist_entry & /* result */,
                    size_t & /* offset */) override;
    };

    class playlist_song_parser : public parser<playlist_song_entry> {
        public:
            playlist_song_parser(type type_, version_t version, const command_parser &command) : parser{type_, version,
                                                                                                   command} {}

            bool parse(
                    std::string & /* error */,
                    playlist_song_entry & /* result */,
                    size_t & /* offset */) override;
    };
}