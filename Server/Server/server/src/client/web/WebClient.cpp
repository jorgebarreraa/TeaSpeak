#define HAVE_JSON

#include "WebClient.h"
#include <log/LogUtils.h>
#include <misc/endianness.h>
#include <src/client/voice/VoiceClient.h>
#include <src/VirtualServerManager.h>
#include <netinet/tcp.h>
#include <src/InstanceHandler.h>
#include <misc/memtracker.h>
#include <src/client/ConnectedClient.h>
#include <misc/std_unique_ptr.h>
#include <src/client/SpeakingClient.h>
#include "../../manager/ActionLogger.h"
#include "../../server/GlobalNetworkEvents.h"

#if defined(TCP_CORK) && !defined(TCP_NOPUSH)
    #define TCP_NOPUSH TCP_CORK
#endif

using namespace std;
using namespace std::chrono;
using namespace ts;
using namespace ts::server;
using namespace ts::protocol;

WebClient::WebClient(WebControlServer* server, int fd) :
    SpeakingClient(server->getTS()->getSql(), server->getTS()),
    handle{server},
    whisper_handler_{this} {
    memtrack::allocated<WebClient>(this);

    assert(server->getTS());
    this->state = ConnectionState::INIT_LOW;
    this->file_descriptor = fd;

    debugMessage(this->server->getServerId(), " Creating WebClient instance at {}", (void*) this);
}

void WebClient::initialize() {
    auto ref_this = dynamic_pointer_cast<WebClient>(this->ref());
    this->command_queue = std::make_unique<ServerCommandQueue>(serverInstance->server_command_executor(), std::make_unique<WebClientCommandHandler>(ref_this));

    int enabled = 1;
    int disabled = 0;
    setsockopt(this->file_descriptor, SOL_SOCKET, SO_KEEPALIVE, &enabled, sizeof(enabled));
    if(setsockopt(this->file_descriptor, IPPROTO_TCP, TCP_NOPUSH, &disabled, sizeof disabled) < 0) {
        logError(this->getServerId(), "{} Cant disable nopush! system error: {} => {}", CLIENT_STR_LOG_PREFIX, errno, strerror(errno));
    }

    this->readEvent = serverInstance->network_event_loop()->allocate_event(this->file_descriptor, EV_READ | EV_PERSIST, WebClient::handleMessageRead, this, nullptr);
    this->writeEvent = serverInstance->network_event_loop()->allocate_event(this->file_descriptor, EV_WRITE, WebClient::handleMessageWrite, this, nullptr);

    {
        this->ws_handler.direct_process(pipes::PROCESS_DIRECTION_IN, true);
        this->ws_handler.direct_process(pipes::PROCESS_DIRECTION_OUT, true);
        this->ws_handler.callback_invalid_request = [&](const http::HttpRequest& request, http::HttpResponse& response) {
            response.setHeader("Connection", {"close"}); /* close the connection to avoid a second request */
            response.code = http::code::code(301, "Invalid request (Not a web socket!)");

            const auto redirect_target = [&]{
                if(request.parameters.count("forward_url"))
                    response.setHeader("Location", {request.parameters.at("forward_url")});
                else if(request.findHeader("Origin"))
                    response.header.push_back({"Location", request.findHeader("Origin").values});
                else
                    response.header.push_back({"Location", {"https://web.teaspeak.de"}});
            };

            /* we're running https */
            if(this->ssl_encrypted) {
                redirect_target();
            } else {
                /* lets redirect to https */
                if(request.findHeader("Host")) {
                    const auto redirect_forward = [&](const std::string& url) {
                        response.setHeader("Location", {"https://" + request.findHeader("Host").values[0] + "/?forward_url=" + http::encode_url(url)});
                    };
                    if(request.parameters.count("forward_url"))
                        redirect_forward(request.parameters.at("forward_url"));
                    else if(request.findHeader("Origin"))
                        redirect_forward(request.findHeader("Origin").values[0]);
                    else
                        redirect_forward("https://web.teaspeak.de");
                } else {
                    /* we could not find our host */
                    redirect_target();
                }
            }
        };

        this->ws_handler.on_connect = std::bind(&WebClient::onWSConnected, this);
        this->ws_handler.on_disconnect = std::bind(&WebClient::onWSDisconnected, this, placeholders::_1);

        this->ws_handler.callback_data([&](const pipes::WSMessage& msg){
            this->onWSMessage(msg);
        });
        this->ws_handler.callback_write([&](const pipes::buffer_view& data) {
            if(this->ssl_encrypted)
                this->ssl_handler.send(data);
            else
                this->enqueue_raw_packet(data);
        });
        this->ws_handler.callback_error([&](int error, const std::string& message) {
            logError(this->getServerId(), "{} Got ws pipeline error ({} | {})", CLIENT_STR_LOG_PREFIX, error, message);
        });
    }

    {
        this->ssl_handler.direct_process(pipes::PROCESS_DIRECTION_IN, true);
        this->ssl_handler.direct_process(pipes::PROCESS_DIRECTION_OUT, true);

        this->ssl_handler.callback_data([&](const pipes::buffer_view& msg) {
            this->ws_handler.process_incoming_data(msg);
        });

        this->ssl_handler.callback_write([&](const pipes::buffer_view& msg) { this->enqueue_raw_packet(msg); });
        this->ssl_handler.callback_error([&](int error, const std::string& message) {
            logError(this->getServerId(), "{} Got ssl pipeline error ({} | {})", CLIENT_STR_LOG_PREFIX, error, message);
        });
    }

    this->ssl_handler.initialize(serverInstance->sslManager()->web_ssl_options());
    this->ws_handler.initialize();
}

