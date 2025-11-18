#include "MusicClient.h"
#include "src/client/voice/VoiceClient.h"
#include <misc/endianness.h>
#include <log/LogUtils.h>
#include <ThreadPool/Timer.h>

using namespace std;
using namespace std::chrono;
using namespace ts;
using namespace ts::server;

MusicClient::loader_t MusicClient::failedLoader(const std::string &message) {
    return make_shared<music::PlayableSong::song_loader_t>([message] (const shared_ptr<VirtualServer>, const shared_ptr<music::PlayableSong>&, std::string& error) mutable -> std::shared_ptr<music::PlayableSong::LoadedData> {
        error = message;
        return nullptr;
    });
}

MusicClient::loader_t MusicClient::providerLoader(const std::string &name, shared_ptr<::music::manager::PlayerProvider> provider) {
    return make_shared<music::PlayableSong::song_loader_t>([name, provider, used_provider = std::move(provider)] (const shared_ptr<VirtualServer> server, const shared_ptr<music::PlayableSong>& entry, std::string& error) mutable -> std::shared_ptr<music::PlayableSong::LoadedData> {
        if(!used_provider)
            used_provider = ::music::manager::resolveProvider(name, entry->getUrl());

        if(!used_provider){
            error = "Could not resolve '" + name + "' provider!";
            return nullptr;
        }

        auto promise = used_provider->createPlayer(entry->getUrl(), nullptr, &*server);
        auto player = promise.waitAndGet(nullptr, system_clock::now() + seconds(60));
        if(!player) {
            if(promise.state() == threads::FutureState::WORKING)
                error = "load timeout";
            else
                error = promise.errorMegssage();
            return nullptr;
        }

        {
            auto data = make_shared<music::PlayableSong::LoadedData>();
            data->player = player;
            data->title = player->songTitle();
            data->description = player->songDescription();
            data->length = player->length();

            for(const auto& thumbnail : player->thumbnails()) {
                if(thumbnail->type() == ::music::THUMBNAIL_URL) {
                    data->thumbnail = static_pointer_cast<::music::ThumbnailUrl>(thumbnail)->url();
                    break;
                }
            }

            return data;
        }
    });
}

MusicClient::MusicClient(const std::shared_ptr<VirtualServer>& handle, const std::string& uniqueId) : ConnectedClient(handle->getSql(), handle) {
    memtrack::allocated<MusicClient>(this);
    this->state = CONNECTED;
    this->properties()[property::CLIENT_TYPE] = ClientType::CLIENT_TEAMSPEAK;
    this->properties()[property::CLIENT_TYPE_EXACT] = ClientType::CLIENT_MUSIC;
    this->properties()[property::CLIENT_UNIQUE_IDENTIFIER] = uniqueId;

    {
        //FIXME handle encoder create error, or assume that this never happens
        int error = 0;
        this->voice.encoder = opus_encoder_create(48000, 2, OPUS_APPLICATION_AUDIO, &error);
    }

    this->_player_state = SLEEPING;
}

MusicClient::~MusicClient() {
    if(this->voice.encoder) {
        opus_encoder_destroy(this->voice.encoder);
        this->voice.encoder = nullptr;
    }
    memtrack::freed<MusicClient>(this);
}

void MusicClient::sendCommand(const ts::Command &command, bool low) { }
void MusicClient::sendCommand(const ts::command_builder &command, bool low) { }

bool MusicClient::close_connection(const std::chrono::system_clock::time_point&) {
    logError(this->getServerId(), "Music manager is forced to disconnect!");

    /*
    if(this->server)
        this->server->unregisterInternalClient(static_pointer_cast<InternalClient>(this->ref()));
        */
    //TODO!
    this->properties()[property::CLIENT_ID] = 0;
    return true;
}

bool MusicClient::disconnect(const std::string &reason) {
    this->player_reset(false);
    if(this->currentChannel)
        this->properties()[property::CLIENT_LAST_CHANNEL] = this->currentChannel->channelId();
    return true;
}

bool MusicClient::notifyClientMoved(
        const std::shared_ptr<ConnectedClient> &client,
        const std::shared_ptr<BasicChannel> &target_channel,
        ViewReasonId reason,
        std::string msg,
        std::shared_ptr<ConnectedClient> invoker,
        bool lock_channel_tree) {

    if(&*client == this && target_channel)
        this->properties()[property::CLIENT_LAST_CHANNEL] = target_channel->channelId();
    return true;
}

