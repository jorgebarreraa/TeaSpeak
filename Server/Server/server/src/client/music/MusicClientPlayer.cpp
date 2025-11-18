#include "MusicClient.h"
#include <memory>
#include "src/client/voice/VoiceClient.h"
#include <misc/endianness.h>
#include <log/LogUtils.h>
#include <ThreadPool/Timer.h>
#include <StringVariable.h>
#include "../../InstanceHandler.h"

using namespace std;
using namespace std::chrono;
using namespace ts;
using namespace ts::server;

#define EVENT_HANDLER_NAME "ts_music"

void MusicClient::player_reset(bool trigger_change) {
    unique_lock song_lock(this->current_song_lock);
    auto song = std::move(this->_current_song);
    song_lock.unlock();

    if(this->playback.task > 0)
        music::MusicBotManager::tick_music.cancelExecute(this->playback.task);


    if(song) {
        auto player = song->get_player();
        if(player) {
            player->unregisterEventHandler(EVENT_HANDLER_NAME);
            threads::Thread(THREAD_EXECUTE_LATER | THREAD_SAVE_OPERATIONS, [player]{
                player->stop(); //May hangs a littlebit up
            }).name("song stopper").execute().detach();
        }

        if(trigger_change) {
            this->schedule_stop_broadcast();
            this->changePlayerState(ReplayState::SLEEPING);
            this->_current_song = nullptr;
            this->notifySongChange(nullptr);
        }
    }
}

//Important: Handle the event first and the set sleeping because sleeping could cause a dry event
#define REPLAY_ERROR(this, message)                                                                     \
do {                                                                                                    \
    this->player_reset(false);                                                                          \
    this->handle_event_song_replay_failed();                                                            \
    if(!this->current_song()) \
        this->changePlayerState(ReplayState::SLEEPING);\
    this->broadcast_text_message(message);                                                              \
    return;                                                                                             \
} while(0)

void MusicClient::replay_song(const shared_ptr<music::PlayableSong> &entry, const std::function<void(const std::shared_ptr<::music::MusicPlayer>&)>& loaded_callback) {
    if(!entry) {
        this->notifySongChange(nullptr);
        this->changePlayerState(ReplayState::SLEEPING);
        return;
    }

    unique_lock song_lock(this->current_song_lock);
    auto loader = entry->get_loader(this->getServer(), true);
    if(!loader) REPLAY_ERROR(this, "An error occurred while trying to replay the next song! (loader is empty)");
    this->changePlayerState(ReplayState::LOADING);
    this->_current_song = entry;
    song_lock.unlock();

    weak_ptr weak_self = dynamic_pointer_cast<MusicClient>(this->ref());
    loader->waitAndGetLater([weak_self, loader, entry, loaded_callback](std::shared_ptr<music::PlayableSong::LoadedData> loaded_data){
        auto self = weak_self.lock();
        if(!self) return;

        auto player = loaded_data ? loaded_data->player : nullptr;
        //If song available this should be executed within a second thread
        ts::music::MusicBotManager::tick_music.execute([weak_self, self, loader, entry, player, loaded_callback] {
            if(self->current_song() != entry) {
                debugMessage(self->getServerId(), "Music loaded but not anymore required! Dropping entry");
                return;
            }

            if(loader->succeeded() && player) {
                if(!player->initialize(2)) //TODO Channel count dynamic
                    REPLAY_ERROR(self, "An error occurred while trying to replay the next song! (Failed to initialize player. (" + player->error() + "))");

                weak_ptr weak_song = entry;
                player->registerEventHandler(EVENT_HANDLER_NAME, [weak_song, weak_self](::music::MusicEvent event) {
                    auto locked = weak_self.lock();
                    if(locked) {
                        locked->musicEventHandler(weak_song, event);
                    }
                });
                self->changePlayerState(ReplayState::PAUSED);

                if(self->properties()[property::CLIENT_FLAG_NOTIFY_SONG_CHANGE].as_unchecked<bool>()) {
                    string invoker = "unknown";
                    {
                        auto info_list = serverInstance->databaseHelper()->queryDatabaseInfo(self->getServer(), {entry->getInvoker()});
                        if(!info_list.empty()) {
                            auto info = info_list.front();
                            invoker = "[URL=client://0/" + info->client_unique_id + "~WolverinDEV]" + info->client_nickname + "[/URL]";
                        }
                    }

                    auto info = entry->song_loaded_data();
                    if(info) {
                        auto message = strvar::transform(config::messages::music::song_announcement,
                                strvar::StringValue{"title", info->title},
                                strvar::StringValue{"description", info->description},
                                strvar::StringValue{"url", entry->getUrl()},
                                strvar::StringValue{"invoker", invoker}
                        );
                        self->broadcast_text_message(message);
                    } else {
                        auto message = strvar::transform(config::messages::music::song_announcement,
                             strvar::StringValue{"title", "unknown title"},
                             strvar::StringValue{"description", "unknown"},
                             strvar::StringValue{"url", entry->getUrl()},
                             strvar::StringValue{"invoker", invoker}
                        );
                        self->broadcast_text_message(message);
                    }
                }
                if(loaded_callback)
                    loaded_callback(player);
                else
                    self->playMusic();
            } else {
                if(loader->state() == threads::FutureState::WORKING) {
                    REPLAY_ERROR(self, "An error occurred while trying to replay the next song! (Loader load timeout!)");
                } else {
                    REPLAY_ERROR(self, "Got an error while trying to load next song: " + loader->errorMegssage());
                }
            }
        });
    }, nullptr, system_clock::now() + seconds(30));

    this->notifySongChange(entry);
}

