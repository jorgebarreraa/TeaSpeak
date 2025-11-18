#include "PlayablePlaylist.h"
#include "src/VirtualServer.h"
#include "src/client/music/MusicClient.h"
#include <log/LogUtils.h>

using namespace ts;
using namespace ts::server;
using namespace ts::music;
using namespace std;
using namespace std::chrono;

PlayablePlaylist::PlayablePlaylist(const std::shared_ptr<MusicBotManager> &handle, const std::shared_ptr<PropertyManager> &props, const std::shared_ptr<permission::v2::PermissionManager>& perms) : Playlist(handle, props, perms) { }

PlayablePlaylist::~PlayablePlaylist() {}

void PlayablePlaylist::load_songs() {
    Playlist::load_songs();

    unique_lock playlist_lock(this->playlist_mutex);
    auto song_id = this->properties()[property::PLAYLIST_CURRENT_SONG_ID].as_or<SongId>(0);
    auto current_song = this->playlist_find(playlist_lock, song_id);
    if(!current_song && song_id != 0) {
        logWarning(this->get_server_id(), "[Playlist] Failed to reinitialize current song index for playlist {}. Song {} is missing.", this->playlist_id(), song_id);
        this->properties()[property::PLAYLIST_CURRENT_SONG_ID] = 0;
    }
    if(current_song) {
        this->current_loading_entry = current_song->entry;
        this->enqueue_load(current_song->entry);
    }
}

void PlayablePlaylist::set_self_ref(const std::shared_ptr<Playlist> &ptr) {
    Playlist::set_self_ref(ptr);
}

void PlayablePlaylist::previous() {
    unique_lock lock(this->currently_playing_lock);

    auto entry = this->playlist_previous_entry();
    if(entry) this->enqueue_load(entry);
    this->properties()[property::PLAYLIST_CURRENT_SONG_ID] = entry ? entry->song_id : 0;
    this->current_loading_entry = entry;
}

void PlayablePlaylist::next() {
    unique_lock lock(this->currently_playing_lock);

    auto entry = this->playlist_next_entry();
    if(entry) this->enqueue_load(entry);
    this->properties()[property::PLAYLIST_CURRENT_SONG_ID] = entry ? entry->song_id : 0;
    this->current_loading_entry = entry;
}

std::shared_ptr<ts::music::PlayableSong> PlayablePlaylist::current_song(bool& await_load) {
    await_load = false;

    unique_lock lock(this->currently_playing_lock);
    auto entry = this->current_loading_entry.lock();

    auto id = entry ? entry->song_id : 0;
    if(this->properties()[property::PLAYLIST_CURRENT_SONG_ID].as_unchecked<SongId>() != id) /* should not happen */
        this->properties()[property::PLAYLIST_CURRENT_SONG_ID] = id;

    if(!entry)
        return nullptr;

    if(entry->metadata.is_loading()) {
        if(entry->metadata.load_begin + seconds(30) < system_clock::now()) {
            this->delete_song(entry->song_id); /* delete song because we cant load it */

            return PlayableSong::create({
                    entry->song_id,
                    entry->original_url,
                    entry->invoker
            }, MusicClient::failedLoader("Song failed to load"));
        }

        await_load = true;
        return nullptr;
    }

    if(!entry->metadata.is_loaded()) {
        assert(entry->metadata.has_failed_to_load());
        this->delete_song(entry->song_id); /* acquired playlist */

        /* return promise */
        return PlayableSong::create({
                entry->song_id,
                entry->original_url,
                entry->invoker
        }, MusicClient::failedLoader(entry->metadata.load_error));
    }

    auto result = entry->metadata.loaded_data;
    assert(result); /* previous tested */

    if(result->type != ::music::UrlType::TYPE_VIDEO && result->type != ::music::UrlType::TYPE_STREAM) {
        /* we cant replay that kind of entry. Skipping it */
        debugMessage(this->get_server_id(), "[PlayList] Tried to replay an invalid entry type (SongID: {}, Type: {})! Skipping song", entry->song_id, result->type);
        return nullptr;
    }

    return PlayableSong::create({
            entry->song_id,
            result->url,
            entry->invoker
    }, MusicClient::providerLoader(entry->url_loader));
}

