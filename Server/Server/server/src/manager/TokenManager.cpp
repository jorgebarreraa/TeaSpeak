//
// Created by wolverindev on 16.11.17.
//

#include <algorithm>
#include <iostream>
#include <random>
#include <misc/rnd.h>
#include <log/LogUtils.h>
#include "TokenManager.h"

using namespace std;
using namespace std::chrono;
using namespace ts;
using namespace ts::server::token;

TokenManager::TokenManager(sql::SqlManager *sql, ServerId server_id) : sql_manager{sql}, virtual_server_id{server_id} {}
TokenManager::~TokenManager() {}

void TokenManager::initialize_cache() {}
void TokenManager::finalize_cache() {}

#define TOKEN_LOAD_PROPERTIES "`token_id`, `token`, `description`, `issuer_database_id`, `max_uses`, `use_count`, `timestamp_created`, `timestamp_expired`"
#define TOKEN_USE_ABLE "(`max_uses` = 0 OR `max_uses` > `use_count`) AND (`timestamp_expired` = 0 OR `timestamp_expired` > :now)"

void TokenManager::load_tokens(std::deque<std::shared_ptr<Token>> &result, sql::command &command) {
    command.query([&](int length, std::string* values, std::string* names) {
        auto index{0};

        auto& token = result.emplace_back();
        token = std::make_shared<Token>();
        try {
            assert(names[index] == "token_id");
            token->id = std::stoull(values[index++]);

            assert(names[index] == "token");
            token->token = values[index++];

            assert(names[index] == "description");
            token->description = values[index++];

            assert(names[index] == "issuer_database_id");
            token->issuer_database_id = std::stoull(values[index++]);

            assert(names[index] == "max_uses");
            token->max_uses = std::stoull(values[index++]);

            assert(names[index] == "use_count");
            token->use_count = std::stoull(values[index++]);

            assert(names[index] == "timestamp_created");
            token->timestamp_created = std::chrono::system_clock::time_point{} + std::chrono::seconds{std::stoull(values[index++])};

            assert(names[index] == "timestamp_expired");
            token->timestamp_expired = std::chrono::system_clock::time_point{} + std::chrono::seconds{std::stoull(values[index++])};

            assert(index == length);
        } catch (std::exception& ex) {
            result.pop_back();
            logError(this->virtual_server_id, "Failed to parse token at index {}: {}",
                     index - 1,
                     ex.what()
            );
            return;
        }
    });
}

size_t TokenManager::client_token_count(ClientDbId client) {
    sql::command command{this->sql_manager, "SELECT COUNT(`token_id`) FROM `tokens` WHERE `server_id` = :server_id AND `issuer_database_id` = :client AND " TOKEN_USE_ABLE};
    command.value(":server_id", this->virtual_server_id);
    command.value(":client", client);
    command.value(":now", std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());

    size_t total_count{(size_t) -1};
    auto query_result = command.query([&](int, std::string* values, std::string*) {
        total_count = std::stoull(values[0]);
    });

    if(!query_result) {
        LOG_SQL_CMD(query_result);
    }

    return total_count;
}

