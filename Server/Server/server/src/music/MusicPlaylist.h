#pragma once

#include <sql/SqlQuery.h>
#include <Properties.h>
#include <shared_mutex>
#include <atomic>
#include <teaspeak/MusicPlayer.h>
#include <query/command3.h>
#include "MusicBotManager.h"
#include "PlaylistPermissions.h"

namespace ts {
    namespace server {
        class ConnectedClient;
    }

    namespace permission {
        class PermissionManager;
    }

    namespace music {
        class Playlist;

        enum struct LoadState {
            REQUIRES_QUERY, /* requires general song query */
            REQUIRES_PARSE, /* requires a parse of the given json string */

            LOADED, /* metadata has been loaded */
            LOAD_ERROR
        };

        struct PlaylistEntryInfo {
            /* static part available all the time */
            SongId previous_song_id{0};
            SongId song_id{0};

            ClientDbId invoker{0};
            std::string original_url{};
            std::string url_loader{};

            /* dynamic part only available after the song has been loaded successfully */
            struct {
                std::mutex load_lock{};
                LoadState load_state{false};

                std::string json_string{};
                std::condition_variable loaded_cv{};

                std::shared_ptr<::music::UrlSongInfo> loaded_data{nullptr}; /* only set when successfully loaded */
                std::string load_error{};

                std::chrono::system_clock::time_point load_begin{};

                [[nodiscard]] inline bool requires_load() const { return this->load_state == LoadState::REQUIRES_PARSE ||this->load_state == LoadState::REQUIRES_QUERY; }
                [[nodiscard]] inline bool is_loading() const { return (this->load_begin.time_since_epoch().count() > 0) && (this->load_state == LoadState::REQUIRES_PARSE ||this->load_state == LoadState::REQUIRES_QUERY); }
                [[nodiscard]] inline bool is_loaded() const { return this->load_state == LoadState::LOADED; }
                [[nodiscard]] inline bool has_failed_to_load() const { return this->load_state == LoadState::LOAD_ERROR; }
            } metadata;
        };

        class PlaylistEntry {
            friend class Playlist;
            public:
                /* linking for the list */
                std::shared_ptr<PlaylistEntry> previous_song;
                std::shared_ptr<PlaylistEntry> next_song;

                /* the entry */
                std::shared_ptr<PlaylistEntryInfo> entry;

                bool modified = false;
            private:
                inline void set_previous_song(const std::shared_ptr<PlaylistEntry>& song) {
                    assert(this->entry);

                    this->previous_song = song;
                    this->entry->previous_song_id = song ? song->entry->song_id : 0;
                    this->modified = true;
                }
        };

        //TODO add some kind of play history?
        class Playlist : public PlaylistPermissions {
                friend class MusicBotManager;
            public:
                struct Type {
                    enum value {
                        BOT_BOUND,
                        GENERAL
                    };
                };

                Playlist(const std::shared_ptr<MusicBotManager>& /* manager */, std::shared_ptr<PropertyManager>  /* properties */, std::shared_ptr<permission::v2::PermissionManager> /* permissions */);
                virtual ~Playlist();

                virtual void load_songs();
                inline bool loaded() { return this->_songs_loaded; };

                virtual std::deque<std::shared_ptr<PlaylistEntryInfo>> list_songs();
                virtual std::shared_ptr<PlaylistEntryInfo> find_song(SongId /* song */);
                virtual std::shared_ptr<PlaylistEntryInfo> add_song(const std::shared_ptr<server::ConnectedClient>& /* invoker */, const std::string& /* url */, const std::string& /* url loader */, SongId /* previous */ = 0);
                virtual std::shared_ptr<PlaylistEntryInfo> add_song(ClientDbId /* invoker */, const std::string& /* url */, const std::string& /* url loader */, SongId /* previous */ = 0);
                virtual bool delete_song(SongId /* song */);
                virtual bool reorder_song(SongId /* song */, SongId /* new id */);

                inline PropertyManager& properties() const { return *this->_properties; }

                inline PlaylistId playlist_id() {
                    return this->properties()[property::PLAYLIST_ID].as_unchecked<PlaylistId>();
                }

                inline Type::value playlist_type() {
                    return this->properties()[property::PLAYLIST_TYPE].as_unchecked<Type::value>();
                }

