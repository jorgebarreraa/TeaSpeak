#include "Properties.h"
#include "query/Command.h"
#include <algorithm>
#include <zstd.h>
#include <src/server/QueryServer.h>
#include <src/VirtualServerManager.h>
#include <src/InstanceHandler.h>
#include <log/LogUtils.h>
#include <misc/digest.h>
#include "QueryClient.h"
#include <misc/base64.h>
#include <src/ShutdownHelper.h>
#include <ThreadPool/Timer.h>
#include <numeric>
#include "src/manager/ActionLogger.h"
#include "../../groups/GroupManager.h"
#include "src/client/command_handler/helpers.h"

using namespace std;
using namespace std::chrono;
using namespace ts;
using namespace ts::server;

QueryClientCommandHandler::QueryClientCommandHandler(const std::shared_ptr<QueryClient> &client) : client_ref{client} {}

bool QueryClientCommandHandler::handle_command(const std::string_view &command) {
    auto client = this->client_ref.lock();
    if(!client) {
        return false;
    }

    if(command.empty() || command.find_first_not_of(' ') == std::string::npos) {
        logTrace(LOG_QUERY, "[{}:{}] Got query idle command.", client->getLoggingPeerIp(), client->getPeerPort());
        client->resetIdleTime();
        return true;
    }

    unique_ptr<Command> cmd;
    command_result error{};
    try {
        cmd = make_unique<Command>(Command::parse(command, true, !ts::config::server::strict_ut8_mode));
    } catch(std::invalid_argument& ex) {
        logTrace(LOG_QUERY, "[{}:{}] Failed to parse command (invalid argument): {}", client->getLoggingPeerIp(), client->getPeerPort(), command);
        error.reset(command_result{error::parameter_convert});
        goto handle_error;
    } catch(std::exception& ex) {
        logTrace(LOG_QUERY, "[{}:{}] Failed to parse command (exception: {}): {}", client->getLoggingPeerIp(), client->getPeerPort(), ex.what(), command);
        error.reset(command_result{error::vs_critical, std::string{ex.what()}});
        goto handle_error;
    }

    try {
        std::lock_guard execute_lock{client->command_lock};
        if(client->state >= ConnectionState::DISCONNECTING) {
            return false;
        }

        client->handleCommandFull(*cmd);
    } catch(std::exception& ex) {
        error.reset(command_result{error::vs_critical, std::string{ex.what()}});
        goto handle_error;
    }
    return true;

    handle_error:
    client->notifyError(error);
    error.release_data();

    return true;
}

constexpr unsigned int string_hash(const char* str, unsigned int h = 0) {
    return !str[h] ? 5381 : (string_hash(str, h + 1U) * 33U) ^ (unsigned int) str[h];
}

command_result QueryClient::handleCommand(Command& cmd) {
    /*
        if (cmd.command() == "exit" || cmd.command() == "quit") return this->handleCommandExit(cmd);
        else if (cmd.command() == "use" || cmd.command() == "serverselect") return this->handleCommandServerSelect(cmd);
        else if (cmd.command() == "serverinfo") return this->handleCommandServerInfo(cmd);
        else if (cmd.command() == "channellist") return this->handleCommandChannelList(cmd);
        else if (cmd.command() == "login") return this->handleCommandLogin(cmd);
        else if (cmd.command() == "logout") return this->handleCommandLogout(cmd);
        else if (cmd.command() == "join") return this->handleCommandJoin(cmd);
        else if (cmd.command() == "left") return this->handleCommandLeft(cmd);
        else if (cmd.command() == "globalmessage" || cmd.command() == "gm") return this->handleCommandGlobalMessage(cmd);

        else if (cmd.command() == "serverlist") return this->handleCommandServerList(cmd);
        else if (cmd.command() == "servercreate") return this->handleCommandServerCreate(cmd);
        else if (cmd.command() == "serverstart") return this->handleCommandServerStart(cmd);
        else if (cmd.command() == "serverstop") return this->handleCommandServerStop(cmd);
        else if (cmd.command() == "serverdelete") return this->handleCommandServerDelete(cmd);
        else if (cmd.command() == "serveridgetbyport") return this->handleCommandServerIdGetByPort(cmd);
        else if (cmd.command() == "instanceinfo") return this->handleCommandInstanceInfo(cmd);
        else if (cmd.command() == "instanceedit") return this->handleCommandInstanceEdit(cmd);
        else if (cmd.command() == "hostinfo") return this->handleCommandHostInfo(cmd);

        else if (cmd.command() == "bindinglist") return this->handleCommandBindingList(cmd);

        else if (cmd.command() == "serversnapshotdeploy") return this->handleCommandServerSnapshotDeploy(cmd);
        else if (cmd.command() == "serversnapshotcreate") return this->handleCommandServerSnapshotCreate(cmd);

        else if (cmd.command() == "serverprocessstop") return this->handleCommandServerProcessStop(cmd);

        else if (cmd.command() == "servernotifyregister") return this->handleCommandServerNotifyRegister(cmd);
        else if (cmd.command() == "servernotifylist") return this->handleCommandServerNotifyList(cmd);
        else if (cmd.command() == "servernotifyunregister") return this->handleCommandServerNotifyUnregister(cmd);
     */
    auto command = cmd.command();
    auto command_hash = string_hash(command.c_str());
    switch (command_hash) {
        case string_hash("exit"):
        case string_hash("quit"):
            return this->handleCommandExit(cmd);
        case string_hash("use"):
        case string_hash("serverselect"):
            return this->handleCommandServerSelect(cmd);
        case string_hash("serverinfo"):
            return this->handleCommandServerInfo(cmd);
        case string_hash("channellist"):
            return this->handleCommandChannelList(cmd);
        case string_hash("login"):
            return this->handleCommandLogin(cmd);
        case string_hash("logout"):
            return this->handleCommandLogout(cmd);
        case string_hash("globalmessage"):
        case string_hash("gm"):
            return this->handleCommandGlobalMessage(cmd);
        case string_hash("serverlist"):
            return this->handleCommandServerList(cmd);
        case string_hash("servercreate"):
            return this->handleCommandServerCreate(cmd);
        case string_hash("serverstart"):
            return this->handleCommandServerStart(cmd);
        case string_hash("serverstop"):
            return this->handleCommandServerStop(cmd);
        case string_hash("serverdelete"):
            return this->handleCommandServerDelete(cmd);
        case string_hash("serveridgetbyport"):
            return this->handleCommandServerIdGetByPort(cmd);
        case string_hash("instanceinfo"):
            return this->handleCommandInstanceInfo(cmd);
        case string_hash("instanceedit"):
            return this->handleCommandInstanceEdit(cmd);
        case string_hash("hostinfo"):
            return this->handleCommandHostInfo(cmd);
        case string_hash("bindinglist"):
            return this->handleCommandBindingList(cmd);
        case string_hash("serversnapshotdeploy"): {
            auto cmd_str = cmd.build();
            ts::command_parser parser{cmd_str};
            if(!parser.parse(true)) {
                return command_result{error::vs_critical};
            }

            return this->handleCommandServerSnapshotDeployNew(parser);
        }
        case string_hash("serversnapshotcreate"):
            return this->handleCommandServerSnapshotCreate(cmd);
        case string_hash("serverprocessstop"):
            return this->handleCommandServerProcessStop(cmd);
        case string_hash("servernotifyregister"):
            return this->handleCommandServerNotifyRegister(cmd);
        case string_hash("servernotifylist"):
            return this->handleCommandServerNotifyList(cmd);
        case string_hash("servernotifyunregister"):
            return this->handleCommandServerNotifyUnregister(cmd);
        default:
            break;
    }

    return ConnectedClient::handleCommand(cmd);
}

