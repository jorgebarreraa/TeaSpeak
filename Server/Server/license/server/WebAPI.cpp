//
// Created by wolverindev on 04.09.18.
//

#include <csignal>
#include <misc/std_unique_ptr.h>
#include <misc/net.h>
#include <misc/endianness.h>
#include <json/json.h>
#include <log/LogUtils.h>
#include "StatisticManager.h"
#include "WebAPI.h"

using namespace license;
using namespace license::server;
using namespace license::web;
using namespace ts::ssl;
using namespace std;
using namespace std::chrono;

WebStatistics::WebStatistics(const shared_ptr<database::DatabaseHandler> &manager, const std::shared_ptr<stats::StatisticManager>& stats) : license_manager(manager), statistics_manager(stats) {}
WebStatistics::~WebStatistics() {}

#define SFAIL(message)                                                                          \
do {                                                                                            \
    error = message;          \
    this->stop();                                                                         \
    return false;                                                                               \
} while(0)

static int enabled = 1;
static int disabled = 0;
bool WebStatistics::start(std::string &error, uint16_t port, const std::shared_ptr<ts::ssl::SSLContext> &ssl) {
	{
		std::lock_guard<std::recursive_mutex> lock(this->running_lock);
		if(this->_running) return false;
		this->_running = true;
	}
	this->ssl = ssl;

	{
		memset(&this->socket.address, 0, sizeof(sockaddr_in));
        this->socket.address.sin_family = AF_INET;
        this->socket.address.sin_addr.s_addr = INADDR_ANY;
        this->socket.address.sin_port = htons(port);
	}

	this->socket.file_descriptor = ::socket(AF_INET, SOCK_STREAM, 0);
	if (this->socket.file_descriptor < 0) SFAIL("Could not create new socket");

	if(setsockopt(this->socket.file_descriptor, SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled)) < 0) SFAIL("could not set reuse address");
	if(setsockopt(this->socket.file_descriptor, IPPROTO_TCP, TCP_CORK, &disabled, sizeof(disabled)) < 0) SFAIL("could not set no push");
	if(bind(this->socket.file_descriptor, (struct sockaddr *) &this->socket.address, sizeof(sockaddr_in)) < 0) SFAIL("Could not bind socket on " + string(inet_ntoa(this->socket.address.sin_addr)));

	if(listen(this->socket.file_descriptor, 32) < 0) SFAIL("Could not listen on socket");

	this->socket.event_base = event_base_new();
	this->socket.event_accept = event_new(this->socket.event_base, this->socket.file_descriptor, EV_READ | EV_PERSIST, WebStatistics::handleEventAccept, this);
	event_add(this->socket.event_accept, nullptr);

	this->socket.event_base_dispatch = make_unique<threads::Thread>(THREAD_SAVE_OPERATIONS, [&](){
		signal(SIGABRT, SIG_IGN);
		event_base_dispatch(this->socket.event_base);
	});

	return true;
}

void WebStatistics::stop() {
	{
		std::lock_guard<std::recursive_mutex> lock(this->running_lock);
		if(!this->_running) return;
		this->_running = false;
	}
	for(const auto& client : this->get_clients())
		this->close_connection(client);

	if(this->socket.event_accept) {
		event_del(this->socket.event_accept);
		event_free(this->socket.event_accept);
	}
	this->socket.event_accept = nullptr;

	if(this->socket.event_base)
		event_base_loopbreak(this->socket.event_base);

	if(this->socket.event_base_dispatch)
		this->socket.event_base_dispatch->join(seconds(5));
	this->socket.event_base_dispatch = nullptr;

	if(this->socket.event_base) {
		event_base_free(this->socket.event_base);
		this->socket.event_base = nullptr;
	}

	if(this->socket.file_descriptor != 0) {
		shutdown(this->socket.file_descriptor, SHUT_RDWR);
		close(this->socket.file_descriptor);
		this->socket.file_descriptor = 0;
	}
}

