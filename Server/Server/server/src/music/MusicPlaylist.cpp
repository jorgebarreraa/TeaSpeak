#include <linked_helper.h>
#include <json/json.h>
#include "MusicPlaylist.h"
#include "src/VirtualServer.h"
#include "src/client/ConnectedClient.h"
#include <log/LogUtils.h>

#include <utility>
#include "teaspeak/MusicPlayer.h"

using namespace ts;
using namespace ts::music;
using namespace std;
using namespace std::chrono;

Playlist::Playlist(const std::shared_ptr<ts::music::MusicBotManager> &manager, std::shared_ptr<ts::PropertyManager> properties, std::shared_ptr<permission::v2::PermissionManager> permissions) :
        PlaylistPermissions{std::move(permissions)}, _properties{std::move(properties)}, manager{manager} { }

Playlist::~Playlist() {
    this->destroy_tree();
}

void Playlist::set_self_ref(const std::shared_ptr<ts::music::Playlist> &ref) {
    assert(&*ref == this);
    this->_self = ref;
}

std::shared_ptr<server::VirtualServer> Playlist::get_server() {
    auto handle = this->ref_handle();
    if(!handle) return nullptr;
    return handle->ref_server();
}

ServerId Playlist::get_server_id() {
    auto server = this->get_server();
    if(!server) return 0;

    return server->getServerId();
}

sql::SqlManager* Playlist::get_sql() {
    auto server = this->get_server();
    if(!server) return nullptr;

    return server->getSql();
}

std::shared_ptr<PlaylistEntry> Playlist::playlist_end(const std::unique_lock<std::shared_mutex>& lock) {
    assert(lock.owns_lock());

    auto current = this->playlist_head;
    while(current && current->next_song)
        current = current->next_song;
    return current;
}

std::shared_ptr<PlaylistEntry> Playlist::playlist_find(const std::unique_lock<std::shared_mutex> &lock, ts::SongId id) {
    assert(lock.owns_lock());

    auto current = this->playlist_head;
    while(current) {
        assert(current->entry);

        if(current->entry->song_id == id) {
            return current;
        }

        current = current->next_song;
    }

    return nullptr;
}

bool Playlist::playlist_insert(const std::unique_lock<std::shared_mutex> &lock, const shared_ptr<PlaylistEntry> &entry, shared_ptr<PlaylistEntry> previous, bool link_only) {
    assert(lock.owns_lock());

    entry->next_song = nullptr;
    if(!this->playlist_head) {
        this->playlist_head = entry;
        previous = nullptr;
    } else {
        if(previous) {
            if(previous->next_song) {
                assert(previous->next_song->previous_song == previous);
                entry->next_song = previous->next_song;
                if(link_only)
                    previous->next_song->previous_song = entry;
                else
                    previous->next_song->set_previous_song(entry);

            }
            previous->next_song = entry;
        } else {
            if(link_only)
                this->playlist_head->previous_song = entry;
            else
                this->playlist_head->set_previous_song(entry);
            entry->next_song = this->playlist_head;
            this->playlist_head = entry;
        }
    }
    if(link_only)
        entry->previous_song = previous;
    else
        entry->set_previous_song(previous);

    return true;
}

bool Playlist::playlist_reorder(const std::unique_lock<std::shared_mutex>& lock, const shared_ptr<PlaylistEntry> &entry, std::shared_ptr<PlaylistEntry> previous) {
    assert(lock.owns_lock());

    if(!this->playlist_remove(lock, entry)) return false;
    return this->playlist_insert(lock, entry, std::move(previous));
}

bool Playlist::playlist_remove(const std::unique_lock<std::shared_mutex>& lock, const std::shared_ptr<ts::music::PlaylistEntry> &entry) {
    assert(lock.owns_lock());

    if(entry == this->playlist_head) {
        this->playlist_head = entry->next_song;
        if(this->playlist_head)
            this->playlist_head->set_previous_song(nullptr);
    } else {
        assert(entry->previous_song); /* if its not head it must have a previous song */
        entry->previous_song->next_song = entry->next_song;
        if(entry->next_song)
            entry->next_song->set_previous_song(entry->previous_song);
    }

    /* release references */
    entry->previous_song = nullptr;
    entry->next_song = nullptr;

    return true;
}

