#pragma once

#include <Definitions.h>
#include <optional>
#include <cstring>
#include <utility>
#include <vector>
#include <sql/SqlQuery.h>
#include <Properties.h>
#include <PermissionManager.h>
#include <query/command3.h>
#include "../absl/btree/set.h"

namespace ts::server {
    class ConnectedClient;
    class QueryClient;
}

namespace ts::server::log {
    struct Invoker {
        std::string name{};
        std::string unique_id{};
        ClientDbId database_id{0};
    };
    extern Invoker kInvokerSystem;

    enum struct LoggerGroup {
        CUSTOM,
        SERVER,
        CHANNEL,
        PERMISSION,
        CLIENT,
        FILES,
        QUERY,
        MAX
    };
    constexpr static std::array<std::string_view, (int) LoggerGroup::MAX> kLoggerGroupName{
        "custom",
        "server",
        "channel",
        "permission",
        "client",
        "files",
        "query"
    };


    enum struct Action {
        UNKNOWN,

        SERVER_CREATE,
        SERVER_START,
        SERVER_EDIT,
        SERVER_STOP,
        SERVER_DELETE,

        CHANNEL_CREATE,
        CHANNEL_EDIT,
        CHANNEL_DELETE,

        PERMISSION_ADD_VALUE,
        PERMISSION_ADD_GRANT,

        PERMISSION_EDIT_VALUE,
        PERMISSION_EDIT_GRANT,

        PERMISSION_REMOVE_VALUE,
        PERMISSION_REMOVE_GRANT,

        GROUP_CREATE,
        GROUP_RENAME,
        GROUP_PERMISSION_COPY,
        GROUP_DELETE,

        GROUP_ASSIGNMENT_ADD,
        GROUP_ASSIGNMENT_REMOVE,

        CLIENT_JOIN,
        CLIENT_EDIT,
        CLIENT_MOVE,
        CLIENT_KICK,
        CLIENT_LEAVE,

        FILE_UPLOAD,
        FILE_DOWNLOAD,
        FILE_RENAME,
        FILE_DELETE,
        FILE_DIRECTORY_CREATE,

        CUSTOM_LOG,

        QUERY_AUTHENTICATE,
        QUERY_JOIN,
        QUERY_LEAVE,

        MAX
    };

    constexpr static std::array<std::string_view, (int) Action::MAX> kActionName{
        "unknown",
        "server-create",
        "server-start",
        "server-edit",
        "server-stop",
        "server-delete",

        "channel-create",
        "channel-edit",
        "channel-delete",

        "permission-add-value",
        "permission-add-grant",

        "permission-edit-value",
        "permission-edit-grant",

        "permission-remove-value",
        "permission-remove-grant",

        "group-create",
        "group-rename",
        "group-permission-copy",
        "group-delete",

        "group-assignment-add",
        "group-assignment-remove",

        "client-join",
        "client-edit",
        "client-move",
        "client-kick",
        "client-leave",

        "file-upload",
        "file-download",
        "file-rename",
        "file-delete",
        "file-directory-create",

        "custom-log",

        "query-authenticate",
        "query-join",
        "query-leave"
    };

    struct LogEntryInfo {
        std::chrono::system_clock::time_point timestamp{};
        Action action{Action::UNKNOWN};

        standalone_command_builder_bulk info{};
    };

    class ActionLogger;

    struct LogGroupSettings {
        [[nodiscard]] virtual bool is_activated(ServerId server_id) const = 0;
    };

    template <bool kDefaultValue>
    struct BiasedLogGroupSettings : public LogGroupSettings {
        public:
            BiasedLogGroupSettings() = default;

            [[nodiscard]] bool is_activated(ServerId server_id) const override {
                auto it = this->status.find(server_id);
                return it == this->status.end() ? kDefaultValue : !kDefaultValue;
            }

            inline void toggle_activated(ServerId server_id, bool enabled) {
                if(enabled == kDefaultValue) {
                    this->status.erase(enabled);
                } else {
                    this->status.emplace(server_id);
                }
            }
        private:
            btree::set<ServerId> status{};
    };

