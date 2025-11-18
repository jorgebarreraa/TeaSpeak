//
// Created by WolverinDEV on 26/06/2020.
//

#include "ActionLogger.h"
#include "../client/ConnectedClient.h"
#include <sql/sqlite/SqliteSQL.h>
#include <log/LogUtils.h>

using namespace ts::server;
using namespace ts::server::log;

Invoker log::kInvokerSystem{"System", "system", 0};

ConstLogGroupSettings<true> log::kLogGroupSettingTrue{};
ConstLogGroupSettings<false> log::kLogGroupSettingFalse{};

void AbstractActionLogger::do_log(ServerId sid, Action action, sql::command &command) const {
    if(!this->enabled_) return;

    command.executeLater().waitAndGetLater([sid, action](const sql::result& result) {
        if(!result) {
            logError(sid, "Failed to log action {}: {}", kActionName[(int) action], result.fmtStr());
        }
    }, sql::result{});
}

template <>
void TypedActionLogger<>::compile_model(sql::model &result, const std::string &command, const ActionLogger *logger) {
    std::exchange(result, sql::model{logger->sql_manager(), command});
}

template <>
void TypedActionLogger<>::register_invoker(ActionLogger *logger, const Invoker &invoker) {
    logger->register_invoker(invoker);
}

template <>
std::string TypedActionLogger<>::generate_insert(const std::string &table_name,
                                               const FixedHeaderKeys& header,
                                                 DatabaseColumn *payload_columns,
                                               size_t payload_columns_length) {
    std::string result{"INSERT INTO `"};
    result.reserve(256);

    result += table_name + "` (";
    result += "`" + header.timestamp.name + "`,";
    result += "`" + header.server_id.name + "`,";
    result += "`" + header.invoker_id.name + "`,";
    result += "`" + header.invoker_name.name + "`,";
    result += "`" + header.action.name + "`";

    for(size_t index{0}; index < payload_columns_length; index++)
        result += ",`" + payload_columns[index].name + "`";

    result += ") VALUES (:h0, :h1, :h2, :h3, :h4";

    for(size_t index{0}; index < payload_columns_length; index++)
        result += ", " + TypedActionLogger::value_binding_name(index);
    result += ");";

    return result;
}

template <>
sql::command TypedActionLogger<>::compile_query(
                                              ActionLogger *logger,
                                              uint64_t server_id,
                                              const std::chrono::system_clock::time_point &begin_timestamp,
                                              const std::chrono::system_clock::time_point &end_timestamp, size_t limit,
                                              const std::string& table_name,
                                              const FixedHeaderKeys& header,
                                              DatabaseColumn *payload_columns,
                                              size_t payload_columns_length) {
    std::string command{"SELECT "};
    command.reserve(256);

    command += "`" + header.timestamp.name + "`,";
    command += "`" + header.server_id.name + "`,";
    command += "`" + header.invoker_id.name + "`,";
    command += "`" + header.invoker_name.name + "`,";
    command += "`" + header.action.name + "`";

    for(size_t index{0}; index < payload_columns_length; index++)
        command += ",`" + payload_columns[index].name + "`";

    command += " FROM `" + table_name + "` WHERE `" + header.server_id.name + "` = " + std::to_string(server_id);

    if(begin_timestamp.time_since_epoch().count() > 0)
        command += " AND `" + header.timestamp.name + "` <= " + std::to_string(std::chrono::ceil<std::chrono::milliseconds>(begin_timestamp.time_since_epoch()).count());

    if(begin_timestamp.time_since_epoch().count() > 0)
        command += " AND `" + header.timestamp.name + "` >= " + std::to_string(std::chrono::floor<std::chrono::milliseconds>(end_timestamp.time_since_epoch()).count());

    command += " ORDER BY `timestamp` DESC";
    if(limit > 0)
        command += " LIMIT " + std::to_string(limit);
    command += ";";

    debugMessage(0, "Log query: {}", command);
    return sql::command{logger->sql_manager(), command};
}