WebClient::~WebClient() {
    memtrack::freed<WebClient>(this);
    debugMessage(this->server->getServerId(), " Destroying WebClient instance at {}", (void*) this);
}

static Json::StreamWriterBuilder stream_write_builder = []{
    Json::StreamWriterBuilder builder;

    builder["commentStyle"] = "None";
    builder["indentation"] = "";

    return builder;
}();

void WebClient::sendJson(const Json::Value& json) {
    unique_ptr<Json::StreamWriter> writer{stream_write_builder.newStreamWriter()};

    std::ostringstream stream;
    if(writer->write(json, &stream) != 0) {
        //TODO log error, but this shall never happen
        return;
    }
    if(!stream.good()) {
        //TODO log error
        return;
    }

    auto data = stream.str();
    pipes::buffer buffer = buffer::allocate_buffer(data.length());
    buffer.write(data.data(), data.length());

    this->ws_handler.send({pipes::TEXT, std::move(buffer)});
}

void WebClient::sendCommand(const ts::Command &command, bool low) {
    auto command_payload = command.build();
    if(this->allow_raw_commands) {
        Json::Value value{};
        value[Json::StaticString("type")] = "command-raw";
        value[Json::StaticString("payload")] = std::move(command_payload);
        this->sendJson(value);
    } else {
        /* TODO: Fully remove this mode. */
        ts::command_parser parser{command_payload};

        std::string_view key{};
        std::string value{};

        Json::Value json_command{};
        json_command[Json::StaticString("type")] = "command";
        json_command[Json::StaticString("command")] = std::string{parser.identifier()};

        int bulk_index{0};
        for(auto& bulk : parser.bulks()) {
            auto& json_bulk = json_command[Json::StaticString("data")][bulk_index++];

            size_t index{0};
            while(bulk.next_entry(index, key, value)) {
                const std::string key_str(key);
                json_bulk[Json::StaticString(key_str.c_str())] = std::move(value);
            }
        }

        this->sendJson(json_command);
    }
}

void WebClient::sendCommand(const ts::command_builder &command, bool low) {
    if(this->allow_raw_commands) {
        Json::Value value{};
        value[Json::StaticString("type")] = "command-raw";
        value[Json::StaticString("payload")] = command.build();
        this->sendJson(value);
    } else {
        auto data = command.build();
        Command parsed_command = Command::parse(data, true, false);
        this->sendCommand(parsed_command, low);
    }
}