void Playlist::load_songs() {
    if(!this->_songs_loaded)
        this->_songs_loaded = true;
    else return;

    auto entries = this->load_entries();
    this->build_tree(std::move(entries));
}

bool Playlist::sql_add(const std::shared_ptr<ts::music::PlaylistEntryInfo> &entry) {
    auto sql_handle = this->get_sql();
    if(!sql_handle) return false;

    entry->song_id = ++current_id;

    //`serverId` INT NOT NULL, `playlist_id` INT, `song_id` INT, `order_id` INT, `invoker_dbid` INT, `url` TEXT
    auto sql_result = sql::command(sql_handle, "INSERT INTO `playlist_songs` (`serverId`, `playlist_id`, `song_id`, `order_id`, `invoker_dbid`, `url`, `url_loader`, `loaded`, `metadata`) VALUES (:server_id, :playlist_id, :song_id, :order_id, :invoker_dbid, :url, :url_loader, :loaded, :metadata)",
            variable{":server_id", this->get_server_id()},
            variable{":playlist_id", this->playlist_id()},
            variable{":song_id", entry->song_id},
            variable{":order_id", entry->previous_song_id},
            variable{":invoker_dbid", entry->invoker},
            variable{":url", entry->original_url},
            variable{":url_loader", entry->url_loader},
            variable{":loaded", entry->metadata.load_state == LoadState::REQUIRES_PARSE || entry->metadata.load_state == LoadState::LOADED},
            variable{":metadata", entry->metadata.json_string}
    ).execute();

    LOG_SQL_CMD(sql_result);

    return !!sql_result;
}

bool Playlist::sql_apply_changes(const std::shared_ptr<ts::music::PlaylistEntryInfo> &entry) {
    auto sql_handle = this->get_sql();
    if(!sql_handle) return false;
    sql::command(sql_handle, "UPDATE `playlist_songs` SET `order_id` = :order_id, `invoker_dbid` = :invoker_dbid, `url` = :url , `url_loader` = :url_loader, `loaded` = :loaded, `metadata` = :metadata WHERE `serverId` = :server_id AND `playlist_id` = :playlist_id AND `song_id` = :song_id",
         variable{":server_id", this->get_server_id()},
         variable{":playlist_id", this->playlist_id()},
         variable{":song_id", entry->song_id},
         variable{":order_id", entry->previous_song_id},
         variable{":invoker_dbid", entry->invoker},
         variable{":url", entry->original_url},
         variable{":url_loader", entry->url_loader},
         variable{":loaded", entry->metadata.load_state == LoadState::REQUIRES_PARSE || entry->metadata.load_state == LoadState::LOADED},
         variable{":metadata", entry->metadata.json_string}
    ).executeLater().waitAndGetLater(LOG_SQL_CMD, {-1, "failed future"});

    return true;
}

bool Playlist::sql_flush_all_changes() {
    std::unique_lock list_lock{this->playlist_mutex};
    std::deque<std::shared_ptr<PlaylistEntryInfo>> changed_entries;
    auto head = this->playlist_head;
    while(head) {
        if(head->modified) {
            changed_entries.push_back(head->entry);
            head->modified = false;
            continue;
        }

        head = head->next_song;
    }

    list_lock.unlock();

    for(const auto& entry : changed_entries)
        this->sql_apply_changes(entry); //TODO Whats when we encounter an error?

    return true;
}