template <>
bool TypedActionLogger<>::create_table(
        std::string& error,
        ActionLogger *logger,
        const std::string& table_name,
        const FixedHeaderKeys& header,
        DatabaseColumn *payload_columns,
        size_t payload_columns_length) {
    std::string command{};
    /* ATTENTION: If I implement MySQL add "CHARACTER SET=utf8" AT THE END! */
    if(logger->sql_manager()->getType() != sql::TYPE_SQLITE) {
        error = "unsupported database type";
        return false;
    }

    command.reserve(256);
    command += "CREATE TABLE `" + table_name + "` (";
    command += "`id` INTEGER PRIMARY KEY NOT NULL,";
    command += "`" + header.timestamp.name + "` " + header.timestamp.type + ",";
    command += "`" + header.server_id.name + "` " + header.server_id.type + ",";
    command += "`" + header.invoker_id.name + "` " + header.invoker_id.type + ",";
    command += "`" + header.invoker_name.name + "` " + header.invoker_name.type + ",";
    command += "`" + header.action.name + "` " + header.action.type;

    for(size_t index{0}; index < payload_columns_length; index++)
        command += ",`" + payload_columns[index].name + "` " + payload_columns[index].type;

    command += ");";

    auto result = sql::command{logger->sql_manager(), command}.execute();
    if(!result) {
        error = result.fmtStr();
        return false;
    }
    return true;
}

template <>
bool TypedActionLogger<>::parse_fixed_header(FixedHeaderValues &result, std::string *values, size_t values_length) {
    if(values_length< 5)
        return false;

    int64_t timestamp;
    try {
        timestamp = std::stoll(values[0]);
        result.server_id = std::stoull(values[1]);
        result.invoker.database_id = std::stoull(values[2]);
    } catch (std::exception&) {
        return false;
    }
    result.timestamp = std::chrono::system_clock::time_point{} + std::chrono::milliseconds{timestamp};
    result.invoker.name = values[3];

    result.action = Action::UNKNOWN;
    for(size_t index{0}; index < (size_t) Action::MAX; index++) {
        auto action = static_cast<Action>(index);
        if(kActionName[index] == values[4]) {
            result.action = action;
        }
    }

    return result.action != Action::UNKNOWN;
}

template <>
void TypedActionLogger<>::bind_fixed_header(sql::command &command, const FixedHeaderValues &fhvalues) {
    command.value(":h0", std::chrono::floor<std::chrono::milliseconds>(fhvalues.timestamp).time_since_epoch().count());
    command.value(":h1", fhvalues.server_id);
    command.value(":h2", fhvalues.invoker.database_id);
    command.value(":h3", fhvalues.invoker.name);
    command.value(":h4", kActionName[(int) fhvalues.action]);
}

ActionLogger::ActionLogger() = default;
ActionLogger::~ActionLogger() {
    this->finalize();
}

