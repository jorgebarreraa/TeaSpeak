#include <functional>
#include <iostream>
#include <utility>
#include "log/LogUtils.h"
#include "SqlQuery.h"

using namespace std;
using namespace std::chrono;

namespace sql {
    result result::success = result("", 0, "success");
    sql::command model::command() { return ::sql::command(*this); }
    sql::model model::copy() { return sql::model(this->_data); }
    model::model(const std::shared_ptr<CommandData>& data) : command_base(data->handle->copyCommandData(data)) {}
    /**
     * Command class itself
     */

    threads::Future<result> command::executeLater() {
        return this->_data->handle->pool->executeLater(*this);
    }

    AsyncSqlPool::AsyncSqlPool(size_t threads) : _threads(new threads::ThreadPool(threads, "AsyncSqlPool")) {
        debugMessage(LOG_GENERAL, "Created a new async thread pool!");
    }
    AsyncSqlPool::~AsyncSqlPool() {
        delete _threads;
    }

    threads::Future<result> AsyncSqlPool::executeLater(const command& cmd) {
        threads::Future<result> fut;
        this->_threads->execute([cmd, fut]{ //cmd for copy and stmt survive
            command copy = cmd;
            result res;
            while((res = copy.execute()).code() == SQLITE_BUSY){
                cerr << "Execute busy!" << endl;
                usleep(1000);
            }
            fut.executionSucceed(res);
        });
        return fut;
    }

    SqlManager::SqlManager(SqlType type) : type(type) {
        this->pool = new AsyncSqlPool(1);
    }

    SqlManager::~SqlManager() {
        if(this->pool) this->pool->threads()->wait_for();
        delete this->pool;
        this->pool = nullptr;
    }
}