    template <bool kValue>
    struct ConstLogGroupSettings : public LogGroupSettings {
        public:
            ConstLogGroupSettings() = default;

            [[nodiscard]] bool is_activated(ServerId server_id) const override {
                return kValue;
            }
    };
    extern ConstLogGroupSettings<true> kLogGroupSettingTrue;
    extern ConstLogGroupSettings<false> kLogGroupSettingFalse;

    class AbstractActionLogger {
        public:
            explicit AbstractActionLogger(ActionLogger* impl, LogGroupSettings* group_settings) : logger_{impl}, group_settings_{group_settings} {}

            [[nodiscard]] virtual bool setup(int /* database version */, std::string& /* error */) = 0;
            virtual void finalize() { this->enabled_ = false; }

            [[nodiscard]] inline ActionLogger* logger() const { return this->logger_; }

            [[nodiscard]] inline bool enabled() const { return this->enabled_; }
            inline void set_enabled(bool flag) { this->enabled_ = flag; }

            [[nodiscard]] virtual std::deque<LogEntryInfo> query(
                    uint64_t /* server id */,
                    const std::chrono::system_clock::time_point& /* begin timestamp */,
                    const std::chrono::system_clock::time_point& /* end timestamp */,
                    size_t /* limit */
            ) = 0;
        protected:
            ActionLogger* logger_;
            LogGroupSettings* group_settings_;
            bool enabled_{false};

            void do_log(ServerId /* server id */, Action /* action */, sql::command& /* command */) const;
    };

    struct DatabaseColumn {
        std::string name{};
        std::string type{};
    };

    struct FixedHeaderKeys {
        DatabaseColumn timestamp{};
        DatabaseColumn server_id{};
        DatabaseColumn invoker_id{};
        DatabaseColumn invoker_name{};
        DatabaseColumn action{};
    };

    template <typename>
    using T2DatabaseColumn = DatabaseColumn;

    struct FixedHeaderValues {
        std::chrono::system_clock::time_point timestamp{};
        ServerId server_id{0};
        Invoker invoker{};
        Action action{};
    };

    template <typename... LoggingArguments>
    class TypedActionLogger : public AbstractActionLogger {
        public:
            constexpr static auto kFixedHeaderColumnCount = 5;
            static inline std::string value_binding_name(size_t index) {
                return ":v" + std::to_string(index);
            }

            static void compile_model(sql::model& result, const std::string& command, const ActionLogger* logger);
            static void register_invoker(ActionLogger* logger, const Invoker& invoker);
            static std::string generate_insert(
                    const std::string& /* table name */,
                    const FixedHeaderKeys& /* headers */,
                    DatabaseColumn* /* payload columns */,
                    size_t /* payload column count */
            );
            static sql::command compile_query(
                    ActionLogger* /* logger */,
                    uint64_t /* server id */,
                    const std::chrono::system_clock::time_point& /* begin timestamp */,
                    const std::chrono::system_clock::time_point& /* end timestamp */,
                    size_t /* limit */,
                    const std::string& /* table name */,
                    const FixedHeaderKeys& /* headers */,
                    DatabaseColumn* /* payload columns */,
                    size_t /* payload column count */
            );
            static bool create_table(
                    std::string& /* error */,
                    ActionLogger* /* logger */,
                    const std::string& /* table name */,
                    const FixedHeaderKeys& /* headers */,
                    DatabaseColumn* /* payload columns */,
                    size_t /* payload column count */
            );
            static void bind_fixed_header(sql::command& /* command */, const FixedHeaderValues& /* values */);
            static bool parse_fixed_header(FixedHeaderValues& /* result */, std::string* /* values */, size_t /* values length */);

            explicit TypedActionLogger(ActionLogger* impl, LogGroupSettings* group_settings, std::string table_name, FixedHeaderKeys fh_keys, const T2DatabaseColumn<LoggingArguments>&... keys)
                : AbstractActionLogger{impl, group_settings}, fh_keys{std::move(fh_keys)}, table_name{std::move(table_name)} {
                this->bind_key(0, keys...);
            }

