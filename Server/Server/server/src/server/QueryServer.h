#pragma once

#include <vector>
#include <string>
#include <chrono>
#include <memory>
#include <ThreadPool/Thread.h>
#include <deque>
#include <ThreadPool/Mutex.h>
#include <netinet/in.h>
#include <sqlite3.h>
#include <sql/SqlQuery.h>
#include "../Group.h"
#include <event.h>
#include <misc/net.h>
#include "../manager/IpListManager.h"

namespace ts {
    class BasicChannel;
    class Group;

    namespace server {
        class VirtualServer;
        class QueryClient;

        class QueryAccount {
            public:
                std::string username;
                std::string unique_id;

                ServerId bound_server;

                QueryAccount() = default;
                virtual ~QueryAccount() = default;
        };

        class PasswortedQueryAccount : public QueryAccount {
            public:
                std::string password;

                PasswortedQueryAccount() = default;
                virtual ~PasswortedQueryAccount() = default;
        };

        class QueryServer {
                friend class QueryClient;
            public:
                struct Binding {
                    sockaddr_storage address{};
                    int file_descriptor = 0;
                    ::event* event_accept = nullptr;

                    inline std::string as_string() { return net::to_string(address, true); }
                };

                explicit QueryServer(sql::SqlManager*);
                ~QueryServer();

                bool start(const std::deque<std::shared_ptr<Binding>>& /* bindings */, std::string& /* error */);
                void stop();
                bool running(){ return active; }

                void unregisterConnection(const std::shared_ptr<QueryClient> &);

                std::deque<std::shared_ptr<QueryAccount>> list_query_accounts(OptionalServerId /* server */);
                std::shared_ptr<QueryAccount> create_query_account(const std::string& /* name */, ServerId /* server */, const std::string& /* owner unique id */, const std::string& /* password */);
                std::shared_ptr<PasswortedQueryAccount> load_password(const std::shared_ptr<QueryAccount>& /* account */);
                bool delete_query_account(const std::shared_ptr<QueryAccount>& /* account */);

                std::shared_ptr<QueryAccount> find_query_account_by_name(const std::string& /* name */);
                std::deque<std::shared_ptr<QueryAccount>> find_query_accounts_by_unique_id(const std::string &unique_id /* unique id */);

                bool rename_query_account(const std::shared_ptr<QueryAccount>& /* account */, const std::string& /* new name */);
                bool change_query_password(const std::shared_ptr<QueryAccount>& /* account */, const std::string& /* new password */);
            private:
                sql::SqlManager* sql;

                bool active{false};
                std::deque<std::shared_ptr<Binding>> bindings;

                std::mutex server_reserve_fd_lock{};
                int server_reserve_fd{-1}; /* -1 = unset | 0 = in use | > 0 ready to use */

                std::unique_ptr<IpListManager> ip_whitelist;
                std::unique_ptr<IpListManager> ip_blacklist;

                std::chrono::system_clock::time_point accept_event_deleted;

                std::mutex connected_clients_mutex{};
                std::deque<std::shared_ptr<QueryClient>> connected_clients{};
                std::condition_variable connected_client_disconnected_notify{};

                std::mutex client_connect_mutex{};
                std::chrono::system_clock::time_point client_connect_last_decrease{};
                std::map<std::string, uint32_t> client_connect_count{};
                std::map<std::string, std::chrono::system_clock::time_point> client_connect_bans{};

                bool tick_active{false};
                std::mutex tick_mutex{};
                std::thread tick_thread{};
                std::condition_variable tick_notify{};
                std::deque<std::weak_ptr<QueryClient>> tick_pending_disconnects{};
                std::deque<std::weak_ptr<QueryClient>> tick_pending_connection_close{};
                std::chrono::system_clock::time_point tick_next_client_timestamp{};

                static void on_client_receive(int, short, void *);
                void tick_clients();
                void tick_executor();

                void enqueue_query_disconnect(const std::shared_ptr<QueryClient>& /* client */);
                void execute_query_disconnect(const std::shared_ptr<QueryClient>& /* client */, bool /* shutdown disconnect */);

                void enqueue_query_connection_close(const std::shared_ptr<QueryClient>& /* client */);
                void execute_query_connection_close(const std::shared_ptr<QueryClient>& /* client */, bool /* warn on unknown client */);
        };
    }
}