void MusicClient::tick_server(const std::chrono::system_clock::time_point &time) {
    ConnectedClient::tick_server(time);

    if(this->_player_state == ReplayState::SLEEPING)
        this->handle_event_song_dry();
    else if(!this->current_song()) {
        auto playlist = this->playlist();
        if(playlist) { /* may bot just got initialized */
            bool await_load;
            if(auto song = playlist->current_song(await_load); song) {
                auto player_state = this->_player_state;
                this->replay_song(song, [player_state](const shared_ptr<::music::MusicPlayer>& player){
                    if(player_state == ReplayState::STOPPED)
                        player->stop();
                    else if(player_state == ReplayState::PAUSED)
                        player->pause();
                    else
                        player->play();
                });
            } else if(!await_load) {
                this->replay_next_song();
            }
        }
        if(!playlist || !this->current_song()) {
            this->changePlayerState(ReplayState::SLEEPING);
        }
    }
}


std::shared_ptr<ts::music::PlayableSong> MusicClient::current_song() {
    unique_lock song_lock(this->current_song_lock, defer_lock_t{});
    if(!song_lock.try_lock_for(milliseconds(5)))
        return nullptr; /* failed to acquire lock */

    return this->_current_song;
}

std::shared_ptr<::music::MusicPlayer> MusicClient::current_player() {
    auto song = this->current_song();
    return song ? song->get_player() : nullptr;
}

void MusicClient::playMusic() {
    auto self = dynamic_pointer_cast<MusicClient>(this->ref());
    ts::music::MusicBotManager::tick_music.execute([self]{
        auto playlist = self->playlist();
        if(playlist->properties()[property::PLAYLIST_FLAG_FINISHED].as_unchecked<bool>()) {
            playlist->properties()[property::PLAYLIST_FLAG_FINISHED] = false;
            debugMessage(self->getServerId(), "{} Received play, but playlist had finished. Restarting playlist.", CLIENT_STR_LOG_PREFIX_(self));
        }

        auto player = self->current_player();
        if(player)
            player->play();
        else
            debugMessage(self->getServerId(), "Tried to start music without a music player!");
    });
}

void MusicClient::stopMusic() {
    auto self = dynamic_pointer_cast<MusicClient>(this->ref());
    ts::music::MusicBotManager::tick_music.execute([self]{
        auto player = self->current_player();
        if(player)
            player->stop();
        else
            debugMessage(self->getServerId(), "Tried to stop music without a music player!");
    });
}

