#include <src/VirtualServer.h>
#include <misc/rnd.h>
#include <misc/digest.h>
#include <misc/base64.h>
#include <src/client/music/MusicClient.h>
#include <src/InstanceHandler.h>
#include <algorithm>
#include <log/LogUtils.h>

using namespace std;
using namespace std::chrono;
using namespace ts;
using namespace ts::server;
using namespace ts::music;

extern InstanceHandler* serverInstance;
threads::ThreadPool MusicBotManager::tick_music{config::threads::music::execute_per_bot, "music tick "};
threads::ThreadPool MusicBotManager::load_music{4, "music loader "};

void MusicBotManager::adjustTickPool() {
    size_t bots = 0;
    for(const auto& server : serverInstance->getVoiceServerManager()->serverInstances()) bots += server->music_manager_->current_bot_count();
    if(bots == 0)
        tick_music.setThreads(1);
    else
        tick_music.setThreads(min(config::threads::music::execute_limit, bots * config::threads::music::execute_per_bot));
}

void MusicBotManager::shutdown() {
    tick_music.shutdown();
    load_music.shutdown();
}

MusicBotManager::MusicBotManager(const shared_ptr<server::VirtualServer>& server) : handle(server) { }

MusicBotManager::~MusicBotManager() { }

void MusicBotManager::cleanup_semi_bots() {
    for(const auto& bot : this->available_bots())
        if(bot->get_bot_type() == MusicClient::Type::SEMI_PERMANENT || bot->get_bot_type() == MusicClient::Type::TEMPORARY)
            this->deleteBot(bot);
}

void MusicBotManager::cleanup_client_bots(ts::ClientDbId clientid) {
    for(const auto& bot : this->available_bots())
        if(bot->get_bot_type() == MusicClient::Type::TEMPORARY && bot->getOwner() == clientid)
            this->deleteBot(bot);
}

std::deque<std::shared_ptr<ts::server::MusicClient>> MusicBotManager::available_bots() {
    lock_guard lock(music_bots_lock);
    return this->music_bots;
}

std::shared_ptr<ts::server::MusicClient> MusicBotManager::find_bot_by_playlist(const std::shared_ptr<ts::music::PlayablePlaylist> &playlist) {
    for(const auto& bot : this->available_bots())
        if(bot->playlist() == playlist)
            return bot;
    return nullptr;
}

std::deque<std::shared_ptr<ts::server::MusicClient>> MusicBotManager::listBots(ClientDbId clid) {
    lock_guard lock(music_bots_lock);
    std::deque<std::shared_ptr<server::MusicClient>> res;
    for(const auto& bot : this->music_bots)
        if(bot->properties()[property::CLIENT_OWNER] == clid) res.push_back(bot);
    return res;
}

std::shared_ptr<server::MusicClient> MusicBotManager::createBot(ClientDbId owner) {
    if(!config::license->isPremium()) {
        if(this->current_bot_count() >= this->max_bots()) return nullptr; //Test the license
    }

    auto handle = this->handle.lock();
    assert(handle);

    auto uid = base64::encode(digest::sha1("music#" + rnd_string(15)));
    auto musicBot = make_shared<MusicClient>(this->handle.lock(), uid);
    musicBot->initialize_weak_reference(musicBot);
    musicBot->manager = this;
    musicBot->server = handle;
    DatabaseHelper::assignDatabaseId(handle->getSql(), handle->getServerId(), musicBot);
    if(config::music::enabled) {
        lock_guard lock(this->music_bots_lock);
        this->music_bots.push_back(musicBot);
    }
    (LOG_SQL_CMD)(sql::command(handle->getSql(), "INSERT INTO `musicbots` (`serverId`, `botId`, `uniqueId`, `owner`) VALUES (:sid, :botId, :uid, :owner)",
                                                     variable{":sid", handle->getServerId()}, variable{":botId", musicBot->getClientDatabaseId()}, variable{":uid", musicBot->getUid()}, variable{":owner", owner}).execute());
    musicBot->properties()[property::CLIENT_OWNER] = owner;
    musicBot->setDisplayName("Im a music bot!");
    musicBot->properties()[property::CLIENT_LASTCONNECTED] = duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
    musicBot->properties()[property::CLIENT_CREATED] = duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
    musicBot->properties()[property::CLIENT_VERSION] = "TeaMusic";
    musicBot->properties()[property::CLIENT_PLATFORM] = "internal";


    if(!config::music::enabled) return nullptr;
    handle->registerClient(musicBot);

    {
        auto playlist = this->create_playlist(owner, "owned via bot");
        playlist->properties()[property::PLAYLIST_TYPE] = Playlist::Type::BOT_BOUND;
        musicBot->set_playlist(playlist);
    }

    MusicBotManager::adjustTickPool();
    return musicBot;
}

