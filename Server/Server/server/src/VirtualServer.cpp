#include <cstring>
#include <functional>
#include <protocol/buffers.h>
#include <netinet/in.h>
#include <bitset>
#include <tomcrypt.h>

#include <log/LogUtils.h>
#include <misc/digest.h>
#include <misc/base64.h>

#include <files/FileServer.h>

#include "./client/web/WebClient.h"
#include "./client/voice/VoiceClient.h"
#include "./client/InternalClient.h"
#include "./client/music/MusicClient.h"
#include "./client/query/QueryClient.h"
#include "music/MusicBotManager.h"
#include "server/VoiceServer.h"
#include "server/QueryServer.h"
#include "InstanceHandler.h"
#include "Configuration.h"
#include "VirtualServer.h"
#include "./rtc/lib.h"
#include "src/manager/ConversationManager.h"
#include <misc/sassert.h>
#include <src/manager/ActionLogger.h>
#include "./groups/GroupManager.h"
#include "./PermissionCalculator.h"

using namespace std;
using namespace std::chrono;
using namespace ts;
using namespace ts::server;
using namespace ts::protocol;
using namespace ts::buffer;

#define ECC_TYPE_INDEX 5

#ifndef BUILD_VERSION
#define BUILD_VERSION "Unknown build"
#endif

VirtualServer::VirtualServer(uint16_t serverId, sql::SqlManager* database) : serverId(serverId), sql(database) {
    memtrack::allocated<VirtualServer>(this);
}