            [[nodiscard]] std::deque<LogEntryInfo> query(
                    uint64_t server_id,
                    const std::chrono::system_clock::time_point& begin_timestamp,
                    const std::chrono::system_clock::time_point& end_timestamp,
                    size_t limit
            ) override {
                std::deque<LogEntryInfo> result{};

                auto sql_command = TypedActionLogger<>::compile_query(this->logger_, server_id, begin_timestamp, end_timestamp, limit, this->table_name, this->fh_keys, this->payload_column_names.data(), this->payload_column_names.size());

                FixedHeaderValues header{};
                auto sql_result = sql_command.query([&](int length, std::string* values, std::string* names) {
                    if(length < kFixedHeaderColumnCount + sizeof...(LoggingArguments))
                        return;

                    if(!TypedActionLogger<>::parse_fixed_header(header, values, length))
                        return;

                    auto& entry = result.emplace_back();
                    entry.action = header.action;
                    entry.timestamp = header.timestamp;

                    auto& info = entry.info;
                    info.reserve(160);
                    info.put_unchecked("timestamp", std::chrono::floor<std::chrono::milliseconds>(header.timestamp.time_since_epoch()).count());
                    info.put_unchecked("invoker_database_id", header.invoker.database_id);
                    info.put_unchecked("invoker_nickname", header.invoker.name);
                    info.put_unchecked("action", kActionName[(int) header.action]);

                    for(size_t index{0}; index < sizeof...(LoggingArguments); index++)
                        info.put_unchecked(this->payload_column_names[index].name, values[index + kFixedHeaderColumnCount]);
                });

                return result;
            };
        protected:
            void do_log(const FixedHeaderValues& fhvalues, const LoggingArguments&... vvalues) {
                if(!this->enabled_)
                    return;

                sql::command command = this->sql_model.command();

                TypedActionLogger<>::bind_fixed_header(command, fhvalues);
                TypedActionLogger::bind_value(command, 0, vvalues...);

                AbstractActionLogger::do_log(fhvalues.server_id, fhvalues.action, command);
                TypedActionLogger<>::register_invoker(this->logger_, fhvalues.invoker);
            }

            bool setup(int db_version, std::string &error) override {
                TypedActionLogger<>::compile_model(
                        this->sql_model,
                        TypedActionLogger<>::generate_insert(this->table_name, this->fh_keys, this->payload_column_names.data(), this->payload_column_names.size()),
                        this->logger_);
                if(db_version == 0) {
                    if(!TypedActionLogger<>::create_table(error, this->logger_, this->table_name, this->fh_keys, this->payload_column_names.data(), this->payload_column_names.size()))
                        return false;
                }
                return true;
            }

        private:
            template <typename T, typename... Rest>
            static void bind_value(sql::command& result, int index, const T& value, const Rest&... vvalues) {
                result.value(TypedActionLogger::value_binding_name(index), value);
                TypedActionLogger::bind_value(result, index + 1, vvalues...);
            }

            static void bind_value(sql::command&, int) {}

            FixedHeaderKeys fh_keys;
            sql::model sql_model{nullptr};
            std::string table_name;
            std::array<DatabaseColumn, sizeof...(LoggingArguments)> payload_column_names{};

            template <typename T, typename... Rest>
            void bind_key(int index, const T& value, const Rest&... vkeys) {
                this->payload_column_names[index] = value;
                this->bind_key(index + 1, vkeys...);
            }

            void bind_key(int) {}
    };

    //Table: `id`, `timestamp`, `server_id`, `invoker_database_id`, `invoker_name`, `action`, `property`, `value_old`, `value_new`
    class ServerEditActionLogger : public TypedActionLogger<std::string_view, std::string, std::string> {
        public:
            explicit ServerEditActionLogger(ActionLogger* impl, LogGroupSettings* group_settings);