bool MusicBotManager::assign_playlist(const std::shared_ptr<ts::server::MusicClient> &bot, const std::shared_ptr<ts::music::PlayablePlaylist> &playlist) {
    lock_guard lock(music_bots_lock);

    if(playlist) {
        auto assigned_bot = this->find_bot_by_playlist(playlist);
        if(assigned_bot)
            return assigned_bot == bot;
    }

    auto old_playlist = bot->playlist();
    bot->set_playlist(playlist);

    if(old_playlist && old_playlist->playlist_type() == Playlist::Type::BOT_BOUND) {
        string error;
        if(!this->delete_playlist(old_playlist->playlist_id(), error)) {
            logError(this->ref_server()->getServerId(), "Failed to delete music bot bound playlist. Error: {}", error);
        }
    }

    return true;
}

void MusicBotManager::deleteBot(std::shared_ptr<server::MusicClient> musicBot) {
    {
        lock_guard lock(this->music_bots_lock);
        auto found = find(this->music_bots.begin(), this->music_bots.end(), musicBot);
        if(found == this->music_bots.end()) return;
        this->music_bots.erase(found);
    }
    auto handle = this->handle.lock();
    assert(handle);

    MusicBotManager::adjustTickPool();
    {
        unique_lock server_channel_lock(handle->channel_tree_mutex);
        handle->client_move(musicBot, nullptr, nullptr, "Music bot deleted", ViewReasonId::VREASON_SERVER_LEFT, true, server_channel_lock);
        handle->unregisterClient(musicBot, "bot deleted", server_channel_lock);
    }
    serverInstance->databaseHelper()->deleteClient(handle, musicBot->getClientDatabaseId());
    serverInstance->databaseHelper()->deleteClient(nullptr, musicBot->getClientDatabaseId());

    this->assign_playlist(musicBot, nullptr); /* remove any playlists */
    /*
    auto playlist = musicBot->playlist();
    if(playlist && playlist->playlist_type() == Playlist::Type::BOT_BOUND) {
        string error;
        if(!this->delete_playlist(playlist->playlist_id(), error)) {
            logError(this->ref_server()->getServerId(), "Failed to delete music bot bound playlist. Error: {}", error);
        }
    }
     */

    sql::command(handle->getSql(), "DELETE FROM `musicbots` WHERE `serverId` = :sid AND `botId` = :bid", variable{":sid", handle->getServerId()}, variable{":bid", musicBot->getClientDatabaseId()}).executeLater().waitAndGetLater(LOG_SQL_CMD,{-1,"future failed"});

    std::thread([musicBot]{
        musicBot->player_reset(false);
    }).detach();
}

int MusicBotManager::max_bots() {
    int bots = this->handle.lock()->properties()[property::VIRTUALSERVER_MUSIC_BOT_LIMIT];
    if(!config::license->isPremium())
        return bots == 0 ? 0 : 1;
    return bots;
}

int MusicBotManager::current_bot_count() {
    return this->music_bots.size();
}

std::shared_ptr<server::MusicClient> MusicBotManager::findBotById(ClientDbId id) {
    lock_guard lock(music_bots_lock);
    for(const auto& bot : this->music_bots)
        if(bot->getClientDatabaseId() == id) return bot;
    return nullptr;
}