bool Playlist::sql_remove(const std::shared_ptr<ts::music::PlaylistEntryInfo> &entry) {
    auto sql_handle = this->get_sql();
    if(!sql_handle) return false;
    sql::command(sql_handle, "DELETE FROM `playlist_songs` WHERE `serverId` = :server_id AND `playlist_id` = :playlist_id AND `song_id` = :song_id",
                 variable{":server_id", this->get_server_id()},
                 variable{":playlist_id", this->playlist_id()},
                 variable{":song_id", entry->song_id}
    ).executeLater().waitAndGetLater(LOG_SQL_CMD, {-1, "failed future"});

    return true;
}

std::deque<std::shared_ptr<PlaylistEntryInfo>> Playlist::load_entries() {
    std::deque<std::shared_ptr<PlaylistEntryInfo>> result;

    this->current_id = 0;
    auto sql_handle = this->get_sql();
    assert(sql_handle);
    auto sql_result = sql::command(sql_handle, "SELECT `song_id`, `order_id`, `invoker_dbid`, `url`, `url_loader`, `loaded`, `metadata` FROM `playlist_songs` WHERE `serverId` = :server_id AND `playlist_id` = :playlist_id",
            variable{":server_id", this->get_server_id()},
            variable{":playlist_id", this->playlist_id()})
    .query([&](int length, string* values, string* columns) {
        auto entry = make_shared<PlaylistEntryInfo>();
        for(int index = 0; index < length; index++) {
            try {
                if(columns[index] == "song_id")
                    entry->song_id = (SongId) stoll(values[index]);
                else if(columns[index] == "order_id")
                    entry->previous_song_id = (SongId) stoll(values[index]);
                else if(columns[index] == "invoker_dbid")
                    entry->invoker = (ClientDbId) stoll(values[index]);
                else if(columns[index] == "url")
                    entry->original_url = values[index];
                else if(columns[index] == "url_loader")
                    entry->url_loader = values[index];
                else if(columns[index] == "loaded")
                    entry->metadata.load_state = (values[index].length() > 0 && stoll(values[index]) == 1) ? LoadState::REQUIRES_PARSE : LoadState::REQUIRES_QUERY;
                else if(columns[index] == "metadata")
                    entry->metadata.json_string = values[index];
            } catch(const std::exception& ex) {
                logError(this->get_server_id(), "[PlayList] Failed to parse song entry property in playlist {}. Key: {}, Value: {}, Error: {}", this->playlist_id(), columns[index], values[index], ex.what());
                return;
            }
        }

        if(entry->song_id > this->current_id)
            this->current_id = entry->song_id;
        result.push_back(move(entry));
    });
    LOG_SQL_CMD(sql_result);

    map<PlaylistId, size_t> count;
    for(const auto& entry : result)
        ++count[entry->song_id];

    for(const auto& entry : count) {
        if(entry.second <= 1) continue;

        logError(this->get_server_id(), "[PlayList] Playlist {} contains {} times a song with ID {}. removing all!", this->playlist_id(), entry.second, entry.first);
        auto sql_command = sql::command(sql_handle, "DELETE FROM `playlist_songs` WHERE `serverId` = :server_id AND `playlist_id` = :playlist_id AND `song_id` = :song_id",
                     variable{":server_id", this->get_server_id()},
                     variable{":playlist_id", this->playlist_id()},
                     variable{":song_id", entry.first}
        ).execute();
        LOG_SQL_CMD(sql_command);
    }

    {

        std::deque<std::shared_ptr<PlaylistEntryInfo>> _result;
        for(const auto& entry : result)
            if(count[entry->song_id] <= 1) {
                this->enqueue_load(entry);
                _result.push_back(entry);
            }

        return _result;
    }
}