            bool setup(int, std::string &) override;
            void log_server_edit(
                            ServerId /* server id */,
                            const std::shared_ptr<ConnectedClient>& /* editor (may be null) */,
                            const property::PropertyDescription& /* property */,
                            const std::string& /* old value */,
                            const std::string& /* new value */);
    };

    enum struct ServerCreateReason {
        USER_ACTION,
        SNAPSHOT_DEPLOY,
        INITIAL_SERVER,
        MAX
    };
    constexpr static std::array<std::string_view, (int) ServerCreateReason::MAX> kServerCreateReasonName {
            "user-action",
            "snapshot-deploy",
            "initial-server"
    };

    //Table: `id`, `timestamp`, `server_id`, `invoker_database_id`, `invoker_name`, `action`, `reason`
    class ServerActionLogger : public TypedActionLogger<std::string_view> {
        public:
            explicit ServerActionLogger(ActionLogger* impl, LogGroupSettings* group_settings);

            bool setup(int, std::string &) override;

            void log_server_create(
                    ServerId /* server id */,
                    const std::shared_ptr<ConnectedClient>& /* editor (may be null) */,
                    ServerCreateReason /* create reason */);

            void log_server_delete(
                    ServerId /* server id */,
                    const std::shared_ptr<ConnectedClient>& /* editor (may be null) */);

            void log_server_start(
                    ServerId /* server id */,
                    const std::shared_ptr<ConnectedClient>& /* editor (may be null) */);

            void log_server_stop(
                    ServerId /* server id */,
                    const std::shared_ptr<ConnectedClient>& /* editor (may be null) */);
    };

    enum struct ChannelDeleteReason {
        PARENT_DELETED,
        USER_ACTION,
        SERVER_STOP,
        EMPTY,
        MAX
    };
    constexpr static std::array<std::string_view, (int) ChannelDeleteReason::MAX> kChannelDeleteReasonName {
        "parent-deleted",
        "user-action",
        "server-stop",
        "empty"
    };

    enum struct ChannelType {
        TEMPORARY,
        SEMI_PERMANENT,
        PERMANENT,
        MAX
    };
    constexpr static std::array<std::string_view, (int) ChannelType::MAX> kChannelTypeName {
            "temporary",
            "semi-permanent",
            "permanent"
    };

    //Table: `id`, `timestamp`, `server_id`, `invoker_database_id`, `invoker_name`, `action`, `property` (may be the delete reason), `value_old`, `value_new`
    class ChannelActionLogger : public TypedActionLogger<ChannelId, std::string_view, std::string, std::string> {
        public:
            explicit ChannelActionLogger(ActionLogger* impl, LogGroupSettings* group_settings);

            bool setup(int, std::string &) override;

            void log_channel_create(
                    ServerId /* server id */,
                    const std::shared_ptr<ConnectedClient>& /* editor (may be null) */,
                    ChannelId /* channel id */,
                    ChannelType /* type */);

            void log_channel_move(
                    ServerId /* server id */,
                    const std::shared_ptr<ConnectedClient>& /* editor (may be null) */,
                    ChannelId /* channel id */,
                    ChannelId /* old parent channel */,
                    ChannelId /* new parent channel */,
                    ChannelId /* old channel order */,
                    ChannelId /* new channel order */);

            void log_channel_edit(
                    ServerId /* server id */,
                    const std::shared_ptr<ConnectedClient>& /* editor (may be null) */,
                    ChannelId /* channel id */,
                    const property::PropertyDescription& /* property */,
                    const std::string& /* old value */,
                    const std::string& /* new value */);

            void log_channel_delete(
                    ServerId /* server id */,
                    const std::shared_ptr<ConnectedClient>& /* editor (may be null) */,
                    ChannelId /* channel id */,
                    ChannelDeleteReason /* reason */);
    };

    enum struct PermissionTarget {
        SERVER_GROUP,
        CHANNEL_GROUP,
        CHANNEL,
        CLIENT,
        CLIENT_CHANNEL,
        PLAYLIST,
        PLAYLIST_CLIENT,

