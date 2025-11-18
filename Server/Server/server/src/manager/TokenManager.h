#pragma once

#include <chrono>
#include <string>
#include <vector>
#include <memory>
#include <sql/SqlQuery.h>
#include "Definitions.h"

namespace ts::server::token {
    typedef uint64_t TokenId;
    typedef uint64_t TokenActionId;

    enum struct ActionType {
        /**
         * Add a server group to the client.
         * `id1` will contain the server group id.
         */
        AddServerGroup = 0x01,
        /**
         * Remove a server group from the client (if he is assigned to it)
         * `id1` will contain the server group id.
         */
        RemoveServerGroup = 0x02,
        /**
         * Set the channel group of the client for a channel.
         * `id1` will contain the channel group id.
         * `id2` will contain the target channel id
         */
        SetChannelGroup = 0x03,
        /**
         * Allow the client to join (on server join) to a certain channel.
         * `id2` will contain the target channel id
         */
        AllowChannelJoin = 0x04,

        /**
         * Sentinel for internal handling
         */
        ActionSqlFailed = 0xFD,
        ActionDeleted = 0xFE,
        ActionIgnore = 0xFF
    };

    struct Token {
        TokenId id{0};

        std::string token{};
        std::string description{};

        ClientDbId issuer_database_id{0};

        size_t max_uses{0};
        size_t use_count{0};

        std::chrono::system_clock::time_point timestamp_expired{};
        std::chrono::system_clock::time_point timestamp_created{};

        [[nodiscard]] inline bool is_expired() const {
            return this->timestamp_expired.time_since_epoch().count() > 0 && std::chrono::system_clock::now() >= this->timestamp_expired;
        }

        [[nodiscard]] inline bool use_limit_reached() const {
            return this->max_uses > 0 && this->use_count >= this->max_uses;
        }
    };

    struct TokenAction {
        TokenActionId id{0};
        ActionType type;
        uint64_t id1{0};
        uint64_t id2{0};
        std::string text{};
    };

    class TokenManager {
        public:
            explicit TokenManager(sql::SqlManager*, ServerId);
            ~TokenManager();

            void initialize_cache();
            void finalize_cache();

            [[nodiscard]] std::shared_ptr<Token> load_token(const std::string& /* token */, bool /* ignore limits */);
            [[nodiscard]] std::shared_ptr<Token> load_token_by_id(TokenId /* token id */, bool /* ignore limits */);

            void log_token_use(TokenId /* token id */);

            /**
             * Create a new token
             * @returns `nullptr` if some database errors occurred else the generated token.
             */
            [[nodiscard]] std::shared_ptr<Token> create_token(
                    ClientDbId /* issuer */,
                    const std::string& /* description */,
                    size_t /* max uses */,
                    const std::chrono::system_clock::time_point& /* expire timestamp */
            );

            /**
             * List all tokens
             * @return
             */
            [[nodiscard]] std::deque<std::shared_ptr<Token>> list_tokens(
                    size_t& /* total tokens */,
                    const std::optional<size_t>& /* offset */,
                    const std::optional<size_t>& /* limit */,
                    const std::optional<ClientDbId>& /* owner */
            );

            [[nodiscard]] size_t client_token_count(ClientDbId /* client */);

            /**
             * If the operation succeeds the new action id will be applied to the token.
             * Note: Actions set to `ActionType::ActionDelete` or `ActionType::ActionIgnore` will be ignored.
             *       If the sql insert failed the type will be set to ActionSqlFailed
             */
            void add_token_actions(TokenId /* token */, std::vector<TokenAction>& /* actions */);

            /**
             * Query all actions related to the token.
             */
            [[nodiscard]] bool query_token_actions(TokenId /* token */, std::deque<TokenAction>& /* result */);

            /**
             * Delete an action from the token.
             */
            void delete_token_actions(TokenId /* token */, const std::vector<TokenActionId>& /* actions */);

            /**
             * Save any modifications made to the token.
             * Editable fields:
             * - `description`
             * - `max_uses`
             * - `timestamp_expired`
             */
            void save_token(const std::shared_ptr<Token>& /* token */);
            void delete_token(TokenId /* token id */);

            void handle_server_group_deleted(GroupId);
            void handle_channel_group_deleted(GroupId);
            void handle_channel_deleted(ChannelId);
        private:
            ServerId virtual_server_id;
            sql::SqlManager* sql_manager;

            void load_tokens(std::deque<std::shared_ptr<Token>>& /* result */, sql::command& /* command */);
    };
}
DEFINE_VARIABLE_TRANSFORM_ENUM(ts::server::token::ActionType, uint8_t);