void WebStatistics::initialize_client(const std::shared_ptr<license::web::WebStatistics::Client> &client) {
	weak_ptr<Client> weak_client = client;

	auto send_message = [&](const std::shared_ptr<Client>& client, const pipes::buffer_view& message) {
		{
			std::lock_guard<std::recursive_mutex> lock(client->execute_lock);
			client->buffer_write.push_back(message.string());
		}

		if(client->event_write)
			event_add(client->event_write, nullptr);
	};

	{ //WebSocket and SSL setup

		client->pipe_websocket = make_unique<pipes::WebSocket>();

		client->pipe_websocket->direct_process(pipes::PROCESS_DIRECTION_IN, true);
		client->pipe_websocket->direct_process(pipes::PROCESS_DIRECTION_OUT, true);

		client->pipe_websocket->callback_error([&, weak_client](int code, const std::string &reason) {
			auto _client = weak_client.lock();
			if(!_client) return;

			logError(LOG_LICENSE_WEB, "[{}][WS] Catched an error. code: {} reason: {}.", _client->client_prefix(), code, reason);
			logError(LOG_LICENSE_WEB, "[{}][WS] Disconnecting client", _client->client_prefix());
			if(_client->pipe_websocket && _client->pipe_websocket->getState() == pipes::CONNECTED) _client->pipe_websocket->disconnect(1100, "Catched a server sided error");
			else this->close_connection(_client);
		});
		client->pipe_websocket->callback_write([weak_client, send_message](const pipes::buffer_view& message) {
			auto _client = weak_client.lock();
			if(!_client) return;

			if(_client->pipe_ssl)
				_client->pipe_ssl->send(message);
			else send_message(_client, message);
		});
		client->pipe_websocket->callback_data([&, weak_client](const pipes::WSMessage& message) {
			auto _client = weak_client.lock();
			if(!_client) return;

			this->handle_message(_client, message); //TODO if return false error handling!
		});
		client->pipe_websocket->on_connect = [&, weak_client] {
			auto _client = weak_client.lock();
			if(!_client) return;

			logMessage(LOG_LICENSE_WEB, "[{}] WebSocket handshake completed!", _client->client_prefix());
		};
		client->pipe_websocket->on_disconnect = [&, weak_client](const std::string& reason) {
			auto _client = weak_client.lock();
			if(!_client) return;

			logMessage(LOG_LICENSE_WEB, "[{}] Remote connection disconnected ({} | {})", _client->client_prefix(), reason.length() >= 2 ? be2le16(reason.data()) : -1, reason.length() > 2 ? reason.substr(2) : "");
			this->close_connection(_client);
		};
		client->pipe_websocket->callback_invalid_request = [&, weak_client](const http::HttpRequest& request, http::HttpResponse& response) {
			auto _client = weak_client.lock();
			if(!_client) return;

			auto lmethod = request.method;
			transform(lmethod.begin(), lmethod.end(), lmethod.begin(), ::tolower);
			if(lmethod == "get" && request.parameters.count("type") && !request.parameters.at("type").empty())
				this->handle_request(_client, request, response);
		};

		client->pipe_websocket->initialize();
	}

	{
		client->pipe_ssl = make_unique<pipes::SSL>();

		client->pipe_ssl->direct_process(pipes::PROCESS_DIRECTION_IN, true);
		client->pipe_ssl->direct_process(pipes::PROCESS_DIRECTION_OUT, true);

		client->pipe_ssl->callback_error([&, weak_client](int code, const std::string &reason) {
			auto _client = weak_client.lock();
			if(!_client) return;

			logError(LOG_LICENSE_WEB, "[{}][SSL] Catched an error. code: {} reason: {}.", _client->client_prefix(), code, reason);
			logError(LOG_LICENSE_WEB, "[{}][SSL] Disconnecting client", _client->client_prefix());
			if(_client->pipe_websocket && _client->pipe_websocket->getState() == pipes::CONNECTED) _client->pipe_websocket->disconnect(1100, "Catched a server sided error (SSL)");
			else this->close_connection(_client);
		});
		client->pipe_ssl->callback_write([weak_client, send_message](const pipes::buffer_view& message) {
			auto _client = weak_client.lock();
			if(!_client) return;

			send_message(_client, message);
		});
		client->pipe_ssl->callback_data([&, weak_client](const pipes::buffer_view& message) {
			auto _client = weak_client.lock();
			if(!_client) return;

			if(_client->pipe_websocket) _client->pipe_websocket->process_incoming_data(message);
		});

		{
		    std::string error{};
			auto options = make_shared<pipes::SSL::Options>();
			options->type = pipes::SSL::SERVER;
			options->context_method = TLS_method();
			options->free_unused_keypairs = false; /* we dont want our keys get removed */

			options->default_keypair({this->ssl->privateKey, this->ssl->certificate});
			if(!client->pipe_ssl->initialize(options, error)) {
				logError(LOG_LICENSE_WEB, "[{}][SSL] Failed to setup ssl ({})! Disconnecting client", client->client_prefix(), error);
				this->close_connection(client);
			}
		}
	}
}