command_result QueryClient::handleCommandExit(Command &) {
    logMessage(LOG_QUERY, "[Query] {}:{} disconnected. (Requested by client)", this->getLoggingPeerIp(), this->getPeerPort());
    this->close_connection(system_clock::now() + seconds(1));
    return command_result{error::ok};
}

//login client_login_name=andreas client_login_password=meinPW
command_result QueryClient::handleCommandLogin(Command& cmd) {
    CMD_RESET_IDLE;

    std::string username, password;
    if(cmd[0].has("client_login_name") && cmd[0].has("client_login_password")){
        username = cmd["client_login_name"].string();
        password = cmd["client_login_password"].string();
    } else {
        username = cmd[0][0].key();
        password = cmd[0][1].key();
    }
    debugMessage(LOG_QUERY, "Having query login attempt for username {}", username);

    auto _account = serverInstance->getQueryServer()->find_query_account_by_name(username);
    auto account = _account ? serverInstance->getQueryServer()->load_password(_account) : nullptr;

    {
        std::lock_guard connect_lock{this->handle->client_connect_mutex};

        if(!account) {
            serverInstance->action_logger()->query_authenticate_logger.log_query_authenticate(this->getServerId(), std::dynamic_pointer_cast<QueryClient>(this->ref()), username, log::QueryAuthenticateResult::UNKNOWN_USER);
            return command_result{error::client_invalid_password, "username or password dose not match"};
        }

        if (account->password != password) {
            if(!this->whitelisted) {
                this->handle->client_connect_count[this->getPeerIp()]++;
                if(this->handle->client_connect_count[this->getPeerIp()] > 3) {
                    this->handle->client_connect_bans[this->getPeerIp()] = system_clock::now() + seconds(
                            serverInstance->properties()[property::SERVERINSTANCE_SERVERQUERY_BAN_TIME].as_unchecked<uint64_t>()); //TODO configurable | Disconnect all others?
                    this->postCommandHandler.emplace_back([&](){
                        this->close_connection(system_clock::now() + seconds(1));
                    });
                    return command_result{error::ban_flooding};
                }
            }

            serverInstance->action_logger()->query_authenticate_logger.log_query_authenticate(this->getServerId(), std::dynamic_pointer_cast<QueryClient>(this->ref()), username, log::QueryAuthenticateResult::INVALID_PASSWORD);
            return command_result{error::client_invalid_password, "username or password dose not match"};
        }
    }
    if(!this->properties()[property::CLIENT_LOGIN_NAME].as_unchecked<string>().empty()) {
        Command log("logout");
        auto result = this->handleCommandLogout(log);
        if(result.has_error()) {
            result.release_data();
            logError(this->getServerId(), "Query client failed to login from old login.");
            return command_result{error::vs_critical};
        }
    }

    this->query_account = account;

    auto joined_channel = this->currentChannel;
    if(this->server) {
        {
            unique_lock tree_lock(this->server->channel_tree_mutex);
            if(joined_channel) {
                this->server->client_move(this->ref(), nullptr, nullptr, "", ViewReasonId::VREASON_USER_ACTION, false, tree_lock);
            }
            this->server->unregisterClient(this->ref(), "login", tree_lock);
        }
    }

    logMessage(LOG_QUERY, "Got new authenticated client. Username: {}, Unique-ID: {}, Bounded Server: {}", account->username, account->unique_id, account->bound_server);

    this->properties()[property::CLIENT_LOGIN_NAME] = username;
    this->properties()[property::CLIENT_UNIQUE_IDENTIFIER] = account->unique_id; //TODO load from table
    this->properties()[property::CLIENT_NICKNAME] = username;

    auto target_server = this->server; /* keep the server alive 'ill we've joined the server */
    if(account->bound_server) {
        target_server = serverInstance->getVoiceServerManager()->findServerById(account->bound_server);
        if(target_server != this->server) {
            joined_channel = nullptr;
        }

        if(!target_server) {
            return command_result{error::server_invalid_id, "bound server does not exists"};
        }
    }
    this->server = target_server;

    DatabaseHelper::assignDatabaseId(this->sql, static_cast<ServerId>(target_server ? target_server->getServerId() : 0), this->ref());
    if(target_server) {
        target_server->registerClient(this->ref());

        {
            shared_lock server_tree_lock(target_server->channel_tree_mutex);
            if(joined_channel) {
                /* needs only notify if we were already on that server within a channel */
                target_server->notifyClientPropertyUpdates(this->ref(), deque<property::ClientProperties>{property::CLIENT_NICKNAME, property::CLIENT_UNIQUE_IDENTIFIER});
            }

            unique_lock client_tree_lock(this->channel_tree_mutex);
            this->channel_tree->reset();
            this->channel_tree->insert_channels(target_server->channelTree->tree_head(), true, false);
            this->subscribeChannel(this->server->channelTree->channels(), false, false);
        }

        if(joined_channel) {
            std::unique_lock tree_lock{this->server->channel_tree_mutex};
            this->server->client_move(this->ref(), joined_channel, nullptr, "", ViewReasonId::VREASON_USER_ACTION, false, tree_lock);
        } else {
            this->server->assignDefaultChannel(this->ref(), true);
        }
    } else {
        this->task_update_needed_permissions.enqueue();
    }

    this->properties()[property::CLIENT_TOTALCONNECTIONS].increment_by<uint64_t>(1);
    this->task_update_channel_client_properties.enqueue();

    serverInstance->action_logger()->query_authenticate_logger.log_query_authenticate(this->getServerId(), std::dynamic_pointer_cast<QueryClient>(this->ref()), username, log::QueryAuthenticateResult::SUCCESS);
    return command_result{error::ok};
}