bool VirtualServer::initialize(bool test_properties) {
    assert(self.lock());

    std::string error{};
    this->rtc_server_ = std::make_unique<rtc::Server>();

    this->_properties = serverInstance->databaseHelper()->loadServerProperties(self.lock());
    this->_properties->registerNotifyHandler([&](Property& prop){
        if(prop.type() == property::VIRTUALSERVER_DISABLE_IP_SAVING) {
            this->_disable_ip_saving = prop.as_or<bool>(false);
            return;
        } else if(prop.type() == property::VIRTUALSERVER_CODEC_ENCRYPTION_MODE) {
            this->_voice_encryption_mode = prop.as_or<int>(0);
            return;
        } else if(prop.type() == property::VIRTUALSERVER_MAX_UPLOAD_TOTAL_BANDWIDTH) {
            auto file_vs = file::server()->find_virtual_server(this->getServerId());
            if(!file_vs) return;
            file_vs->max_networking_upload_bandwidth(prop.as_or<int64_t>(-1));
        } else if(prop.type() == property::VIRTUALSERVER_MAX_DOWNLOAD_TOTAL_BANDWIDTH) {
            auto file_vs = file::server()->find_virtual_server(this->getServerId());
            if(!file_vs) return;
            file_vs->max_networking_download_bandwidth(prop.as_or<int64_t>(-1));
        }
        std::string sql{};
        if(prop.type() == property::VIRTUALSERVER_HOST)
            sql = "UPDATE `servers` SET `host` = :value WHERE `serverId` = :sid";
        else if(prop.type() == property::VIRTUALSERVER_PORT)
            sql = "UPDATE `servers` SET `port` = :value WHERE `serverId` = :sid";
        if(sql.empty()) return;
        sql::command(this->sql, sql, variable{":sid", this->getServerId()}, variable{":value", prop.value()})
                .executeLater().waitAndGetLater(LOG_SQL_CMD, sql::result{1, "future failed"});
    });

    this->properties()[property::VIRTUALSERVER_PLATFORM] = config::server::DefaultServerPlatform;
    this->properties()[property::VIRTUALSERVER_VERSION] = config::server::DefaultServerVersion;
    this->properties()[property::VIRTUALSERVER_ID] = serverId;
    this->_disable_ip_saving = this->properties()[property::VIRTUALSERVER_DISABLE_IP_SAVING];

    /* initialize logging */
    {
        auto server_id = this->serverId;
        auto sync_property = [server_id](Property& prop) {
            log::LoggerGroup action_type;
            switch (prop.type().property_index) {
                case property::VIRTUALSERVER_LOG_SERVER:
                    action_type = log::LoggerGroup::SERVER;
                    break;

                case property::VIRTUALSERVER_LOG_CHANNEL:
                    action_type = log::LoggerGroup::CHANNEL;
                    break;

                case property::VIRTUALSERVER_LOG_CLIENT:
                    action_type = log::LoggerGroup::CLIENT;
                    break;

                case property::VIRTUALSERVER_LOG_FILETRANSFER:
                    action_type = log::LoggerGroup::FILES;
                    break;

                case property::VIRTUALSERVER_LOG_PERMISSIONS:
                    action_type = log::LoggerGroup::PERMISSION;
                    break;

                case property::VIRTUALSERVER_LOG_QUERY:
                    action_type = log::LoggerGroup::QUERY;
                    break;

                default:
                    return;
            }

            serverInstance->action_logger()->toggle_logging_group(server_id, action_type,
                                                                  prop.as_or<bool>(true));
        };

        for(const property::VirtualServerProperties& property : {
            property::VIRTUALSERVER_LOG_SERVER,
            property::VIRTUALSERVER_LOG_CHANNEL,
            property::VIRTUALSERVER_LOG_CLIENT,
            property::VIRTUALSERVER_LOG_FILETRANSFER,
            property::VIRTUALSERVER_LOG_QUERY,
            property::VIRTUALSERVER_LOG_PERMISSIONS
        }) {
            auto prop = this->_properties->get(property::PROP_TYPE_SERVER, property);
            sync_property(prop);
        }

        this->_properties->registerNotifyHandler([sync_property](Property& prop){
            sync_property(prop);
        });
    }

    if(!properties()[property::VIRTUALSERVER_KEYPAIR].value().empty()){
        debugMessage(this->serverId, "Importing server keypair");
        this->_serverKey = new ecc_key;
        auto bytes = base64::decode(properties()[property::VIRTUALSERVER_KEYPAIR].value());
        int err;
        if((err = ecc_import(reinterpret_cast<const unsigned char *>(bytes.data()), bytes.length(), this->_serverKey)) != CRYPT_OK){
            logError(this->getServerId(), "Cant import key. ({} => {})", err, error_to_string(err));
            logError(this->serverId, "Could not import server keypair! {} ({}). Generating new one!", err, error_to_string(err));
            delete this->_serverKey;
            this->_serverKey = nullptr;
            properties()[property::VIRTUALSERVER_KEYPAIR] = "";
        }
    }

    int err;
    if(!_serverKey){
        debugMessage(this->serverId, "Generating new server keypair");
        this->_serverKey = new ecc_key;
        prng_state state{};
        if((err = ecc_make_key_ex(&state, find_prng("sprng"), this->_serverKey, &ltc_ecc_sets[ECC_TYPE_INDEX])) != CRYPT_OK){
            logError(this->serverId, "Could not generate a server keypair! {} ({})", err, error_to_string(err));
            delete this->_serverKey;
            this->_serverKey = nullptr;
            return false;
        }

        size_t bytesBufferLength = 1024;
        char bytesBuffer[bytesBufferLength];
        if((err = ecc_export(reinterpret_cast<unsigned char *>(bytesBuffer), &bytesBufferLength, PK_PRIVATE, this->_serverKey)) != CRYPT_OK){
            logError(this->serverId, "Could not export the server keypair (private)! {} ({})", err, error_to_string(err));
            delete this->_serverKey;
            this->_serverKey = nullptr;
            return false;
        }
        properties()[property::VIRTUALSERVER_KEYPAIR] = base64_encode(bytesBuffer, bytesBufferLength);
        this->properties()[property::VIRTUALSERVER_CREATED] = duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
    }
    if(_serverKey){
        size_t bufferLength = 265;
        char buffer[bufferLength];
        if((err = ecc_export(reinterpret_cast<unsigned char *>(buffer), &bufferLength, PK_PUBLIC, this->_serverKey)) != CRYPT_OK)
            logError(this->serverId, "Could not generate server uid! (Could not export the server keypair (public)! {} ({}))", err, error_to_string(err));
        properties()[property::VIRTUALSERVER_UNIQUE_IDENTIFIER] = base64::encode(digest::sha1(base64::encode(buffer, bufferLength)));
    }

    this->conversation_manager_ = make_shared<conversation::ConversationManager>(this->ref());
    this->conversation_manager_->initialize(this->conversation_manager_);

    channelTree = new ServerChannelTree(self.lock(), this->sql);
    channelTree->loadChannelsFromDatabase();

    this->groups_manager_ = std::make_shared<groups::GroupManager>(this->getSql(), this->getServerId(), serverInstance->group_manager());
    if(!this->groups_manager_->initialize(this->groups_manager_, error)) {
        logCritical(this->getServerId(), "Failed to initialize group manager: {}", error);
        return false;
    }

    channelTree->deleteSemiPermanentChannels();
    if(channelTree->channel_count() == 0) {
        logMessage(this->serverId, "Creating new channel tree (Copy from server 0)");
        LOG_SQL_CMD(sql::command(this->getSql(), "INSERT INTO `channels` (`serverId`, `channelId`, `type`, `parentId`) SELECT :serverId AS `serverId`, `channelId`, `type`, `parentId` FROM `channels` WHERE `serverId` = 0", variable{":serverId", this->serverId}).execute());
        LOG_SQL_CMD(sql::command(this->getSql(), "INSERT INTO `properties` (`serverId`, `type`, `id`, `key`, `value`) SELECT :serverId AS `serverId`, `type`, `id`, `key`, `value` FROM `properties` WHERE `serverId` = 0 AND `type` = :type",
                                 variable{":serverId", this->serverId}, variable{":type", property::PROP_TYPE_CHANNEL}).execute());
        LOG_SQL_CMD(sql::command(this->getSql(), "INSERT INTO `permissions` (`serverId`, `type`, `id`, `channelId`, `permId`, `value`, `grant`, `flag_skip`, `flag_negate`) "
                                                 "SELECT :serverId AS `serverId`, `type`, `id`, `channelId`, `permId`, `value`, `grant`, `flag_skip`, `flag_negate` FROM `permissions` WHERE `serverId` = 0 AND `type` = :type",
                                 variable{":serverId", this->serverId}, variable{":type", permission::SQL_PERM_CHANNEL}).execute());

        channelTree->loadChannelsFromDatabase();
        if(channelTree->channel_count() == 0){
            logCritical(this->serverId, "Failed to setup channel tree!");
            return false;
        }
        if(!channelTree->getDefaultChannel()) {
            logError(this->serverId, "Missing default channel! Using first one!");
            channelTree->setDefaultChannel(channelTree->channels().front());
        }
    }
    if(!channelTree->getDefaultChannel()) channelTree->setDefaultChannel(*channelTree->channels().begin());
    auto default_channel = channelTree->getDefaultChannel();
    assert(default_channel);
    if(default_channel->properties()[property::CHANNEL_FLAG_PASSWORD].as_or<bool>(false))
        default_channel->properties()[property::CHANNEL_FLAG_PASSWORD] = false;

    this->tokenManager = new token::TokenManager(this->sql, this->getServerId());
    this->tokenManager->initialize_cache();

    this->complains = new ComplainManager(this);
    if(!this->complains->loadComplains()) logError(this->serverId, "Could not load complains");

    {
        using groups::GroupLoadResult;

        bool initialize_groups{false};
        switch(this->groups_manager_->server_groups()->load_data(true)) {
            case GroupLoadResult::SUCCESS:
                break;
            case GroupLoadResult::NO_GROUPS:
                initialize_groups = true;
                break;

            case GroupLoadResult::DATABASE_ERROR:
                logError(this->getServerId(), "Failed to load server groups (Database error)");
                return false;
        }

        switch(this->groups_manager_->channel_groups()->load_data(true)) {
            case GroupLoadResult::SUCCESS:
                break;
            case GroupLoadResult::NO_GROUPS:
                initialize_groups = true;
                break;

            case GroupLoadResult::DATABASE_ERROR:
                logError(this->getServerId(), "Failed to load channel groups (Database error)");
                return false;
        }

        if(!this->groups_manager_->assignments().load_data(error)) {
            logError(this->getServerId(), "Failed to load group assignments: {}", error);
            return false;
        }

        if (initialize_groups) {
            if(!this->properties()[property::VIRTUALSERVER_AUTOGENERATED_PRIVILEGEKEY].value().empty()) {
                logCritical(this->getServerId(), "Missing default groups. Applying permission reset!");
            }

            string token;
            if(!this->resetPermissions(token)) {
                logCritical(this->serverId, "Failed to reset server permissions! This could be fatal!");
            }
            logMessageFmt(true, this->serverId, "---------------------- Token ----------------------");
            logMessageFmt(true, this->serverId, "{:^51}", "The server's serveradmin token:");
            logMessageFmt(true, this->serverId, "{:^51}", token);
            logMessageFmt(true, this->serverId, "");
            logMessageFmt(true, this->serverId, "{:^51}", "Note: This token could be used just once!");
            logMessageFmt(true, this->serverId, "---------------------- Token ----------------------");
        }
    }

    if(test_properties)
        this->ensureValidDefaultGroups();

    letters = new letter::LetterManager(this);

    server_statistics_ = make_shared<stats::ConnectionStatistics>(serverInstance->getStatistics());

    this->serverRoot = std::make_shared<InternalClient>(this->sql, self.lock(),
                                                        this->properties()[property::VIRTUALSERVER_NAME].value(), false);
    this->serverRoot->initialize_weak_reference(this->serverRoot);

    this->properties()->registerNotifyHandler([&](Property& property) {
        if(property.type() == property::VIRTUALSERVER_NAME) {
            static_pointer_cast<InternalClient>(this->serverRoot)->properties()[property::CLIENT_NICKNAME] = property.value();
        }
    });
    this->serverRoot->server = nullptr;

    this->serverAdmin = std::make_shared<InternalClient>(this->sql, self.lock(), "serveradmin", true);
    this->serverAdmin->initialize_weak_reference(this->serverAdmin);
    DatabaseHelper::assignDatabaseId(this->sql, this->serverId, this->serverAdmin);
    this->serverAdmin->server = nullptr;
    this->registerInternalClient(this->serverAdmin); /* lets assign server id 0  */

    {
        using ErrorType = file::filesystem::ServerCommandErrorType;

        auto file_vs = file::server()->register_server(this->getServerId());
        auto initialize_result = file::server()->file_system().initialize_server(file_vs);
        if(!initialize_result->wait_for(std::chrono::seconds{5})) {
            logError(this->getServerId(), "Failed to wait for file directory initialisation.");
        } else if(!initialize_result->succeeded()) {
            switch (initialize_result->error().error_type) {
                case ErrorType::FAILED_TO_CREATE_DIRECTORIES:
                    logError(this->getServerId(), "Failed to create server file directories ({}).", initialize_result->error().error_message);
                    break;

                case ErrorType::UNKNOWN:
                case ErrorType::FAILED_TO_DELETE_DIRECTORIES:
                    logError(this->getServerId(), "Failed to initialize server file directory due to an unknown error: {}/{}",
                            (int) initialize_result->error().error_type, initialize_result->error().error_message);
                    break;
            }
        }


        this->properties()[property::VIRTUALSERVER_FILEBASE] = file::server()->file_base_path();
        file_vs->max_networking_download_bandwidth(
                this->properties()[property::VIRTUALSERVER_MAX_DOWNLOAD_TOTAL_BANDWIDTH].as_or<int64_t>(-1));
        file_vs->max_networking_upload_bandwidth(
                this->properties()[property::VIRTUALSERVER_MAX_UPLOAD_TOTAL_BANDWIDTH].as_or<int64_t>(-1));
    }

    this->channelTree->printChannelTree([&](std::string msg){ debugMessage(this->serverId, msg); });
    this->music_manager_ = make_shared<music::MusicBotManager>(self.lock());
    this->music_manager_->_self = this->music_manager_;
    this->music_manager_->load_playlists();
    this->music_manager_->load_bots();

#if 0
    if(this->properties()[property::VIRTUALSERVER_ICON_ID] != (IconId) 0)
        if(!serverInstance->getFileServer()->iconExists(self.lock(), this->properties()[property::VIRTUALSERVER_ICON_ID])) {
            debugMessage(this->getServerId(), "Removing invalid icon id of server");
            this->properties()[property::VIRTUALSERVER_ICON_ID] = 0;
       }
#endif

    for(const auto& type : vector<property::VirtualServerProperties>{
        property::VIRTUALSERVER_DOWNLOAD_QUOTA,
        property::VIRTUALSERVER_UPLOAD_QUOTA,
        property::VIRTUALSERVER_MAX_UPLOAD_TOTAL_BANDWIDTH,
        property::VIRTUALSERVER_MAX_DOWNLOAD_TOTAL_BANDWIDTH,
    }) {
        const auto& info = property::describe(type);
        auto prop = this->properties()[type];
        if(prop.default_value() == prop.value()) continue;

        if(!info.validate_input(this->properties()[type].value())) {
            this->properties()[type] = info.default_value;
            logMessage(this->getServerId(), "Server property " + std::string{info.name} + " contains an invalid value! Resetting it.");
        }
    }

    /* lets cleanup the conversations for not existent channels */
    this->conversation_manager_->synchronize_channels();

    {
        auto ref_self = this->self;
        this->task_notify_channel_group_list = multi_shot_task{serverInstance->general_task_executor(), "server notify channel group list", [ref_self]{
            auto this_ = ref_self.lock();
            if(this_) {
                this_->forEachClient([](const std::shared_ptr<ConnectedClient>& client) {
                    std::optional<ts::command_builder> generated_notify{};
                    client->notifyChannelGroupList(generated_notify, true);
                });
            }
        }};
        this->task_notify_server_group_list = multi_shot_task{serverInstance->general_task_executor(), "server notify server group list", [ref_self]{
            auto this_ = ref_self.lock();
            if(this_) {
                this_->forEachClient([](const std::shared_ptr<ConnectedClient>& client) {
                    std::optional<ts::command_builder> generated_notify{};
                    client->notifyServerGroupList(generated_notify, true);
                });
            }
        }};
    }

    return true;
}

