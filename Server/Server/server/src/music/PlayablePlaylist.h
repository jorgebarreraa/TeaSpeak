#pragma once

#include "../client/music/Song.h"
#include "MusicPlaylist.h"

namespace ts {
    namespace server {
        class ConnectedClient;
        class MusicClient;
    }

    namespace music {
        class MusicBotManager;

        class PlayablePlaylist : public Playlist {
                friend class server::MusicClient;
            public:
                struct ReplayMode {
                    enum value {
                        /* 0 = normal | 1 = loop list | 2 = loop entry | 3 = shuffle */
                        LINEAR = 0,
                        LINEAR_LOOPED,
                        SINGLE_LOOPED,
                        SHUFFLE
                    };
                };
                PlayablePlaylist(const std::shared_ptr<MusicBotManager>& /* manager */, const std::shared_ptr<PropertyManager>& /* properties */, const std::shared_ptr<permission::v2::PermissionManager>& /* permissions */);
                virtual ~PlayablePlaylist();

                void load_songs() override;

                inline ReplayMode::value replay_mode() { return this->properties()[property::PLAYLIST_REPLAY_MODE].as_unchecked<ReplayMode::value>(); }
                inline void set_replay_mode(ReplayMode::value mode) { this->properties()[property::PLAYLIST_REPLAY_MODE] = mode; }

                inline SongId currently_playing() { return this->properties()[property::PLAYLIST_CURRENT_SONG_ID]; }
                bool set_current_song(SongId /* song */);

                void previous();
                void next();
                std::shared_ptr<music::PlayableSong> current_song(bool& await_load);
                inline std::shared_ptr<server::MusicClient> current_bot() { return this->_current_bot.lock(); }
            protected:
                std::mutex currently_playing_lock;
                std::weak_ptr<PlaylistEntryInfo> current_loading_entry;
                std::weak_ptr<server::MusicClient> _current_bot;

                void set_self_ref(const std::shared_ptr<Playlist> &ptr) override;

                std::shared_ptr<PlaylistEntryInfo> playlist_previous_entry();
                std::shared_ptr<PlaylistEntryInfo> playlist_next_entry();
        };
    }
}
DEFINE_TRANSFORMS(ts::music::PlayablePlaylist::ReplayMode::value, uint8_t);