        MAX
    };
    constexpr static std::array<std::string_view, (int) PermissionTarget::MAX> kPermissionTargetName {
            "server-group",
            "channel-group",
            "channel",
            "client",
            "client-channel",
            "playlist",
            "playlist-client"
    };

    //Table: `id`, `timestamp`, `server_id`, `invoker_database_id`, `invoker_name`, `action`, `target`, `id1`, `id1_name`, `id2`, `id2_name`, `permission`, `old_value`, `old_negated`, `old_skipped`, `new_value`, `new_negated`, `new_skipped`
    class PermissionActionLogger : public TypedActionLogger<std::string_view, uint64_t, std::string, uint64_t, std::string, std::string, int32_t, bool, bool, int32_t, bool, bool> {
        public:
            explicit PermissionActionLogger(ActionLogger* impl, LogGroupSettings* group_settings);

            bool setup(int, std::string &) override;

            void log_permission_add_value(
                    ServerId /* server id */,
                    const std::shared_ptr<ConnectedClient>& /* editor (may be null) */,
                    ts::server::log::PermissionTarget /* target */,
                    uint64_t /* id1 */, const std::string& /* id1 name */,
                    uint64_t /* id2 */, const std::string& /* id2 name */,
                    const permission::PermissionTypeEntry& /* permission */,
                    int32_t /* value */,
                    bool /* negated */,
                    bool /* skipped */);

            void log_permission_add_grant(
                    ServerId /* server id */,
                    const std::shared_ptr<ConnectedClient>& /* editor (may be null) */,
                    ts::server::log::PermissionTarget /* target */,
                    uint64_t /* id1 */, const std::string& /* id1 name */,
                    uint64_t /* id2 */, const std::string& /* id2 name */,
                    const permission::PermissionTypeEntry& /* permission */,
                    int32_t /* value */);

            void log_permission_edit_value(
                    ServerId /* server id */,
                    const std::shared_ptr<ConnectedClient>& /* editor (may be null) */,
                    ts::server::log::PermissionTarget /* target */,
                    uint64_t /* id1 */, const std::string& /* id1 name */,
                    uint64_t /* id2 */, const std::string& /* id2 name */,
                    const permission::PermissionTypeEntry& /* permission */,
                    int32_t /* old value */,
                    bool /* old negated */,
                    bool /* old skipped */,
                    int32_t /* new value */,
                    bool /* new negated */,
                    bool /* new skipped */);

            void log_permission_edit_grant(
                    ServerId /* server id */,
                    const std::shared_ptr<ConnectedClient>& /* editor (may be null) */,
                    ts::server::log::PermissionTarget /* target */,
                    uint64_t /* id1 */, const std::string& /* id1 name */,
                    uint64_t /* id2 */, const std::string& /* id2 name */,
                    const permission::PermissionTypeEntry& /* permission */,
                    int32_t /* old value */,
                    int32_t /* new value */);

            void log_permission_remove_value(
                    ServerId /* server id */,
                    const std::shared_ptr<ConnectedClient>& /* editor (may be null) */,
                    ts::server::log::PermissionTarget /* target */,
                    uint64_t /* id1 */, const std::string& /* id1 name */,
                    uint64_t /* id2 */, const std::string& /* id2 name */,
                    const permission::PermissionTypeEntry& /* permission */,
                    int32_t /* old value */,
                    bool /* old negate */,
                    bool /* old skip */);

            void log_permission_remove_grant(
                    ServerId /* server id */,
                    const std::shared_ptr<ConnectedClient>& /* editor (may be null) */,
                    ts::server::log::PermissionTarget /* target */,
                    uint64_t /* id1 */, const std::string& /* id1 name */,
                    uint64_t /* id2 */, const std::string& /* id2 name */,
                    const permission::PermissionTypeEntry& /* permission */,
                    int32_t /* old value */);
    };

    enum struct GroupType {
        NORMAL,
        TEMPLATE,
        QUERY,
        MAX
    };
    constexpr static std::array<std::string_view, (int) GroupType::MAX> kGroupTypeName {
            "normal",
            "template",
            "query"
    };

