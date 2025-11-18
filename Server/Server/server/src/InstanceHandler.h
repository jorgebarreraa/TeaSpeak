#pragma once

#include <sql/SqlQuery.h>
#include <Properties.h>
#include "VirtualServerManager.h"
#include <ssl/SSLManager.h>
#include <src/lincense/LicenseService.h>
#include "manager/SqlDataManager.h"
#include "lincense/TeamSpeakLicense.h"
#include <misc/task_executor.h>

namespace ts {
    namespace permission {
        class PermissionNameMapper;
    }

    namespace server {
        namespace license {
            class LicenseService;
        }

        namespace file {
            class FileServerHandler;
        }

        namespace log {
            class ActionLogger;
        }

        namespace groups {
            class GroupManager;
        }

        class NetworkEventLoop;
        class ServerCommandExecutor;
        class InstanceHandler;

        class InstanceHandler {
            public:
                explicit InstanceHandler(SqlDataManager*);
                ~InstanceHandler();

                bool startInstance();
                void stopInstance();

                inline PropertyWrapper properties() { return PropertyWrapper{this->_properties}; }
                inline const PropertyWrapper properties() const { return PropertyWrapper{this->_properties}; }

                std::shared_ptr<ts::server::InternalClient> getInitialServerAdmin(){ return globalServerAdmin; }
                const auto& group_manager(){ return this->group_manager_; }

                /**
                 * Get the default instance server query group.
                 * @return the default group or `nullptr` if the group doesn't exists any more.
                 */
                [[nodiscard]] std::shared_ptr<groups::ServerGroup> guest_query_group();

                std::shared_ptr<ts::ServerChannelTree> getChannelTree() { return this->default_tree; }
                std::shared_mutex& getChannelTreeLock() { return this->default_tree_lock; }

                VirtualServerManager* getVoiceServerManager(){ return this->voiceServerManager; }
                QueryServer* getQueryServer(){ return queryServer; }
                DatabaseHelper* databaseHelper(){ return this->dbHelper; }
                BanManager* banManager(){ return this->banMgr; }
                ssl::SSLManager* sslManager(){ return this->sslMgr; }
                sql::SqlManager* getSql(){ return sql->sql(); }
                log::ActionLogger* action_logger() { return &*this->action_logger_; }
                file::FileServerHandler* getFileServerHandler() { return this->file_server_handler_; }

                std::chrono::time_point<std::chrono::system_clock> getStartTimestamp(){ return startTimestamp; }

                bool reloadConfig(std::vector<std::string>& /* errors */, bool /* reload file */);
                void setWebCertRoot(const std::string& /* key */, const std::string& /* certificate */, const std::string& /* revision */);

                const std::shared_ptr<ConnectedClient>& musicRoot() { return this->_musicRoot; }

                std::chrono::milliseconds calculateSpokenTime();
                void resetSpeechTime();

                bool resetMonthlyStats();

                [[nodiscard]] inline const auto& general_task_executor(){ return this->general_task_executor_; }
                [[nodiscard]] inline const auto& network_event_loop(){ return this->network_event_loop_; }

                [[nodiscard]] inline std::shared_ptr<stats::ConnectionStatistics> getStatistics(){ return statistics; }
                [[nodiscard]] std::shared_ptr<license::InstanceLicenseInfo> generateLicenseData();

                [[nodiscard]] inline std::shared_ptr<TeamSpeakLicense> getTeamSpeakLicense() { return this->teamspeak_license; }
                [[nodiscard]] inline PropertyWrapper getDefaultServerProperties() { return PropertyWrapper{this->default_server_properties}; }

                [[nodiscard]] inline std::shared_ptr<permission::PermissionNameMapper> getPermissionMapper() { return this->permission_mapper; }
                [[nodiscard]] inline std::shared_ptr<ts::event::EventExecutor> getConversationIo() { return this->conversation_io; }

                [[nodiscard]] inline const auto& server_command_executor() { return this->server_command_executor_; }

                [[nodiscard]] inline std::shared_ptr<license::LicenseService> license_service() { return this->license_service_; }
            private:
                std::mutex activeLock;
                std::condition_variable activeCon;
                bool active = false;

                task_id tick_task_id{};

                std::chrono::system_clock::time_point startTimestamp;
                std::chrono::system_clock::time_point sqlTestTimestamp;
                std::chrono::system_clock::time_point speachUpdateTimestamp;
                std::chrono::system_clock::time_point groupSaveTimestamp;
                std::chrono::system_clock::time_point channelSaveTimestamp;
                std::chrono::system_clock::time_point generalUpdateTimestamp;
                std::chrono::system_clock::time_point statisticsUpdateTimestamp;
                std::chrono::system_clock::time_point memcleanTimestamp;
                SqlDataManager* sql;

                QueryServer* queryServer = nullptr;
                VirtualServerManager* voiceServerManager = nullptr;
                DatabaseHelper* dbHelper = nullptr;
                BanManager* banMgr = nullptr;
                ssl::SSLManager* sslMgr = nullptr;
                file::FileServerHandler* file_server_handler_{nullptr};
                std::unique_ptr<log::ActionLogger> action_logger_{nullptr};
                std::unique_ptr<NetworkEventLoop> network_event_loop_{nullptr};

                std::shared_ptr<ts::PropertyManager> _properties{};

                std::shared_ptr<ServerCommandExecutor> server_command_executor_{};

                std::shared_ptr<ts::event::EventExecutor> conversation_io = nullptr;

                std::shared_ptr<ts::PropertyManager> default_server_properties = nullptr;
                std::shared_mutex default_tree_lock;
                std::shared_ptr<ts::ServerChannelTree> default_tree = nullptr;

                std::shared_ptr<groups::GroupManager> group_manager_{nullptr};

                std::shared_ptr<ts::server::InternalClient> globalServerAdmin = nullptr;
                std::shared_ptr<ConnectedClient> _musicRoot = nullptr;

                std::shared_ptr<license::LicenseService> license_service_{nullptr};
                std::shared_ptr<stats::ConnectionStatistics> statistics = nullptr;

                std::shared_ptr<task_executor> general_task_executor_{nullptr};

                std::shared_ptr<permission::PermissionNameMapper> permission_mapper = nullptr;
                std::shared_ptr<TeamSpeakLicense> teamspeak_license = nullptr;

                std::string web_cert_revision{};

                threads::Mutex lock_tick;
            private:
                bool setupDefaultGroups();
                void tickInstance();

                void save_group_permissions();
                void save_channel_permissions();

                void loadWebCertificate();
                bool validate_default_groups();
        };
    }
}
extern ts::server::InstanceHandler* serverInstance;