#define CURRENT_VERSION 1
bool ActionLogger::initialize(std::string &error) {
    int db_version{0};

    this->sql_handle = new sql::sqlite::SqliteManager{};
    auto result = this->sql_handle->connect("sqlite://InstanceLogs.sqlite");
    if(!result) {
        error = result.fmtStr();
        goto error_exit;
    }

    {
        if(sql_handle->getType() == sql::TYPE_MYSQL) {
            sql::command(this->sql_handle, "SET NAMES utf8").execute();
            //sql::command(this->manager, "DEFAULT CHARSET=utf8").execute();
        } else if(sql_handle->getType() == sql::TYPE_SQLITE) {
            sql::command(this->sql_handle, "PRAGMA locking_mode = EXCLUSIVE;").execute();
            sql::command(this->sql_handle, "PRAGMA synchronous = NORMAL;").execute();
            sql::command(this->sql_handle, "PRAGMA journal_mode = WAL;").execute();
            sql::command(this->sql_handle, "PRAGMA encoding = \"UTF-8\";").execute();
        }
    }

    /* begin transaction, if available */
    if(this->sql_handle->getType() == sql::TYPE_SQLITE) {
        result = sql::command(this->sql_handle, "BEGIN TRANSACTION;").execute();
        if(!result) {
            error = "failed to begin transaction (" + result.fmtStr() + ")";
            goto error_exit;
        }
    }

    {
        result = sql::command{this->sql_handle, "CREATE TABLE IF NOT EXISTS general(`key` VARCHAR(256), `value` TEXT);"}.execute();
        if(!result) {
            error = "failed to create if not exists the general table: " + result.fmtStr();
            goto error_exit;
        }

        sql::command{this->sql_handle, "SELECT `value` FROM `general` WHERE `key` = 'version';"}.query([&](int, std::string* values, std::string*) {
            db_version = std::stoi(values[0]);
        });
    }

    switch (db_version) {
        case 0:
            /* initial setup */
        case CURRENT_VERSION:
            /* current version */
            break;

        default:
            error = CURRENT_VERSION < db_version ? "database version is newer than supported (supported: " + std::to_string(CURRENT_VERSION) + ", database: " + std::to_string(db_version) + ")" :
                                                    "invalid database version (" + std::to_string(db_version) + ")";
    }

    if(!this->server_logger.setup(db_version, error)) {
        error = "server logger: " + error;
        goto error_exit;
    }

    if(!this->server_edit_logger.setup(db_version, error)) {
        error = "server edit logger: " + error;
        goto error_exit;
    }

    if(!this->channel_logger.setup(db_version, error)) {
        error = "channel logger: " + error;
        goto error_exit;
    }

    if(!this->permission_logger.setup(db_version, error)) {
        error = "permission logger: " + error;
        goto error_exit;
    }

    if(!this->group_logger.setup(db_version, error)) {
        error = "group logger: " + error;
        goto error_exit;
    }

    if(!this->group_assignment_logger.setup(db_version, error)) {
        error = "group assignment logger: " + error;
        goto error_exit;
    }

    if(!this->client_channel_logger.setup(db_version, error)) {
        error = "client channel logger: " + error;
        goto error_exit;
    }

    if(!this->client_edit_logger.setup(db_version, error)) {
        error = "client edit logger: " + error;
        goto error_exit;
    }

    if(!this->file_logger.setup(db_version, error)) {
        error = "file logger: " + error;
        goto error_exit;
    }

    if(!this->custom_logger.setup(db_version, error)) {
        error = "custom logger: " + error;
        goto error_exit;
    }

    if(!this->query_logger.setup(db_version, error)) {
        error = "query logger: " + error;
        goto error_exit;
    }

    if(!this->query_authenticate_logger.setup(db_version, error)) {
        error = "query authenticate logger: " + error;
        goto error_exit;
    }


    /* update the version */
    {
        std::string command{};
        if(db_version == 0)
            command = "INSERT INTO `general` (`key`, `value`) VALUES ('version', :version);";
        else
            command = "UPDATE `general` SET `value` = :version WHERE `key` = 'version';";

        auto res = sql::command{this->sql_handle, command, variable{":version", CURRENT_VERSION}}.execute();
        if(!res) {
            logCritical(LOG_INSTANCE, "Failed to increment action log database version!");
            goto error_exit;
        }
    }

    if(this->sql_handle->getType() == sql::TYPE_SQLITE) {
        result = sql::command(this->sql_handle, "COMMIT;").execute();
        if(!result) {
            error = "failed to commit changes";
            return false;
        }
    }

    /* enable all loggers */
    this->server_logger.set_enabled(true);
    this->server_edit_logger.set_enabled(true);
    this->channel_logger.set_enabled(true);
    this->permission_logger.set_enabled(true);
    this->group_logger.set_enabled(true);
    this->group_assignment_logger.set_enabled(true);
    this->client_channel_logger.set_enabled(true);
    this->client_edit_logger.set_enabled(true);
    this->file_logger.set_enabled(true);
    this->custom_logger.set_enabled(true);
    this->query_logger.set_enabled(true);
    this->query_authenticate_logger.set_enabled(true);
    return true;

    error_exit:
    if(this->sql_handle && this->sql_handle->connected()) {
        result = sql::command(this->sql_handle, "ROLLBACK;").execute();
        if (!result) {
            logCritical(LOG_GENERAL, "Failed to rollback log database after transaction.");
        } else {
            debugMessage(LOG_GENERAL, "Rollbacked log database successfully.");
        }
    }
    this->finalize();
    return false;
}

void ActionLogger::finalize() {
    if(!this->sql_handle)
        return;

    this->server_logger.finalize();
    this->server_edit_logger.finalize();
    this->channel_logger.finalize();
    this->permission_logger.finalize();
    this->group_logger.finalize();
    this->group_assignment_logger.finalize();
    this->client_channel_logger.finalize();
    this->client_edit_logger.finalize();
    this->custom_logger.finalize();
    this->file_logger.finalize();
    this->query_logger.finalize();
    this->query_authenticate_logger.finalize();

    this->sql_handle->pool->threads()->wait_for(std::chrono::seconds{1});
    this->sql_handle->disconnect();
    delete this->sql_handle;
    this->sql_handle = nullptr;
}