// CREATE_TABLE("musicbots", "`serverId` INT, `botId` INT, `uniqueId` TEXT, `owner` INT");
void MusicBotManager::load_bots() {
    if(!config::music::enabled) return;

    auto handle = this->handle.lock();
    assert(handle);

    auto res = sql::command(handle->getSql(), "SELECT `botId`, `uniqueId`, `owner` FROM `musicbots` WHERE `serverId` = :sid", variable{":sid", handle->getServerId()}).query(&MusicBotManager::sqlCreateMusicBot, this);
    (LOG_SQL_CMD)(res);
    MusicBotManager::adjustTickPool();

    auto bot_limit = this->max_bots();
    if(bot_limit >= 0) {
        size_t disabled_bots = 0;
        for(const auto& bot : this->music_bots) {
            if(bot_limit >= 1) {
                if(!bot->properties()[property::CLIENT_DISABLED].as_unchecked<bool>())
                    bot_limit--;
            } else {
                //TODO log message
                bot->properties()[property::CLIENT_DISABLED] = true;
                disabled_bots++;
            }
        }
        if(disabled_bots > 0){
            logMessage(handle->getServerId(), "[Music] Disabled {} music bots, because they exceed tha max music bot limit", disabled_bots);
        }
    }
}

int MusicBotManager::sqlCreateMusicBot(int length, std::string* values, std::string* names) {
    auto handle = this->handle.lock();
    assert(handle);

    ClientDbId botId = 0, owner = 0;
    std::string uid;
    for(int index = 0; index < length; index++)
        if(names[index] == "botId")
            botId = stoll(values[index]);
        else if(names[index] == "uniqueId")
            uid = values[index];
        else if(names[index] == "owner")
            owner = stoll(values[index]);
    if(botId == 0 || uid.empty()) return 0;

    auto musicBot = make_shared<MusicClient>(handle, uid);
    musicBot->initialize_weak_reference(musicBot);
    musicBot->manager = this;
    DatabaseHelper::assignDatabaseId(handle->getSql(), handle->getServerId(), musicBot);
    musicBot->properties()[property::CLIENT_OWNER] = owner;
    if(musicBot->properties()[property::CLIENT_UPTIME_MODE] == MusicClient::UptimeMode::TIME_SINCE_SERVER_START) {
        musicBot->properties()[property::CLIENT_LASTCONNECTED] = duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
    }

    if(musicBot->getClientDatabaseId() != botId) logCritical(handle->getServerId(),"Invalid music bot id mapping!");

    {
        auto playlist = this->find_playlist(musicBot->properties()[property::CLIENT_PLAYLIST_ID]);
        if(!playlist) {
            debugMessage(this->ref_server()->getServerId(), "Bot {} hasn't a valid playlist ({}). Creating a new one", musicBot->getClientDatabaseId(), musicBot->properties()[property::CLIENT_PLAYLIST_ID].value());
            playlist = this->create_playlist(0, "Music Manager");
            playlist->properties()[property::PLAYLIST_TYPE] = Playlist::Type::BOT_BOUND;
        }
        playlist->load_songs();
        musicBot->set_playlist(playlist);
    }

    {
        lock_guard lock(this->music_bots_lock);
        this->music_bots.push_back(musicBot);
    }

    return 0;
}

void MusicBotManager::connectBots() {
    for(const auto& bot : this->music_bots) {
        bot->server = this->handle.lock();
        if(!bot->currentChannel || bot->getClientId() == 0)
            bot->initialize_bot();
    }
}

void MusicBotManager::disconnectBots() {
    auto handle = this->handle.lock();
    assert(handle);

    for(const auto& bot : this->music_bots) {
        if(bot->currentChannel) {
            unique_lock server_channel_lock(handle->channel_tree_mutex);
            handle->client_move(bot, nullptr, nullptr, "Music bot deleted", ViewReasonId::VREASON_SERVER_LEFT, true, server_channel_lock);
        }
        bot->server = nullptr;
    }
}