bool WebClient::close_connection(const std::chrono::system_clock::time_point& timeout) {
    bool flushing = timeout.time_since_epoch().count() > 0;

    auto self_lock = dynamic_pointer_cast<WebClient>(this->ref());
    auto server_lock = this->server;
    assert(self_lock);

    unique_lock state_lock(this->state_lock);
    if(this->state == ConnectionState::DISCONNECTED) return false;
    if(this->state == ConnectionState::DISCONNECTING && flushing) return true;
    this->state = flushing ? ConnectionState::DISCONNECTING : ConnectionState::DISCONNECTED;

    unique_lock close_lock(this->close_lock);
    state_lock.unlock();

    if(this->readEvent)
        event_del_noblock(this->readEvent);

    if(this->server){
        {
            unique_lock server_channel_lock(this->server->channel_tree_mutex);
            this->server->unregisterClient(this->ref(), "disconnected", server_channel_lock);
        }
        //this->server = nullptr;
    }

    if(flushing){
        this->flush_thread = std::thread([self_lock, server_lock, timeout] {
            bool flag_flushed;
            while(true) {
                {
                    lock_guard lock(self_lock->state_lock);
                    if(self_lock->state != ConnectionState::DISCONNECTING) return; /* somebody else had this problem now */
                }

                flag_flushed = true;

                {
                    lock_guard lock(self_lock->queue_mutex);
                    flag_flushed &= self_lock->queue_write.empty();
                }

                if(flag_flushed)
                    break;

                auto now = system_clock::now();
                if(timeout < now)
                    break;
                else
                    this_thread::sleep_for(min(duration_cast<milliseconds>(timeout - now), milliseconds(10)));
            }

            {
                lock_guard lock(self_lock->state_lock);
                if(self_lock->state != ConnectionState::DISCONNECTING) return; /* somebody else had this problem now */
                self_lock->state = ConnectionState::DISCONNECTED;
            }
            /* we can lock here again because we've already ensured that we're still disconnecting and updated the status to disconnected.
             * So no thread will wait for this thread while close_lock had been locked
             */
            lock_guard close_lock(self_lock->close_lock);
            self_lock->disconnectFinal();
        });
    } else {
        /* lets wait 'till the flush thread recognized that we're overriding close */
        if(this->flush_thread.joinable())
            this->flush_thread.join();

        disconnectFinal();
    }
    return true;
}

command_result WebClient::handleCommand(Command &command) {
    if(this->connectionState() == ConnectionState::INIT_HIGH && this->handshake.state == HandshakeState::SUCCEEDED){
        if(command.command() == "clientinit") {
            auto result = this->handleCommandClientInit(command);
            if(result.has_error()) {
                this->close_connection(system_clock::now() + seconds(1));
            }
            return result;
        }
    }

    if(command.command() == "whispersessioninitialize") {
        return this->handleCommandWhisperSessionInitialize(command);
    } else if(command.command() == "whispersessionreset") {
        return this->handleCommandWhisperSessionReset(command);
    }
    return SpeakingClient::handleCommand(command);
}

void WebClient::tick_server(const std::chrono::system_clock::time_point& point) {
    SpeakingClient::tick_server(point);

    if(this->ping.last_request + seconds(1) < point) {
        if(this->ping.last_response > this->ping.last_request || this->ping.last_response + this->ping.timeout < point) {
            this->ping.current_id++;
            this->ping.last_request = point;

            char buffer[2];
            le2be8(this->ping.current_id, buffer);
            this->ws_handler.send({pipes::PING, {buffer, 2}});
        }
    }
    if(this->js_ping.last_request + seconds(1) < point) {
        if(this->js_ping.last_response > this->js_ping.last_request || this->js_ping.last_request + this->js_ping.timeout < point) {
            this->js_ping.current_id++;
            this->js_ping.last_request = point;

            Json::Value jsonCandidate;
            jsonCandidate[Json::StaticString("type")] = "ping";
            jsonCandidate[Json::StaticString("payload")] = to_string(this->js_ping.current_id);

            this->sendJson(jsonCandidate);
        }
    }
}