    enum struct GroupTarget {
        SERVER,
        CHANNEL,
        MAX
    };
    constexpr static std::array<std::string_view, (int) GroupTarget::MAX> kGroupTargetName {
            "server",
            "channel"
    };

    //Table: `id`, `timestamp`, `server_id`, `invoker_database_id`, `invoker_name`, `action`, `type`, `target`, `group`, `group_name`, `source`, `source_name`
    class GroupActionLogger : public TypedActionLogger<std::string_view, std::string_view, uint64_t, std::string, uint64_t, std::string> {
        public:
            explicit GroupActionLogger(ActionLogger* impl, LogGroupSettings* group_settings);

            bool setup(int, std::string &) override;

            void log_group_create(
                    ServerId /* server id */,
                    const std::shared_ptr<ConnectedClient>& /* editor (may be null) */,
                    GroupTarget /* target */,
                    GroupType /* type */,
                    uint64_t /* group */,
                    const std::string& /* group name */,
                    uint64_t /* source */,
                    const std::string& /* source name */
            );

            void log_group_rename(
                    ServerId /* server id */,
                    const std::shared_ptr<ConnectedClient>& /* editor (may be null) */,
                    GroupTarget /* target */,
                    GroupType /* type */,
                    uint64_t /* group */,
                    const std::string& /* target name */,
                    const std::string& /* source name */
            );

            void log_group_permission_copy(
                    ServerId /* server id */,
                    const std::shared_ptr<ConnectedClient>& /* editor (may be null) */,
                    GroupTarget /* target */,
                    GroupType /* type */,
                    uint64_t /* target group */,
                    const std::string& /* target group name */,
                    uint64_t /* source */,
                    const std::string& /* source name */
            );

            void log_group_delete(
                    ServerId /* server id */,
                    const std::shared_ptr<ConnectedClient>& /* editor (may be null) */,
                    GroupTarget /* target */,
                    GroupType /* type */,
                    uint64_t /* group */,
                    const std::string& /* group name */
            );
    };


    //Table: `id`, `timestamp`, `server_id`, `invoker_database_id`, `invoker_name`, `action`, `target`, `group`, `group_name`, `client`, `client_name`
    class GroupAssignmentActionLogger : public TypedActionLogger<std::string_view, uint64_t, std::string, uint64_t, std::string> {
        public:
            explicit GroupAssignmentActionLogger(ActionLogger *impl, LogGroupSettings* group_settings);

            bool setup(int, std::string &) override;

            void log_group_assignment_add(
                    ServerId /* server id */,
                    const std::shared_ptr<ConnectedClient>& /* editor (may be null) */,
                    GroupTarget /* target */,
                    uint64_t /* group */,
                    const std::string& /* group name */,
                    uint64_t /* client */,
                    const std::string& /* client name */
            );

            void log_group_assignment_remove(
                    ServerId /* server id */,
                    const std::shared_ptr<ConnectedClient>& /* editor (may be null) */,
                    GroupTarget /* target */,
                    uint64_t /* group */,
                    const std::string& /* group name */,
                    uint64_t /* client */,
                    const std::string& /* client name */
            );
    };

    //Table: `id`, `timestamp`, `server_id`, `invoker_database_id`, `invoker_name`, `action`, `target_client`, `target_client_name`, `source_channel`, `source_channel_name`, `target_channel`, `target_channel_name`
    class ClientChannelActionLogger : public TypedActionLogger<uint64_t, std::string, uint64_t, std::string, uint64_t, std::string> {
        public:
            explicit ClientChannelActionLogger(ActionLogger *impl, LogGroupSettings* group_settings);

            bool setup(int, std::string &) override;


            void log_client_join(
                    ServerId /* server id */,
                    const std::shared_ptr<ConnectedClient>& /* subject */,
                    uint64_t /* target channel */,
                    const std::string& /* target channel name */
            );