void ActionLogger::register_invoker(const Invoker &) {}

std::vector<LogEntryInfo> ActionLogger::query(
        std::vector<LoggerGroup> groups,
        uint64_t server,
        const std::chrono::system_clock::time_point &begin,
        const std::chrono::system_clock::time_point &end,
        size_t limit) {

    std::vector<LogEntryInfo> result{};
    result.reserve(limit == 0 ? 1024 * 8 : limit * 2);

    std::vector<std::deque<LogEntryInfo>> intresult{};
    intresult.reserve(8);

    for(size_t index{0}; index < (size_t) LoggerGroup::MAX; index++) {
        auto group = static_cast<LoggerGroup>(index);
        if(!groups.empty() && std::find(groups.begin(), groups.end(), group) == groups.end())
            continue;

        intresult.clear();
        switch (group) {
            case LoggerGroup::SERVER:
                std::exchange(intresult.emplace_back(), this->server_logger.query(server, begin, end, limit));
                std::exchange(intresult.emplace_back(), this->server_edit_logger.query(server, begin, end, limit));
                std::exchange(intresult.emplace_back(), this->group_logger.query(server, begin, end, limit));
                std::exchange(intresult.emplace_back(), this->group_assignment_logger.query(server, begin, end, limit));
                break;

            case LoggerGroup::CHANNEL:
                std::exchange(intresult.emplace_back(), this->channel_logger.query(server, begin, end, limit));
                break;

            case LoggerGroup::CLIENT:
                std::exchange(intresult.emplace_back(), this->client_channel_logger.query(server, begin, end, limit));
                std::exchange(intresult.emplace_back(), this->client_edit_logger.query(server, begin, end, limit));
                break;

            case LoggerGroup::FILES:
                std::exchange(intresult.emplace_back(), this->file_logger.query(server, begin, end, limit));
                break;

            case LoggerGroup::PERMISSION:
                std::exchange(intresult.emplace_back(), this->permission_logger.query(server, begin, end, limit));
                break;

            case LoggerGroup::CUSTOM:
                std::exchange(intresult.emplace_back(), this->custom_logger.query(server, begin, end, limit));
                break;

            case LoggerGroup::QUERY:
                std::exchange(intresult.emplace_back(), this->query_logger.query(server, begin, end, limit));
                std::exchange(intresult.emplace_back(), this->query_authenticate_logger.query(server, begin, end, limit));
                break;

            case LoggerGroup::MAX:
            default:
                assert(false);
                break;
        }

        for(auto& qresult : intresult) {
            if(qresult.empty())
                continue;
            /* qresult is already sorted */
            result.insert(result.begin(), qresult.begin(), qresult.begin() + std::min(qresult.size(), limit));

            std::sort(result.begin(), result.end(), [](const LogEntryInfo& a, const LogEntryInfo& b){
                if(a.timestamp == b.timestamp)
                    return &a > &b;
                return a.timestamp > b.timestamp;
            });
            if(limit > 0 && result.size() > limit)
                result.erase(result.begin() + limit, result.end());
        }
    }

    return result;
}

void ActionLogger::toggle_logging_group(ServerId server_id, LoggerGroup group, bool flag) {
    switch (group) {
        case LoggerGroup::SERVER:
            this->log_group_server_.toggle_activated(server_id, flag);
            break;

        case LoggerGroup::PERMISSION:
            this->log_group_permissions_.toggle_activated(server_id, flag);
            break;

        case LoggerGroup::QUERY:
            this->log_group_query_.toggle_activated(server_id, flag);
            break;

        case LoggerGroup::FILES:
            this->log_group_file_transfer_.toggle_activated(server_id, flag);
            break;

        case LoggerGroup::CLIENT:
            this->log_group_client_.toggle_activated(server_id, flag);
            break;

        case LoggerGroup::CHANNEL:
            this->log_group_channel_.toggle_activated(server_id, flag);
            break;

        case LoggerGroup::CUSTOM:
        case LoggerGroup::MAX:
        default:
            break;
    }
}