command_result QueryClient::handleCommandLogout(Command &) {
    CMD_RESET_IDLE;
    if(this->properties()[property::CLIENT_LOGIN_NAME].as_unchecked<string>().empty()) return command_result{error::client_not_logged_in};
    this->properties()[property::CLIENT_LOGIN_NAME] = "";
    this->query_account = nullptr;

    auto joined = this->currentChannel;
    if(this->server){
        {
            unique_lock tree_lock(this->server->channel_tree_mutex);
            if(joined) {
                this->server->client_move(this->ref(), nullptr, nullptr, "", ViewReasonId::VREASON_USER_ACTION, false, tree_lock);
            }
            this->server->unregisterClient(this->ref(), "logout", tree_lock);
        }
    }

    this->properties()[property::CLIENT_UNIQUE_IDENTIFIER] = "UnknownQuery"; //TODO load from table
    this->properties()[property::CLIENT_NICKNAME] = string() + "ServerQuery#" + this->getLoggingPeerIp() + "/" + to_string(ntohs(this->getPeerPort()));
    DatabaseHelper::assignDatabaseId(this->sql, static_cast<ServerId>(this->server ? this->server->getServerId() : 0), this->ref());

    if(this->server){
        this->server->registerClient(this->ref());

        {
            shared_lock server_channel_r_lock(this->server->channel_tree_mutex);
            unique_lock client_channel_lock(this->channel_tree_mutex);

            this->channel_tree->reset();
            this->channel_tree->insert_channels(this->server->channelTree->tree_head(), true, false);
            this->subscribeChannel(this->server->channelTree->channels(), false, false);
        }

        if(joined) {
            unique_lock server_channel_w_lock(this->server->channel_tree_mutex, defer_lock);
            this->server->client_move(this->ref(), joined, nullptr, "", ViewReasonId::VREASON_USER_ACTION, false, server_channel_w_lock);
        } else {
            this->server->assignDefaultChannel(this->ref(), true);
        }
    } else {
        this->task_update_needed_permissions.enqueue();
    }

    this->task_update_channel_client_properties.enqueue();
    serverInstance->action_logger()->query_authenticate_logger.log_query_authenticate(this->getServerId(), std::dynamic_pointer_cast<QueryClient>(this->ref()), "", log::QueryAuthenticateResult::SUCCESS);

    return command_result{error::ok};
}

command_result QueryClient::handleCommandServerSelect(Command &cmd) {
    CMD_RESET_IDLE;

    std::optional<ServerId> target_server_id{};
    std::shared_ptr<VirtualServer> target{};

    if(cmd[0].has("port")) {
        target = serverInstance->getVoiceServerManager()->findServerByPort(cmd["port"].as<uint16_t>());
    } else if(cmd[0].has("sid")) {
        target_server_id = std::make_optional(cmd["sid"].as<ServerId>());
    } else {
        for(const auto& parm : cmd[0].keys()) {
            if(parm.length() < 6 && parm.find_first_not_of("0123456789") == string::npos) {
                target_server_id = std::make_optional(std::stoul(parm));
                break;
            }
        }
    }

    if(!target && target_server_id.has_value()) {
        target = serverInstance->getVoiceServerManager()->findServerById(*target_server_id);
    }

    if(target_server_id.has_value()) {
        if(*target_server_id > 0) {
            target = serverInstance->getVoiceServerManager()->findServerById(*target_server_id);

            if(!target) {
                return ts::command_result{error::server_invalid_id};
            }
        }
    } else if(target) {
        target_server_id = std::make_optional(target->getServerId());
    } else {
        return ts::command_result{error::server_invalid_id};
    }

    if(target == this->server) {
        return ts::command_result{error::ok};
    }

    auto old_server_id = this->getServerId();
    if(target) {
        if(target->getState() != ServerState::ONLINE && target->getState() != ServerState::OFFLINE) {
            return command_result{error::server_is_not_running};
        }

        if(this->query_account && this->query_account->bound_server > 0) {
            if(target->getServerId() != this->query_account->bound_server) {
                return command_result{error::server_invalid_id, "You're a server bound query, and the target server isn't your origin."};
            }
        } else {
            auto allowed = target->calculate_permission(permission::b_virtualserver_select, this->getClientDatabaseId(), this->getType(), 0);
            if(!permission::v2::permission_granted(1, allowed)) {
                return command_result{permission::b_virtualserver_select};
            }
        }
    }

    this->resetEventMask();

    this->disconnect_from_virtual_server("server switch");
    this->server = target;

    auto target_client_nickname = cmd["client_nickname"].optional_string();
    if(target_client_nickname.has_value()) {
        this->properties()[property::CLIENT_NICKNAME] = *target_client_nickname;
    }

#if 0
    command_result QueryClient::handleCommandJoin(Command &) {
        CMD_REQ_SERVER;
        CMD_RESET_IDLE;
        if(this->server->state != ServerState::ONLINE)
            return command_result{error::server_is_not_running};

        if(this->currentChannel)
            return command_result{error::server_already_joined, "already joined!"};

        this->server->assignDefaultChannel(this->ref(), true);
        return command_result{error::ok};
    }

    command_result QueryClient::handleCommandLeft(Command&) {
        CMD_REQ_SERVER;
        CMD_REQ_CHANNEL;
        CMD_RESET_IDLE;
        ACTION_REQUIRES_GLOBAL_PERMISSION(permission::b_virtualserver_select_godmode, 1);

        unique_lock server_channel_lock(this->server->channel_tree_mutex);
        this->server->client_move(this->ref(), nullptr, nullptr, "leaving", ViewReasonId::VREASON_SERVER_LEFT, true, server_channel_lock);
        return command_result{error::ok};
    }
#endif

    DatabaseHelper::assignDatabaseId(this->sql, static_cast<ServerId>(this->server ? this->server->getServerId() : 0), this->ref());
    if(this->server) {
        this->server->registerClient(this->ref());

        {
            std::shared_lock server_channel_lock{target->channel_tree_mutex};
            std::unique_lock client_channel_lock{this->channel_tree_mutex};

            this->subscribeToAll = true;
            this->channel_tree->insert_channels(this->server->channelTree->tree_head(), true, false);
            this->subscribeChannel(this->server->channelTree->channels(), false, false);
        }

        this->server->assignDefaultChannel(this->ref(), true);
    } else {
        this->task_update_needed_permissions.enqueue();
    }

    this->task_update_channel_client_properties.enqueue();
    serverInstance->action_logger()->query_logger.log_query_switch(std::dynamic_pointer_cast<QueryClient>(this->ref()), this->properties()[property::CLIENT_LOGIN_NAME].value(), old_server_id, this->getServerId());
    return command_result{error::ok};
}