VirtualServer::~VirtualServer() {
    memtrack::freed<VirtualServer>(this);
    delete this->tokenManager;
    delete this->channelTree;
    delete this->letters;
    delete this->complains;
    this->conversation_manager_.reset();

    if(this->_serverKey) ecc_free(this->_serverKey);
    delete this->_serverKey;
}

inline bool evaluateAddress4(const string &input, in_addr &address) {
    if(input == "0.0.0.0") {
        address.s_addr = INADDR_ANY;
        return true;
    };
    auto record = gethostbyname(input.c_str());
    if(!record) return false;
    address.s_addr = ((in_addr*) record->h_addr)->s_addr;
    return true;
}

inline bool evaluateAddress6(const string &input, in6_addr &address) {
    if(input == "::") {
        address = IN6ADDR_ANY_INIT;
        return true;
    };
    auto record = gethostbyname2(input.c_str(), AF_INET6);
    if(!record) return false;
    address = *(in6_addr*) record->h_addr;
    return true;
}

inline string strip(std::string message) {
    while(!message.empty()) {
        if(message[0] == ' ')
            message = message.substr(1);
        else if(message[message.length() - 1] == ' ')
            message = message.substr(0, message.length() - 1);
        else break;
    }
    return message;
}

inline vector<string> split_hosts(const std::string& message, char delimiter) {
    vector<string> result;
    size_t found, index = 0;
    do {
        found = message.find(delimiter, index);
        result.push_back(strip(message.substr(index, found - index)));
        index = found + 1;
    } while(index != 0);
    return result;
}

