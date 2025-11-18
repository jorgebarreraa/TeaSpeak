#pragma once

#include <deque>
#include <memory>
#include <Definitions.h>
#include <ThreadPool/ThreadPool.h>
#include <ThreadPool/Mutex.h>

namespace ts {
    namespace server {
        class MusicClient;
        class VirtualServer;
    }

    namespace music {
        class PlayablePlaylist;

        class MusicBotManager {
                friend class server::VirtualServer;
                friend class server::MusicClient;
            public:
                static threads::ThreadPool tick_music;
                static threads::ThreadPool load_music;
                static void shutdown();

                static void adjustTickPool();

                explicit MusicBotManager(const std::shared_ptr<server::VirtualServer>&);
                ~MusicBotManager();

                void load_bots();
                void load_playlists();

                void connectBots();
                void disconnectBots();

                void cleanup_semi_bots();
                void cleanup_client_bots(ClientDbId /* client */);

                std::deque<std::shared_ptr<server::MusicClient>> available_bots();
                std::deque<std::shared_ptr<server::MusicClient>> listBots(ClientDbId);
                std::shared_ptr<server::MusicClient> find_bot_by_playlist(const std::shared_ptr<PlayablePlaylist>& /* playlist */);
                std::shared_ptr<server::MusicClient> findBotById(ClientDbId);
                std::shared_ptr<server::MusicClient> createBot(ClientDbId);
                void deleteBot(std::shared_ptr<server::MusicClient>);
                bool assign_playlist(const std::shared_ptr<server::MusicClient>& /*  bot */, const std::shared_ptr<PlayablePlaylist>& /* playlist */);

                int max_bots();
                int current_bot_count();

                inline std::deque<std::shared_ptr<PlayablePlaylist>> playlists() {
                    std::lock_guard list_lock(this->playlists_lock);
                    return this->playlists_list;
                }
                std::shared_ptr<PlayablePlaylist> find_playlist(PlaylistId /* id */);
                std::deque<std::shared_ptr<PlayablePlaylist>> find_playlists_by_client(ClientDbId /* owner */);
                std::shared_ptr<PlayablePlaylist> create_playlist(ClientDbId /* owner */, const std::string& /* owner name */);
                bool delete_playlist(PlaylistId /* id */, std::string& /* error */);

                void execute_tick();

                inline std::shared_ptr<server::VirtualServer> ref_server() { return this->handle.lock(); }
                inline std::shared_ptr<MusicBotManager> ref() { return this->_self.lock(); }
            private:
                std::weak_ptr<MusicBotManager> _self;

                std::weak_ptr<server::VirtualServer> handle;
                int sqlCreateMusicBot(int, std::string*, std::string*);

                std::recursive_mutex music_bots_lock;
                std::deque<std::shared_ptr<server::MusicClient>> music_bots;

                std::recursive_mutex playlists_lock;
                std::atomic<PlaylistId> playlists_index{0};
                std::deque<std::shared_ptr<PlayablePlaylist>> playlists_list;
        };
    }
}