            void log_client_move(
                    ServerId /* server id */,
                    const std::shared_ptr<ConnectedClient>& /* issuer (may be null) */,
                    const std::shared_ptr<ConnectedClient>& /* subject */,
                    uint64_t /* target channel */,
                    const std::string& /* target channel name */,
                    uint64_t /* source channel */,
                    const std::string& /* source channel name */
            );

            void log_client_kick(
                    ServerId /* server id */,
                    const std::shared_ptr<ConnectedClient>& /* issuer (may be null) */,
                    const std::shared_ptr<ConnectedClient>& /* subject */,
                    uint64_t /* target channel */,
                    const std::string& /* target channel name */,
                    uint64_t /* source channel */,
                    const std::string& /* source channel name */
            );

            void log_client_leave(
                    ServerId /* server id */,
                    const std::shared_ptr<ConnectedClient>& /* subject */,
                    uint64_t /* source channel */,
                    const std::string& /* source channel name */
            );
    };

    //Table: `id`, `timestamp`, `server_id`, `invoker_database_id`, `invoker_name`, `action`, `target_client`, `target_client_name`, `property`, `old_value`, `new_value`
    class ClientEditActionLogger : public TypedActionLogger<uint64_t, std::string, std::string_view, std::string, std::string> {
        public:
            explicit ClientEditActionLogger(ActionLogger *impl, LogGroupSettings* group_settings);

            bool setup(int, std::string &) override;

            void log_client_edit(
                    ServerId /* server id */,
                    const std::shared_ptr<ConnectedClient>& /* issuer (may be null) */,
                    const std::shared_ptr<ConnectedClient>& /* subject */,
                    const property::PropertyDescription& /* property */,
                    const std::string& /* old value */,
                    const std::string& /* new value */
            );
    };

    //Table: `id`, `timestamp`, `server_id`, `invoker_database_id`, `invoker_name`, `action`, `source_channel_id`, `source_path`, `target_channel_id`, `target_path`
    class FilesActionLogger : public TypedActionLogger<uint64_t, std::string, uint64_t, std::string> {
        public:
            explicit FilesActionLogger(ActionLogger *impl, LogGroupSettings* group_settings);

            bool setup(int, std::string &) override;

            void log_file_upload(
                    ServerId /* server id */,
                    const std::shared_ptr<ConnectedClient>& /* invoker */,
                    uint64_t /* channel id */,
                    const std::string& /* path */
            );

            void log_file_download(
                    ServerId /* server id */,
                    const std::shared_ptr<ConnectedClient>& /* invoker */,
                    uint64_t /* channel id */,
                    const std::string& /* path */
            );

            void log_file_rename(
                    ServerId /* server id */,
                    const std::shared_ptr<ConnectedClient>& /* invoker */,
                    uint64_t /* source channel id */,
                    const std::string& /* source name */,
                    uint64_t /* target channel id */,
                    const std::string& /* target name */
            );

            void log_file_delete(
                    ServerId /* server id */,
                    const std::shared_ptr<ConnectedClient>& /* invoker */,
                    uint64_t /* channel id */,
                    const std::string& /* source name */
            );

            void log_file_directory_create(
                    ServerId /* server id */,
                    const std::shared_ptr<ConnectedClient>& /* invoker */,
                    uint64_t /* channel id */,
                    const std::string& /* path */
            );
    };

    //Table: `id`, `timestamp`, `server_id`, `invoker_database_id`, `invoker_name`, `action`, `message`
    class CustomLogger : public TypedActionLogger<std::string> {
        public:
            explicit CustomLogger(ActionLogger *impl, LogGroupSettings* group_settings);

            bool setup(int, std::string &) override;


            void add_log_message(
                    ServerId /* server id */,
                    const std::shared_ptr<ConnectedClient>& /* issuer (may be null) */,
                    const std::string& /* message */
            );
    };

    enum struct QueryAuthenticateResult {
        SUCCESS,
        UNKNOWN_USER,
        INVALID_PASSWORD,
        MAX
    };

    constexpr static std::array<std::string_view, (int) QueryAuthenticateResult::MAX> kQueryAuthenticateResultName {
            "success",
            "unknown-user",
            "invalid-password"
    };