command_result QueryClient::handleCommandServerInfo(Command &) {
    CMD_RESET_IDLE;
    ACTION_REQUIRES_GLOBAL_PERMISSION(permission::b_virtualserver_info_view, 1);

    Command cmd("");

    auto properties = this->server ? this->server->properties() : serverInstance->getDefaultServerProperties();
    for(const auto &prop : properties->list_properties(property::FLAG_SERVER_VIEW | property::FLAG_SERVER_VARIABLE, this->getType() == CLIENT_TEAMSPEAK ? property::FLAG_NEW : (uint16_t) 0)) {
        cmd[prop.type().name] = prop.as_unchecked<string>();
        if(prop.type() == property::VIRTUALSERVER_HOST)
            cmd["virtualserver_ip"] = prop.as_unchecked<string>();
    }

    cmd["virtualserver_status"] = this->server ? ServerState::string(this->server->state) : "template";


    if(this->server && permission::v2::permission_granted(1, this->calculate_permission(permission::b_virtualserver_connectioninfo_view, 0))) {
        auto total_stats = this->server->getServerStatistics()->total_stats();
        auto report_second = this->server->server_statistics_->second_stats();
        auto report_minute = this->server->server_statistics_->minute_stats();
        cmd["connection_bandwidth_sent_last_second_total"] = std::accumulate(report_second.connection_bytes_sent.begin(), report_second.connection_bytes_sent.end(), (size_t) 0U);
        cmd["connection_bandwidth_sent_last_minute_total"] = std::accumulate(report_minute.connection_bytes_sent.begin(), report_minute.connection_bytes_sent.end(), (size_t) 0U);
        cmd["connection_bandwidth_received_last_second_total"] = std::accumulate(report_second.connection_bytes_received.begin(), report_second.connection_bytes_received.end(), (size_t) 0U);
        cmd["connection_bandwidth_received_last_minute_total"] = std::accumulate(report_minute.connection_bytes_received.begin(), report_minute.connection_bytes_received.end(), (size_t) 0U);

        cmd["connection_filetransfer_bandwidth_sent"] = report_minute.file_bytes_sent;
        cmd["connection_filetransfer_bandwidth_received"] = report_minute.file_bytes_received;
        cmd["connection_filetransfer_bytes_sent_total"] = total_stats.file_bytes_sent;
        cmd["connection_filetransfer_bytes_received_total"] = total_stats.file_bytes_received;

        cmd["connection_packets_sent_speech"] = total_stats.connection_packets_sent[stats::ConnectionStatistics::category::VOICE];
        cmd["connection_bytes_sent_speech"] = total_stats.connection_bytes_sent[stats::ConnectionStatistics::category::VOICE];
        cmd["connection_packets_received_speech"] = total_stats.connection_packets_received[stats::ConnectionStatistics::category::VOICE];
        cmd["connection_bytes_received_speech"] = total_stats.connection_bytes_received[stats::ConnectionStatistics::category::VOICE];

        cmd["connection_packets_sent_keepalive"] = total_stats.connection_packets_sent[stats::ConnectionStatistics::category::KEEP_ALIVE];
        cmd["connection_packets_received_keepalive"] = total_stats.connection_bytes_sent[stats::ConnectionStatistics::category::KEEP_ALIVE];
        cmd["connection_bytes_received_keepalive"] = total_stats.connection_packets_received[stats::ConnectionStatistics::category::KEEP_ALIVE];
        cmd["connection_bytes_sent_keepalive"] = total_stats.connection_bytes_received[stats::ConnectionStatistics::category::KEEP_ALIVE];

        cmd["connection_packets_sent_control"] = total_stats.connection_packets_sent[stats::ConnectionStatistics::category::COMMAND];
        cmd["connection_bytes_sent_control"] = total_stats.connection_bytes_sent[stats::ConnectionStatistics::category::COMMAND];
        cmd["connection_packets_received_control"] = total_stats.connection_packets_received[stats::ConnectionStatistics::category::COMMAND];
        cmd["connection_bytes_received_control"] = total_stats.connection_bytes_received[stats::ConnectionStatistics::category::COMMAND];

        cmd["connection_packets_sent_total"] = std::accumulate(total_stats.connection_packets_sent.begin(), total_stats.connection_packets_sent.end(), (size_t) 0U);
        cmd["connection_bytes_sent_total"] = std::accumulate(total_stats.connection_bytes_sent.begin(), total_stats.connection_bytes_sent.end(), (size_t) 0U);
        cmd["connection_packets_received_total"] = std::accumulate(total_stats.connection_packets_received.begin(), total_stats.connection_packets_received.end(), (size_t) 0U);
        cmd["connection_bytes_received_total"] = std::accumulate(total_stats.connection_bytes_received.begin(), total_stats.connection_bytes_received.end(), (size_t) 0U);
    } else {
        cmd["connection_bandwidth_sent_last_second_total"] = "0";
        cmd["connection_bandwidth_sent_last_minute_total"] = "0";
        cmd["connection_bandwidth_received_last_second_total"] = "0";
        cmd["connection_bandwidth_received_last_minute_total"] = "0";

        cmd["connection_filetransfer_bandwidth_sent"] = "0";
        cmd["connection_filetransfer_bandwidth_received"] = "0";
        cmd["connection_filetransfer_bytes_sent_total"] = "0";
        cmd["connection_filetransfer_bytes_received_total"] = "0";

        cmd["connection_packets_sent_speech"] = "0";
        cmd["connection_bytes_sent_speech"] = "0";
        cmd["connection_packets_received_speech"] = "0";
        cmd["connection_bytes_received_speech"] = "0";

        cmd["connection_packets_sent_keepalive"] = "0";
        cmd["connection_packets_received_keepalive"] = "0";
        cmd["connection_bytes_received_keepalive"] = "0";
        cmd["connection_bytes_sent_keepalive"] = "0";

        cmd["connection_packets_sent_control"] = "0";
        cmd["connection_bytes_sent_control"] = "0";
        cmd["connection_packets_received_control"] = "0";
        cmd["connection_bytes_received_control"] = "0";

        cmd["connection_packets_sent_total"] = "0";
        cmd["connection_bytes_sent_total"] = "0";
        cmd["connection_packets_received_total"] = "0";
        cmd["connection_bytes_received_total"] = "0";
    }

    this->sendCommand(cmd);
    return command_result{error::ok};
}