std::deque<std::shared_ptr<Token>> TokenManager::list_tokens(size_t &total_count, const std::optional<size_t> &offset,
                                                             const std::optional<size_t> &limit, const std::optional<ClientDbId>& owner) {
    {
        std::string sql_command{};
        sql_command += "SELECT COUNT(`token_id`) FROM `tokens` WHERE `server_id` = :server_id AND " TOKEN_USE_ABLE;
        if(owner.has_value()) {
            sql_command += " AND `issuer_database_id` = :owner";
        }

        sql::command command{this->sql_manager, sql_command};
        command.value(":server_id", this->virtual_server_id);
        command.value(":owner", owner.value_or(0));
        command.value(":now", std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
        auto query_result = command.query([&](int, std::string* values, std::string*) {
            total_count = std::stoull(values[0]);
        });

        if(!query_result) {
            LOG_SQL_CMD(query_result);
            total_count = 0;
        }
    }

    std::deque<std::shared_ptr<Token>> result{};
    {
        std::string sql_command{};
        sql_command += "SELECT " TOKEN_LOAD_PROPERTIES " FROM `tokens` WHERE `server_id` = :server_id AND " TOKEN_USE_ABLE;
        if(owner.has_value()) {
            sql_command += " AND `issuer_database_id` = :owner";
        }
        sql_command += " LIMIT :offset, :limit";

        sql::command command{this->sql_manager, sql_command};
        command.value(":server_id", this->virtual_server_id);
        command.value(":owner", owner.value_or(0));
        command.value(":offset", offset.value_or(0));
        command.value(":limit", limit.has_value() ? (int64_t) *limit : -1);
        command.value(":now", std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());

        this->load_tokens(result, command);
    }

    return result;
}

std::shared_ptr<Token> TokenManager::load_token(const std::string &token, bool ignore_limits) {
    std::deque<std::shared_ptr<Token>> result{};

    std::string sql_command{"SELECT " TOKEN_LOAD_PROPERTIES " FROM `tokens` WHERE `token` = :token"};
    if(!ignore_limits) {
        sql_command += " AND " TOKEN_USE_ABLE;
    }

    sql::command command{this->sql_manager, sql_command};
    command.value(":token", token);
    command.value(":now", std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    this->load_tokens(result, command);

    return result.empty() ? nullptr : result.front();
}

std::shared_ptr<Token> TokenManager::load_token_by_id(TokenId token_id, bool ignore_limits) {
    std::deque<std::shared_ptr<Token>> result{};

    std::string sql_command{"SELECT " TOKEN_LOAD_PROPERTIES " FROM `tokens` WHERE `token_id` = :token_id"};
    if(!ignore_limits) {
        sql_command += " AND " TOKEN_USE_ABLE;
    }

    sql::command command{this->sql_manager, sql_command};
    command.value(":token_id", token_id);
    command.value(":now", std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    this->load_tokens(result, command);

    return result.empty() ? nullptr : result.front();
}

std::shared_ptr<Token> TokenManager::create_token(
        ClientDbId issuer_database_id,
        const std::string &description,
        size_t max_uses,
        const std::chrono::system_clock::time_point &timestamp_expired
) {
    auto token = rnd_string(32);
    auto created_timestamp = std::chrono::system_clock::now();

    sql::command command{this->sql_manager, "INSERT INTO `tokens`(`server_id`, `token`, `description`, `issuer_database_id`, `max_uses`, `timestamp_created`, `timestamp_expired`)"
                                            " VALUES (:server_id, :token, :description, :issuer_database_id, :max_uses, :timestamp_created, :timestamp_expired);"};
    command.value(":server_id", this->virtual_server_id);
    command.value(":token", token);
    command.value(":description", description);
    command.value(":issuer_database_id", issuer_database_id);
    command.value(":max_uses", max_uses);
    command.value(":timestamp_created", std::chrono::floor<std::chrono::seconds>(created_timestamp.time_since_epoch()).count());
    command.value(":timestamp_expired", std::chrono::floor<std::chrono::seconds>(timestamp_expired.time_since_epoch()).count());

    auto insert_result = command.execute();
    if(!insert_result) {
        logError(this->virtual_server_id, "Failed to register new token: {}", insert_result.fmtStr());
        return nullptr;
    }

    auto result = std::make_shared<Token>();
    result->id = insert_result.last_insert_rowid();
    result->token = token;
    result->issuer_database_id = issuer_database_id;

    result->description = description;
    result->max_uses = max_uses;
    result->use_count = 0;

    result->timestamp_expired = timestamp_expired;
    result->timestamp_created = created_timestamp;

    return result;
}

bool TokenManager::query_token_actions(TokenId token_id, std::deque<TokenAction> &actions) {
    sql::command command{this->sql_manager,
                         "SELECT `action_id`, `type`, `id1`, `id2`, `text` FROM `token_actions` WHERE `token_id` = :token_id"};

    command.value(":token_id", token_id);
    command.query([&](int length, std::string* values, std::string* names) {
        auto index{0};

        auto& action = actions.emplace_back();
        try {
            assert(names[index] == "action_id");
            action.id = std::stoull(values[index++]);

            assert(names[index] == "type");
            action.type = (ActionType) std::stoull(values[index++]);

            assert(names[index] == "id1");
            action.id1 = std::stoull(values[index++]);

            assert(names[index] == "id2");
            action.id2 = std::stoull(values[index++]);

            assert(names[index] == "text");
            action.text = values[index++];

            assert(index == length);
        } catch (std::exception& ex) {
            actions.pop_back();
            logError(this->virtual_server_id, "Failed to parse token action at index {}: {}",
                     index - 1,
                     ex.what()
            );
            return;
        }
    });

    return true;
}

void TokenManager::add_token_actions(TokenId token_id, std::vector<TokenAction> &actions) {
    sql::model insert{this->sql_manager, "INSERT INTO `token_actions`(`server_id`, `token_id`, `type`, `id1`, `id2`, `text`) VALUES (:server_id, :token_id, :type, :id1, :id2, :text)"};
    insert.value(":server_id", this->virtual_server_id);
    insert.value(":token_id", token_id);

    for(auto& action : actions) {
        if(action.type == ActionType::ActionDeleted || action.type == ActionType::ActionSqlFailed || action.type == ActionType::ActionIgnore) {
            continue;
        }

        auto command = insert.command();
        command.value(":type", (uint8_t) action.type);
        command.value(":id1", action.id1);
        command.value(":id2", action.id2);
        command.value(":text", action.text);

        auto result = command.execute();
        if(!result) {
            logError(this->virtual_server_id, "Failed to insert token action {} for token id {}: {}", (uint32_t) action.type, token_id, result.fmtStr());
            action.type = ActionType::ActionSqlFailed;
        } else {
            action.id = result.last_insert_rowid();
        }
    }
}

void TokenManager::delete_token_actions(TokenId, const std::vector<TokenActionId> &actions) {
    if(actions.empty()) {
        return;
    }

    std::string sql_command{};
    sql_command += "DELETE FROM `token_actions` WHERE `action_id` IN (";
    for(size_t index{0}; index < actions.size(); index++) {
        if(index > 0) {
            sql_command += ", ";
        }

        sql_command += std::to_string(actions[index]);
    }
    sql_command += ");";

    sql::command command{this->sql_manager, sql_command};
    command.executeLater().waitAndGetLater(LOG_SQL_CMD, {-1, "failed to delete token actions"});
}

void TokenManager::save_token(const std::shared_ptr<Token> &token) {
    sql::command command{this->sql_manager, "UPDATE `tokens` SET `description` = :description, `max_uses` = :max_uses, `timestamp_expired` = :timestamp_expired WHERE `token_id` = :token_id;"};
    command.value(":token_id", token->id);
    command.value(":description", token->description);
    command.value(":max_uses", token->max_uses);
    command.value(":timestamp_expired", std::chrono::floor<std::chrono::seconds>(token->timestamp_expired.time_since_epoch()).count());
    command.executeLater().waitAndGetLater(LOG_SQL_CMD, {-1, "failed update token"});
}

void TokenManager::delete_token(TokenId token_id) {
    {
        sql::command command{this->sql_manager, "DELETE FROM `tokens` WHERE `token_id` = :token_id;"};
        command.value(":token_id", token_id);
        command.executeLater().waitAndGetLater(LOG_SQL_CMD, {-1, "failed delete token"});
    }

    {
        sql::command command{this->sql_manager, "DELETE FROM `token_actions` WHERE `token_id` = :token_id;"};
        command.value(":token_id", token_id);
        command.executeLater().waitAndGetLater(LOG_SQL_CMD, {-1, "failed delete token actions"});
    }
}

void TokenManager::log_token_use(TokenId token_id) {
    sql::command command{this->sql_manager, "UPDATE `tokens` SET `use_count` = `use_count` + 1 WHERE `token_id` = :token_id;"};
    command.value(":token_id", token_id);
    command.executeLater().waitAndGetLater(LOG_SQL_CMD, {-1, "failed to log toke use"});
}

void TokenManager::handle_channel_deleted(ChannelId channel_id) {
    sql::command command{this->sql_manager, "DELETE FROM `token_actions` WHERE `server_id` = :server_id AND `type` = :type AND `id2` = :id2;"};
    command.value(":server_id", this->virtual_server_id);
    command.value(":type", (uint8_t) ActionType::SetChannelGroup);
    command.value(":id2", channel_id);
    command.executeLater().waitAndGetLater(LOG_SQL_CMD, {-1, "failed cleanup token actions for a deleted channel"});
}

void TokenManager::handle_channel_group_deleted(GroupId group_id) {
    sql::command command{this->sql_manager, "DELETE FROM `token_actions` WHERE `server_id` = :server_id AND `type` = :type AND `id1` = :id1;"};
    command.value(":server_id", this->virtual_server_id);
    command.value(":type", (uint8_t) ActionType::SetChannelGroup);
    command.value(":id1", group_id);
    command.executeLater().waitAndGetLater(LOG_SQL_CMD, {-1, "failed cleanup token actions for a deleted channel group"});
}

void TokenManager::handle_server_group_deleted(GroupId group_id) {
    {
        sql::command command{this->sql_manager, "DELETE FROM `token_actions` WHERE `server_id` = :server_id AND `type` = :type AND `id1` = :id1;"};
        command.value(":server_id", this->virtual_server_id);
        command.value(":type", (uint8_t) ActionType::AddServerGroup);
        command.value(":id1", group_id);
        command.executeLater().waitAndGetLater(LOG_SQL_CMD, {-1, "failed cleanup token actions for a deleted server group"});
    }

    {
        sql::command command{this->sql_manager, "DELETE FROM `token_actions` WHERE `server_id` = :server_id AND `type` = :type AND `id1` = :id1;"};
        command.value(":server_id", this->virtual_server_id);
        command.value(":type", (uint8_t) ActionType::RemoveServerGroup);
        command.value(":id1", group_id);
        command.executeLater().waitAndGetLater(LOG_SQL_CMD, {-1, "failed cleanup token actions for a deleted server group"});
    }
}