void WebClient::onWSConnected() {
    this->state = ConnectionState::INIT_HIGH;
    this->handshake.state = HandshakeState::BEGIN;
    debugMessage(this->getServerId(), "{} WebSocket handshake completed!", CLIENT_STR_LOG_PREFIX);
    //TODO here!

    this->properties()[property::CLIENT_TYPE] = ClientType::CLIENT_TEAMSPEAK;
    this->properties()[property::CLIENT_TYPE_EXACT] = ClientType::CLIENT_WEB;
    this->properties()[property::CLIENT_UNIQUE_IDENTIFIER] = "UnknownWebClient";
    this->properties()[property::CLIENT_NICKNAME] = string() + "UnknownWebClient #";

    /*
    Command init("handshakeidentify");
    init["successfully"] = true;
    this->sendCommand(init);
    */
}

void WebClient::onWSDisconnected(const string& error) {
    string message;
    uint16_t close_code = 0;
    if(error.length() < 2)
        close_code = static_cast<uint16_t>(-1);
    else {
        close_code = be2le16(&error[0]);
        message = error.substr(2);
    }

    debugMessage(this->getServerId(), "{} WS disconnected ({}). Application data: {}", CLIENT_STR_LOG_PREFIX, close_code, message);
    this->close_connection(); //TODO?
}

void WebClient::onWSMessage(const pipes::WSMessage &message) {
    if(message.code == pipes::OpCode::TEXT)
        this->handleMessage(message.data);
    else if(message.code == pipes::OpCode::PING) {
        logTrace(this->getServerId(), "{} Received ping on web socket. Application data length: {}. Sending pong", CLIENT_STR_LOG_PREFIX, message.data.length());
        this->ws_handler.send({pipes::PONG, message.data});
    } else if(message.code == pipes::OpCode::PONG) {
        if(message.data.length() < 1) {
            logError(this->getServerId(), "{} Received pong on web socket with too short payload: {}. Ignoring pong", CLIENT_STR_LOG_PREFIX, message.data.length());
            return;
        }

        uint8_t response_id = be2le8(&message.data[0]);
        if(response_id != this->ping.current_id) {
            debugMessage(
                    this->getServerId(),
                    "{} Received pong on web socket which is older than the last request. Delay may over {}ms? (Index: {}, Current index: {})",
                    CLIENT_STR_LOG_PREFIX,
                    duration_cast<milliseconds>(this->ping.timeout).count(),
                    response_id,
                    this->ping.current_id
            );
            return;
        }
        this->ping.last_response = system_clock::now();
        this->ping.value = duration_cast<nanoseconds>(this->ping.last_response - this->ping.last_request);
        /*
        debugMessage(
                this->getServerId(),
                "{} Received pong on web socket. Ping: {}, PingID: {}",
                CLIENT_STR_LOG_PREFIX,
                (float) duration_cast<microseconds>(this->ping.value).count() / 1000.f,
                response_id
        );
         */
    }
}

/* called while helding close_lock*/
void WebClient::disconnectFinal() {
    auto self_lock = this->ref();
    {
        /* waiting to finish all executes */
        lock_guard lock(this->execute_mutex);
    }

    if(this->flush_thread.get_id() == this_thread::get_id()) {
        this->flush_thread.detach();
    } else {
        assert(!this->flush_thread.joinable()); /* shall be already joined via closeConnection(...)*/
    }

    {
        ::event *event_read, *event_write;
        {
            unique_lock event_lock(this->event_mutex);

            event_read = this->readEvent;
            event_write = this->writeEvent;

            this->readEvent = nullptr;
            this->writeEvent = nullptr;
        }

        if(event_read) {
            event_del_block(event_read);
            event_free(event_read);
        }

        if(event_write) {
            event_del_block(event_write);
            event_free(event_write);
        }
    }

    if(this->file_descriptor > 0){
        shutdown(this->file_descriptor, SHUT_RDWR);
        close(this->file_descriptor);
        this->file_descriptor = -1;
    }

    this->state = ConnectionState::DISCONNECTED;
    this->processLeave();

    /* We do not finalize here since we might still try to send some data */
    /* this->ssl_handler.finalize(); */
    this->handle->unregisterConnection(static_pointer_cast<WebClient>(self_lock));
}

