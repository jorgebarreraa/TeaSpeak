#pragma once

#include "../ConnectedClient.h"
#include "../../music/MusicBotManager.h"
#include "src/music/PlayablePlaylist.h"
#include <opus/opus.h>
#include <teaspeak/MusicPlayer.h>

namespace ts::server {
    class MusicClient : public ConnectedClient {
            friend class music::MusicBotManager;
        public:
            struct UptimeMode {
                enum value {
                    TIME_SINCE_CREATED = 0,
                    TIME_SINCE_SERVER_START
                };
            };

            struct Type {
                enum value {
                    PERMANENT,
                    SEMI_PERMANENT,
                    TEMPORARY
                };
            };

            //Helper methodes
            typedef std::shared_ptr<music::PlayableSong::song_loader_t> loader_t;
            static loader_t failedLoader(const std::string& /* error */);
            static loader_t providerLoader(const std::string& name = "", std::shared_ptr<::music::manager::PlayerProvider> = nullptr);

            enum ReplayState {
                SLEEPING = 0,
                LOADING,

                PLAYING,
                PAUSED,
                STOPPED
            };

            MusicClient(const std::shared_ptr<VirtualServer>&,const std::string&);
            virtual ~MusicClient() override;

            //Basic TeaSpeak stuff
            void sendCommand(const ts::Command &command, bool low) override;
            void sendCommand(const ts::command_builder &command, bool low) override;

            bool disconnect(const std::string &reason) override;
            bool close_connection(const std::chrono::system_clock::time_point& = std::chrono::system_clock::time_point()) override;
            void initialize_bot();

            //Music stuff
            inline Type::value get_bot_type() { return this->properties()[property::CLIENT_BOT_TYPE]; }
            inline void set_bot_type(Type::value type) { this->properties()[property::CLIENT_BOT_TYPE] = type; }

            ClientDbId getOwner() { return this->properties()[property::CLIENT_OWNER]; }
            ReplayState player_state() { return this->_player_state; }

            std::shared_ptr<music::PlayablePlaylist> playlist() { return this->_playlist; }

            void player_reset(bool /* trigger change */);
            void stopMusic();
            void playMusic();
            void player_pause();

            void forwardSong();
            void resetSong();
            void rewindSong();

            std::shared_ptr<music::MusicPlayer> current_player();
            std::shared_ptr<ts::music::PlayableSong> current_song();

            float volumeModifier(){ return this->playback.volume_modifier; }
            void volume_modifier(float vol);

            void add_subscriber(const std::shared_ptr<ConnectedClient>&);
            void remove_subscriber(const std::shared_ptr<ConnectedClient>&);
            bool is_subscriber(const std::shared_ptr<ConnectedClient>&);

            bool notifyClientMoved(
                    const std::shared_ptr<ConnectedClient> &client,
                    const std::shared_ptr<BasicChannel> &target_channel,
                    ViewReasonId reason,
                    std::string msg,
                    std::shared_ptr<ConnectedClient> invoker,
                    bool lock_channel_tree
            ) override;
        protected:

            void broadcast_text_message(const std::string &message);

            void tick_server(const std::chrono::system_clock::time_point &time) override;
            std::chrono::system_clock::time_point next_music_tick;

            void execute_music_tick(const std::shared_ptr<ts::music::PlayableSong>&);
            void schedule_music_tick(const std::unique_lock<std::recursive_timed_mutex>&, const std::shared_ptr<ts::music::PlayableSong>&, const std::chrono::system_clock::time_point&, bool = false);
            void schedule_stop_broadcast();
            void broadcast_music_stop(); /* only invoke while ticking */

            void musicEventHandler(const std::weak_ptr<music::PlayableSong>&,::music::MusicEvent);

            ReplayState _player_state;
            std::recursive_timed_mutex current_song_lock;
            std::recursive_timed_mutex current_song_tick_lock;
            std::atomic_uint8_t current_song_lock_fail{0};

            std::shared_ptr<music::PlayableSong> _current_song;
            std::shared_ptr<music::PlayablePlaylist> _playlist;
            ts::music::MusicBotManager* manager = nullptr;

            struct {
                bool last_frame_silence = true;
                uint8_t frame_header = 0;

                OpusEncoder* encoder = nullptr;
                uint16_t packet_id = 0;
            } voice;

            struct {
                threads::tracking_id task = 0;
                float volume_modifier = 1.f;
            } playback;

            std::mutex subscriber_lock;
            std::deque<std::tuple<std::weak_ptr<ConnectedClient>, std::chrono::system_clock::time_point>> subscribers;


            void apply_volume(const std::shared_ptr<::music::SampleSegment> &);
            void replay_song(const std::shared_ptr<music::PlayableSong> &, const std::function<void(const std::shared_ptr<::music::MusicPlayer>&)>& /* loaded callback */ = NULL);

            void changePlayerState(ReplayState state);
            void notifySongChange(const std::shared_ptr<music::SongInfo>&);

            void handle_event_song_ended();
            void handle_event_song_replay_failed();
            void handle_event_song_dry(); /* repeating */

            void replay_next_song();

            inline void set_playlist(const std::shared_ptr<music::PlayablePlaylist>& playlist) {
                this->properties()[property::CLIENT_PLAYLIST_ID] = playlist ? playlist->playlist_id() : 0;
                this->_playlist = playlist;
                if(playlist)
                    playlist->_current_bot = std::dynamic_pointer_cast<MusicClient>(this->ref());
            }
    };
}
DEFINE_TRANSFORMS(ts::server::MusicClient::ReplayState, uint8_t);
DEFINE_TRANSFORMS(ts::server::MusicClient::UptimeMode::value, uint8_t);
DEFINE_TRANSFORMS(ts::server::MusicClient::Type::value, uint8_t);