bool Playlist::build_tree(deque<shared_ptr<PlaylistEntryInfo>> entries) {
    this->playlist_head = nullptr;

    if(entries.empty()) {
        return true;
    }

    std::unique_lock list_lock{this->playlist_mutex};
    auto find_entry = [&](SongId id) -> shared_ptr<PlaylistEntryInfo> {
        for(const auto& entry : entries)
            if(entry->song_id == id)
                return entry;
        return nullptr;
    };

    deque<shared_ptr<linked::entry>> l_entries;
    for(const auto& entry : entries)
        l_entries.push_back(move(linked::create_entry(0, entry->song_id, entry->previous_song_id)));

    deque<string> errors;
    auto head = linked::build_chain(l_entries, errors);

    shared_ptr<PlaylistEntry> current_tail = nullptr;
    while(head) {
        auto entry = make_shared<PlaylistEntry>();
        entry->entry = find_entry(head->entry_id);

        if(!current_tail) {
            /* initialization */
            if(head->modified)
                entry->set_previous_song(nullptr);
            else
                entry->previous_song = nullptr;

            this->playlist_head = entry;
        } else {
            if(head->modified) {
                entry->set_previous_song(current_tail);
            } else {
                entry->previous_song = current_tail;
            }
        }

        if(current_tail) {
            current_tail->next_song = entry;
        }
        current_tail = entry;

        head = head->next;
    }

    return true;
}

void Playlist::destroy_tree() {
    unique_lock list_lock(this->playlist_mutex);
    auto element = this->playlist_head;
    while(element) {
        element->entry = nullptr;
        if(element->previous_song) {
            element->previous_song->next_song = nullptr;
            element->previous_song = nullptr;
        }
        element = element->next_song;
    }
}

std::deque<std::shared_ptr<PlaylistEntryInfo>> Playlist::list_songs() {
    unique_lock list_lock(this->playlist_mutex);
    return this->_list_songs(list_lock);
}

std::deque<std::shared_ptr<PlaylistEntryInfo>> Playlist::_list_songs(const std::unique_lock<std::shared_mutex>& lock) {
    assert(lock.owns_lock());

    deque<shared_ptr<PlaylistEntryInfo>> result;

    auto head = this->playlist_head;
    while(head) {
        result.push_back(head->entry);
        head = head->next_song;
    }

    return result;
}

std::shared_ptr<PlaylistEntryInfo> Playlist::find_song(ts::SongId id) {
    unique_lock list_lock(this->playlist_mutex);
    auto head = this->playlist_head;
    while(head) {
        if(head->entry->song_id == id)
            return head->entry;

        head = head->next_song;
    }

    return nullptr;
}


std::shared_ptr<PlaylistEntryInfo> Playlist::add_song(const std::shared_ptr<ts::server::ConnectedClient> &client, const std::string &url, const std::string& url_loader, ts::SongId order) {
    return this->add_song(client ? client->getClientDatabaseId() : 0, url, url_loader, order);
}

std::shared_ptr<PlaylistEntryInfo> Playlist::add_song(ClientDbId client, const std::string &url, const std::string& url_loader, ts::SongId order) {
    auto entry = std::make_shared<PlaylistEntryInfo>();

    entry->previous_song_id = order;
    entry->invoker = client;
    entry->original_url = url;
    entry->url_loader = url_loader;

    if(!this->sql_add(entry)) return nullptr;

    auto list_entry = make_shared<PlaylistEntry>();
    list_entry->entry = entry;

    unique_lock list_lock(this->playlist_mutex);
    if(order == 0) {
        auto end = playlist_end(list_lock);
        entry->previous_song_id = end ? end->entry->song_id : 0;
        list_entry->modified = true;
    }
    auto order_entry = this->playlist_find(list_lock, entry->previous_song_id);
    this->playlist_insert(list_lock, list_entry, order_entry);

    if(order_entry ? order_entry->entry->song_id : 0 != order || list_entry->modified) {
        list_entry->modified = false;
        entry->previous_song_id = list_entry->previous_song ? list_entry->previous_song->entry->song_id : 0;
        list_lock.unlock();

        this->sql_apply_changes(entry);
    } else {
        list_lock.unlock();
    }

    this->enqueue_load(entry);
    this->notify_song_add(entry);
    this->properties()[property::PLAYLIST_FLAG_FINISHED] = false;
    return entry;
}