bool VirtualServer::start(std::string& error) {
    {
        std::unique_lock state_lock{this->state_mutex};
        if(this->state != ServerState::OFFLINE){
            error = "Server isn't offline";
            return false;
        }
        this->state = ServerState::BOOTING;
    }
    this->serverRoot->server = self.lock();
    this->serverAdmin->server = self.lock();

    auto host = this->properties()[property::VIRTUALSERVER_HOST].value();
    if(config::binding::enforce_default_voice_host)
        host = config::binding::DefaultVoiceHost;

    if(host.empty()){
        error = "invalid host (\"" + host + "\")";
        this->stop("failed to start", true);
        return false;
    }
    if(this->properties()[property::VIRTUALSERVER_PORT].as_or<uint16_t>(0) <= 0){
        error = "invalid port";
        this->stop("failed to start", true);
        return false;
    }

    std::deque<sockaddr_storage> bindings{};
    for(const auto& address : split_hosts(host, ',')) {
        sockaddr_storage binding{};
        memset(&binding, 0, sizeof(binding));

        if(net::is_ipv4(address)) {
            auto address_v4 = (sockaddr_in*) &binding;
            address_v4->sin_family = AF_INET;
            address_v4->sin_port = htons(this->properties()[property::VIRTUALSERVER_PORT].as_or<uint16_t>(0));
            if(!evaluateAddress4(address, address_v4->sin_addr)) {
                logError(this->serverId, "Fail to resolve v4 address info for \"{}\"", address);
                continue;
            }
        } else if(net::is_ipv6(address)) {
            auto address_v6 = (sockaddr_in6*) &binding;
            address_v6->sin6_family = AF_INET6;
            address_v6->sin6_port = htons(this->properties()[property::VIRTUALSERVER_PORT].as_or<uint16_t>(0));
            if(!evaluateAddress6(address, address_v6->sin6_addr)) {
                logError(this->serverId, "Fail to resolve v6 address info for \"{}\"", address);
                continue;
            }
        } else {
            logError(this->serverId, "Failed to determinate address type for \"{}\"", address);
            continue;
        }

        bindings.emplace_back(std::move(binding));
    }

    //Setup voice server
    udpVoiceServer = std::make_shared<VoiceServer>(self.lock());
    if(!udpVoiceServer->start(bindings, error)) {
        error = "could not start voice server. Message: " + error;
        this->stop("failed to start", false);
        return false;
    }

    if(ts::config::web::activated && serverInstance->sslManager()->web_ssl_options()) {
        string web_host_string = this->properties()[property::VIRTUALSERVER_WEB_HOST];
        if(web_host_string.empty())
            web_host_string = this->properties()[property::VIRTUALSERVER_HOST].as_or<string>(0);

        auto web_port = this->properties()[property::VIRTUALSERVER_WEB_PORT].as_or<uint16_t>(0);
        if(web_port == 0)
            web_port = this->properties()[property::VIRTUALSERVER_PORT].as_or<uint16_t>(0);

        startTimestamp = std::chrono::system_clock::now();
#ifdef COMPILE_WEB_CLIENT
        webControlServer = new WebControlServer(self.lock());


        auto web_bindings = net::resolve_bindings(web_host_string, web_port);
        deque<shared_ptr<WebControlServer::Binding>> bindings;

        for(auto& binding : web_bindings) {
            if(!get<2>(binding).empty()) {
                logError(this->serverId, "[Web] Failed to resolve binding for {}: {}", get<0>(binding), get<2>(binding));
                continue;
            }
            auto entry = make_shared<WebControlServer::Binding>();
            memcpy(&entry->address, &get<1>(binding), sizeof(sockaddr_storage));

            entry->file_descriptor = -1;
            entry->event_accept = nullptr;
            bindings.push_back(entry);
        }

        logMessage(this->serverId, "[Web] Starting server on {}:{}", web_host_string, web_port);
        if(!webControlServer->start(bindings, error)) {
            error = "could not start web server. Message: " + error;
            this->stop("failed to start", false);
            return false;
        }
#endif
    }

    auto weak_this = this->self;
    serverInstance->general_task_executor()->schedule_repeating(
            this->tick_task_id,
            "server tick " + std::to_string(this->serverId),
            std::chrono::milliseconds {500},
            [weak_this](const auto& scheduled){
                auto ref_self = weak_this.lock();
                if(ref_self) {
                    ref_self->executeServerTick();
                }
            }
    );

    properties()[property::VIRTUALSERVER_CLIENTS_ONLINE] = 0;
    properties()[property::VIRTUALSERVER_QUERYCLIENTS_ONLINE] = 0;
    properties()[property::VIRTUALSERVER_CHANNELS_ONLINE] = 0;
    properties()[property::VIRTUALSERVER_UPTIME] = 0;
    this->startTimestamp = system_clock::now();

    this->music_manager_->cleanup_semi_bots();
    this->music_manager_->connectBots();

    {
        std::unique_lock state_lock{this->state_mutex};
        this->state = ServerState::ONLINE;
    }
    return true;
}