void MusicClient::player_pause() {
    auto self = dynamic_pointer_cast<MusicClient>(this->ref());
    ts::music::MusicBotManager::tick_music.execute([self]{
        auto player = self->current_player();
        if(player)
            player->pause();
        else
            debugMessage(self->getServerId(), "Tried to pause music without a music player!");
    });
}

void MusicClient::forwardSong() {
    auto song = this->current_song();
    auto playlist = this->playlist();
    this->handle_event_song_ended();

    /* explicitly wanted a "next" song so start over again */
    if(playlist->properties()[property::PLAYLIST_FLAG_FINISHED].as_unchecked<bool>()) {
        playlist->properties()[property::PLAYLIST_FLAG_FINISHED] = false;
        this->handle_event_song_ended();
    }
    if(song == this->current_song())
        this->player_reset(true);
}

void MusicClient::resetSong() {
    auto song = this->current_song();

    auto playlist = this->playlist();
    if(playlist) {
        bool await_load;
        if(auto song = playlist->current_song(await_load); song) {
            this->replay_song(song);
            return;
        }
    }
    this->player_reset(true);
}

void MusicClient::rewindSong() {
    auto song = this->current_song();

    auto playlist = this->playlist();
    if(playlist) {
        playlist->previous();

        bool await_load;
        if(auto song = playlist->current_song(await_load); song) {
            this->replay_song(song);
            return;
        }
    }
    this->player_reset(true);
}

void MusicClient::musicEventHandler(const std::weak_ptr<ts::music::PlayableSong>& weak_player, ::music::MusicEvent event) {
    unique_lock song_lock(this->current_song_lock);
    auto player = weak_player.lock();
    if(!player || player != this->_current_song) return;

    logTrace(this->server ? this->server->getServerId() : 0, "[{}] Got event " + to_string(event), CLIENT_STR_LOG_PREFIX);
    if(event == ::music::EVENT_PLAY){
        debugMessage(this->getServerId(), "[MusicBot] Got play event from music player. Starting bot!");

        this->next_music_tick = system_clock::time_point();
        this->schedule_music_tick(song_lock, this->_current_song, this->next_music_tick);
        song_lock.unlock();

        this->changePlayerState(ReplayState::PLAYING);
    } else if(event == ::music::EVENT_PAUSE || event == ::music::EVENT_STOP){
        debugMessage(this->getServerId(), "[MusicBot] Got stop or pause event from player. Stopping bot!");
        music::MusicBotManager::tick_music.cancelExecute(this->playback.task);
        this->playback.task = 0;
        song_lock.unlock();

        this->changePlayerState(event == ::music::EVENT_PAUSE ? ReplayState::PAUSED : ReplayState::STOPPED);
        this->schedule_stop_broadcast();
    } else if(event == ::music::EVENT_END) {
        debugMessage(this->getServerId(), "[MusicBot] Got end event from music player. Playing next song!");
        song_lock.unlock();

        auto song = this->current_song();
        this->handle_event_song_ended();
        if(song == this->current_song())
            this->player_reset(true);
    } else if(event == ::music::EVENT_ABORT) {
        debugMessage(this->getServerId(), "[MusicBot] Got abort event from music player. Playing next song!");
        song_lock.unlock();
        this->broadcast_text_message("Song replay aborted due to an unrecoverable error. Replaying next song.");

        auto song = this->current_song();
        this->handle_event_song_ended();
        if(song == this->current_song())
            this->player_reset(true);
    } else if(event == ::music::EVENT_INFO_UPDATE) {

    }
}

void MusicClient::volume_modifier(float vol) {
    this->playback.volume_modifier = vol;
    this->properties()[property::CLIENT_PLAYER_VOLUME] = this->playback.volume_modifier;
    this->server->notifyClientPropertyUpdates(this->ref(), std::deque<property::ClientProperties>{property::CLIENT_PLAYER_VOLUME});
}