WebClientCommandHandler::WebClientCommandHandler(const std::shared_ptr<WebClient> &client) : client_ref{client} {}

bool WebClientCommandHandler::handle_command(const std::string_view &command) {
    auto client = this->client_ref.lock();
    if(!client) {
        return false;
    }

    return client->process_next_message(command);
}

Json::CharReaderBuilder json_reader_builder = []() noexcept {
    Json::CharReaderBuilder reader_builder;

    return reader_builder;
}();

void WebClient::handleMessage(const pipes::buffer_view &message) {
    /* Not really a need, this will directly be called via the ssl ws pipe, which has been triggered via progress message */
    threads::MutexLock lock(this->execute_mutex);
    Json::Value val;
    try {
        unique_ptr<Json::CharReader> reader{json_reader_builder.newCharReader()};

        string error_message;
        if(!reader->parse(message.data_ptr<char>(),message.data_ptr<char>() + message.length(), &val, &error_message)) {
            throw Json::Exception("Could not parse payload! (" + error_message + ")");
        }
    } catch (const std::exception& ex) {
        logError(this->server->getServerId(), "Could not parse web message! Message: {}", std::string{ex.what()});
        logTrace(this->server->getServerId(), "Payload: {}", message.string());
        return;
    }
    logTrace(this->server->getServerId(), "[{}] Read message {}", CLIENT_STR_LOG_PREFIX_(this), std::string_view{message.data_ptr<char>(), message.length()});

    try {
        if(val[Json::StaticString("type")].isNull()) {
            logError(this->server->getServerId(), "[{}] Invalid web json package!");
            return;
        }
        if(val[Json::StaticString("type")].asString() == "command") {
            Command cmd(val[Json::StaticString("command")].asString());
            for(int index = 0; index < val[Json::StaticString("data")].size(); index++) {
                for(auto it = val[Json::StaticString("data")][index].begin(); it != val[Json::StaticString("data")][index].end(); it++)
                    cmd[index][it.key().asString()] = (*it).asString();
            }
            for (const auto &index : val[Json::StaticString("flags")]) {
                cmd.enableParm(index.asString());
            }

            this->handleCommandFull(cmd, true);
        } else if(val[Json::StaticString("type")].asString() == "ping") {
            Json::Value response;
            response[Json::StaticString("type")] = "pong";
            response[Json::StaticString("payload")] = val[Json::StaticString("payload")];
            response[Json::StaticString("ping_native")] = to_string(duration_cast<microseconds>(this->ping.value).count());
            this->sendJson(response);
            return;
        } else if(val[Json::StaticString("type")].asString() == "pong") {
            auto payload = val[Json::StaticString("payload")].isString() ? val[Json::StaticString("payload")].asString() : "";
            uint8_t response_id = 0;
            try {
                response_id = (uint8_t) stoul(payload);
            } catch(std::exception& ex) {
                debugMessage(this->getServerId(), "[{}] Failed to parse pong payload.");
                return;
            }

            if(response_id != this->js_ping.current_id) {
                debugMessage(
                        this->getServerId(),
                        "{} Received pong on web socket from javascript which is older than the last request. Delay may over {}ms? (Index: {}, Current index: {})",
                        CLIENT_STR_LOG_PREFIX,
                        duration_cast<milliseconds>(this->js_ping.timeout).count(),
                        response_id,
                        this->js_ping.current_id
                );
                return;
            }
            this->js_ping.last_response = system_clock::now();
            this->js_ping.value = duration_cast<nanoseconds>(this->js_ping.last_response - this->js_ping.last_request);
        } else if(val[Json::StaticString("type")].asString() == "enable-raw-commands") {
            this->allow_raw_commands = true;
        }
    } catch (const std::exception& ex) {
        logError(this->server->getServerId(), "Could not handle json packet! Message {}", ex.what());
        logTrace(this->server->getServerId(), "Message: {}", message);
    }
}
//candidate:3260795824 1 udp 2122194687 192.168.43.141 37343 typ host generation 0 ufrag JCsw network-id 2
//candidate:0 1 UDP 2122252543 192.168.43.141 34590 typ host

