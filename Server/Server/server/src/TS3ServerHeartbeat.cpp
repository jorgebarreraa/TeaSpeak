#include <cstring>
#include <protocol/buffers.h>
#include "client/voice/VoiceClient.h"
#include <log/LogUtils.h>
#include "InstanceHandler.h"
#include "VirtualServer.h"
#include "./manager/ConversationManager.h"
#include "./music/MusicBotManager.h"
#include "./groups/GroupManager.h"

using namespace std;
using namespace std::chrono;
using namespace ts::server;
using namespace ts::protocol;
using namespace ts::buffer;

inline void banClientFlood(VirtualServer* server, const shared_ptr<ConnectedClient>& cl, time_point<system_clock> until){
    auto time = until.time_since_epoch().count() == 0 ? 0L : chrono::ceil<chrono::seconds>(until - system_clock::now()).count();

    std::string reason{"You're flooding too much"};
    serverInstance->banManager()->registerBan(server->getServerId(), cl->getClientDatabaseId(), reason, cl->getUid(), cl->getLoggingPeerIp(), "", "", until);

    for(const auto &client : server->findClientsByUid(cl->getUid())) {
        server->notify_client_ban(client, server->getServerRoot(), reason, time);
        client->close_connection(system_clock::now() + seconds(1));
    }
}

#define BEGIN_TIMINGS() timing_begin = system_clock::now()
#define END_TIMINGS(variable) \
timing_end = system_clock::now(); \
variable = duration_cast<decltype(variable)>(timing_end - timing_begin);