                inline int32_t max_songs() {
                    return this->properties()[property::PLAYLIST_MAX_SONGS];
                }

                inline std::shared_ptr<MusicBotManager> ref_handle() { return this->manager.lock(); }

                template <typename T = Playlist, typename std::enable_if<std::is_base_of<Playlist, T>::value, int>::type = 0>
                inline std::shared_ptr<Playlist> ref() { return std::dynamic_pointer_cast<T>(this->_self.lock()); }


                void add_subscriber(const std::shared_ptr<server::ConnectedClient>&);
                void remove_subscriber(const std::shared_ptr<server::ConnectedClient>&);
                bool is_subscriber(const std::shared_ptr<server::ConnectedClient>&);
            protected:
                virtual void set_self_ref(const std::shared_ptr<Playlist>& /* playlist */);
                [[nodiscard]] bool is_playlist_owner(ClientDbId database_id) const override { return
                            this->properties()[property::PLAYLIST_OWNER_DBID].as_or<ClientDbId>(0) == database_id; }

                std::atomic<SongId> current_id;
                std::shared_ptr<PropertyManager> _properties;
                std::weak_ptr<MusicBotManager> manager;
                std::weak_ptr<Playlist> _self;
                bool _songs_loaded = false;

                sql::SqlManager* get_sql();
                std::shared_ptr<server::VirtualServer> get_server();
                ServerId get_server_id();

                std::shared_mutex playlist_mutex{};
                std::shared_ptr<PlaylistEntry> playlist_head{};

                std::mutex subscriber_lock{};
                std::deque<std::weak_ptr<server::ConnectedClient>> subscribers{};

                virtual std::deque<std::shared_ptr<PlaylistEntryInfo>> _list_songs(const std::unique_lock<std::shared_mutex>& /* playlist lock */);

                /* playlist functions are thread safe */
                std::shared_ptr<PlaylistEntry> playlist_find(const std::unique_lock<std::shared_mutex>& /* playlist lock */, SongId /* song */);
                std::shared_ptr<PlaylistEntry> playlist_end(const std::unique_lock<std::shared_mutex> &);
                bool playlist_insert(
                        const std::unique_lock<std::shared_mutex>& /* playlist lock */,
                        const std::shared_ptr<PlaylistEntry>& /* song */,
                        std::shared_ptr<PlaylistEntry> /* previous */ = nullptr,
                        bool link_only = false
                );
                bool playlist_remove(const std::unique_lock<std::shared_mutex>& /* playlist lock */, const std::shared_ptr<PlaylistEntry>& /* song */);
                bool playlist_reorder(const std::unique_lock<std::shared_mutex>& /* playlist lock */, const std::shared_ptr<PlaylistEntry>& /* song */, std::shared_ptr<PlaylistEntry> /* previous */ = nullptr);

                std::deque<std::shared_ptr<PlaylistEntryInfo>> load_entries();
                bool build_tree(std::deque<std::shared_ptr<PlaylistEntryInfo>> /* entries */);
                void destroy_tree();

                bool sql_remove(const std::shared_ptr<PlaylistEntryInfo>& /* entry */);
                bool sql_add(const std::shared_ptr<PlaylistEntryInfo>& /* entry */); /* also assigns an ID */
                bool sql_apply_changes(const std::shared_ptr<PlaylistEntryInfo>& /* entry */);
                bool sql_flush_all_changes();

                void broadcast_notify(const command_builder& /* command */);
                bool notify_song_add(const std::shared_ptr<PlaylistEntryInfo>& /* entry */);
                bool notify_song_remove(const std::shared_ptr<PlaylistEntryInfo>& /* entry */);
                bool notify_song_reorder(const std::shared_ptr<PlaylistEntryInfo>& /* entry */);
                bool notify_song_loaded(const std::shared_ptr<PlaylistEntryInfo>& /* entry */);

                void enqueue_load(const std::shared_ptr<PlaylistEntryInfo>& /* entry */);
                void execute_async_load(const std::shared_ptr<ts::music::PlaylistEntryInfo> &/* entry */);
        };
    }
}

DEFINE_TRANSFORMS(ts::music::Playlist::Type::value, uint8_t);