void MusicClient::initialize_bot() {
    this->_player_state = this->properties()[property::CLIENT_PLAYER_STATE];
    if(this->_player_state == ReplayState::LOADING)
        this->_player_state = ReplayState::PLAYING;

    if(!this->properties()->has(property::CLIENT_COUNTRY) || this->properties()[property::CLIENT_COUNTRY].value().empty())
        this->properties()[property::CLIENT_COUNTRY] = config::geo::countryFlag;
    if(this->properties()[property::CLIENT_UPTIME_MODE].as_unchecked<MusicClient::UptimeMode::value>() == MusicClient::UptimeMode::TIME_SINCE_SERVER_START) {
        this->properties()[property::CLIENT_LASTCONNECTED] = duration_cast<seconds>(this->server->start_timestamp().time_since_epoch()).count();
    } else {
        this->properties()[property::CLIENT_LASTCONNECTED] = this->properties()[property::CLIENT_CREATED].value();
    }

    this->properties()[property::CLIENT_LASTCONNECTED] = this->properties()[property::CLIENT_CREATED].value();

    ChannelId last = this->properties()[property::CLIENT_LAST_CHANNEL];
    auto channel = this->server->getChannelTree()->findChannel(last);
    if(!channel) channel = this->server->getChannelTree()->getDefaultChannel();

    if(this->getClientId() == 0 || this->server->find_client_by_id(this->getClientId()) != this->ref()) {
        this->server->registerClient(this->ref());
    }
    {
        unique_lock server_channel_lock(this->server->channel_tree_mutex);
        this->server->client_move(this->ref(), channel, nullptr, "", ViewReasonId::VREASON_USER_ACTION, true, server_channel_lock);
    }
    this->playback.volume_modifier = this->properties()[property::CLIENT_PLAYER_VOLUME];
}

void MusicClient::broadcast_text_message(const std::string &message) {
    this->server->send_text_message(this->currentChannel, this->ref(), message);
}

//https://en.wikipedia.org/wiki/Decibel
void MusicClient::apply_volume(const std::shared_ptr<::music::SampleSegment> &seg) {
    for(int sample = 0; sample < seg->segmentLength; sample++){
        for(int channel = 0; channel < seg->channels; channel++){
            int16_t lane = seg->segments[sample * seg->channels + channel];
            //lane *= pow(10.f, this->_volumeModifier * .05f);
            lane *= this->playback.volume_modifier;
            seg->segments[sample * seg->channels + channel] = (int16_t) lane;
        }
    }
}

void MusicClient::add_subscriber(const std::shared_ptr<ts::server::ConnectedClient> &client) {
    lock_guard lock(this->subscriber_lock);
    this->subscribers.erase(remove_if(this->subscribers.begin(), this->subscribers.end(), [&](const std::tuple<std::weak_ptr<ConnectedClient>, system_clock::time_point>& entry) {
        auto e = get<0>(entry).lock();
        return !e || e == client;
    }), this->subscribers.end());

    this->subscribers.push_back({client, system_clock::time_point{}});
}

bool MusicClient::is_subscriber(const std::shared_ptr<ts::server::ConnectedClient> &client) {
    lock_guard lock(this->subscriber_lock);
    for(const auto& entry : this->subscribers)
        if(std::get<0>(entry).lock() == client)
            return true;
    return false;
}

void MusicClient::remove_subscriber(const std::shared_ptr<ts::server::ConnectedClient> &client) {
    lock_guard lock(this->subscriber_lock);
    this->subscribers.erase(remove_if(this->subscribers.begin(), this->subscribers.end(), [&](const std::tuple<std::weak_ptr<ConnectedClient>, system_clock::time_point>& entry) {
        auto e = get<0>(entry).lock();
        return !e || e == client;
    }), this->subscribers.end());
}

void MusicClient::changePlayerState(ReplayState state) {
    if(this->_player_state == state) return;
    this->_player_state = state;
    this->properties()[property::CLIENT_PLAYER_STATE] = state;
    this->properties()[property::CLIENT_FLAG_TALKING] = state == ReplayState::PLAYING;
    this->server->notifyClientPropertyUpdates(this->ref(), deque<property::ClientProperties>{property::CLIENT_PLAYER_STATE});
}

void MusicClient::notifySongChange(const std::shared_ptr<music::SongInfo>& song) {
    this->server->forEachClient([&](shared_ptr<ConnectedClient> client) {
        client->notifyMusicPlayerSongChange(dynamic_pointer_cast<MusicClient>(this->ref()), song);
    });
}

void MusicClient::handle_event_song_ended() {
    auto playlist = this->playlist();
    if(playlist) playlist->next();
    this->replay_next_song();
}

void MusicClient::handle_event_song_dry() {
    this->replay_next_song();
}

void MusicClient::handle_event_song_replay_failed() {
    auto playlist = this->playlist();
    if(playlist) playlist->next();
    this->replay_next_song();
}

void MusicClient::replay_next_song() {
    auto playlist = this->playlist();
    if(playlist) {
        bool await_load;
        if(auto song = playlist->current_song(await_load); song)
            this->replay_song(song);
        else if(!await_load)
            playlist->next();
    } else
        this->replay_song(nullptr);
}