void VirtualServer::executeServerTick() {
    std::shared_lock state_lock{this->state_mutex};
    if(!this->running()) {
        return;
    }

    auto tick_timestamp = std::chrono::system_clock::now();
    try {
        if(this->lastTick.time_since_epoch().count() > 0) {
            auto delay = tick_timestamp - this->lastTick;
            auto delay_ms = std::chrono::duration_cast<std::chrono::milliseconds>(delay).count();
            if(delay_ms > 510) {
                if(delay_ms < 750) {
                    logWarning(this->getServerId(),
                            "Found variances within the server tick! (Supposed: 500ms Hold: {}ms)", delay_ms);
                } else {
                    logError(this->getServerId(),
                            "Found variances within the server tick! This long delay could be an issue. (Supposed: 500ms Hold: {}ms)", delay_ms);
                }
            }
        }
        this->lastTick = tick_timestamp;

        system_clock::time_point timing_begin, timing_end;
        milliseconds timing_update_states, timing_client_tick, timing_channel, timing_statistic, timing_groups, timing_ccache, music_manager;

        auto client_list = this->getClients();

        {
            BEGIN_TIMINGS();

            size_t clientOnline{0};
            size_t queryOnline{0};
            for(const auto& conn : client_list){
                switch(conn->connectionState()) {
                    case ConnectionState::CONNECTED:
                    case ConnectionState::INIT_HIGH:
                        break;

                    case ConnectionState::INIT_LOW:
                    case ConnectionState::DISCONNECTING:
                    case ConnectionState::DISCONNECTED:
                        continue;

                    case ConnectionState::UNKNWON:
                    default:
                        assert(false);
                        continue;
                }

                switch (conn->getType()) {
                    case ClientType::CLIENT_TEAMSPEAK:
                    case ClientType::CLIENT_TEASPEAK:
                    case ClientType::CLIENT_WEB:
                        clientOnline++;
                        break;

                    case ClientType::CLIENT_QUERY:
                    case ClientType::CLIENT_MUSIC:
                        queryOnline++;
                        break;

                    case ClientType::CLIENT_INTERNAL:
                    case ClientType::MAX:
                    default:
                        break;
                }
            }

            properties()[property::VIRTUALSERVER_UPTIME] = std::chrono::duration_cast<std::chrono::seconds>(tick_timestamp - this->startTimestamp).count();
            properties()[property::VIRTUALSERVER_CLIENTS_ONLINE] = clientOnline + queryOnline;
            properties()[property::VIRTUALSERVER_QUERYCLIENTS_ONLINE] = queryOnline;
            if(clientOnline + queryOnline == 0) {
                //We don't need to tick, when server is empty!
                return;
            }

            properties()[property::VIRTUALSERVER_CHANNELS_ONLINE] = this->channelTree->channel_count();
            properties()[property::VIRTUALSERVER_TOTAL_PING] = this->generate_network_report().average_ping;
            END_TIMINGS(timing_update_states);
        }
        
        {
            BEGIN_TIMINGS();

            auto flood_decrease = this->properties()[property::VIRTUALSERVER_ANTIFLOOD_POINTS_TICK_REDUCE].as_or<FloodPoints>(0);
            auto flood_block = this->properties()[property::VIRTUALSERVER_ANTIFLOOD_POINTS_NEEDED_IP_BLOCK].as_or<FloodPoints>(0);

            bool flag_update_spoken = this->spoken_time_timestamp + seconds(30) < system_clock::now();

            system_clock::time_point tick_client_begin, tick_client_end = system_clock::now();
            for(const auto& cl : client_list) {
                tick_client_begin = tick_client_end;
                if(cl->server != this) {
                    logError(this->getServerId(), "Got registered client, but client does not think hes bound to this server!");
                    std::unique_lock tree_lock{this->channel_tree_mutex};
                    this->unregisterClient(cl, "invalid server handle", tree_lock);
                    continue;
                }

                if(cl->floodPoints > flood_block){
                    if(!cl->ignoresFlood()) {
                        banClientFlood(this, cl, system_clock::now() + minutes(10));
                        continue;
                    }
                }

                if(cl->floodPoints > flood_decrease) {
                    cl->floodPoints -= flood_decrease;
                } else {
                    cl->floodPoints = 0;
                }

                cl->tick_server(tick_client_end);
                auto voice = dynamic_pointer_cast<SpeakingClient>(cl);
                if(flag_update_spoken && voice) {
                    this->spoken_time += voice->takeSpokenTime();
                }
                tick_client_end = system_clock::now();

                auto passed_time = tick_client_end - tick_client_begin;
                if(passed_time > microseconds{2500}) {
                    if(passed_time > milliseconds{10}) {
                        logError(this->serverId, "Ticking of client {:1} ({:2}) needs more that 2500 microseconds! ({:3} microseconds)",
                                 cl->getLoggingPeerIp() + ":" + to_string(cl->getPeerPort()),
                                 cl->getDisplayName(),
                                 duration_cast<microseconds>(tick_client_end - tick_client_begin).count()
                        );
                    } else {
                        logWarning(this->serverId, "Ticking of client {:1} ({:2}) needs more that 2500 microseconds! ({:3} microseconds)",
                                 cl->getLoggingPeerIp() + ":" + to_string(cl->getPeerPort()),
                                 cl->getDisplayName(),
                                 duration_cast<microseconds>(tick_client_end - tick_client_begin).count()
                        );
                    }
                }

                auto client_permissions = cl->clientPermissions;
                if(client_permissions->require_db_updates()) {
                    auto begin = system_clock::now();
                    serverInstance->databaseHelper()->saveClientPermissions(this->ref(), cl->getClientDatabaseId(), client_permissions);
                    auto end = system_clock::now();
                    debugMessage(this->serverId, "Saved client permissions for client {} ({}) in {}ms", cl->getClientDatabaseId(), cl->getDisplayName(), duration_cast<milliseconds>(end - begin).count());
                }
            }

            if(flag_update_spoken) {
                this->spoken_time_timestamp = system_clock::now();
            }

            END_TIMINGS(timing_client_tick);
        }


        {
            BEGIN_TIMINGS();

            std::unique_lock channel_lock{this->channel_tree_mutex};

            auto channels = this->channelTree->channels();
            for(const auto& channel : channels) {
                auto server_channel = dynamic_pointer_cast<ServerChannel>(channel);
                assert(server_channel);

                if(channel->channelType() == ChannelType::temporary) {
                    if(server_channel->client_count() > 0 || !this->isChannelRootEmpty(channel, false)) {
                        continue;
                    }

                    /* seconds */
                    auto channel_delete_timeout = channel->properties()[property::CHANNEL_DELETE_DELAY].as_or<uint64_t>(0);
                    auto empty_seconds = channel->empty_seconds();

                    if(empty_seconds > channel_delete_timeout) {
                        this->delete_channel(server_channel, this->serverRoot, "temporary auto delete", channel_lock, true);
                        if(!channel_lock.owns_lock()) {
                            channel_lock.lock();
                        }

                        /* no need to tick the channel any more since it has been deleted */
                        continue;
                    }
                }

                {
                    auto permission_manager = channel->permissions();
                    if(permission_manager->require_db_updates()) {
                        auto begin = system_clock::now();
                        serverInstance->databaseHelper()->saveChannelPermissions(this->ref(), channel->channelId(), permission_manager);
                        auto end = system_clock::now();
                        debugMessage(this->serverId, "Saved channel permissions for channel {} ({}) in {}ms", channel->channelId(), channel->name(), duration_cast<milliseconds>(end - begin).count());
                    }
                }
            }

            END_TIMINGS(timing_channel);
        }

        {
            BEGIN_TIMINGS();

            this->server_statistics_->tick();
            {
                lock_guard lock{this->join_attempts_lock};
                if(tick_timestamp > this->join_last_decrease + seconds(5)) {
                    std::erase_if(this->join_attempts, [](auto& entry) {
                        return --entry.second <= 0;
                    });

                    this->join_last_decrease = tick_timestamp;
                }
            }
            END_TIMINGS(timing_statistic);
        }

        {
            BEGIN_TIMINGS();
            this->group_manager()->save_permissions();
            END_TIMINGS(timing_groups);
        }

        {
            BEGIN_TIMINGS();
            if(this->conversation_cache_cleanup_timestamp + minutes(15) < system_clock::now()) {
                debugMessage(this->serverId, "Cleaning up conversation cache.");
                this->conversation_manager_->cleanup_cache();
                conversation_cache_cleanup_timestamp = system_clock::now();
            }
            END_TIMINGS(timing_ccache);
        }

        {
            BEGIN_TIMINGS();
            this->music_manager_->execute_tick();
            END_TIMINGS(music_manager);
        }

        if(system_clock::now() - lastTick > milliseconds(100)) {
            //milliseconds timing_update_states, timing_client_tick, timing_channel, timing_statistic;
            logError(this->serverId, "Server tick took to long ({}ms => Status updates: {}ms Client tick: {}ms, Channel tick: {}ms, Statistic tick: {}ms, Groups: {}ms, Conversation cache: {}ms)",
                    duration_cast<milliseconds>(system_clock::now() - lastTick).count(),
                     timing_update_states.count(),
                     timing_client_tick.count(),
                     timing_channel.count(),
                     timing_statistic.count(),
                     timing_groups.count(),
                     timing_ccache.count()
            );
        }
    } catch (std::exception& ex) {
        logCritical(this->serverId, "Failed to tick server! Got exception message: {}", ex.what());
    }
}