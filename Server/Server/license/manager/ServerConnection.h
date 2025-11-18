#pragma once

#include <netinet/in.h>
#include <event.h>
#include <ThreadPool/Future.h>
#include <deque>
#include <thread>
#include <ThreadPool/Thread.h>
#include "shared/include/license/license.h"

#define FLSUCCESS(listener, object)      \
do {                                    \
	auto l = listener.release();        \
	if(l)                               \
		l->executionSucceed(object);    \
	delete l;                           \
} while(0)

#define FLERROR(listener, object)       \
do {                                    \
	auto l = listener.release();        \
	if(l)                               \
		l->executionFailed(object);     \
	delete l;                           \
} while(0)


namespace license::manager {
    enum ConnectionState {
        UNCONNECTED,
        CONNECTING,
        CONNECTED,
        DISCONNECTING
    };
    class ServerConnection {
        public:
            ServerConnection();
            ~ServerConnection();

            threads::Future<bool> connect(const std::string& host, uint16_t port);
            void disconnect(const std::string&);

            void ping();

            threads::Future<bool> login(const std::string&, const std::string&);
            threads::Future<std::pair<std::shared_ptr<license::License>, std::shared_ptr<license::LicenseInfo>>> registerLicense(
                    const std::string& first_name,
                    const std::string& last_name,
                    const std::string& username,
                    const std::string& email,
                    license::LicenseType type,
                    const std::chrono::system_clock::time_point& end,
                    const std::chrono::system_clock::time_point& begin,
                    const std::string& old_license
            );

            threads::Future<std::map<std::string, std::shared_ptr<license::LicenseInfo>>> list(int offset, int count);
            threads::Future<bool> deleteLicense(const std::string& key, bool full = false);

            bool verbose = true;
        private:
            struct {
                ConnectionState state = ConnectionState::UNCONNECTED;
                sockaddr_in address_remote;
                int file_descriptor = 0;

                event* event_read = nullptr;
                event* event_write = nullptr;
                struct event_base* event_base = nullptr;
                std::thread event_base_dispatch;

                threads::Thread* flush_thread = nullptr;

                threads::Mutex queue_lock;
                std::deque<std::string> queue_write;

                std::unique_ptr<protocol::packet> current_packet;


                std::string overhead;
            } network;


            struct {
                protocol::RequestState state;
                std::string crypt_key = "";

                std::mutex ping_lock;
                std::condition_variable ping_notify;
                std::thread ping_thread;
            } protocol;

            struct {
                std::unique_ptr<threads::Future<bool>> future_connect;
                std::unique_ptr<threads::Future<bool>> future_login;
                std::unique_ptr<threads::Future<std::pair<std::shared_ptr<license::License>, std::shared_ptr<license::LicenseInfo>>>> future_register;
                std::unique_ptr<threads::Future<std::map<std::string, std::shared_ptr<license::LicenseInfo>>>> future_list;
                std::unique_ptr<threads::Future<bool>> future_delete;
            } listener;

            std::string local_disconnect_message;

            static void handleEventRead(int, short, void*);
            static void handleEventWrite(int, short, void*);

            void closeConnection();
            void sendPacket(const protocol::packet&);
            void handleMessage(const std::string&);

            void handlePacketDisconnect(const std::string&);
            void handlePacketHandshake(const std::string&);
            void handlePacketAuthResponse(const std::string&);
            void handlePacketCreateResponse(const std::string&);
            void handlePacketListResponse(const std::string&);
            void handlePacketDeleteResponse(const std::string&);
    };
}