std::string VirtualServer::publicServerKey() {
    size_t keyBufferLength = 265;
    char keyBuffer[keyBufferLength];
    if(ecc_export((unsigned char *) keyBuffer, &keyBufferLength, PK_PUBLIC, this->_serverKey) != CRYPT_OK) return "";
    return base64::encode(string(keyBuffer, keyBufferLength));
}

bool VirtualServer::running() {
    return this->state == ServerState::BOOTING || this->state == ServerState::ONLINE;
}

void VirtualServer::preStop(const std::string& reason) {
    {
        std::unique_lock state_lock{this->state_mutex};
        if(!this->running() && this->state != ServerState::SUSPENDING) {
            return;
        }
        this->state = ServerState::SUSPENDING;
    }

    for(const auto& cl : this->getClients()) {
        unique_lock channel_lock(cl->channel_tree_mutex);
        if (cl->currentChannel) {
            auto vc = dynamic_pointer_cast<VoiceClient>(cl);
            if(vc) {
                vc->disconnect(VREASON_SERVER_SHUTDOWN, reason, nullptr, false);
            } else {
                cl->notifyClientLeftView(cl, nullptr, ViewReasonId::VREASON_SERVER_SHUTDOWN, reason, nullptr, false);
            }
        }
        cl->visibleClients.clear();
        cl->mutedClients.clear();
    }
}

void VirtualServer::stop(const std::string& reason, bool disconnect_query) {
    auto self_lock = this->self.lock();
    assert(self_lock);
    {
        std::unique_lock state_lock{this->state_mutex};
        if(!this->running() && this->state != ServerState::SUSPENDING) return;
        this->state = ServerState::SUSPENDING;
    }

    this->preStop(reason);

    for(const auto& cl : this->getClients()) { //start disconnecting
        if(cl->getType() == CLIENT_TEAMSPEAK || cl->getType() == CLIENT_TEASPEAK || cl->getType() == CLIENT_WEB) {
            cl->close_connection(chrono::system_clock::now() + chrono::seconds(1));
        } else if(cl->getType() == CLIENT_QUERY){
            threads::MutexLock lock(cl->command_lock);
            cl->currentChannel = nullptr;

            if(disconnect_query) {
                auto qc = dynamic_pointer_cast<QueryClient>(cl);
                qc->disconnect_from_virtual_server("server disconnect");
            }
        } else if(cl->getType() == CLIENT_MUSIC) {
            cl->disconnect("");
            cl->currentChannel = nullptr;
        } else if(cl->getType() == CLIENT_INTERNAL) {

        } else {
            logError(this->serverId, "Got client with unknown type: " + to_string(cl->getType()));
        }
    }
    this->music_manager_->disconnectBots();

    serverInstance->general_task_executor()->cancel_task(this->tick_task_id);
    this->tick_task_id = 0;

    if(this->udpVoiceServer) this->udpVoiceServer->stop();
    this->udpVoiceServer = nullptr;

#ifdef COMPILE_WEB_CLIENT
    if(this->webControlServer) this->webControlServer->stop();
    delete this->webControlServer;
    this->webControlServer = nullptr;
#endif

    properties()[property::VIRTUALSERVER_CLIENTS_ONLINE] = 0;
    properties()[property::VIRTUALSERVER_QUERYCLIENTS_ONLINE] = 0;
    properties()[property::VIRTUALSERVER_CHANNELS_ONLINE] = 0;
    properties()[property::VIRTUALSERVER_UPTIME] = 0;

    {
        std::unique_lock state_lock{this->state_mutex};
        this->state = ServerState::OFFLINE;
    }
    this->serverRoot->server = nullptr;
    this->serverAdmin->server = nullptr;
}

ServerSlotUsageReport VirtualServer::onlineStats() {
    ServerSlotUsageReport response{};
    response.server_count = 1;
    response.max_clients = this->properties()[property::VIRTUALSERVER_MAXCLIENTS].as_or<size_t>(0);
    response.reserved_clients = this->properties()[property::VIRTUALSERVER_RESERVED_SLOTS].as_or<size_t>(0);
    {
        std::lock_guard shared_lock{this->channel_tree_mutex};
        response.online_channels = this->channelTree->channel_count();
    }
    for(const auto &client : this->getClients()) {
        switch (client->getType()) {
            case CLIENT_TEAMSPEAK:
                response.clients_teamspeak++;
                break;

            case CLIENT_TEASPEAK:
                response.clients_teaspeak++;
                break;

            case CLIENT_WEB:
                response.clients_teaweb++;
                break;

            case CLIENT_QUERY:
                response.queries++;
                break;

            case CLIENT_MUSIC:
                response.music_bots++;
                break;

            case CLIENT_INTERNAL:
            case MAX:
            default:
                break;
        }
    }

    return response;
}

std::shared_ptr<ConnectedClient> VirtualServer::find_client_by_id(uint16_t client_id) {
    std::lock_guard lock{this->clients_mutex};
    auto it = this->clients.find(client_id);
    if(it == this->clients.end()) {
        return nullptr;
    } else {
        return it->second;
    }
}

deque<shared_ptr<ConnectedClient>> VirtualServer::findClientsByCldbId(uint64_t cldbId) {
    std::deque<shared_ptr<ConnectedClient>> result;

    std::lock_guard lock{this->clients_mutex};
    for(const auto& [_, client] : this->clients) {
        if(client->getClientDatabaseId() == cldbId) {
            result.push_back(client);
        }
    }

    return result;
}

deque<shared_ptr<ConnectedClient>> VirtualServer::findClientsByUid(std::string uid) {
    std::deque<shared_ptr<ConnectedClient>> result;

    std::lock_guard lock{this->clients_mutex};
    for(const auto& [_, client] : this->clients) {
        if(client->getUid() == uid) {
            result.push_back(client);
        }
    }

    return result;
}

std::shared_ptr<ConnectedClient> VirtualServer::findClient(std::string name, bool ignoreCase) {
    if(ignoreCase) {
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);
    }

    std::lock_guard lock{this->clients_mutex};
    for(const auto& [_, client] : this->clients) {
        string clName = client->getDisplayName();
        if(ignoreCase) {
            std::transform(clName.begin(), clName.end(), clName.begin(), ::tolower);
        }

        if(clName == name) {
            return client;
        }
    }

    return nullptr;
}