std::shared_ptr<PlaylistEntryInfo> PlayablePlaylist::playlist_next_entry() {
    if(!this->playlist_head) return nullptr; /* fuzzy check if we're not empty */
    auto replay_mode = this->properties()[property::PLAYLIST_REPLAY_MODE].as_unchecked<ReplayMode::value>();

    unique_lock playlist_lock(this->playlist_mutex);
    auto old_song = this->playlist_find(playlist_lock, this->currently_playing());

    if(replay_mode == ReplayMode::SINGLE_LOOPED) {
        if(old_song) return old_song->entry;
        if(this->playlist_head) return this->playlist_head->entry;

        this->properties()[property::PLAYLIST_FLAG_FINISHED] = true;
        return nullptr;
    }

    std::shared_ptr<PlaylistEntryInfo> result;
    if(replay_mode == ReplayMode::LINEAR || replay_mode == ReplayMode::LINEAR_LOOPED) {
        if(old_song) {
            if(old_song->next_song)
                result = old_song->next_song->entry;
            else if(replay_mode == ReplayMode::LINEAR_LOOPED && this->playlist_head)
                result = this->playlist_head->entry;
            else {
                this->properties()[property::PLAYLIST_FLAG_FINISHED] = true;
                result = nullptr;
            }
        } else if(replay_mode == ReplayMode::LINEAR_LOOPED || !this->properties()[property::PLAYLIST_FLAG_FINISHED]) {
            result = this->playlist_head ? this->playlist_head->entry : nullptr;
        }
    } else if(replay_mode == ReplayMode::SHUFFLE) {
        auto songs = this->_list_songs(playlist_lock);

        //TODO may add a already played list?
        if(songs.size() == 0) {
            this->properties()[property::PLAYLIST_FLAG_FINISHED] = true;
            result = nullptr;
        } else if(songs.size() == 1) {
            result = songs.front();
        } else {
            size_t index;
            while(songs[index = (rand() % songs.size())] == (!old_song ? nullptr : old_song->entry)) { }

            result = songs[index];
        }
    }
    playlist_lock.unlock();
    if(old_song && this->properties()[property::PLAYLIST_FLAG_DELETE_PLAYED].as_unchecked<bool>()) {
        this->delete_song(old_song->entry->song_id);
    }
    return result;
}

std::shared_ptr<PlaylistEntryInfo> PlayablePlaylist::playlist_previous_entry() {
    if(!this->playlist_head) return nullptr; /* fuzzy check if we're not empty */
    auto replay_mode = this->properties()[property::PLAYLIST_REPLAY_MODE].as_unchecked<ReplayMode::value>();

    unique_lock playlist_lock(this->playlist_mutex);
    this->properties()[property::PLAYLIST_FLAG_FINISHED] = false;

    auto current_song = this->playlist_find(playlist_lock, this->currently_playing());
    if(current_song) {
        if(replay_mode == ReplayMode::LINEAR || replay_mode == ReplayMode::LINEAR_LOOPED) {
            if(current_song->previous_song) return current_song->previous_song->entry;
            if(!this->playlist_head) return nullptr;

            if(replay_mode == ReplayMode::LINEAR)
                return this->playlist_head->entry;

            auto tail = this->playlist_head;
            while(tail->next_song) tail = tail->next_song;
            return tail->entry;
        } else if(replay_mode == ReplayMode::SINGLE_LOOPED) {
            return current_song ? current_song->entry : this->playlist_head ? this->playlist_head->entry : nullptr;
        } else {
            return current_song ? current_song->entry : nullptr; /* not possible to rewind shuffle mode  */
        }
    } else {
        return this->playlist_head ? this->playlist_head->entry : nullptr;
    }
}

bool PlayablePlaylist::set_current_song(SongId song_id) {
    {
        unique_lock playlist_lock(this->playlist_mutex);
        auto current_song = this->playlist_find(playlist_lock, song_id);
        if(!current_song && song_id != 0) return false;

        this->current_loading_entry = current_song->entry;
        if(current_song) {
            this->enqueue_load(current_song->entry);
            auto id = current_song ? current_song->entry->song_id : 0;
            if(this->properties()[property::PLAYLIST_CURRENT_SONG_ID].as_unchecked<SongId>() != id)
                this->properties()[property::PLAYLIST_CURRENT_SONG_ID] = id;
        }
    }
    {
        auto bot = this->current_bot();
        bot->resetSong(); /* reset the song and use the song which is supposed to be the "current" */
    }

    return true;
}