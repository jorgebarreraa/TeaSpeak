#pragma once

#include <event.h>
#include <protocol/buffers.h>
#include <deque>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <ThreadPool/Thread.h>
#include <shared/include/license/license.h>
#include <arpa/inet.h>
#include <misc/net.h>
#include <json/json.h>

#include <pipes/ws.h>
#include <pipes/ssl.h>
#include <ssl/SSLManager.h>

namespace license {
	namespace server {
		class DatabaseHandler;
	}
	namespace stats {
		class StatisticManager;
	}

	namespace web {
		class WebStatistics {
				struct Client {
					std::unique_ptr<sockaddr_in> peer_address;
					int file_descriptor = 0;

					event* event_read = nullptr;
					event* event_write = nullptr;

					std::recursive_mutex execute_lock;
					std::deque<std::string> buffer_write;

					std::unique_ptr<pipes::WebSocket> pipe_websocket;
					std::unique_ptr<pipes::SSL> pipe_ssl;

					std::chrono::system_clock::time_point flood_reset;
					int flood_points;

					inline std::string client_prefix() { return peer_address ? net::to_string(peer_address->sin_addr) : "unconnected"; }
				};
			public:
				WebStatistics(const std::shared_ptr<server::database::DatabaseHandler>& /* license manager */, const std::shared_ptr<stats::StatisticManager>& /* stats manager */);
				virtual ~WebStatistics();

				bool start(std::string& /* error */, uint16_t /* port */, const std::shared_ptr<ts::ssl::SSLContext>& /* ssl */);
				inline bool running() {
					std::lock_guard<std::recursive_mutex> lock(this->running_lock);
					return this->_running;
				}
				void stop();

				inline std::deque<std::shared_ptr<Client>> get_clients() {
					std::lock_guard<std::recursive_mutex> lock(this->clients_lock);
					return this->clients;
				}

				void close_connection(const std::shared_ptr<Client>& /* client */);
				std::shared_ptr<Client> find_client_by_fd(int /* file descriptor */);
				bool send_message(const std::shared_ptr<Client>& /* client */, const pipes::buffer_view& /* data */);

				void broadcast_message(const Json::Value& /* message */);

				void async_broadcast_notify_general_update();
				void broadcast_notify_general_update();

			private:
				bool _running = false;
				std::recursive_mutex running_lock;

				std::shared_ptr<server::database::DatabaseHandler> license_manager;
				std::shared_ptr<stats::StatisticManager> statistics_manager;

				struct {
                    sockaddr_in address{};
				    int file_descriptor{0};
					event* event_accept{nullptr};
					struct event_base* event_base{nullptr};
					std::unique_ptr<threads::Thread> event_base_dispatch;
				} socket;

				std::shared_ptr<ts::ssl::SSLContext> ssl;

				std::recursive_mutex clients_lock;
				std::deque<std::shared_ptr<Client>> clients;

				threads::ThreadPool scheduler{1, "WebStatistics #"};
			protected:
				static void handleEventAccept(int, short, void*);
				static void handleEventRead(int, short, void*);
				static void handleEventWrite(int, short, void*);

				void initialize_client(const std::shared_ptr<Client>& /* client */);
				virtual bool handle_message(const std::shared_ptr<Client>& /* client */, const pipes::WSMessage& message);
				virtual bool handle_request(const std::shared_ptr<license::web::WebStatistics::Client> &client, const http::HttpRequest& /* request */, http::HttpResponse& /* response */);

				bool update_flood(const std::shared_ptr<license::web::WebStatistics::Client> &client, int flood_points);
		};
	}
}