bool VirtualServer::forEachClient(std::function<void(std::shared_ptr<ConnectedClient>)> function) {
    for(const auto& elm : this->getClients()) {
        shared_lock close_lock(elm->finalDisconnectLock, try_to_lock_t{});
        if(close_lock.owns_lock()) {
            //If not locked than client is on the way to disconnect
            if(elm->state == ConnectionState::CONNECTED && elm->getType() != ClientType::CLIENT_INTERNAL) {
                function(elm);
            }
        }
    }
    return true;
}

std::vector<std::shared_ptr<ConnectedClient>> VirtualServer::getClients() {
    std::vector<shared_ptr<ConnectedClient>> result{};

    std::lock_guard lock{this->clients_mutex};
    result.reserve(this->clients.size());

    for(const auto& [_, client] : this->clients) {
        result.push_back(client);
    }

    return result;
}

/* Note: This method **should** not lock the channel tree else we've a lot to do! */
deque<shared_ptr<ConnectedClient>> VirtualServer::getClientsByChannel(std::shared_ptr<BasicChannel> channel) {
    assert(this);

    auto s_channel = dynamic_pointer_cast<ServerChannel>(channel);
    assert(s_channel);

    if(!s_channel) {
        return {};
    }

    std::shared_lock client_lock{s_channel->client_lock};
    auto weak_clients = s_channel->clients;
    client_lock.unlock();

    std::deque<std::shared_ptr<ConnectedClient>> result;
    for(const auto& weak_client : weak_clients) {
        auto client = weak_client.lock();
        if(!client) {
            continue;
        }

        if(client->connectionState() != ConnectionState::CONNECTED) {
            continue;
        }

        if(client->getChannel() != channel) {
            /* This should not happen! */
            continue;
        }

        result.push_back(move(client));
    }
    return result;
}

deque<shared_ptr<ConnectedClient>> VirtualServer::getClientsByChannelRoot(const std::shared_ptr<BasicChannel> &root, bool lock) {
    assert(this);

    shared_lock channel_lock(this->channel_tree_mutex, defer_lock);
    if(lock)
        channel_lock.lock();

    std::deque<std::shared_ptr<ConnectedClient>> result;
    for(const auto& channel : this->channelTree->channels(root)) {
        auto clients = this->getClientsByChannel(channel);
        result.insert(result.end(), clients.begin(), clients.end());
    }

    return result;
}

size_t VirtualServer::countChannelRootClients(const std::shared_ptr<BasicChannel> &root, size_t limit, bool lock_channel_tree) {
    std::shared_lock channel_lock{this->channel_tree_mutex, defer_lock};
    if(lock_channel_tree) {
        channel_lock.lock();
    }

    size_t result{0};
    for(const auto& channel : this->channelTree->channels(root)) {
        auto channel_clients = this->getClientsByChannel(channel);
        result += channel_clients.size();

        if(result >= limit) {
            return limit;
        }
    }

    return result;
}

bool VirtualServer::isChannelRootEmpty(const std::shared_ptr<BasicChannel> &root, bool lock_channel_tree) {
    std::shared_lock channel_lock{this->channel_tree_mutex, defer_lock};
    if(lock_channel_tree) {
        channel_lock.lock();
    }

    for(const auto& channel : this->channelTree->channels(root)) {
        auto channel_clients = this->getClientsByChannel(channel);
        if(!channel_clients.empty()) {
            return false;
        }
    }

    return true;
}

bool VirtualServer::notifyServerEdited(std::shared_ptr<ConnectedClient> invoker, deque<string> keys) {
    if(!invoker) return false;

    Command cmd("notifyserveredited");

    cmd["invokerid"] = invoker->getClientId();
    cmd["invokername"] = invoker->getDisplayName();
    cmd["invokeruid"] = invoker->getUid();
    cmd["reasonid"] = ViewReasonId::VREASON_EDITED;
    for(const auto& key : keys) {
        const auto& info = property::find<property::VirtualServerProperties>(key);
        if(info == property::VIRTUALSERVER_UNDEFINED) {
            logError(this->getServerId(), "Tried to broadcast a server update with an unknown info: " + key);
            continue;
        }
        cmd[key] = properties()[info].value();
    }
    this->forEachClient([&cmd](shared_ptr<ConnectedClient> client){
        client->sendCommand(cmd);
    });
    return true;
}

bool VirtualServer::notifyClientPropertyUpdates(std::shared_ptr<ConnectedClient> client, const deque<const property::PropertyDescription*>& keys, bool selfNotify) {
    if(keys.empty() || !client) return false;
    this->forEachClient([&](const shared_ptr<ConnectedClient>& cl) {
        shared_lock client_channel_lock(cl->channel_tree_mutex);
        if(cl->isClientVisible(client, false) || (cl == client && selfNotify))
            cl->notifyClientUpdated(client, keys, false);
    });
    return true;
}

void VirtualServer::broadcastMessage(std::shared_ptr<ConnectedClient> invoker, std::string message) {
    if(!invoker) {
        logCritical(this->serverId, "Tried to broadcast with an invalid invoker!");
        return;
    }
    this->forEachClient([&](shared_ptr<ConnectedClient> cl){
        cl->notifyTextMessage(ChatMessageMode::TEXTMODE_SERVER, invoker, 0, 0, system_clock::now(), message);
    });
}

ts_always_inline bool channel_ignore_permission(ts::permission::PermissionType type) {
    return permission::i_icon_id == type;
}

vector<pair<ts::permission::PermissionType, ts::permission::v2::PermissionFlaggedValue>> VirtualServer::calculate_permissions(
        const std::deque<permission::PermissionType>& permissions,
        ClientDbId client_dbid,
        ClientType client_type,
        ChannelId channel_id,
        bool calculate_granted) {
    ClientPermissionCalculator calculator{this->ref(), client_dbid, client_type, channel_id};
    return calculator.calculate_permissions(permissions, calculate_granted);
}

permission::v2::PermissionFlaggedValue VirtualServer::calculate_permission(
        permission::PermissionType permission,
        ClientDbId cldbid,
        ClientType type,
        ChannelId channel,
        bool granted) {
    auto result = this->calculate_permissions({permission}, cldbid, type, channel, granted);
    if(result.empty()) return {0, false};

    return result.front().second;
}