bool Playlist::delete_song(ts::SongId id) {
    unique_lock list_lock(this->playlist_mutex);
    auto song = this->playlist_find(list_lock, id);
    if(!song) return false;

    this->playlist_remove(list_lock, song);
    list_lock.unlock();

    this->sql_remove(song->entry);
    this->notify_song_remove(song->entry);
    return true;
}

bool Playlist::reorder_song(ts::SongId song_id, ts::SongId order_id) {
    unique_lock list_lock(this->playlist_mutex);
    auto song = this->playlist_find(list_lock, song_id);
    auto order = this->playlist_find(list_lock, order_id);
    if(!song) return false;
    if(!order && order_id != 0) return false;
    if(song->previous_song == order) return true;

    if(!this->playlist_reorder(list_lock, song, order)) return false;
    list_lock.unlock();

    this->sql_flush_all_changes();
    this->notify_song_reorder(song->entry);
    return true;
}

void Playlist::enqueue_load(const std::shared_ptr<ts::music::PlaylistEntryInfo> &entry) {
    assert(entry);
    std::lock_guard song_lock_lock{entry->metadata.load_lock};
    if(!entry->metadata.requires_load())
        return;

    entry->metadata.load_begin = system_clock::now();
    std::weak_ptr weak_self = this->ref<Playlist>();
    std::weak_ptr weak_entry{entry};

    logTrace(this->get_server_id(), "[PlayList] Enqueueing song {} for loading", entry->song_id);
    MusicBotManager::load_music.execute([weak_self, weak_entry] {
            shared_ptr<Playlist> self = weak_self.lock();
            auto entry = weak_entry.lock();
            if(!self || !entry)
                return; /* we're old */
            self->execute_async_load(entry);
    });
}

#define FORCE_LOAD \
goto load_entry

/*
    struct UrlSongInfo : public UrlInfo {
        std::string title;
        std::string description;

        std::map<std::string, std::string> metadata;
    };

    struct UrlPlaylistInfo : public UrlInfo {
        std::deque<std::shared_ptr<UrlSongInfo>> entries;
    };
 */

using UrlType = ::music::UrlType;
using UrlPlaylistInfo = ::music::UrlPlaylistInfo;
using UrlSongInfo = ::music::UrlSongInfo;

#define load_finished(state, error, result) \
do { \
    { \
        std::lock_guard slock{entry->metadata.load_lock}; \
        entry->metadata.load_state = state; \
        entry->metadata.loaded_data = result; \
        entry->metadata.load_error = error; \
        entry->metadata.loaded_cv.notify_all(); \
    } \
    this->notify_song_loaded(entry);\
} while(0)