void MusicClient::schedule_music_tick(const unique_lock<recursive_timed_mutex>& song_lock, const std::shared_ptr<ts::music::PlayableSong> &song, const std::chrono::system_clock::time_point& timepoint, bool ignore_lock) {
    if(!ignore_lock) {
        assert(song_lock.owns_lock() && song_lock.mutex() == &this->current_song_lock);
    }
    if(this->_current_song != song) return; //abort scheduling

    weak_ptr weak_self = dynamic_pointer_cast<MusicClient>(this->ref());
    weak_ptr weak_song = song;
    this->playback.task = music::MusicBotManager::tick_music.executeLater([weak_self, weak_song] {
        shared_ptr<MusicClient> self = weak_self.lock();
        auto song = weak_song.lock();

        if(!self || !song) return; /* we're outdated */

        unique_lock song_tick_lock(self->current_song_tick_lock, try_to_lock_t{});
        if(!song_tick_lock.owns_lock()) {
            if(self->current_song_lock_fail++ > 10) {
                logError(self->getServerId(), "[{}][Music] Failed to invoke next tick over 10 times. Ticking lock is still acquired. Breaking reschedule loop.", CLIENT_STR_LOG_PREFIX_(self));
                return;
            }

            unique_lock song_lock(self->current_song_lock, try_to_lock_t{});
            /*
            if(!song_lock) {
                logError(self->getServerId(), "[{}][Music] Failed to invoke next tick. Ticking and song lock is still acquired. Breaking loop", CLIENT_STR_LOG_PREFIX_(self));
            } else {
            */
            logError(self->getServerId(), "[{}][Music] Failed to invoke next tick. Ticking lock is still acquired. Rescheduling in 20ms.", CLIENT_STR_LOG_PREFIX_(self));
            self->schedule_music_tick(song_lock, song, system_clock::now() + milliseconds(20), true);
            //}
            return; //Current tick is still going on
        }
        self->current_song_lock_fail = 0;
        self->execute_music_tick(song);
    }, timepoint);
}

void MusicClient::schedule_stop_broadcast() {
    weak_ptr weak_self = dynamic_pointer_cast<MusicClient>(this->ref());

    this->playback.task = music::MusicBotManager::tick_music.executeLater([weak_self] {
        auto self = weak_self.lock();

        if(!self) return; /* we're outdated */

        unique_lock song_tick_lock(self->current_song_tick_lock, defer_lock_t{});
        if(!song_tick_lock.try_lock_for(milliseconds(5))) {
            logError(self->getServerId(), "[{}][Music] Failed to schedule stop. Ticking lock is still acquired.", CLIENT_STR_LOG_PREFIX_(self));
            return; //Current tick is still going on
        }

        self->broadcast_music_stop();
    }, system_clock::now());
}

void MusicClient::broadcast_music_stop() {
    if(this->voice.last_frame_silence) return;

    size_t buffer_size = 750;
    size_t voice_header_length = 5;
    char voice_buffer[buffer_size];

    this->voice.last_frame_silence = true;
    le2be16(this->voice.packet_id++, voice_buffer, 0);
    le2be16(this->getClientId(), voice_buffer, 2);
    voice_buffer[4] = 5; //Voice type opus music


    SpeakingClient::VoicePacketFlags flags{};
    for(const auto& cl : this->server->getClientsByChannel<SpeakingClient>(this->currentChannel)) {
        if(cl->shouldReceiveVoice(this->ref())) {
            cl->send_voice_packet(pipes::buffer_view{voice_buffer, voice_header_length}, flags);
        }
    }
}