bool VirtualServer::verifyServerPassword(std::string password, bool hashed) {
    if(!this->properties()[property::VIRTUALSERVER_FLAG_PASSWORD].as_or<bool>(false)) {
        return true;
    }

    if(password.empty()) {
        return false;
    }

    if(!hashed) {
        password = base64::encode(digest::sha1(password));
    }

    return password == this->properties()[property::VIRTUALSERVER_PASSWORD].value();
}

VirtualServer::NetworkReport VirtualServer::generate_network_report() {
    double total_ping{0}, total_loss{0};
    size_t pings_counted{0}, loss_counted{0};

    this->forEachClient([&](const std::shared_ptr<ConnectedClient>& client) {
        if(auto vc = dynamic_pointer_cast<VoiceClient>(client); vc) {
            total_ping += vc->current_ping().count();
            total_loss += vc->current_packet_loss();
            pings_counted++;
            loss_counted++;
        }
#ifdef COMPILE_WEB_CLIENT
        else if(client->getType() == ClientType::CLIENT_WEB) {
            pings_counted++;
            total_ping += duration_cast<milliseconds>(dynamic_pointer_cast<WebClient>(client)->client_ping()).count();
        }
#endif
    });

    VirtualServer::NetworkReport result{};
    if(loss_counted) result.average_loss = total_loss / loss_counted;
    if(pings_counted) result.average_ping = total_ping / pings_counted;
    return result;
}

bool VirtualServer::resetPermissions(std::string& new_permission_token) {
    std::map<GroupId, GroupId> server_group_mapping{};
    std::map<GroupId, GroupId> channel_group_mapping{};
    {
        this->group_manager()->server_groups()->reset_groups(server_group_mapping);
        this->group_manager()->channel_groups()->reset_groups(channel_group_mapping);
        this->group_manager()->assignments().reset_all();
    }

    /* assign the properties */
    {
        this->properties()[property::VIRTUALSERVER_DEFAULT_SERVER_GROUP] =
                server_group_mapping[serverInstance->properties()[property::SERVERINSTANCE_TEMPLATE_SERVERDEFAULT_GROUP].as_or<GroupId>(0)];

        this->properties()[property::VIRTUALSERVER_DEFAULT_MUSIC_GROUP] =
                server_group_mapping[serverInstance->properties()[property::SERVERINSTANCE_TEMPLATE_MUSICDEFAULT_GROUP].as_or<GroupId>(0)];

        this->properties()[property::VIRTUALSERVER_DEFAULT_CHANNEL_ADMIN_GROUP] =
                channel_group_mapping[serverInstance->properties()[property::SERVERINSTANCE_TEMPLATE_CHANNELADMIN_GROUP].as_or<GroupId>(0)];

        this->properties()[property::VIRTUALSERVER_DEFAULT_CHANNEL_GROUP] =
                channel_group_mapping[serverInstance->properties()[property::SERVERINSTANCE_TEMPLATE_CHANNELDEFAULT_GROUP].as_or<GroupId>(0)];
    }

    {
        this->properties()[property::VIRTUALSERVER_ASK_FOR_PRIVILEGEKEY] = false;

        auto admin_group_id = server_group_mapping[serverInstance->properties()[property::SERVERINSTANCE_TEMPLATE_SERVERADMIN_GROUP].as_or<GroupId>(0)];
        auto admin_group = this->group_manager()->server_groups()->find_group(groups::GroupCalculateMode::GLOBAL, admin_group_id);
        if(!admin_group) {
            logCritical(this->getServerId(), "Missing default server admin group. Don't generate a new token.");
            new_permission_token = "missing server admin group";
        } else {
            auto token = this->tokenManager->create_token(0, "default server admin token", 1, std::chrono::system_clock::time_point{});
            if(!token) {
                logCritical(this->serverId, "Failed to register the default server admin token.");
            } else {
                std::vector<token::TokenAction> actions{};
                actions.push_back(token::TokenAction{
                        .id = 0,
                        .type = token::ActionType::AddServerGroup,
                        .id1 = admin_group->group_id(),
                        .id2 = 0
                });

                this->tokenManager->add_token_actions(token->id, actions);
                new_permission_token = token->token;

                this->properties()[property::VIRTUALSERVER_AUTOGENERATED_PRIVILEGEKEY] = token->token;
                this->properties()[property::VIRTUALSERVER_ASK_FOR_PRIVILEGEKEY] = true;
            }
        }
    }

    this->ensureValidDefaultGroups();

    this->task_notify_channel_group_list.enqueue();
    this->task_notify_server_group_list.enqueue();

    for(const auto& client : this->getClients()) {
        client->task_update_displayed_groups.enqueue();
        client->task_update_needed_permissions.enqueue();
        client->task_update_channel_client_properties.enqueue();
    }

    return true;
}

void VirtualServer::ensureValidDefaultGroups() {
    /* TODO: FIXME: Impl! */
#if 0
    auto default_server_group = this->group_manager()->defaultGroup(GROUPTARGET_SERVER, true);
    if(!default_server_group) {
        logError(this->serverId, "Missing server's default server group! (Id: {})", this->properties()[property::VIRTUALSERVER_DEFAULT_SERVER_GROUP].value());

        default_server_group = this->group_manager()->availableServerGroups(false).front();
        logError(this->serverId, "Using {} ({}) instead!", default_server_group->groupId(), default_server_group->name());
        this->properties()[property::VIRTUALSERVER_DEFAULT_SERVER_GROUP] = default_server_group->groupId();
    }

    auto default_music_group = this->group_manager()->defaultGroup(GROUPTARGET_SERVER, true);
    if(!default_music_group) {
        logError(this->serverId, "Missing server's default music group! (Id: {})", this->properties()[property::VIRTUALSERVER_DEFAULT_MUSIC_GROUP].value());

        default_music_group =  default_server_group;
        logError(this->serverId, "Using {} ({}) instead!", default_music_group->groupId(), default_music_group->name());
        this->properties()[property::VIRTUALSERVER_DEFAULT_MUSIC_GROUP] = default_music_group->groupId();
    }

    auto default_channel_group = this->group_manager()->defaultGroup(GROUPTARGET_CHANNEL, true);
    if(!default_channel_group) {
        logError(this->serverId, "Missing server's default channel group! (Id: {})", this->properties()[property::VIRTUALSERVER_DEFAULT_CHANNEL_GROUP].value());

        default_channel_group = this->group_manager()->availableChannelGroups(false).front();
        logError(this->serverId, "Using {} ({}) instead!", default_channel_group->groupId(), default_channel_group->name());
        this->properties()[property::VIRTUALSERVER_DEFAULT_CHANNEL_GROUP] = default_channel_group->groupId();
    }

    auto admin_channel_group = this->group_manager()->findGroupLocal(
            this->properties()[property::VIRTUALSERVER_DEFAULT_CHANNEL_ADMIN_GROUP].as_or<GroupId>(0));
    if(!admin_channel_group) {
        logError(this->serverId, "Missing server's default channel admin group! (Id: {})", this->properties()[property::VIRTUALSERVER_DEFAULT_CHANNEL_ADMIN_GROUP].value());

        admin_channel_group = this->group_manager()->availableChannelGroups(false).front();
        logError(this->serverId, "Using {} ({}) instead!", admin_channel_group->groupId(), admin_channel_group->name());
        this->properties()[property::VIRTUALSERVER_DEFAULT_CHANNEL_ADMIN_GROUP] = admin_channel_group->groupId();
    }
#endif
}