void Playlist::execute_async_load(const std::shared_ptr<ts::music::PlaylistEntryInfo> &entry) {
    //TODO: Song might get removed while being loaded. Need better handling specially with the notified etc.
    if(entry->metadata.load_state == LoadState::REQUIRES_PARSE) { /* we need to parse the metadata */
        Json::Value data;
        Json::CharReaderBuilder rbuilder;
        std::string errs;

        istringstream jsonStream(entry->metadata.json_string);
        bool parsingSuccessful = Json::parseFromStream(rbuilder, jsonStream, &data, &errs);
        if (!parsingSuccessful) {
            logError(this->get_server_id(), "[PlayList] Failed to parse loaded metadata for song {}. Metadata data!", entry->song_id);
            FORCE_LOAD;
        }

        if(!data[Json::StaticString("type")].isUInt()) {
            logError(this->get_server_id(), "[PlayList] Failed to parse loaded metadata for song {}. Metadata has an invalid key 'type'. Query data again!", entry->song_id);
            FORCE_LOAD;
        }
        if(!data[Json::StaticString("url")].isString()) {
            logError(this->get_server_id(), "[PlayList] Failed to parse loaded metadata for song {}. Metadata has an invalid key 'url'. Query data again!", entry->song_id);
            FORCE_LOAD;
        }

        auto type = (UrlType) data[Json::StaticString("type")].asUInt();
        if(type != UrlType::TYPE_VIDEO && type != UrlType::TYPE_STREAM) {
            /* we're currently not able to parse playlists */
            FORCE_LOAD;
        } else {
            auto info = make_shared<UrlSongInfo>();
            if(data[Json::StaticString("metadata")].isObject()) {
                for(const auto& key : data[Json::StaticString("metadata")])
                    info->metadata[key.asString()] = data[Json::StaticString("metadata")][key.asString()].asString();
            }
            info->url = data[Json::StaticString("url")].asString();
            info->title = data[Json::StaticString("title")].asString();
            info->type = type;
            info->description = data[Json::StaticString("description")].asString();

            load_finished(LoadState::LOADED, "", info);
            return;
        }
    }

    load_entry:
    auto provider = ::music::manager::resolveProvider(entry->url_loader, entry->original_url);
    if(!provider) {
        load_finished(LoadState::LOAD_ERROR, "failed to load info provider", nullptr);
        return;
    }

    auto info_future = provider->query_info(entry->original_url, nullptr, &*this->get_server());
    info_future.waitAndGet(system_clock::now() + seconds(30)); /* TODO load timeout configurable? */

    if(info_future.succeeded() && info_future.get()) {
        auto result = *info_future.get();

        /* create the json string */
        {
            Json::Value root;
            root[Json::StaticString("type")] = result->type;
            root[Json::StaticString("url")] = result->url;
            if(result->type == UrlType::TYPE_PLAYLIST) {
                auto casted = static_pointer_cast<UrlPlaylistInfo>(result);
            } else if(result->type == UrlType::TYPE_STREAM || result->type == UrlType::TYPE_VIDEO) {
                auto casted = static_pointer_cast<UrlSongInfo>(result);
                root[Json::StaticString("length")] = chrono::ceil<chrono::milliseconds>(casted->length).count();
                root[Json::StaticString("title")] = casted->title;
                root[Json::StaticString("description")] = casted->description;
                if(casted->thumbnail) {
                    if(auto thump = dynamic_pointer_cast<::music::ThumbnailUrl>(casted->thumbnail); thump)
                        root[Json::StaticString("thumbnail")] = thump->url();
                }
                for(const auto& meta : casted->metadata)
                    root[Json::StaticString("metadata")][meta.first] = meta.second;
            }

            Json::StreamWriterBuilder builder;
            builder["indentation"] = ""; // If you want whitespace-less output
            entry->metadata.json_string = Json::writeString(builder, root);
        }

        if(result->type == UrlType::TYPE_PLAYLIST) {
            auto playlist = static_pointer_cast<UrlPlaylistInfo>(result);
            debugMessage(this->get_server_id(), "[PlayList] Song {} shows up as a playlist. Inserting entries {}", entry->song_id, playlist->entries.size());

            SongId previous_id = entry->song_id;
            size_t current_songs = this->list_songs().size();
            auto max_songs = this->max_songs();

            for(const auto& element : playlist->entries) {
                if(max_songs != -1 && max_songs < /* = (+1 because we delete the main playlist) */ current_songs) {
                    logMessage(this->get_server_id(), "[PlayList][{}] Added playlist {} contains more songs than allowed by setting. Dropping the rest.", this->playlist_id(), element->url);
                    break;
                }
                auto queued_element = this->add_song(entry->invoker, element->url, "", previous_id);
                if(!queued_element) continue; //TODO log insert fail?

                previous_id = queued_element->song_id;
            }

            this->delete_song(entry->song_id); /* delete playlist song entry because it has been resolved completely */
            return;
        }

        load_finished(LoadState::LOADED, "", static_pointer_cast<UrlSongInfo>(result));
        this->sql_apply_changes(entry);
    } else {
        load_finished(LoadState::LOAD_ERROR, info_future.failed() ? "failed to query info: " + info_future.errorMegssage() : "info load timeout", nullptr);
        return;
    }
}