void WebStatistics::handleEventAccept(int fd, short, void *ptr_server) {
	auto server = (WebStatistics*) ptr_server;

	auto client = make_shared<Client>();

	{ //Network accept
		auto address = make_unique<sockaddr_in>();
		auto address_length = (socklen_t) sizeof(*address);

		client->file_descriptor = accept(fd, (struct sockaddr *) address.get(), &address_length);
		if (client->file_descriptor < 0) {
			logCritical(LOG_LICENSE_WEB, "Failed to accept new client. ({} | {}/{})", client->file_descriptor, errno, strerror(errno));
			return;
		}
		if(setsockopt(client->file_descriptor, SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled)) < 0);// CERR("could not set reuse addr");
		if(setsockopt(client->file_descriptor, IPPROTO_TCP, TCP_CORK, &disabled, sizeof(disabled)) < 0);// CERR("could not set no push");
		client->peer_address = std::move(address);
	}
	server->initialize_client(client);

	{ //Client registration
		std::lock_guard<std::recursive_mutex> lock(server->clients_lock);
		server->clients.push_back(client);
	}

	{ //IO Init
		client->event_read = event_new(server->socket.event_base, client->file_descriptor, EV_READ | EV_PERSIST, WebStatistics::handleEventRead, server);
		client->event_write = event_new(server->socket.event_base, client->file_descriptor, EV_WRITE, WebStatistics::handleEventWrite, server);
		event_add(client->event_read, nullptr);
	}

	logMessage(LOG_LICENSE_WEB, "Accepted new client from {}", net::to_string(client->peer_address->sin_addr));
}

void WebStatistics::handleEventRead(int file_descriptor, short, void* ptr_server) {
	auto server = (WebStatistics*) ptr_server;
	auto client = server->find_client_by_fd(file_descriptor);
	if(!client || client->file_descriptor == 0) {
		//TODO error
		return;
	}

	pipes::buffer buffer(1024);

	sockaddr_in remote_address{};
	socklen_t remote_address_size = sizeof(remote_address);
	auto read = recvfrom(file_descriptor, buffer.data_ptr(), buffer.length(), 0, reinterpret_cast<sockaddr *>(&remote_address), &remote_address_size);

	if(read < 0){
		if(errno == EWOULDBLOCK) return;
		logError(LOG_LICENSE_WEB, "[{}] Invalid read: {}/{}. Closing connection.", client->client_prefix(), errno, strerror(errno));
		if(client->event_read)
			event_del_noblock(client->event_read);
		server->close_connection(client);
		return;
	} else if(read == 0) {
		debugMessage(LOG_LICENSE_WEB, "[{}] Invalid read (eof). Closing connection", client->client_prefix());
		if(client->event_read)
			event_del_noblock(client->event_read);
		server->close_connection(client);
		return;
	}

	buffer.resize(read);

	lock_guard<recursive_mutex> lock(client->execute_lock);
	if(client->file_descriptor == 0) return;

	if(client->pipe_ssl) {
		client->pipe_ssl->process_incoming_data(buffer);
	} else if(client->pipe_websocket) {
		client->pipe_websocket->process_incoming_data(buffer);
	}
	else; //TODO error handling
}