    //Table: `id`, `timestamp`, `server_id`, `invoker_database_id`, `invoker_name`, `action`, `ip`, `username`, `result`
    class QueryAuthenticateLogger : public TypedActionLogger<std::string, std::string, std::string_view> {
        public:
            explicit QueryAuthenticateLogger(ActionLogger *impl, LogGroupSettings* group_settings);

            bool setup(int, std::string &) override;

            void log_query_authenticate(
                    ServerId /* server id */,
                    const std::shared_ptr<QueryClient>& /* query */,
                    const std::string& /* username (if empty he logs out) */,
                    QueryAuthenticateResult /* result */
            );
    };

    //Table: `id`, `timestamp`, `server_id`, `invoker_database_id`, `invoker_name`, `action`, `ip`, `username`, `source_server`
    class QueryServerLogger : public TypedActionLogger<std::string, std::string, bool> {
        public:
            explicit QueryServerLogger(ActionLogger *impl, LogGroupSettings* group_settings);

            bool setup(int, std::string &) override;

            void log_query_switch(
                    const std::shared_ptr<QueryClient>& /* query */,
                    const std::string& /* authenticated username */,
                    ServerId /* source */,
                    ServerId /* target */
            );
    };

    /*
    VIRTUALSERVER_LOG_CLIENT, (Bans: TODO!, Updates: Done, Channel Tree: Done)
    VIRTUALSERVER_LOG_QUERY, (Check))
    VIRTUALSERVER_LOG_CHANNEL, (Check)
    VIRTUALSERVER_LOG_PERMISSIONS, (Check)
    VIRTUALSERVER_LOG_SERVER, (Check; Groups: Check; Assignments: Check; Administrate: Check)
    VIRTUALSERVER_LOG_FILETRANSFER (Check)
    */
    class ActionLogger {
        public:
            explicit ActionLogger();
            ~ActionLogger();

            [[nodiscard]] bool initialize(std::string& /* error */);
            void finalize();

            [[nodiscard]] inline sql::SqlManager* sql_manager() const { return this->sql_handle; }

            void register_invoker(const Invoker&);

            [[nodiscard]] virtual std::vector<LogEntryInfo> query(
                    std::vector<LoggerGroup> /* groups */,
                    uint64_t /* server id */,
                    const std::chrono::system_clock::time_point& /* begin timestamp */,
                    const std::chrono::system_clock::time_point& /* end timestamp */,
                    size_t /* limit */
            );

            [[nodiscard]] bool logging_group_enabled(ServerId /* server id */, LoggerGroup /* group */) const;
            void toggle_logging_group(ServerId /* server id */, LoggerGroup /* group */, bool /* flag */);

            ServerActionLogger server_logger{this, &log_group_server_};
            ServerEditActionLogger server_edit_logger{this, &log_group_server_};
            ChannelActionLogger channel_logger{this, &log_group_channel_};
            PermissionActionLogger permission_logger{this, &log_group_permissions_};
            GroupActionLogger group_logger{this, &log_group_server_};
            GroupAssignmentActionLogger group_assignment_logger{this, &log_group_server_};
            ClientChannelActionLogger client_channel_logger{this, &log_group_client_};
            ClientEditActionLogger client_edit_logger{this, &log_group_client_};
            FilesActionLogger file_logger{this, &log_group_file_transfer_};
            CustomLogger custom_logger{this, &kLogGroupSettingTrue};
            QueryServerLogger query_logger{this, &log_group_query_};
            QueryAuthenticateLogger query_authenticate_logger{this, &log_group_query_};
        private:
            sql::SqlManager* sql_handle{nullptr};

            BiasedLogGroupSettings<true> log_group_client_{};
            BiasedLogGroupSettings<true> log_group_query_{};
            BiasedLogGroupSettings<true> log_group_channel_{};
            BiasedLogGroupSettings<true> log_group_permissions_{};
            BiasedLogGroupSettings<true> log_group_server_{};
            BiasedLogGroupSettings<true> log_group_file_transfer_{};
    };
}