command_result QueryClient::handleCommandChannelList(Command& cmd) {
    CMD_RESET_IDLE;
    ACTION_REQUIRES_GLOBAL_PERMISSION(permission::b_virtualserver_channel_list, 1);

    int index = 0;
    shared_lock channel_lock(this->server ? this->server->channel_tree_mutex : serverInstance->getChannelTreeLock());
    auto entries = this->server ? this->channel_tree->channels() : serverInstance->getChannelTree()->channels();
    channel_lock.unlock();

    command_builder result{"", 1024, entries.size()};
    for(const auto& channel : entries){
        if(!channel) continue;

        const auto channel_clients = this->server ? this->server->getClientsByChannel(channel).size() : 0;
        result.put_unchecked(index, "cid", channel->channelId());
        result.put_unchecked(index, "pid", channel->properties()[property::CHANNEL_PID].as_unchecked<string>());
        result.put_unchecked(index, "channel_name", channel->name());
        result.put_unchecked(index, "channel_order", channel->channelOrder());
        result.put_unchecked(index, "total_clients", channel_clients);
        /* result.put_unchecked(index, "channel_needed_subscribe_power", channel->permissions()->getPermissionValue(permission::i_channel_needed_subscribe_power, channel, 0)); */

        if(cmd.hasParm("flags")){
            result.put_unchecked(index, "channel_flag_default",
                                 channel->properties()[property::CHANNEL_FLAG_DEFAULT].as_unchecked<string>());
            result.put_unchecked(index, "channel_flag_password",
                                 channel->properties()[property::CHANNEL_FLAG_PASSWORD].as_unchecked<string>());
            result.put_unchecked(index, "channel_flag_permanent",
                                 channel->properties()[property::CHANNEL_FLAG_PERMANENT].as_unchecked<string>());
            result.put_unchecked(index, "channel_flag_semi_permanent",
                                 channel->properties()[property::CHANNEL_FLAG_SEMI_PERMANENT].as_unchecked<string>());
        }
        if(cmd.hasParm("voice")){
            result.put_unchecked(index, "channel_codec",
                                 channel->properties()[property::CHANNEL_CODEC].as_unchecked<string>());
            result.put_unchecked(index, "channel_codec_quality",
                                 channel->properties()[property::CHANNEL_CODEC_QUALITY].as_unchecked<string>());
            result.put_unchecked(index, "channel_needed_talk_power",
                                 channel->properties()[property::CHANNEL_NEEDED_TALK_POWER].as_unchecked<string>());
        }
        if(cmd.hasParm("icon")){
            result.put_unchecked(index, "channel_icon_id",
                                 channel->properties()[property::CHANNEL_ICON_ID].as_unchecked<string>());
        }
        if(cmd.hasParm("limits")){
            result.put_unchecked(index, "total_clients_family", this->server ? this->server->getClientsByChannelRoot(channel, false).size() : 0);
            result.put_unchecked(index, "total_clients", this->server ? this->server->getClientsByChannel(channel).size() : 0);

            result.put_unchecked(index, "channel_maxclients",
                                 channel->properties()[property::CHANNEL_MAXCLIENTS].as_unchecked<string>());
            result.put_unchecked(index, "channel_maxfamilyclients",
                                 channel->properties()[property::CHANNEL_MAXFAMILYCLIENTS].as_unchecked<string>());

            {
                auto needed_power = channel->permissions()->permission_value_flagged(permission::i_channel_subscribe_power);
                result.put_unchecked(index, "channel_needed_subscribe_power", needed_power.has_value ? needed_power.value : 0);
            }
        }
        if(cmd.hasParm("topic")) {
            result.put_unchecked(index, "channel_topic",
                                 channel->properties()[property::CHANNEL_TOPIC].as_unchecked<string>());
        }
        if(cmd.hasParm("times") || cmd.hasParm("secondsempty")){
            result.put_unchecked(index, "seconds_empty", channel_clients == 0 ? channel->empty_seconds() : 0);
        }
        index++;
    }

    this->sendCommand(result, false);
    return command_result{error::ok};
}

command_result QueryClient::handleCommandServerList(Command& cmd) {
    CMD_RESET_IDLE;
    ACTION_REQUIRES_INSTANCE_PERMISSION(permission::b_serverinstance_virtualserver_list, 1);

    auto servers = serverInstance->getVoiceServerManager()->serverInstances();
    command_builder result{"", 256, servers.size()};

    size_t index = 0;
    for(const auto& server : serverInstance->getVoiceServerManager()->serverInstances()) {
        result.put_unchecked(index, "virtualserver_id", server->getServerId());
        result.put_unchecked(index, "virtualserver_host",
                             server->properties()[property::VIRTUALSERVER_HOST].as_unchecked<string>());
        result.put_unchecked(index, "virtualserver_port",
                             server->properties()[property::VIRTUALSERVER_PORT].as_unchecked<string>());
        result.put_unchecked(index, "virtualserver_web_host",
                             server->properties()[property::VIRTUALSERVER_WEB_HOST].as_unchecked<string>());
        result.put_unchecked(index, "virtualserver_web_port",
                             server->properties()[property::VIRTUALSERVER_WEB_PORT].as_unchecked<string>());
        result.put_unchecked(index, "virtualserver_status", ServerState::string(server->state));
        result.put_unchecked(index, "virtualserver_clientsonline",
                             server->properties()[property::VIRTUALSERVER_CLIENTS_ONLINE].as_unchecked<string>());
        result.put_unchecked(index, "virtualserver_queryclientsonline",
                             server->properties()[property::VIRTUALSERVER_QUERYCLIENTS_ONLINE].as_unchecked<string>());
        result.put_unchecked(index, "virtualserver_maxclients",
                             server->properties()[property::VIRTUALSERVER_MAXCLIENTS].as_unchecked<string>());
        if(server->startTimestamp.time_since_epoch().count() > 0 && server->state == ServerState::ONLINE)
            result.put_unchecked(index, "virtualserver_uptime", duration_cast<seconds>(system_clock::now() - server->startTimestamp).count());
        else
            result.put_unchecked(index, "virtualserver_uptime", 0);
        result.put_unchecked(index, "virtualserver_name",
                             server->properties()[property::VIRTUALSERVER_NAME].as_unchecked<string>());
        result.put_unchecked(index, "virtualserver_autostart",
                             server->properties()[property::VIRTUALSERVER_AUTOSTART].as_unchecked<string>());
        result.put_unchecked(index, "virtualserver_machine_id",
                             server->properties()[property::VIRTUALSERVER_MACHINE_ID].as_unchecked<string>());
        if(cmd.hasParm("uid"))
            result.put_unchecked(index, "virtualserver_unique_identifier",
                                 server->properties()[property::VIRTUALSERVER_UNIQUE_IDENTIFIER].as_unchecked<string>());
        else result.put_unchecked(index, "virtualserver_unique_identifier", "");
        index++;
    }

    this->sendCommand(result, false);
    return command_result{error::ok};
}

