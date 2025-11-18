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
#include "DatabaseHandler.h"

namespace license {
	namespace web {
		class WebStatistics;
	}
	namespace stats {
		class StatisticManager;
	}
	class UserManager;

	enum ClientType {
		SERVER,
		MANAGER
	};
    struct ConnectedClient {
        public:
		    struct {
			    sockaddr_in remoteAddr;
			    int fileDescriptor = 0;

			    std::mutex event_mutex{};
			    event* readEvent = nullptr; /* protected via state_lock (check state and the use these variables) */
			    event* writeEvent = nullptr;

			    std::mutex write_queue_lock{};
			    TAILQ_HEAD(, ts::buffer::RawBuffer) write_queue;
		    } network;

		    struct {
		        std::mutex state_lock{};
			    protocol::RequestState state = protocol::UNCONNECTED;

			    std::chrono::system_clock::time_point last_read;
			    std::string cryptKey = "";

			    int version{2}; /* current version is 3 */
		    } protocol;

		    ClientType type = ClientType::SERVER;
		    std::string username;
		    std::string key;
		    uint64_t key_pending_upgrade{0};
		    std::string unique_identifier;

		    bool invalid_license = false;

            void init();
            void uninit(bool /* blocking */);
            void sendPacket(const protocol::packet&);

		    inline std::string address() { return inet_ntoa(network.remoteAddr.sin_addr); }
    };

    struct WebCertificate {
    	std::string revision;
    	std::string key;
    	std::string certificate;
    };

    class LicenseServer {
        public:
            explicit LicenseServer(const sockaddr_in&, std::shared_ptr<server::database::DatabaseHandler> , std::shared_ptr<stats::StatisticManager>  /* stats */, std::shared_ptr<web::WebStatistics>  /* web stats */, std::shared_ptr<UserManager>  /* user manager */);
            ~LicenseServer();

            bool start();
            bool isRunning(){ return this->running; }
            void stop();

            std::shared_ptr<ConnectedClient> findClient(int fileDescriptor);
		    void disconnectClient(const std::shared_ptr<ConnectedClient>&, const std::string& reason);
            void closeConnection(const std::shared_ptr<ConnectedClient>&, bool blocking = false);

			std::deque<std::shared_ptr<ConnectedClient>> getClients() {
				std::lock_guard lock(this->client_lock);
				return clients;
			}

			std::shared_ptr<WebCertificate> web_certificate{nullptr};
        private:
            void unregisterClient(const std::shared_ptr<ConnectedClient>&);
		    void cleanup_clients();

		    std::shared_ptr<web::WebStatistics> web_statistics;
		    std::shared_ptr<stats::StatisticManager> statistics;
		    std::shared_ptr<server::database::DatabaseHandler> manager;
		    std::shared_ptr<UserManager> user_manager;

            std::mutex client_lock;
            std::deque<std::shared_ptr<ConnectedClient>> clients;

            bool running = false; /* also secured by client_lock */

            sockaddr_in localAddr{};
            int server_socket = 0;
            event_base* evBase = nullptr;
            event* event_accept = nullptr;
            event* event_cleanup = nullptr;

            std::thread event_base_dispatch{};

		    static void handleEventCleanup(int, short, void*);
            static void handleEventAccept(int, short, void*);
            static void handleEventRead(int, short, void*);
            static void handleEventWrite(int, short, void*);

            void handleMessage(std::shared_ptr<ConnectedClient>&, const std::string&);

		    bool handleDisconnect(std::shared_ptr<ConnectedClient>&, protocol::packet&, std::string& error);
		    bool handleHandshake(std::shared_ptr<ConnectedClient>&, protocol::packet&, std::string& error);
		    bool handleServerValidation(std::shared_ptr<ConnectedClient> &, protocol::packet &, std::string &error);
            bool handlePacketLicenseUpgrade(std::shared_ptr<ConnectedClient> &client, protocol::packet &packet, std::string &error);
		    bool handlePacketPropertyUpdate(std::shared_ptr<ConnectedClient> &, protocol::packet &, std::string &error);

		    bool handlePacketAuth(std::shared_ptr<ConnectedClient> &, protocol::packet &, std::string &error);
		    bool handlePacketLicenseCreate(std::shared_ptr<ConnectedClient> &, protocol::packet &, std::string &error);
		    bool handlePacketLicenseList(std::shared_ptr<ConnectedClient> &, protocol::packet &, std::string &error);
            bool handlePacketLicenseDelete(std::shared_ptr<ConnectedClient> &, protocol::packet &, std::string &error);
    };
}