bool WebClient::disconnect(const std::string &reason) {
    auto old_channel = this->currentChannel;
    if(old_channel) {
        serverInstance->action_logger()->client_channel_logger.log_client_leave(this->getServerId(), this->ref(), old_channel->channelId(), old_channel->name());
    }
    {
        unique_lock server_channel_lock(this->server->channel_tree_mutex);
        {
            unique_lock own_lock(this->channel_tree_mutex);
            this->notifyClientLeftViewKicked(this->ref(), nullptr, reason, this->server->serverRoot, false);
        }
        this->server->client_move(this->ref(), nullptr, this->server->serverRoot, reason, ViewReasonId::VREASON_SERVER_KICK, false, server_channel_lock);
        this->server->unregisterClient(this->ref(), "disconnected", server_channel_lock);
    }
    return this->close_connection(system_clock::now() + seconds(1));
}

command_result WebClient::handleCommandClientInit(Command &command) {
    if(!config::server::clients::teaweb) {
        return command_result{error::client_type_is_not_allowed, config::server::clients::teaweb_not_allowed_message};
    }

    return SpeakingClient::handleCommandClientInit(command);
}

void WebClient::send_voice_packet(const pipes::buffer_view &view, const SpeakingClient::VoicePacketFlags &flags) {
    /* Should never be called! */
}

command_result WebClient::handleCommandWhisperSessionInitialize(Command &command) {
    auto server = this->getServer();
    if(!server) {
        return command_result{error::server_unbound};
    }

    auto stream_id = command["ssrc"].as<uint32_t>();
    if(command.hasParm("new")) {
        auto type = command["type"].as<uint8_t>();
        auto target = command["target"].as<uint8_t>();
        auto target_id = command["id"].as<uint64_t>();

        return this->whisper_handler_.initialize_session_new(stream_id, type, target, target_id);
    } else {
        if(command.bulkCount() > 255) {
            return command_result{error::parameter_invalid_count};
        }

        std::vector<ClientId> client_ids{};
        std::vector<ChannelId> channel_ids{};

        client_ids.reserve(command.bulkCount());
        channel_ids.reserve(command.bulkCount());

        std::optional<decltype(server->getClients())> server_clients{};

        for(size_t bulk{0}; bulk < command.bulkCount(); bulk++) {
            if(command[bulk].has("cid")) {
                channel_ids.push_back(command[bulk]["cid"]);
            }

            if(command[bulk].has("clid")) {
                channel_ids.push_back(command[bulk]["clid"]);
            }

            if(command[bulk].has("cluid")) {
                auto client_unique_id = command[bulk]["cluid"].string();
                if(!server_clients.has_value()) {
                    server_clients = server->getClients();
                }

                for(const auto& client : *server_clients) {
                    if(client->getUid() == client_unique_id) {
                        client_ids.push_back(client->getClientId());
                    }
                }
            }
        }

        return this->whisper_handler_.initialize_session_old(stream_id, &client_ids[0], client_ids.size(), &channel_ids[0], channel_ids.size());
    }
}

command_result WebClient::handleCommandWhisperSessionReset(Command &command) {
    this->whisper_handler_.signal_session_reset();
    return command_result{error::ok};
}