void Playlist::add_subscriber(const std::shared_ptr<server::ConnectedClient> &client) {
    std::lock_guard slock{this->subscriber_lock};
    this->subscribers.push_back(client);
}

void Playlist::remove_subscriber(const std::shared_ptr<server::ConnectedClient> &tclient) {
    std::lock_guard slock{this->subscriber_lock};
    this->subscribers.erase(std::remove_if(this->subscribers.begin(), this->subscribers.end(), [&](const std::weak_ptr<server::ConnectedClient>& wclient) {
        server::ConnectedLockedClient client{wclient.lock()};
        if(!client || client == tclient) return true;

        return false;
    }), this->subscribers.end());
}

inline bool write_song(const std::shared_ptr<PlaylistEntryInfo> &entry, command_builder& builder, size_t index) {
    if(!entry) {
        builder.put_unchecked(index, "song_id", "0");
        return true;
    }

    builder.put_unchecked(index, "song_id", entry->song_id);
    builder.put_unchecked(index, "song_previous_song_id", entry->previous_song_id);

    builder.put_unchecked(index, "song_url", entry->original_url);
    builder.put_unchecked(index, "song_url_loader", entry->url_loader);
    builder.put_unchecked(index, "song_invoker", entry->invoker);

    std::lock_guard llock{entry->metadata.load_lock};
    builder.put_unchecked(index, "song_loaded", entry->metadata.is_loaded());
    builder.put_unchecked(index, "song_metadata", entry->metadata.json_string);

    return true;
}

void Playlist::broadcast_notify(const ts::command_builder &command) {
    std::lock_guard slock{this->subscriber_lock};
    this->subscribers.erase(std::remove_if(this->subscribers.begin(), this->subscribers.end(), [&](const std::weak_ptr<server::ConnectedClient>& wclient) {
        server::ConnectedLockedClient client{wclient.lock()};
        if(!client) return true;

        client->sendCommand(command);
        return false;
    }), this->subscribers.end());
}

bool Playlist::notify_song_add(const std::shared_ptr<PlaylistEntryInfo> &entry) {
    command_builder result{"notifyplaylistsongadd"};

    result.put_unchecked(0, "playlist_id", this->playlist_id());
    write_song(entry, result, 0);

    this->broadcast_notify(result);
    return true;
}

bool Playlist::notify_song_remove(const std::shared_ptr<PlaylistEntryInfo> &entry) {
    command_builder result{"notifyplaylistsongremove"};

    result.put_unchecked(0, "playlist_id", this->playlist_id());
    result.put_unchecked(0, "song_id", entry->song_id);

    this->broadcast_notify(result);
    return true;
}

bool Playlist::notify_song_reorder(const std::shared_ptr<PlaylistEntryInfo> &entry) {
    command_builder result{"notifyplaylistsongreorder"};

    result.put_unchecked(0, "playlist_id", this->playlist_id());
    result.put_unchecked(0, "song_id", entry->song_id);
    result.put_unchecked(0, "song_previous_song_id", entry->previous_song_id);

    this->broadcast_notify(result);
    return true;
}

bool Playlist::notify_song_loaded(const std::shared_ptr<PlaylistEntryInfo> &entry) {
    command_builder result{"notifyplaylistsongloaded"};

    result.put_unchecked(0, "playlist_id", this->playlist_id());
    result.put_unchecked(0, "song_id", entry->song_id);


    std::lock_guard llock{entry->metadata.load_lock};
    if(entry->metadata.has_failed_to_load()) {
        result.put_unchecked(0, "success", "0");
        result.put_unchecked(0, "load_error_msg", entry->metadata.load_error);
    } else {
        result.put_unchecked(0, "success", "1");
        result.put_unchecked(0, "song_metadata", entry->metadata.json_string);
    }

    this->broadcast_notify(result);
    return true;
}