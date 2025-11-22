#include <src/log/LogUtils.h>
#include "SqliteSQL.h"

using namespace std;
using namespace sql;
using namespace sqlite;

SqliteManager::SqliteManager() : SqlManager(SqlType::TYPE_SQLITE) { }

SqliteManager::~SqliteManager() {
    if(this->connected()) this->disconnect();
}

result SqliteManager::connect(const std::string &string) {
    auto url = string;
    if(url.find("sqlite://") == 0) url = url.substr(strlen("sqlite://"));

    auto result = sqlite3_open(url.c_str(), &this->database);
    if(!this->database)
        return {"connect", -1, -1, "could not open database. Code: " + to_string(result)};
    return result::success;
}

bool SqliteManager::connected() {
    return this->database != nullptr;
}

result SqliteManager::disconnect() {
    if(!this->database) {
        return {"disconnect", -1, -1, "database not open"};
    }

    this->pool->threads()->wait_for();
    auto result = sqlite3_close(this->database);
    if(result == 0) {
        this->database = nullptr;
        return result::success;
    };
    return {"disconnect", -1, -1, "Failed to close database. Code: " + to_string(result)};
}

std::shared_ptr<CommandData> SqliteManager::allocateCommandData() {
    return std::make_shared<SqliteCommand>();
}

std::shared_ptr<CommandData> SqliteManager::copyCommandData(std::shared_ptr<CommandData> ptr) {
    auto _new = this->allocateCommandData();
    _new->handle = ptr->handle;
    _new->lock = ptr->lock;
    _new->sql_command = ptr->sql_command;
    _new->variables = ptr->variables;

    auto __new = static_pointer_cast<SqliteCommand>(_new);
    auto __ptr = static_pointer_cast<SqliteCommand>(ptr);
    __new->stmt = __ptr->stmt;
    return __new;
}

namespace sql::sqlite {
    inline void bindVariable(sqlite3_stmt* stmt, int& valueIndex, const variable& val){
        valueIndex = sqlite3_bind_parameter_index(stmt, val.key().c_str());
        if(valueIndex == 0){ //TODO maybe throw an exception
            //cerr << "Cant find variable '" + val.key() + "' -> '" + val.value() + "' in query '" + sqlite3_sql(stmt) + "'" << endl;
            return;
        }


        int resultState = 0;
        if(val.type() == VARTYPE_NULL)
            resultState = sqlite3_bind_null(stmt, valueIndex);
        else if(val.type() == VARTYPE_TEXT)
            resultState = sqlite3_bind_text(stmt, valueIndex, val.value().c_str(), val.value().length(), SQLITE_TRANSIENT);
        else if(val.type() == VARTYPE_INT || val.type() == VARTYPE_BOOLEAN)
            resultState = sqlite3_bind_int(stmt, valueIndex, val.as<int32_t>());
        else if(val.type() == VARTYPE_LONG)
            resultState = sqlite3_bind_int64(stmt, valueIndex, val.as<int64_t>());
        else if(val.type() == VARTYPE_DOUBLE || val.type() == VARTYPE_FLOAT)
            resultState = sqlite3_bind_double(stmt, valueIndex, val.as<double>());
        else cerr << "Invalid value type!" << endl; //TODO throw exception

        if(resultState != SQLITE_OK){
            cerr << "Invalid bind. " << sqlite3_errmsg(sqlite3_db_handle(stmt)) << " Index: " << valueIndex << endl; //TODO throw exception
        }
    }
}

std::shared_ptr<sqlite3_stmt> SqliteManager::allocateStatement(const std::string& command) {
    sqlite3_stmt* stmt;
    if(sqlite3_prepare_v2(this->database, command.data(), static_cast<int>(command.length()), &stmt, nullptr) != SQLITE_OK)
        return nullptr;

    return std::shared_ptr<sqlite3_stmt>(stmt, [](void* _ptr) {
        sqlite3_finalize(static_cast<sqlite3_stmt*>(_ptr));
    });
}

result SqliteManager::queryCommand(std::shared_ptr<CommandData> _ptr, const QueryCallback &fn) {
    auto ptr = static_pointer_cast<SqliteCommand>(_ptr);
    std::lock_guard<threads::Mutex> lock(ptr->lock);

    result res;
    std::shared_ptr<sqlite3_stmt> stmt;
    if(ptr->stmt){
        stmt = ptr->stmt;
        sqlite3_reset(stmt.get());
    } else {
        ptr->stmt = this->allocateStatement(ptr->sql_command);
        if(!ptr->stmt) {
            return {_ptr->sql_command, 1, -1, sqlite3_errmsg(ptr->sqlHandle<SqliteManager>()->database)};
        }
        stmt = ptr->stmt;
    }

    int varIndex = 0;
    for(auto& var : ptr->variables)
        bindVariable(stmt.get(), varIndex, var);

    int result = 0;
    int columnCount = sqlite3_column_count(stmt.get());
    std::string columnNames[columnCount];
    std::string columnValues[columnCount];

    for(int column = 0; column < columnCount; column++) {
        auto tmp = sqlite3_column_name(stmt.get(), column);
        columnNames[column] = tmp ? tmp : "";
    }

    bool userQuit = false;
    while((result = sqlite3_step(stmt.get())) == SQLITE_ROW){
        for(int column = 0; column < columnCount; column++) {
            const auto * tmp = reinterpret_cast<const char *>(sqlite3_column_text(stmt.get(), column));
            columnValues[column] = tmp ? tmp : "";
        }
        if(fn(columnCount, columnValues, columnNames) != 0) {
            userQuit = true;
            break;
        }
    }

    if(result != SQLITE_DONE && !userQuit) {
        return {_ptr->sql_command, result, -1, sqlite3_errstr(result)};
    }
    return {_ptr->sql_command,0, 0, "success"};
}

result SqliteManager::executeCommand(std::shared_ptr<CommandData> command_data) {
    auto sql_command = static_pointer_cast<SqliteCommand>(command_data);
    std::lock_guard<threads::Mutex> lock(sql_command->lock);

    result res{};
    sqlite3_stmt* stmt{};
    if(sql_command->stmt){
        stmt = sql_command->stmt.get();
        sqlite3_reset(stmt);
    } else {
        sql_command->stmt = this->allocateStatement(sql_command->sql_command);
        if(!sql_command->stmt) {
            return {sql_command->sql_command, 1, -1, sqlite3_errmsg(sql_command->sqlHandle<SqliteManager>()->database)};
        }
        stmt = sql_command->stmt.get();
    }

    int variable_index{0};
    for(const auto& var : sql_command->variables) {
        bindVariable(stmt, variable_index, var);
    }

    int result = sqlite3_step(stmt);

    if(result == SQLITE_DONE) {
        auto last_row = sqlite3_last_insert_rowid(this->database);
        return {sql_command->sql_command, 0, last_row, "success"};
    } else if(result == SQLITE_ROW) {
        return {sql_command->sql_command, -1, -1, "query has a result"};
    } else {
        return {sql_command->sql_command, 1, -1, sqlite3_errstr(result)};
    }
}