command_result QueryClient::handleCommandServerCreate(Command& cmd) {
    CMD_RESET_IDLE;
    ACTION_REQUIRES_INSTANCE_PERMISSION(permission::b_virtualserver_create, 1);

    if(serverInstance->getVoiceServerManager()->getState() != VirtualServerManager::STARTED) {
        return command_result{error::vs_critical, "Server manager isn't started yet or not finished starting"};
    }

    string startError;
    shared_ptr<VirtualServer> server;

    milliseconds time_create, time_wait, time_start, time_global;
    {
        auto start = system_clock::now();
        threads::MutexLock lock(serverInstance->getVoiceServerManager()->server_create_lock);
        auto instances = serverInstance->getVoiceServerManager()->serverInstances();
        if(config::server::max_virtual_server != -1 && instances.size() > config::server::max_virtual_server) {
            return command_result{error::server_max_vs_reached, "You reached the via config.yml enabled virtual server limit."};
        }

        /*
         * 2 ^ 16 = 65536
         * We're using one less since we're using the server with id 65535 as snapshot deploy server.
         */
        if(instances.size() >= 65535) {
            return command_result{error::server_max_vs_reached, "You cant create anymore virtual servers. (Software limit reached)"};
        }

        {
            auto end = system_clock::now();
            time_wait = duration_cast<milliseconds>(end - start);
        }

        std::string host = cmd[0].has("virtualserver_host") ? cmd["virtualserver_host"].as<string>() : config::binding::DefaultVoiceHost;
        uint16_t freePort = serverInstance->getVoiceServerManager()->next_available_port(host);

        uint16_t port = cmd[0].has("virtualserver_port") ? cmd["virtualserver_port"].as<uint16_t>() : freePort;
        {
            auto _start = system_clock::now();
            server = serverInstance->getVoiceServerManager()->create_server(host, port);
            auto _end = system_clock::now();
            time_create = duration_cast<milliseconds>(_end - _start);
        }
        if(!server) return command_result{error::vs_critical, "could not create new server"};

        for(const auto& key : cmd[0].keys()){
            if(key == "virtualserver_port") continue;
            if(key == "virtualserver_host") continue;

            const auto& info = property::find<property::VirtualServerProperties>(key);
            if(info == property::VIRTUALSERVER_UNDEFINED) {
                logError(server->getServerId(), "Tried to change unknown server property " + key);
                continue;
            }
            if(!info.validate_input(cmd[key].as<string>())) {
                logError(server->getServerId(), "Tried to change " + key + " to an invalid value: " + cmd[key].as<string>());
                continue;
            }
            server->properties()[info] = cmd[key].string();
        }

        if(!cmd.hasParm("offline")) {
            auto start_ = system_clock::now();
            if(!server->start(startError));
            auto end_ = system_clock::now();
            time_start = duration_cast<milliseconds>(end_ - start_);
        }
        auto end = system_clock::now();

        time_global = duration_cast<milliseconds>(end - start);
    }
    serverInstance->action_logger()->server_logger.log_server_create(server->getServerId(), this->ref(), log::ServerCreateReason::USER_ACTION);

    size_t total_tokens{};
    auto tokens = server->tokenManager->list_tokens(total_tokens, std::nullopt, { 1 }, std::nullopt);

    Command res("");
    res["sid"] = server->getServerId();
    res["error"] = startError;
    res["virtualserver_port"] = server->properties()[property::VIRTUALSERVER_PORT].as_unchecked<string>();
    res["token"] = tokens.empty() ? "unknown" : tokens[0]->token;
    res["time_create"] = time_create.count();
    res["time_start"] = time_start.count();
    res["time_global"] = time_global.count();
    res["time_wait"] = time_wait.count();
    this->sendCommand(res);
    return command_result{error::ok};
}

command_result QueryClient::handleCommandServerDelete(Command& cmd) {
    CMD_RESET_IDLE;
    ACTION_REQUIRES_INSTANCE_PERMISSION(permission::b_virtualserver_delete, 1);

    if(serverInstance->getVoiceServerManager()->getState() != VirtualServerManager::STARTED)
        return command_result{error::vs_critical, "Server manager isn't started yet or not finished starting"};

    auto server = serverInstance->getVoiceServerManager()->findServerById(cmd["sid"]);
    if(!server) return command_result{error::server_invalid_id, "invalid bounded server"};
    if(!serverInstance->getVoiceServerManager()->deleteServer(server)) return command_result{error::vs_critical};
    serverInstance->action_logger()->server_logger.log_server_delete(server->getServerId(), this->ref());
    return command_result{error::ok};
}

command_result QueryClient::handleCommandServerStart(Command& cmd) {
    CMD_RESET_IDLE;

    if(serverInstance->getVoiceServerManager()->getState() != VirtualServerManager::STARTED)
        return command_result{error::vs_critical, "Server manager isn't started yet or not finished starting"};

    auto server = serverInstance->getVoiceServerManager()->findServerById(cmd["sid"]);
    if(!server) return command_result{error::server_invalid_id, "invalid bounded server"};

    switch (server->state) {
        case ServerState::BOOTING:
            return command_result{error::server_is_booting};
        case ServerState::ONLINE:
            return command_result{error::server_running};
        case ServerState::SUSPENDING:
            return command_result{error::server_is_shutting_down};
        case ServerState::DELETING:
            return command_result{error::server_invalid_id};
        case ServerState::OFFLINE: break;
    }

    if(!permission::v2::permission_granted(1, server->calculate_permission(permission::b_virtualserver_start, this->getClientDatabaseId(), ClientType::CLIENT_QUERY, 0)))
        ACTION_REQUIRES_INSTANCE_PERMISSION(permission::b_virtualserver_start_any, 1);

    string err;
    if(!server->start(err))
        return command_result{error::vs_critical, err};
    serverInstance->action_logger()->server_logger.log_server_start(server->getServerId(), this->ref());
    return command_result{error::ok};
}

command_result QueryClient::handleCommandServerStop(Command& cmd) {
    CMD_RESET_IDLE;

    if(serverInstance->getVoiceServerManager()->getState() != VirtualServerManager::STARTED)
        return command_result{error::vs_critical, "Server manager isn't started yet or not finished starting"};

    auto server = serverInstance->getVoiceServerManager()->findServerById(cmd["sid"]);
    if(!server) return command_result{error::server_invalid_id, "invalid bounded server"};
    switch (server->state) {
        case ServerState::BOOTING:
            return command_result{error::server_is_booting};
        case ServerState::OFFLINE:
            return command_result{error::server_is_not_running};
        case ServerState::SUSPENDING:
            return command_result{error::server_is_shutting_down};
        case ServerState::DELETING:
            return command_result{error::server_invalid_id};
        case ServerState::ONLINE: break;
    }

    if(!permission::v2::permission_granted(1, server->calculate_permission(permission::b_virtualserver_stop, this->getClientDatabaseId(), ClientType::CLIENT_QUERY, 0)))
        ACTION_REQUIRES_INSTANCE_PERMISSION(permission::b_virtualserver_stop_any, 1);

    server->stop("server stopped", false);
    serverInstance->action_logger()->server_logger.log_server_stop(server->getServerId(), this->ref());
    return command_result{error::ok};
}


command_result QueryClient::handleCommandInstanceInfo(Command& cmd) {
    ACTION_REQUIRES_INSTANCE_PERMISSION(permission::b_serverinstance_info_view, 1);

    Command res("");
    for(const auto& e : serverInstance->properties()->list_properties(property::FLAG_INSTANCE_VARIABLE, this->getType() == CLIENT_TEAMSPEAK ? property::FLAG_NEW : (uint16_t) 0)) {
        res[e.type().name] = e.value();
    }

    if(!this->properties()[property::CLIENT_LOGIN_NAME].value().empty()) {
        res["serverinstance_teaspeak"] = true;
    }

    res["serverinstance_serverquery_max_connections_per_ip"] = res["serverinstance_query_max_connections_per_ip"].as<std::string>();

    this->sendCommand(res);
    return command_result{error::ok};
}

command_result QueryClient::handleCommandInstanceEdit(Command& cmd) {
    ACTION_REQUIRES_INSTANCE_PERMISSION(permission::b_serverinstance_modify_settings, 1);

    for(const auto &key : cmd[0].keys()){
        const auto* info = &property::find<property::InstanceProperties>(key);
        if(key == "serverinstance_serverquery_max_connections_per_ip")
            info = &property::describe(property::SERVERINSTANCE_QUERY_MAX_CONNECTIONS_PER_IP);

        if(*info == property::SERVERINSTANCE_UNDEFINED) {
            logError(LOG_QUERY, "Query {} tried to change a non existing instance property: {}", this->getLoggingPeerIp(), key);
            continue;
        }
        if(!info->validate_input(cmd[key])) {
            logError(LOG_QUERY, "Query {} tried to change {} to an invalid value {}", this->getLoggingPeerIp(), key, cmd[key].as<string>());
            continue;
        }
        if(*info == property::SERVERINSTANCE_VIRTUAL_SERVER_ID_INDEX) {
            /* ensure we've a valid id */
            serverInstance->properties()[info] = cmd[key].as<ServerId>();
            continue;
        }
        serverInstance->properties()[info] = cmd[key].string();
    }
    return command_result{error::ok};
}