void WebStatistics::handleEventWrite(int file_descriptor, short, void* ptr_server) {
	auto server = (WebStatistics*) ptr_server;
	auto client = server->find_client_by_fd(file_descriptor);
	if(!client) {
		//TODO error
		return;
	}


	std::unique_lock elock(client->execute_lock);
	if(client->buffer_write.empty()) return;
	auto& buffer = client->buffer_write.front();

	auto written = send(file_descriptor, buffer.data(), buffer.length(), MSG_DONTWAIT | MSG_NOSIGNAL);
	if(written < 0){
		elock.unlock();

		if(errno == EWOULDBLOCK) return;
		logError(LOG_LICENSE_WEB, "[{}] Invalid write: {}/{}. Closing connection.", client->client_prefix(), errno, strerror(errno));
		server->close_connection(client);
		return;
	} else if(written == 0) {
		elock.unlock();

		logError(LOG_LICENSE_WEB, "[{}] Invalid write (eof). Closing connection", client->client_prefix());
		server->close_connection(client);
		return;
	}

	if(written >= buffer.length())
		client->buffer_write.pop_front();
	else buffer = buffer.substr(written);

	if(!client->buffer_write.empty()) event_add(client->event_write, nullptr);
}

void WebStatistics::close_connection(const std::shared_ptr<license::web::WebStatistics::Client> &client) {
	if(this->socket.event_base_dispatch && *this->socket.event_base_dispatch == pthread_self()) {
		std::thread(bind(&WebStatistics::close_connection, this, client)).detach();
		return;
	}

	{
		std::lock_guard<std::recursive_mutex> lock(this->clients_lock);
		auto entry = find(this->clients.begin(), this->clients.end(), client);
		if(entry != this->clients.end())
			this->clients.erase(entry);
		else; //TODO Error handling?
	}

	std::unique_lock elock(client->execute_lock);
	auto event_read = std::exchange(client->event_read, nullptr);
	auto event_write = std::exchange(client->event_write, nullptr);
	elock.unlock();
	if(event_read) {
		event_del(event_read);
		event_free(event_read);
	}
	if(event_write) {
		event_del(event_write);
		event_free(event_write);
	}

	elock.lock();
	if(client->file_descriptor > 0) {
		if(shutdown(client->file_descriptor, SHUT_RDWR) < 0); //TODO error handling
		if(close(client->file_descriptor) < 0); //TODO error handling
		client->file_descriptor = 0;
	}

	if(client->pipe_websocket)
		client->pipe_websocket = nullptr;

	if(client->pipe_ssl) {
		client->pipe_ssl->finalize();
		client->pipe_ssl = nullptr;
	}
	logMessage(LOG_LICENSE_WEB, "[{}] Connection closed", client->client_prefix());
}

std::shared_ptr<WebStatistics::Client> WebStatistics::find_client_by_fd(int file_descriptor) {
	std::lock_guard<std::recursive_mutex> lock(this->clients_lock);
	for(const auto& client : this->clients)
		if(client->file_descriptor == file_descriptor) return client;
	return nullptr;
}

bool WebStatistics::send_message(const std::shared_ptr<Client> &client, const pipes::buffer_view &buffer) {
    lock_guard<recursive_mutex> lock(client->execute_lock);
    if(client->pipe_websocket) {
        client->pipe_websocket->send({pipes::TEXT, buffer.own_buffer()});
        return true;
    }
    return false;
}

