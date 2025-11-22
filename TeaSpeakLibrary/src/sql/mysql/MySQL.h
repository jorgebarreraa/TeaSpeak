#pragma once

#include <stdexcept>
#include <string>
#include <memory>
#include <condition_variable>
#include "sql/SqlQuery.h"

#include "misc/spin_mutex.h"

#if defined(HAVE_MYSQL_MYSQL_H)
    #include <mysql/mysql.h>
#elif defined(HAVE_MYSQL_H)
    #include <mysql.h>
#else
    typedef void MYSQL;
#endif

#define ERROR_MYSQL_MISSING_DRIVER    -1
#define ERROR_MYSQL_INVLID_CONNECT    -2
#define ERROR_MYSQL_INVLID_PROPERTIES -3
#define ERROR_MYSQL_INVLID_URL        -4

namespace sql::mysql {
    class MySQLManager;

    bool evaluate_sql_query(std::string& sql, const std::vector<variable>& vars, std::vector<variable>& result);

    class MySQLCommand : public CommandData { };

    struct Connection {
        MYSQL* handle = nullptr;

        spin_mutex used_lock;
        bool used = false;

        ~Connection();
    };

    struct AcquiredConnection {
        MySQLManager* owner;
        std::shared_ptr<Connection> connection;

        AcquiredConnection(MySQLManager* owner, std::shared_ptr<Connection> );
        ~AcquiredConnection();
    };

    class MySQLManager : public SqlManager {
            friend struct AcquiredConnection;
        public:
            //typedef std::function<void(const std::shared_ptr<ConnectionEntry>&)> ListenerConnectionDisconnect;
            //typedef std::function<void(const std::shared_ptr<ConnectionEntry>&)> ListenerConnectionCreated;

            typedef std::function<void()> ListenerConnected;
            typedef std::function<void(bool /* wanted */)> ListenerDisconnected;

            MySQLManager();
            virtual ~MySQLManager();

            result connect(const std::string &string) override;
            bool connected() override;
            result disconnect() override;

            ListenerDisconnected listener_disconnected;
        protected:
            std::shared_ptr<CommandData> copyCommandData(std::shared_ptr<CommandData> ptr) override;
            std::shared_ptr<CommandData> allocateCommandData() override;
            result executeCommand(std::shared_ptr<CommandData> command_data) override;
            result queryCommand(std::shared_ptr<CommandData> command_data, const QueryCallback &fn) override;

        public:
            std::unique_ptr<AcquiredConnection> next_connection();
            void connection_closed(const std::shared_ptr<Connection>& /* connection */);

            std::mutex connections_mutex;
            std::condition_variable connections_condition;
            std::deque<std::shared_ptr<Connection>> connections;

            bool disconnecting{false};
    };
}