command_result QueryClient::handleCommandHostInfo(Command &) {
    ACTION_REQUIRES_INSTANCE_PERMISSION(permission::b_serverinstance_info_view, 1);

    Command res("");
    res["instance_uptime"] = duration_cast<seconds>(system_clock::now() - serverInstance->getStartTimestamp()).count();
    res["host_timestamp_utc"] = duration_cast<seconds>(system_clock::now().time_since_epoch()).count();

    auto instance_report = serverInstance->getVoiceServerManager()->instanceSlotUsageReport();
    res["virtualservers_running_total"] = instance_report.server_count;
    res["virtualservers_total_maxclients"] = instance_report.max_clients;
    res["virtualservers_total_clients_online"] = instance_report.voice_clients();
    res["virtualservers_total_channels_online"] = instance_report.online_channels;


    auto total_stats = serverInstance->getStatistics()->total_stats();
    res["connection_packets_sent_total"] = std::accumulate(total_stats.connection_packets_sent.begin(), total_stats.connection_packets_sent.end(), (size_t) 0U);
    res["connection_bytes_sent_total"] = std::accumulate(total_stats.connection_bytes_sent.begin(), total_stats.connection_bytes_sent.end(), (size_t) 0U);
    res["connection_packets_received_total"] = std::accumulate(total_stats.connection_packets_received.begin(), total_stats.connection_packets_received.end(), (size_t) 0U);
    res["connection_bytes_received_total"] = std::accumulate(total_stats.connection_bytes_received.begin(), total_stats.connection_bytes_received.end(), (size_t) 0U);

    auto report_second = serverInstance->getStatistics()->second_stats();
    auto report_minute = serverInstance->getStatistics()->minute_stats();
    res["connection_bandwidth_sent_last_second_total"] = std::accumulate(report_second.connection_bytes_sent.begin(), report_second.connection_bytes_sent.end(), (size_t) 0U);
    res["connection_bandwidth_sent_last_minute_total"] = std::accumulate(report_minute.connection_bytes_sent.begin(), report_minute.connection_bytes_sent.end(), (size_t) 0U);
    res["connection_bandwidth_received_last_second_total"] = std::accumulate(report_second.connection_bytes_received.begin(), report_second.connection_bytes_received.end(), (size_t) 0U);
    res["connection_bandwidth_received_last_minute_total"] = std::accumulate(report_minute.connection_bytes_received.begin(), report_minute.connection_bytes_received.end(), (size_t) 0U);


    res["connection_filetransfer_bandwidth_sent"] = report_second.file_bytes_sent;
    res["connection_filetransfer_bandwidth_received"] = report_second.file_bytes_received;
    res["connection_filetransfer_bytes_sent_total"] = total_stats.file_bytes_sent;
    res["connection_filetransfer_bytes_received_total"] = total_stats.file_bytes_received;

    this->sendCommand(res);
    return command_result{error::ok};
}

command_result QueryClient::handleCommandGlobalMessage(Command& cmd) {
    ACTION_REQUIRES_INSTANCE_PERMISSION(permission::b_serverinstance_textmessage_send, 1);

    for(const auto &server : serverInstance->getVoiceServerManager()->serverInstances())
        if(server->running()) server->broadcastMessage(server->getServerRoot(), cmd["msg"]);

    return command_result{error::ok};
}

command_result QueryClient::handleCommandServerIdGetByPort(Command& cmd) {
    uint16_t port = cmd["virtualserver_port"];
    auto server = serverInstance->getVoiceServerManager()->findServerByPort(port);
    if(!server) return command_result{error::server_invalid_id};
    Command res("");
    res["server_id"] = server->getServerId();
    this->sendCommand(res);
    return command_result{error::ok};
}

command_result QueryClient::handleCommandBindingList(Command& cmd) {
    Command res("");
    res["ip"] = "0.0.0.0 ";
    this->sendCommand(res); //TODO maybe list here all bindings from voice & file and query server?
    return command_result{error::ok};
}

command_result QueryClient::handleCommandServerSnapshotDeployNew(const ts::command_parser &command) {
    CMD_RESET_IDLE;

    if(this->server) {
        ACTION_REQUIRES_GLOBAL_PERMISSION(permission::b_virtualserver_snapshot_deploy, 1);
    } else {
        ACTION_REQUIRES_INSTANCE_PERMISSION(permission::b_virtualserver_snapshot_deploy, 1);
    }

    std::string error{};
    auto server = this->server;
    auto result = serverInstance->getVoiceServerManager()->deploy_snapshot(error, server, command);

    using SnapshotDeployResult = VirtualServerManager::SnapshotDeployResult;
    switch (result) {
        case SnapshotDeployResult::SUCCESS:
            break;

        case SnapshotDeployResult::REACHED_SERVER_ID_LIMIT:
            return command_result{error::server_max_vs_reached, "You cant create anymore virtual servers. (Server ID limit reached)"};

        case SnapshotDeployResult::REACHED_SOFTWARE_SERVER_LIMIT:
            return command_result{error::server_max_vs_reached, "You cant create anymore virtual servers. (Software limit reached)"};

        case SnapshotDeployResult::REACHED_CONFIG_SERVER_LIMIT:
            return command_result{error::server_max_vs_reached, "You reached the via config.yml enabled virtual server limit."};

        case SnapshotDeployResult::CUSTOM_ERROR:
            return command_result{error::vs_critical, error};
    }

    /* TODO: Send mapping */
    ts::command_builder notify{""};
    notify.put_unchecked(0, "virtualserver_port", server->properties()[property::VIRTUALSERVER_PORT].value());
    notify.put_unchecked(0, "sid", server->getServerId());
    this->sendCommand(notify, false);

    return command_result{error::ok};
}