void MusicBotManager::load_playlists() {
    if(!config::music::enabled) return;

    lock_guard playlist_lock(this->playlists_lock);
    auto sql_result = sql::command(this->ref_server()->getSql(), "SELECT `playlist_id` FROM `playlists` WHERE `serverId` = :server_id", variable{":server_id", this->ref_server()->getServerId()}).query([&](int length, string* values, string* names){
        if(length != 1) return;

        PlaylistId playlist_id = 0;
        try {
            playlist_id = stoll(values[0]);
        } catch(const std::exception& ex) {
            logError(this->ref_server()->getServerId(), "Failed to parse playlist id from database. ID: {}", values[0]);
            return;
        }

        for(const auto& playlist : this->playlists_list) {
            if(playlist->playlist_id() == playlist_id) {
                logError(this->ref_server()->getServerId(), "Duplicated playlist it's. ({})", playlist_id);
                return;
            }
        }

        auto properties = serverInstance->databaseHelper()->loadPlaylistProperties(this->ref_server(), playlist_id);
        auto permissions = serverInstance->databaseHelper()->loadPlaylistPermissions(this->ref_server(), playlist_id);
        auto playlist = make_shared<PlayablePlaylist>(this->ref(), properties, permissions);
        playlist->_self = playlist;
        playlist->load_songs();
        this->playlists_list.push_back(playlist);

        if(playlist->playlist_id() > this->playlists_index)
            this->playlists_index = playlist->playlist_id();
    });
    LOG_SQL_CMD(sql_result);
}

std::shared_ptr<PlayablePlaylist> MusicBotManager::find_playlist(ts::PlaylistId id) {
    lock_guard lock(this->playlists_lock);
    for(const auto& playlist : this->playlists_list)
        if(playlist->playlist_id() == id)
            return playlist;
    return nullptr;
}

std::deque<std::shared_ptr<PlayablePlaylist>> MusicBotManager::find_playlists_by_client(ClientDbId client_dbid) {
    std::deque<std::shared_ptr<PlayablePlaylist>> result;

    {
        lock_guard lock(this->playlists_lock);
        for(const auto& playlist : this->playlists_list)
            if(playlist->properties()[property::PLAYLIST_OWNER_DBID] == client_dbid)
                result.push_back(playlist);
    }

    return result;
}

//playlists => "`serverId` INT NOT NULL, `playlist_id` INT"
std::shared_ptr<PlayablePlaylist> MusicBotManager::create_playlist(ts::ClientDbId owner_id, const std::string &owner_name) {
    auto playlist_id = ++this->playlists_index;
    auto sql_result = sql::command(this->ref_server()->getSql(), "INSERT INTO `playlists` (`serverId`, `playlist_id`) VALUES (:server_id, :playlist_id)",
                 variable{":server_id", this->ref_server()->getServerId()},
                 variable{":playlist_id", playlist_id}
    ).execute();
    LOG_SQL_CMD(sql_result);

    if(!sql_result)
        return nullptr;

    auto properties = serverInstance->databaseHelper()->loadPlaylistProperties(this->ref_server(), playlist_id);
    auto permissions = serverInstance->databaseHelper()->loadPlaylistPermissions(this->ref_server(), playlist_id);
    auto playlist = make_shared<PlayablePlaylist>(this->ref(), properties, permissions);
    playlist->_self = playlist;
    playlist->load_songs();
    {
        lock_guard playlist_lock(this->playlists_lock);
        this->playlists_list.push_back(playlist);
    }
    playlist->properties()[property::PLAYLIST_OWNER_DBID] = owner_id;
    playlist->properties()[property::PLAYLIST_OWNER_NAME] = owner_name;

    return playlist;
}

bool MusicBotManager::delete_playlist(ts::PlaylistId id, std::string &error) {
    unique_lock playlist_lock(this->playlists_lock);
    auto playlist_entry = this->find_playlist(id);

    for(const auto& bot : this->available_bots()) {
        if(bot->playlist() == playlist_entry) {
            error = "bot " + to_string(bot->getClientDatabaseId()) + " is using this playlist";
            return false;
        }
    }

    auto it = find(this->playlists_list.begin(), this->playlists_list.end(), playlist_entry);
    if(it != this->playlists_list.end())
        this->playlists_list.erase(it);
    playlist_lock.unlock();

    if(!serverInstance->databaseHelper()->deletePlaylist(this->ref_server(), id)) {
        error = "database deletion failed";
        return false;
    }
    return true;
}

void MusicBotManager::execute_tick() {
    auto vs = this->handle.lock();
    if(!vs) return;

    unique_lock playlist_lock(this->playlists_lock);
    auto playlists = this->playlists_list;
    playlist_lock.unlock();

    auto db_helper = serverInstance->databaseHelper();
    for(auto& playlist : playlists)
        db_helper->savePlaylistPermissions(vs, playlist->playlist_id(), playlist->permission_manager());
}