void VirtualServer::send_text_message(const std::shared_ptr<BasicChannel> &channel, const std::shared_ptr<ConnectedClient> &sender, const std::string &message) {
    assert(channel);
    assert(sender);

    auto client_id = sender->getClientId();
    auto channel_id = channel->channelId();
    auto now = chrono::system_clock::now();

    bool conversation_private;
    auto conversation_mode = channel->properties()[property::CHANNEL_CONVERSATION_MODE].as_or<ChannelConversationMode>(ChannelConversationMode::CHANNELCONVERSATIONMODE_PRIVATE);
    if(conversation_mode == ChannelConversationMode::CHANNELCONVERSATIONMODE_NONE) {
        /* nothing to do */
        return;
    } else {
        conversation_private = conversation_mode == ChannelConversationMode::CHANNELCONVERSATIONMODE_PRIVATE;
    }

    auto flag_password = channel->properties()[property::CHANNEL_FLAG_PASSWORD].as_or<bool>(false);
    for(const auto& client : this->getClients()) {
        if(client->connectionState() != ConnectionState::CONNECTED)
            continue;

        auto type = client->getType();
        if(type == ClientType::CLIENT_INTERNAL || type == ClientType::CLIENT_MUSIC)
            continue;

        auto own_channel = client->currentChannel == channel;
        if(conversation_private && !own_channel)
            continue;

        if(type != ClientType::CLIENT_TEAMSPEAK || own_channel) {
            if(!own_channel && &*client != client) {
                if(flag_password)
                    continue; /* TODO: Send notification about new message. The client then could request messages via message history */

                if(auto err_perm{client->calculate_and_get_join_state(channel)}; err_perm)
                    continue;
            }
            client->notifyTextMessage(ChatMessageMode::TEXTMODE_CHANNEL, sender, client_id, channel_id, now, message);
        }
    }

    if(!conversation_private) {
        auto conversations = this->conversation_manager();
        auto conversation = conversations->get_or_create(channel->channelId());
        conversation->register_message(sender->getClientDatabaseId(), sender->getUid(), sender->getDisplayName(), now, message);
    }
}

void VirtualServer::update_channel_from_permissions(const std::shared_ptr<BasicChannel> &channel, const std::shared_ptr<ConnectedClient>& issuer) {
    bool require_view_update;
    auto property_updates = channel->update_properties_from_permissions(require_view_update);

    if(!property_updates.empty()) {
        this->forEachClient([&](const std::shared_ptr<ConnectedClient>& cl) {
            shared_lock client_channel_lock(cl->channel_tree_mutex);
            cl->notifyChannelEdited(channel, property_updates, issuer, false);
        });
    }

    if(require_view_update) {
        auto l_source = this->channelTree->findLinkedChannel(channel->channelId());
        this->forEachClient([&](const shared_ptr<ConnectedClient>& cl) {
            /* server tree read lock still active */
            auto l_target = !cl->currentChannel ? nullptr : cl->server->channelTree->findLinkedChannel(cl->currentChannel->channelId());
            sassert(l_source);
            if(cl->currentChannel) sassert(l_target);

            {
                unique_lock client_channel_lock(cl->channel_tree_mutex);

                deque<ChannelId> deleted;
                for(const auto& [flag_visible, channel] : cl->channel_tree->update_channel(l_source, l_target)) {
                    if(flag_visible) {
                        cl->notifyChannelShow(channel->channel(), channel->previous_channel);
                    } else {
                        deleted.push_back(channel->channelId());
                    }
                }
                if(!deleted.empty()) {
                    cl->notifyChannelHide(deleted, false);
                }
            }
        });
    }
}

std::shared_ptr<groups::ServerGroup> VirtualServer::default_server_group() {
    auto group_id = this->properties()[property::VIRTUALSERVER_DEFAULT_SERVER_GROUP].as_or<GroupId>(0);
    auto group = this->group_manager()->server_groups()->find_group(groups::GroupCalculateMode::GLOBAL, group_id);
    if(!group) {
        auto groups = this->group_manager()->server_groups()->available_groups(groups::GroupCalculateMode::GLOBAL);
        if(groups.empty()) {
            logCritical(this->serverId, "Having no available server groups.");
            return nullptr;
        }

        /* TODO: Log warning? */
        group = groups.back();
    }

    return group;
}

std::shared_ptr<groups::ChannelGroup> VirtualServer::default_channel_group() {
    auto group_id = this->properties()[property::VIRTUALSERVER_DEFAULT_CHANNEL_GROUP].as_or<GroupId>(0);
    auto group = this->group_manager()->channel_groups()->find_group(groups::GroupCalculateMode::GLOBAL, group_id);
    if(!group) {
        auto groups = this->group_manager()->channel_groups()->available_groups(groups::GroupCalculateMode::GLOBAL);
        if(groups.empty()) {
            logCritical(this->serverId, "Having no available channel groups.");
            return nullptr;
        }

        /* TODO: Log warning? */
        group = groups.back();
    }

    return group;
}