command_result QueryClient::handleCommandServerSnapshotCreate(Command& cmd) {
    ACTION_REQUIRES_GLOBAL_PERMISSION(permission::b_virtualserver_snapshot_create, 1);
    CMD_RESET_IDLE;
    CMD_REQ_SERVER;

    Command snapshot_command("");
    string error;

    int version = cmd[0].has("version") ? cmd[0]["version"] : -1;
    if(version == -1 && (cmd.hasParm("lagacy") || cmd.hasParm("legacy"))) {
        version = 0;
    }

    if(!serverInstance->getVoiceServerManager()->createServerSnapshot(snapshot_command, this->server, version, error)) {
        return command_result{error::vs_critical, error};
    }

    if(version == -1 || version >= 3) {
        auto build_version = snapshot_command[0]["snapshot_version"].as<int>();
        snapshot_command.pop_bulk();

        auto snapshot_data = snapshot_command.build();
        auto max_compressed_size = ZSTD_compressBound(snapshot_data.size());
        if(ZSTD_isError(max_compressed_size)) {
            return command_result{error::vs_critical, "failed to calculate compressed size: " + std::string{ZSTD_getErrorName(max_compressed_size)}};
        }

        std::string buffer{};
        buffer.resize(max_compressed_size);

        auto compressed_size = ZSTD_compress(buffer.data(), buffer.size(), snapshot_data.data(), snapshot_data.length(), 1);
        if(ZSTD_isError(compressed_size)) {
            return command_result{error::vs_critical, "failed to compressed snapshot: " + std::string{ZSTD_getErrorName(compressed_size)}};
        }

        ts::command_builder result{""};
        result.bulk(0).reserve(100 + compressed_size * 4/3);
        result.put_unchecked(0, "snapshot_version", build_version);
        result.put_unchecked(0, "data", base64::encode(std::string_view{buffer.data(), compressed_size}));
        this->sendCommand(result, false);
    } else {
        auto data = snapshot_command.build();
        auto buildHash = base64::encode(digest::sha1(data));

        snapshot_command.push_bulk_front();
        snapshot_command[0]["hash"] = buildHash;

        this->sendCommand(snapshot_command);
    }
    return command_result{error::ok};
}

extern bool mainThreadActive;
command_result QueryClient::handleCommandServerProcessStop(Command& cmd) {
    ACTION_REQUIRES_INSTANCE_PERMISSION(permission::b_serverinstance_stop, 1);

    if(cmd[0].has("type")) {
        if(cmd["type"] == "cancel") {
            auto task = ts::server::scheduledShutdown();
            if(!task) return command_result{error::server_is_not_shutting_down, "There isn't a shutdown scheduled"};
            ts::server::cancelShutdown(true);
            return command_result{error::ok};
        } else if(cmd["type"] == "schedule") {
            if(!cmd[0].has("time")) return command_result{error::parameter_missing, "Missing time"};
            ts::server::scheduleShutdown(system_clock::now() + seconds(cmd["time"].as<uint64_t>()), cmd[0].has("msg") ? cmd["msg"].string() : ts::config::messages::applicationStopped);
            return command_result{error::ok};
        }
    }

    string reason = ts::config::messages::applicationStopped;
    if(cmd[0].has("msg"))
        reason = cmd["msg"].string();
    if(cmd[0].has("reasonmsg"))
        reason = cmd["reasonmsg"].string();
    serverInstance->getVoiceServerManager()->shutdownAll(reason);
    mainThreadActive = false;
    return command_result{error::ok};
}

#define XMACRO_EV(evName0, evSpec0, evName1, evSpec1) \
if(cmd["event"].value() == "all" || (cmd["event"].value() == (evName0) && (cmd["specifier"].value() == "all" || cmd["specifier"].value() == (evSpec0)))) \
    events.push_back({QueryEventGroup::evName1, QueryEventSpecifier::evSpec1});

inline bool parseEvent(ParameterBulk& cmd, vector<pair<QueryEventGroup, QueryEventSpecifier>>& events){
    auto start = events.size();
    #include "XMacroEventTypes.h"
    if(start == events.size()) return false;
    return true;
}
#undef XMACRO_EV


#define XMACRO_LAGACY_EV(lagacyName, gr, spec) \
if(event == lagacyName) this->toggleEvent(QueryEventGroup::gr, QueryEventSpecifier::spec, true);

command_result QueryClient::handleCommandServerNotifyRegister(Command &cmd) {
    CMD_REQ_SERVER;
    CMD_REQ_PARM("event");
    CMD_RESET_IDLE;
    if(!cmd[0].has("specifier") && cmd["event"].as<string>() != "all") { //Lagacy support
        logMessage(this->getServerId(), "{} Client {}:{} uses the lagacy notify system, which is deprecated!", CLIENT_STR_LOG_PREFIX, this->getLoggingPeerIp(), this->getPeerPort());
        string event = cmd["event"];
        std::transform(event.begin(), event.end(), event.begin(), ::tolower);
        #include "XMacroEventTypes.h"
        return command_result{error::ok};
    }

    //TODO implement bulk
    vector<pair<QueryEventGroup, QueryEventSpecifier>> events;
    //parameter_invalid
    auto result = parseEvent(cmd[0], events);
    if(!result) return command_result{error::parameter_invalid};
    for(const auto& ev : events)
        this->toggleEvent(ev.first, ev.second, true);
    return command_result{error::ok};
}
#undef XMACRO_LAGACY_EV

#define XMACRO_EV(evName0, evSpec0, evName1, evSpec1)   \
else if((evName1) == group && (evSpec1) == spec){       \
    res[index]["event"] = evName0;                      \
    res[index]["specifier"] = evSpec0;                  \
    index++;                                            \
}

command_result QueryClient::handleCommandServerNotifyList(Command& cmd) {
    CMD_REQ_SERVER;
    CMD_RESET_IDLE;

    Command res("");
    int index = 0;

    for(int group = QueryEventGroup::QEVENTGROUP_MIN; group < QueryEventGroup::QEVENTGROUP_MAX; group = group + 1)
        for(int spec = QueryEventSpecifier::QEVENTSPECIFIER_MIN; spec < QueryEventSpecifier::QEVENTSPECIFIER_MAX; spec = spec + 1) {
            bool state = this->eventActive((QueryEventGroup) group, (QueryEventSpecifier) spec);
            if(state || cmd.hasParm("all")){
                res[index]["active"] = state;
                if(false);
                #include "XMacroEventTypes.h"
            }
        }

    if(index == 0) return command_result{error::database_empty_result};
    this->sendCommand(res);
    return command_result{error::ok};
}
#undef XMACRO_EV

#define XMACRO_LAGACY_EV(lagacyName, gr, spec)                                                      \
if(event == lagacyName) this->toggleEvent(QueryEventGroup::gr, QueryEventSpecifier::spec, false);

command_result QueryClient::handleCommandServerNotifyUnregister(Command &cmd) {
    CMD_REQ_SERVER;
    CMD_REQ_PARM("event");
    CMD_RESET_IDLE;
    if(!cmd[0].has("specifier")){
        logMessage(this->getServerId(), "{} Client {}:{} uses the lagacy notify system, which is deprecated!", CLIENT_STR_LOG_PREFIX, this->getLoggingPeerIp(), this->getPeerPort());
        string event = cmd["event"];
        std::transform(event.begin(), event.end(), event.begin(), ::tolower);
        #include "XMacroEventTypes.h"
        return command_result{error::ok};
    }

    //TODO implemt bulk
    vector<pair<QueryEventGroup, QueryEventSpecifier>> events;
    auto result = parseEvent(cmd[0], events);
    if(!result) return command_result{error::parameter_invalid};
    for(const auto& ev : events)
        this->toggleEvent(ev.first, ev.second, false);
    return command_result{error::ok};
}
#undef XMACRO_LAGACY_EV