void MusicClient::execute_music_tick(const shared_ptr<ts::music::PlayableSong>& song) {
    unique_lock song_lock(this->current_song_lock);
    if(this->_current_song != song) return; /* old music */
    
    auto player = song->get_player();
    if(!player || player->state() != ::music::STATE_PLAYING) return;
    song_lock.unlock();

    system_clock::time_point next_schedule;
    {
        if(this->next_music_tick.time_since_epoch().count() == 0)
            this->next_music_tick = system_clock::now();
        else if(this->next_music_tick + milliseconds(10) < system_clock::now()) {
            logWarning(this->getServerId(), "[{}] Resend task contained a delay of over 10ms! ({}ms)", CLIENT_STR_LOG_PREFIX, duration_cast<milliseconds>(system_clock::now() - this->next_music_tick).count());
        }
        if(this->next_music_tick + milliseconds(20) < system_clock::now()) {
            this->next_music_tick = system_clock::now();
        } else
            this->next_music_tick += milliseconds(20);

        next_schedule = this->next_music_tick;
    }
    auto timestamp_begin = system_clock::now();

    size_t buffer_size = 750;
    size_t voice_header_length = 5;
    char voice_buffer[buffer_size];
    auto next_segment = player->popNextSegment();

    if(next_segment) {
        this->apply_volume(next_segment);
        ssize_t length = 0;
        if(next_segment->segmentLength == player->preferredSampleCount())
            length = opus_encode(this->voice.encoder, next_segment->segments, next_segment->segmentLength, reinterpret_cast<unsigned char *>(&voice_buffer[voice_header_length]), buffer_size - voice_header_length);
        if(length <= 0) {
            logError(this->getServerId(), "[{}] opus_encode(...) returns invalid code: {}", CLIENT_STR_LOG_PREFIX, length);
        } else {
            le2be16(this->voice.packet_id++, voice_buffer, 0);
            le2be16(this->getClientId(), voice_buffer, 2);
            voice_buffer[4] = 5; //Voice type opus music

            SpeakingClient::VoicePacketFlags flags{};

            /* test if we've to recover from silence */
            if(this->voice.last_frame_silence) {
                this->voice.frame_header = 5;
                this->voice.last_frame_silence = false;
            }
            if(this->voice.frame_header > 0) {
                this->voice.frame_header--;
                flags.head = true;
            }

            auto buffer = pipes::buffer_view{voice_buffer, voice_header_length + length};
            for(const auto& cl : this->server->getClientsByChannel<SpeakingClient>(this->currentChannel))
                if(cl->shouldReceiveVoice(this->ref()))
                    cl->send_voice_packet(buffer, flags);
        }
    } else {
        this->broadcast_music_stop();
    }
    auto timestamp_begin_subscriber = system_clock::now();

    {
        auto now = system_clock::now();

        std::deque<std::shared_ptr<ConnectedClient>> subs;
        {
            lock_guard sub_lock(this->subscriber_lock);
            for(auto& sub : this->subscribers) {
                auto& time_point = std::get<1>(sub);
                if(time_point > now) continue;
                if(time_point.time_since_epoch().count() == 0)
                    time_point = now;
                time_point = time_point + seconds(1); /* TODO allow this interval to be configurable */

                auto client = std::get<0>(sub).lock();
                if(!client) continue;

                subs.push_back(std::move(client));
            }
        }

        auto self = dynamic_pointer_cast<MusicClient>(this->ref());
        for(const auto& sub : subs)
            sub->notifyMusicPlayerStatusUpdate(self);
    }
    auto timestamp_begin_schedule = system_clock::now();


    song_lock.lock();
    if(player->state() != ::music::STATE_PLAYING) return; /* status had been changed */
    this->schedule_music_tick(song_lock, song, next_schedule);
    auto timestamp_end = system_clock::now();
    if(timestamp_end - timestamp_begin > milliseconds(5)) {
        logWarning(this->getServerId(), "[{}] Ticking used more than 5ms to proceed ({}ms). Details: encode={},subscriber={},schedule={}.",
                CLIENT_STR_LOG_PREFIX,
                duration_cast<milliseconds>(timestamp_end - timestamp_begin).count(),


                duration_cast<milliseconds>(timestamp_begin_subscriber - timestamp_begin).count(),
                duration_cast<milliseconds>(timestamp_begin_schedule - timestamp_begin_subscriber).count(),
                duration_cast<milliseconds>(timestamp_end - timestamp_begin_schedule).count()
        );
    }
}