#define HERR(message, ...) \
do {\
	logError(LOG_LICENSE_WEB, "[{}] " message, client->client_prefix(), ##__VA_ARGS__); \
	return false; \
} while(0)

inline pipes::buffer json_dump(const Json::Value& value) {
	Json::StreamWriterBuilder builder;
	builder["indentation"] = ""; // If you want whitespace-less output
	auto json = Json::writeString(builder, value);
	return pipes::buffer((void*) json.c_str(), json.length());
}

bool WebStatistics::handle_message(const std::shared_ptr<license::web::WebStatistics::Client> &client, const pipes::WSMessage &raw_message) {
	if(this->update_flood(client, 10)) {
		static pipes::buffer _response;
		if(_response.empty()) {
			Json::Value response;
			response["type"] = "error";
			response["code"] = "general";
			response["msg"] = "action not available due flood prevention";
			_response = json_dump(response);
		}

		this->send_message(client, _response);
		return true;
	}

	logTrace(LOG_LICENSE_WEB, "[{}] Received message {}", client->client_prefix(), raw_message.data.string());

	Json::Value message;
	try {
		istringstream ss(raw_message.data.string());
		ss >> message;
	} catch (std::exception& ex) {
		logError(LOG_LICENSE_WEB, "[{}] Received an invalid message: {}", client->client_prefix(), raw_message.data.string());
		return false;
	}
	try {
		if(!message["type"].isString()) HERR("Missing/invalid type");

		if(message["type"].asString() == "request") {
			if(!message["request_type"].isString()) HERR("Missing/invalid request type");

			if(message["request_type"].asString() == "general") {
				this->update_flood(client, 50);

				Json::Value response;
				response["type"] = "response";
				response["code"] = message["code"];

				auto stats = this->statistics_manager->general_statistics();
				response["statistics"]["instances"] = to_string(stats->instances);
				response["statistics"]["servers"] = to_string(stats->servers);
				response["statistics"]["clients"] = to_string(stats->clients);
				response["statistics"]["music"] = to_string(stats->bots);

                this->send_message(client, json_dump(response));
				return true;
			} else if(message["request_type"].asString() == "history" ) {
				auto type = message["history_type"].asInt();
				if(type < 0 || type > stats::HistoryStatistics::LAST_HALF_YEAR)
					__throw_range_error("invalid range!");

				if(type == stats::HistoryStatistics::LAST_DAY)
					this->update_flood(client, 50);
				if(type == stats::HistoryStatistics::DAY_YESTERDAY)
					this->update_flood(client, 50);
				if(type == stats::HistoryStatistics::LAST_HALF_YEAR)
					this->update_flood(client, 100);
				if(type == stats::HistoryStatistics::DAY_7DAYS_AGO)
					this->update_flood(client, 60);
				if(type == stats::HistoryStatistics::LAST_WEEK)
					this->update_flood(client, 70);
				if(type == stats::HistoryStatistics::LAST_MONTH)
					this->update_flood(client, 80);


				//TODO: Some kind of handle else this could crash the server on shutdown
				std::thread([&, client, type, message]() {
					auto history = this->statistics_manager->history((stats::HistoryStatistics::HistoryType) type);

					Json::Value response;
					response["type"] = "response";
					response["code"] = message["code"];

					response["history"]["timestamp"] = duration_cast<milliseconds>(history->evaluated.time_since_epoch()).count();
					response["history"]["begin"] = duration_cast<milliseconds>(history->begin.time_since_epoch()).count();
					response["history"]["end"] = duration_cast<milliseconds>(history->end.time_since_epoch()).count();
					response["history"]["interval"] = duration_cast<milliseconds>(history->period).count();


					int index;
					auto stats = history->statistics;
                    auto& history_data = response["history"]["data"];
                    for(index = 0; index < stats->record_count; index++) {
                        auto& indexed_data = history_data[index];
                        indexed_data["instances_empty"] = stats->history[index].instance_empty;
                        indexed_data["instances"] = stats->history[index].instance_online;
                        indexed_data["servers"] = stats->history[index].servers_online;
                        indexed_data["clients"] = stats->history[index].clients_online;
                        indexed_data["music"] = stats->history[index].bots_online;
                    }

                    this->send_message(client, json_dump(response));
				}).detach();
				return true;
			} else if(message["request_type"].asString() == "history_custom") {
			    auto begin = std::chrono::milliseconds{message["history_begin"].asInt64()};
                auto end = std::chrono::milliseconds{message["history_end"].asInt64()};
                auto interval = std::chrono::milliseconds{message["history_interval"].asInt64()};
                auto code = message["code"].asString();

                auto token = message["token"].asString();
                if(token != "blubalutsch") {
                    Json::Value response;
                    response["type"] = "error";
                    response["code"] = code;
                    response["message"] = "invalid token";
                    this->send_message(client, json_dump(response));
                    return true;
                }

                //TODO: Some kind of handle else this could crash the server on shutdown
                std::thread([&, client, begin, end, interval, code]{
                    auto data = this->license_manager->list_statistics_user(
                            std::chrono::system_clock::time_point{} + begin,
                            std::chrono::system_clock::time_point{} + end,
                            interval
                    );

                    Json::Value response;
                    response["type"] = "response";
                    response["code"] = code;

                    response["history"]["timestamp"] = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
                    response["history"]["begin"] = duration_cast<milliseconds>(begin).count();
                    response["history"]["end"] = duration_cast<milliseconds>(end).count();
                    response["history"]["interval"] = duration_cast<milliseconds>(interval).count();


                    int index;
                    auto& history_data = response["history"]["data"];
                    for(index = 0; index < data->record_count; index++) {
                        auto& indexed_data = history_data[index];
                        indexed_data["instances_empty"] = data->history[index].instance_empty;
                        indexed_data["instances"] = data->history[index].instance_online;
                        indexed_data["servers"] = data->history[index].servers_online;
                        indexed_data["clients"] = data->history[index].clients_online;
                        indexed_data["music"] = data->history[index].bots_online;
                    }

                    this->send_message(client, json_dump(response));
                }).detach();
                return true;
			}
		}
	} catch (const std::exception& ex) {
		logError(LOG_LICENSE_WEB, "[{}] Message handling throws exception: {}", client->client_prefix(), ex.what());

		Json::Value response;
		response["type"] = "error";
		response["code"] = message["code"];
		response["message"] = "could not execute action";
        this->send_message(client, json_dump(response));
		return false;
	}

	{
		Json::Value response;
		response["type"] = "error";
		response["code"] = message["code"];
		response["message"] = "could not assign action";
        this->send_message(client, json_dump(response));
	}
	return true;
}

bool WebStatistics::handle_request(const std::shared_ptr<license::web::WebStatistics::Client> &client, const http::HttpRequest &request, http::HttpResponse &response) {
	auto type = request.parameters.at("type");
	logMessage(LOG_LICENSE_WEB, "[{}] Received HTTP status request of type {}", client->client_prefix(), type);

	if(type == "request" && request.parameters.count("request_type") > 0) {
	    const auto& type = request.parameters.at("request_type");
        if(type == "general") {
            Json::Value json;
            json["type"] = "response";
            auto stats = this->statistics_manager->general_statistics();
            json["statistics"]["instances_empty"] = to_string(stats->empty_instances);
            json["statistics"]["instances"] = to_string(stats->instances);
            json["statistics"]["servers"] = to_string(stats->servers);
            json["statistics"]["clients"] = to_string(stats->clients);
            json["statistics"]["music"] = to_string(stats->bots);
            response.setHeader("data", {json_dump(json).string()});
            response.code = http::code::_200;
        }
	}

	return false;
}

bool WebStatistics::update_flood(const std::shared_ptr<license::web::WebStatistics::Client> &client, int flood_points) {
	if(client->flood_reset.time_since_epoch().count() == 0)
		client->flood_reset = system_clock::now();

	client->flood_points += flood_points;

	auto diff = duration_cast<milliseconds>(system_clock::now() - client->flood_reset);
	if(diff.count() > 1000) {
		diff -= milliseconds(1000);
		auto reduce = diff.count() / 10; //Reduce 100fp per second
		if(client->flood_points > reduce)
			client->flood_points = 0;
		else
			client->flood_points -= reduce;

		client->flood_reset = system_clock::now();
	}

	return client->flood_points > 150;
}

void WebStatistics::broadcast_message(const Json::Value &value) {
	auto raw_value = json_dump(value);
	for(const auto& client : this->get_clients()) {
		std::lock_guard<std::recursive_mutex> lock(client->execute_lock);
		if(client->pipe_websocket && client->pipe_websocket->getState() == pipes::WebSocketState::CONNECTED)
			client->pipe_websocket->send({pipes::TEXT, raw_value});
	}
}

void WebStatistics::broadcast_notify_general_update() {
	Json::Value message;
	message["type"] = "notify";
	message["target"] = "general_update";
	this->broadcast_message(message);
}

void WebStatistics::async_broadcast_notify_general_update() {
	this->scheduler.execute([&]{
		this->broadcast_notify_general_update();
	});
}