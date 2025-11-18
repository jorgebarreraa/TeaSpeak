//
// Created by wolverindev on 24.03.18.
//

#include <sql/sqlite/SqliteSQL.h>
#include <src/Configuration.h>
#include <log/LogUtils.h>
#include <Properties.h>
#include <sql/mysql/MySQL.h>
#include <PermissionManager.h>
#include "SqlDataManager.h"

using namespace std;
using namespace std::chrono;
using namespace ts;
using namespace ts::server;

SqlDataManager::SqlDataManager() {}
SqlDataManager::~SqlDataManager() {}

#define EXECUTE(msg, cmd)                                                                                                                   \
result = sql::command(this->manager, cmd).execute();                                                                                        \
if(!result){                                                                                                                                \
    error = string(msg) + " Command '" + std::string(cmd) + "' returns " + result.fmtStr();                                                 \
    return false;                                                                                                                           \
}
#define EXECUTE_(error_message, cmd, ignore)                                                                                                \
result = sql::command(this->manager, cmd).execute();                                                                                        \
if(!result && result.msg().find(ignore) == string::npos){                                                                                   \
    error = string(error_message) + " Command '" + std::string(cmd) + "' returns " + result.fmtStr();                                       \
    return false;                                                                                                                           \
}

#define BUILD_CREATE_TABLE(tblName, types)  "CREATE TABLE IF NOT EXISTS `" tblName "` (" types ")"
#define CREATE_TABLE(table, types, append)          EXECUTE("Could not setup SQL tables! ", BUILD_CREATE_TABLE(table, types) + append);

#define DROP_INDEX(tblName, rowName) EXECUTE_("Failed to drop old indexes", "DROP INDEX IF EXISTS `idx_" tblName "_" rowName "`", "");
#define BUILD_CREATE_INDEX(tblName, rowName) "CREATE INDEX `idx_" tblName "_" rowName "` ON `" tblName "` (`" rowName "`);"
#define CREATE_INDEX(tblName, rowName) EXECUTE_("Could not setup SQL table indexes! ", BUILD_CREATE_INDEX(tblName, rowName), "Duplicate");

#define DROP_INDEX2R(tblName, rowName1, rowName2) EXECUTE_("Failed to drop old indexes", "DROP INDEX IF EXISTS `idx_" tblName "_" rowName1 "_" rowName2 "`;", "");
#define BUILD_CREATE_INDEX2R(tblName, rowName1, rowName2) "CREATE INDEX `idx_" tblName "_" rowName1 "_" rowName2 "` ON `" tblName "` (`" rowName1 "`, `" rowName2 "`);"
#define CREATE_INDEX2R(tblName, rowName1, rowName2) EXECUTE_("Could not setup SQL table indexes! ", BUILD_CREATE_INDEX2R(tblName, rowName1, rowName2), "Duplicate");

#define RESIZE_COLUMN(tblName, rowName, size) up vote EXECUTE("Could not change column size", "ALTER TABLE " tblName " ALTER COLUMN " rowName " varchar(" size ")");

#define CURRENT_DATABASE_VERSION 18
#define CURRENT_PERMISSION_VERSION 9

#define CLIENT_UID_LENGTH "64"
#define CLIENT_NAME_LENGTH "128"
#define UNKNOWN_KEY_LENGTH "256"

template <typename T>
struct alive_watch {
    bool do_notify;
    T notify;

    alive_watch(T&& function) : notify(std::forward<T>(function)), do_notify(true) { }
    ~alive_watch() {
        if(do_notify)
            notify();
    }
};

#define db_version(new_version) \
do { \
    if(!this->change_database_version(new_version)) { \
        error = "failed to update database version"; \
        return false; \
    } \
} while(0)

#define perm_version(new_version) \
do { \
    if(!this->change_permission_version(new_version)) { \
        error = "failed to update permission version"; \
        return false; \
    } \
} while(0)

std::string replace_all(std::string str, const std::string& from, const std::string& to) {
    size_t start_pos = 0;
    while((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
    return str;
}

template <typename T, size_t N>
inline bool execute_commands(sql::SqlManager* sql, std::string& error, const std::array<T, N>& commands) {
    std::string insert_or_ignore{sql->getType() == sql::TYPE_SQLITE ? "INSERT OR IGNORE" : "INSERT IGNORE"};
    std::string auto_increment{sql->getType() == sql::TYPE_SQLITE ? "AUTOINCREMENT" : "AUTO_INCREMENT"};
    for(const auto& cmd : commands) {
        std::string command{cmd};

        command.erase(command.begin(), std::find_if(command.begin(), command.end(), [](int ch) {
            return !std::isspace(ch);
        }));

        command = replace_all(command, "[INSERT_OR_IGNORE]", insert_or_ignore);
        command = replace_all(command, "[AUTO_INCREMENT]", auto_increment);

        auto result = sql::command(sql, command).execute();
        if(!result) {
            error = result.fmtStr();
            return false;
        }
    }

    return true;
}

bool SqlDataManager::initialize(std::string& error) {
    if(ts::config::database::url.find("sqlite://") == 0)
        this->manager = new sql::sqlite::SqliteManager();
    else if(ts::config::database::url.find("mysql://") == 0)
        this->manager = new sql::mysql::MySQLManager();
    else {
        error = "Invalid database type!";
        return false;
    }
    auto result = manager->connect(ts::config::database::url);
    if(!result) {
        error = "Could not connect to " + ts::config::database::url + " (" + result.fmtStr() + ").";
        return false;
    }

    if(manager->getType() == sql::TYPE_MYSQL) {
        sql::command(this->manager, "SET NAMES utf8").execute();
        //sql::command(this->manager, "DEFAULT CHARSET=utf8").execute();
    } else if(manager->getType() == sql::TYPE_SQLITE) {
        if(!config::database::sqlite::journal_mode.empty())
            sql::command(this->manager, "PRAGMA journal_mode=" + config::database::sqlite::journal_mode + ";").execute();

        if(!config::database::sqlite::sync_mode.empty())
            sql::command(this->manager, "PRAGMA synchronous=" + config::database::sqlite::sync_mode + ";").execute();

        if(!config::database::sqlite::locking_mode.empty())
            sql::command(this->manager, "PRAGMA locking_mode=" + config::database::sqlite::locking_mode + ";").execute();

        sql::command(this->manager, "PRAGMA encoding = \"UTF-8\";").execute();
    }

    /* begin transaction, if available */
    if(manager->getType() == sql::TYPE_SQLITE) {
        result = sql::command(this->sql(),"BEGIN TRANSACTION;").execute();
        if(!result) {
            error = "failed to begin transaction (" + result.fmtStr() + ")";
            return false;
        }
    }


    alive_watch rollback_watch([&]{
        if(manager->getType() == sql::TYPE_SQLITE) {
            auto result = sql::command(this->sql(), "ROLLBACK;").execute();
            if (!result) {
                logCritical(LOG_GENERAL, "Failed to rollback database after transaction.");
                return;
            }
            debugMessage(LOG_GENERAL, "Rollbacked database successfully.");
        }
    });

    if(!this->detect_versions()) {
        error = "failed to detect database/permission version";
        return false;
    }

    if(!this->update_database(error)) {
        error = "failed to upgrade database: " + error;
        return false;
    }

    //Advanced locked test
    {
        bool property_exists = false;
        sql::command(this->sql(), "SELECT * FORM `general` WHERE `key` = :key", variable{":key", "lock_test"}).query([&](int, string*, string*) { property_exists = true; });
        sql::result res;
        if(!property_exists) {
            res = sql::command(this->sql(), "INSERT INTO `general` (`key`, `value`) VALUES (:key, :value);", variable{":key", "lock_test"}, variable{":value", "UPDATE ME!"}).execute();
        } else {
            res = sql::command(this->sql(), "UPDATE `general` SET `value`= :value WHERE `key`= :key;", variable{":key", "lock_test"}, variable{":value", "TeaSpeak created by WolverinDEV <3"}).execute();
        }

        if(!res) {
            if(res.msg().find("database is locked") != string::npos) error = "database is locked";
            else error = "Failed to execute lock test! Command result: " + res.fmtStr();
            return false;
        }

        if(this->sql()->getType() == sql::TYPE_SQLITE) {
            res = sql::command{this->sql(), "DELETE FROM `general` WHERE `id` IN (SELECT `id` FROM `general` WHERE `key` = :key ORDER BY `key` LIMIT 1, -1);", variable{":key", "lock_test"}}.execute();
            if(!res) {
                error = res.fmtStr();
                return false;
            }
        }
    }

    if(!this->update_permissions(error)) {
        error = "failed to upgrade permissions: " + error;
        return false;
    }
    rollback_watch.do_notify = false; /* transaction was successful */
    if(manager->getType() == sql::TYPE_SQLITE) {
        result = sql::command(this->sql(), "COMMIT;").execute();
        if(!result) {
            error = "failed to commit changes";
            return false;
        }
    }

    return true;
}

bool SqlDataManager::update_database(std::string &error) {
    if(this->_database_version != CURRENT_DATABASE_VERSION) {
        string command_append_utf8 = manager->getType() == sql::TYPE_MYSQL ? " CHARACTER SET=utf8" : "";

        sql::result result;
        auto timestamp_start = system_clock::now();
        logMessage(LOG_GENERAL, "Upgrading database from version " + to_string(this->_database_version) + " to " + to_string(CURRENT_DATABASE_VERSION) + ". This could take a moment!");

        switch (this->_database_version) {
            case -1:
                CREATE_TABLE("general", "`key` VARCHAR(" UNKNOWN_KEY_LENGTH "), `value` TEXT", command_append_utf8);
                CREATE_TABLE("servers", "`serverId` INT NOT NULL, `host` TEXT NOT NULL, `port` INT", command_append_utf8);

                CREATE_TABLE("assignedGroups", "`serverId` INT NOT NULL, `cldbid` INT NOT NULL, `groupId` INT, `channelId` INT DEFAULT -1, `until` BIGINT DEFAULT -1", command_append_utf8);
                CREATE_TABLE("groups", "`serverId` INT NOT NULL, `groupId` INTEGER, `target` INT, `type` INT, `displayName` VARCHAR(" CLIENT_NAME_LENGTH ")", command_append_utf8);

                CREATE_TABLE("queries", "`username` VARCHAR(" CLIENT_NAME_LENGTH "), `password` TEXT, `uniqueId` VARCHAR(" CLIENT_UID_LENGTH ")", command_append_utf8);
                CREATE_TABLE("clients", "`serverId` INT NOT NULL, `cldbid` INTEGER, `clientUid` VARCHAR(" CLIENT_UID_LENGTH "), `firstConnect` BIGINT, `lastConnect` BIGINT, `connections` INT, `lastName` VARCHAR(" CLIENT_NAME_LENGTH ")", command_append_utf8);
                CREATE_TABLE("channels", "`serverId` INT NOT NULL, `channelId` INT, `type` INT, `parentId` INT", command_append_utf8);

                CREATE_TABLE("properties", "`serverId` INTEGER DEFAULT -1, `type` INTEGER, `id` INTEGER, `key` VARCHAR(" UNKNOWN_KEY_LENGTH "), `value` TEXT", command_append_utf8);
                CREATE_TABLE("permissions", "`serverId` INT NOT NULL, `type` INT, `id` INT, `channelId` INT, `permId` VARCHAR(" UNKNOWN_KEY_LENGTH "), `value` INT, `grant` INT", command_append_utf8);

                CREATE_TABLE("bannedClients", "`banId` INTEGER NOT NULL PRIMARY KEY, `serverId` INT NOT NULL, `invokerDbId` INT NOT NULL, `reason` TEXT, `hwid` VARCHAR(" CLIENT_UID_LENGTH "), `uid` VARCHAR(" CLIENT_UID_LENGTH "), `name` VARCHAR(" CLIENT_NAME_LENGTH "), `ip` VARCHAR(128), `strType` VARCHAR(32), `created` BIGINT DEFAULT -1, `until` BIGINT DEFAULT -1", command_append_utf8);
                CREATE_TABLE("tokens", "`serverId` INT NOT NULL, `type` INT NOT NULL, `token` VARCHAR(128), `targetGroup` INT NOT NULL, `targetChannel` INT, `description` TEXT , `created` INT", command_append_utf8);

                CREATE_TABLE("complains", "`serverId` INT NOT NULL, `targetId` INT NOT NULL, `reporterId` INT NOT NULL, `reason` TEXT, `created` INT", command_append_utf8);
                CREATE_TABLE("letters", "`serverId` INT NOT NULL, `letterId` INTEGER NOT NULL PRIMARY KEY, `sender` VARCHAR(" CLIENT_UID_LENGTH "), `receiver` VARCHAR(" CLIENT_UID_LENGTH "), `created` INT, `subject` TEXT, `message` TEXT, `read` INT", command_append_utf8);
                CREATE_TABLE("musicbots", "`serverId` INT, `botId` INT, `uniqueId` VARCHAR(" CLIENT_UID_LENGTH "), `owner` INT", command_append_utf8);

                db_version(0);
            case 0:
                CREATE_INDEX("general", "key");
                CREATE_INDEX("servers", "serverId");

                CREATE_INDEX2R("assignedGroups", "serverId", "cldbid");
                CREATE_INDEX2R("groups", "serverId", "groupId");
                CREATE_INDEX("groups", "serverId");

                CREATE_INDEX("queries", "username");
                CREATE_INDEX("clients", "serverId");
                CREATE_INDEX2R("clients", "serverId", "cldbid");
                CREATE_INDEX("channels", "serverId");

                CREATE_INDEX("properties", "serverId");
                CREATE_INDEX("permissions", "serverId");
                CREATE_INDEX2R("properties", "serverId", "id");
                CREATE_INDEX2R("permissions", "serverId", "channelId");

                CREATE_INDEX("bannedClients", "serverId");
                CREATE_INDEX("bannedClients", "banId");
                CREATE_INDEX2R("bannedClients", "serverId", "hwid");
                CREATE_INDEX2R("bannedClients", "serverId", "name");
                CREATE_INDEX2R("bannedClients", "serverId", "uid");
                CREATE_INDEX2R("bannedClients", "serverId", "ip");

                CREATE_INDEX("tokens", "serverId");
                CREATE_INDEX("complains", "serverId");
                CREATE_INDEX("letters", "serverId");
                CREATE_INDEX("letters", "letterId");
                CREATE_INDEX("musicbots", "serverId");
                db_version(1);

            case 1:
                sql::command(this->sql(), "UPDATE `properties` SET `type` = :type WHERE `serverId` = 0 AND `id` = 0", variable{":type", property::PropertyType::PROP_TYPE_INSTANCE}).execute();
                db_version(2);

            case 2:
                sql::command(this->sql(), "ALTER TABLE permissions ADD flag_skip BOOL;").execute();
                sql::command(this->sql(), "ALTER TABLE permissions ADD flag_negate BOOL;").execute();
                db_version(3);

            case 3:
                EXECUTE("Failed to update ban table", "ALTER TABLE `bannedClients` ADD COLUMN `triggered` INT DEFAULT 0;");
                CREATE_TABLE("ban_trigger", "`server_id` INT, `ban_id` INT, `unique_id` VARCHAR(" CLIENT_UID_LENGTH "), `hardware_id` VARCHAR(" CLIENT_UID_LENGTH "), `name` VARCHAR(" CLIENT_NAME_LENGTH "), `ip` VARCHAR(128), `timestamp` BIGINT", command_append_utf8);
                CREATE_INDEX2R("ban_trigger", "server_id", "ban_id");
                db_version(4);
            case 4:
                sql::command(this->sql(), "ALTER TABLE queries ADD server INT;").execute();
                db_version(5);

            case 5:
                CREATE_TABLE("playlists", "`serverId` INT NOT NULL, `playlist_id` INT", command_append_utf8);
                CREATE_INDEX("playlists", "serverId");

                CREATE_TABLE("playlist_songs", "`serverId` INT NOT NULL, `playlist_id` INT, `song_id` INT, `order_id` INT, `invoker_dbid` INT, `url` TEXT, `url_loader` TEXT", command_append_utf8);
                CREATE_INDEX2R("playlist_songs", "serverId", "playlist_id");
                db_version(6);

                sql::command(this->sql(), "UPDATE `permissions ` SET `permId` = `b_client_music_create_temporary` WHERE `permId` = `b_client_music_create`").execute();

            case 6:
                sql::command(this->sql(), "ALTER TABLE playlist_songs ADD loaded BOOL;").execute();
                sql::command(this->sql(), "ALTER TABLE playlist_songs ADD metadata TEXT;").execute();

            case 7:
                /* recreate permission table */
                /*
                DROP TABLE `permissions`, `properties`;
                ALTER TABLE permissions_v6 RENAME permissions;
                ALTER TABLE properties_v6 RENAME properties;
                */

                /* MySQL command

                START TRANSACTION;
                SET SESSION sql_mode='STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_AUTO_CREATE_USER,NO_ENGINE_SUBSTITUTION';

                -- Modify the permissions
                ALTER TABLE permissions RENAME TO permissions_v6;
                CREATE TABLE `permissions`(`serverId` INT NOT NULL, `type` INT, `id` INT, `channelId` INT, `permId` VARCHAR(256), `value` INT, `grant` INT, `flag_skip` TINYINT(1), `flag_negate` TINYINT(1), CONSTRAINT PK PRIMARY KEY(`serverId`, `type`, `id`, `channelId`, `permId`)) CHARACTER SET=utf8;
                CREATE INDEX `idx_permissions_serverId` ON `permissions` (`serverId`);
                CREATE INDEX `idx_permissions_serverId_channelId` ON `permissions` (`serverId`, `channelId`);
                INSERT INTO `permissions` SELECT * FROM `permissions_v6` GROUP BY `serverId`, `type`, `id`, `channelId`, `permId`;

                -- Modify the properties
                ALTER TABLE properties RENAME TO properties_v6;
                CREATE TABLE properties(`serverId` INTEGER DEFAULT -1, `type` INTEGER, `id` INTEGER, `key` VARCHAR(256), `value` TEXT, CONSTRAINT PK PRIMARY KEY (`serverId`, `type`, `id`, `key`)) CHARACTER SET=utf8;
                CREATE INDEX `idx_properties_serverId` ON `properties` (`serverId`);
                CREATE INDEX `idx_properties_serverId_id` ON `properties` (`serverId`, `id`);
                INSERT INTO `properties` SELECT * FROM `properties_v6` GROUP BY `serverId`, `type`, `id`, `key`;

                -- Delete orphaned permissions and properties
                DELETE FROM `permissions` WHERE (`type` = 1 OR `type` = 2) AND NOT ((`serverId`, `channelId`) IN (SELECT `serverId`, `channelId` FROM `channels`) OR `channelId` = 0);
                DELETE FROM `permissions` WHERE `type` = 0 AND NOT (`serverId`, `id`) IN (SELECT `serverId`, `groupId` FROM `groups`);
                DELETE FROM `permissions` WHERE `type` = 2 AND NOT (`serverId`, `id`) IN (SELECT `serverId`, `cldbid` FROM `clients`);
                DELETE FROM `properties` WHERE `type` = 1 AND NOT (`serverId` IN (SELECT `serverId` FROM servers) OR `serverId` = 0);

                ROLLBACK;
                 */
                if(manager->getType() == sql::TYPE_MYSQL) {
                    /*
                     * FIXME implement update for MySQL as well!
                    auto mysql = (sql::mysql::MySQLManager*) this->sql();

                    if(!result) {
                        error = "failed to update database (" + result.fmtStr() + ")";
                        return false;
                    }
                     */
                } else {
                    result = sql::command(this->sql(), "ALTER TABLE permissions RENAME TO permissions_v6;").execute();
                    if(!result) {
                        error = "Failed to rename permission to permission v6 (" + result.fmtStr() + ")";
                        return false;
                    }

                    result = sql::command(this->sql(), "CREATE TABLE `permissions`(`serverId` INT NOT NULL, `type` INT, `id` INT, `channelId` INT, `permId` VARCHAR(256), `value` INT, `grant` INT, `flag_skip` BOOL, `flag_negate` BOOL, CONSTRAINT PK PRIMARY KEY(`serverId`, `type`, `id`, `channelId`, `permId`));").execute();
                    if(!result) {
                        error = "Failed to create new permission table (" + result.fmtStr() + ")";
                        return false;
                    }

                    result = sql::command(this->sql(), "INSERT INTO `permissions` SELECT * FROM `permissions_v6` GROUP BY `serverId`, `type`, `id`, `channelId`, `permId`;").execute();
                    if(!result) {
                        error = "Failed to insert unique permissions (" + result.fmtStr() + ")";
                        return false;
                    }

                    DROP_INDEX("permissions", "serverId");
                    DROP_INDEX2R("permissions", "serverId", "channelId");
                    CREATE_INDEX("permissions", "serverId");
                    CREATE_INDEX2R("permissions", "serverId", "channelId");

                    /* recreate property table */
                    result = sql::command(this->sql(), "ALTER TABLE properties RENAME TO properties_v6;").execute();
                    if(!result) {
                        error = "Failed to rename properties to properties v6 (" + result.fmtStr() + ")";
                        return false;
                    }

                    result = sql::command(this->sql(), "CREATE TABLE properties(`serverId` INTEGER DEFAULT -1, `type` INTEGER, `id` INTEGER, `key` VARCHAR(256), `value` TEXT, CONSTRAINT PK PRIMARY KEY (`serverId`, `type`, `id`, `key`));").execute();
                    if(!result) {
                        error = "Failed to create new properties table (" + result.fmtStr() + ")";
                        return false;
                    }

                    /*
                     SET SESSION sql_mode='STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_AUTO_CREATE_USER,NO_ENGINE_SUBSTITUTION';
                      INSERT INTO `properties` SELECT * FROM `properties_v6` GROUP BY `serverId`, `type`, `id`, `key`;
                     */
                    result = sql::command(this->sql(), "INSERT INTO `properties` SELECT * FROM `properties_v6` GROUP BY `serverId`, `type`, `id`, `key`;").execute();
                    if(!result) {
                        error = "Failed to insert unique properties (" + result.fmtStr() + ")";
                        return false;
                    }

                    DROP_INDEX("properties", "serverId");
                    DROP_INDEX2R("properties", "serverId", "id");
                    CREATE_INDEX("properties", "serverId");
                    CREATE_INDEX2R("properties", "serverId", "id");

                    /* SELECT * FROM `permissions` WHERE `type` = 1 AND (`serverId`, `channelId`) IN (SELECT `serverId`, `channelId` FROM `channels`) */
                    /* Channel permission cleanup: DELETE FROM `permissions` WHERE `type` = 1 AND NOT (`serverId`, `channelId`) IN (SELECT `serverId`, `channelId` FROM `channels`) */

                    sql::command(this->sql(), "DELETE FROM `permissions` WHERE (`type` = 1 OR `type` = 2) AND NOT ((`serverId`, `channelId`) IN (SELECT `serverId`, `channelId` FROM `channels`) OR `channelId` = 0);").execute();
                    sql::command(this->sql(), "DELETE FROM `permissions` WHERE `type` = 0 AND NOT (`serverId`, `id`) IN (SELECT `serverId`, `groupId` FROM `groups`);").execute();
                    sql::command(this->sql(), "DELETE FROM `permissions` WHERE `type` = 2 AND NOT (`serverId`, `id`) IN (SELECT `serverId`, `cldbid` FROM `clients`);").execute();

                    sql::command(this->sql(), "DELETE FROM `properties` WHERE `type` = 1 AND NOT (`serverId` IN (SELECT `serverId` FROM servers) OR `serverId` = 0);").execute();
                }
                db_version(8);
            case 8:
                result = sql::command(this->sql(), "UPDATE `queries` SET `server` = 0 WHERE `server` IS NULL").execute();
                if(!result) {
                    error = "Failed to drop null query entries (" + result.fmtStr() + ")";
                    return false;
                }
                db_version(9);
            case 9:
                //
                //"UPDATE `permissions` SET `id` = :id WHERE `type` = :channel_type" ;permission::SQL_PERM_CHANNEL
                result = sql::command(this->sql(), "UPDATE `permissions` SET `id` = :id WHERE `type` = :channel_type;", variable{":channel_type", permission::SQL_PERM_CHANNEL}, variable{":id", 0}).execute();
                if(!result) {
                    if(result.code() == 1) { //constraint failed => duplicated ids for example
                        size_t count = 0;
                        result = sql::command(this->sql(), "SELECT COUNT(*) FROM `permissions` WHERE NOT `id` = :id AND `type` = :type", variable{":type", permission::SQL_PERM_CHANNEL}, variable{":id", 0}).query([&](int, std::string* values, std::string*) {
                            count = stoll(values[0]);
                        });
                        logError(LOG_GENERAL, "Database contains invalid channel permissions. Deleting permissions ({}).", count);
                        result = sql::command(this->sql(), "DELETE FROM `permissions` WHERE NOT `id` = :id AND `type` = :type", variable{":channel_type", permission::SQL_PERM_CHANNEL}, variable{":id", 0}).execute();
                    }
                    if(!result) {
                        error = "Failed to fix channel properties (" + result.fmtStr() + ")";
                        return false;
                    }
                }
                db_version(10);
            case 10:
                CREATE_TABLE("conversations", "`server_id` INT, `channel_id` INT, `conversation_id` INT, `file_path` TEXT", command_append_utf8);
                CREATE_TABLE("conversation_blocks", "`server_id` INT, `conversation_id` INT, `begin_timestamp` INT, `end_timestamp` INT, `block_offset` INT, `flags` INT", command_append_utf8);
                CREATE_INDEX("conversations", "server_id");
                CREATE_INDEX2R("conversation_blocks", "server_id", "conversation_id");
                db_version(11);

            case 11:
                result = sql::command(this->sql(), "UPDATE `letters` SET `created` = 0 WHERE 1;").execute();
                if(!result) {
                    error = "Failed to reset offline messages timestamps (" + result.fmtStr() + ")";
                    return false;
                }

                db_version(12);

            case 12: {
                constexpr static std::string_view kCreateClientsV2{R"(
                    CREATE TABLE `clients_v2` (
                        `client_database_id` INTEGER NOT NULL PRIMARY KEY [AUTO_INCREMENT],
                        `client_unique_id` VARCHAR(40) UNIQUE,
                        `client_created` BIGINT,
                        `client_login_name` VARCHAR(20) UNIQUE
                    );
                )"};

                constexpr static std::string_view kCreateClientsServer{R"(
                    CREATE TABLE `clients_server` (
                        `server_id` INTEGER,
                        `client_unique_id` VARCHAR(40) NOT NULL,
                        `client_database_id` INTEGER NOT NULL,

                        `client_nickname` VARCHAR(128),
                        `client_ip` VARCHAR(64),

                        `client_login_password` VARCHAR(40),

                        `client_last_connected` BIGINT DEFAULT 0,
                        `client_created` BIGINT,
                        `client_total_connections` BIGINT DEFAULT 0,

                        `client_month_upload` BIGINT DEFAULT 0,
                        `client_month_download` BIGINT DEFAULT 0,
                        `client_total_upload` BIGINT DEFAULT 0,
                        `client_total_download` BIGINT DEFAULT 0,

                        `original_client_id` BIGINT DEFAULT 0,
                        PRIMARY KEY(`server_id`, `client_database_id`)
                    );
                )"};
                constexpr static std::string_view kInsertClientsGlobal{"[INSERT_OR_IGNORE] INTO `clients_v2` (`client_database_id`, `client_unique_id`, `client_created`) SELECT `cldbid`, `clientUid`, `firstConnect` FROM `clients` WHERE `serverId` = 0;"};
                constexpr static std::string_view kInsertClientsServer{R"(
                    [INSERT_OR_IGNORE] INTO `clients_server` (`server_id`, `client_unique_id`, `client_database_id`, `client_nickname`, `client_created`, `client_last_connected`, `client_total_connections`)
	                    SELECT `serverId`, `clientUid`, `cldbid`, `lastName`, `firstConnect`, `lastConnect`, `connections` FROM `clients`;
                )"};

                constexpr static std::array<std::string_view, 11> kUpdateCommands{
                        kCreateClientsV2,
                        kCreateClientsServer,
                        kInsertClientsGlobal,
                        kInsertClientsServer,
                        "CREATE INDEX `idx_clients_unique_id` ON `clients_v2` (`client_unique_id`);",
                        "CREATE INDEX `idx_clients_database_id` ON `clients_v2` (`client_database_id`);",
                        "CREATE INDEX `idx_clients_login` ON `clients_v2` (`client_login_name`);",

                        "CREATE INDEX `idx_clients_server_server_unique` ON `clients_server` (`server_id`, `client_unique_id`);",
                        "CREATE INDEX `idx_clients_server_server_database` ON `clients_server` (`server_id`, `client_database_id`);",

                        "DROP TABLE `clients`;",
                        "ALTER TABLE `clients_v2` RENAME TO `clients`;",
                };

                if(!execute_commands(this->sql(), error, kUpdateCommands))
                    return false;

                db_version(13);
            }

            case 13: {
                constexpr static std::array<std::string_view, 8> kUpdateCommands{
                    "CREATE INDEX `idx_properties_serverid_id_value` ON `properties` (`serverId`, `id`, `key`);",
                    "CREATE INDEX `idx_properties_serverid_id_type` ON `properties` (`serverId`, `id`, `type`);",

                    "CREATE TABLE `groups_v2` (`serverId` INT NOT NULL, `groupId` INTEGER NOT NULL PRIMARY KEY [AUTO_INCREMENT], `target` INT, `type` INT, `displayName` VARCHAR(128), `original_group_id` INTEGER);",
                    "INSERT INTO `groups_v2` (`serverId`, `groupId`, `target`, `type`, `displayName`) SELECT `serverId`, `groupId`, `target`, `type`, `displayName` FROM `groups`;",
                    "DROP TABLE `groups`;",
                    "ALTER TABLE `groups_v2` RENAME TO groups;",

                    "CREATE INDEX `idx_groups_serverId_groupId` ON `groups` (`serverId`);",
                    "CREATE INDEX `idx_groups_serverid` ON `groups` (`serverId`, `groupId`);"
                };

                if(!execute_commands(this->sql(), error, kUpdateCommands))
                    return false;

                db_version(14);
            }
            case 14: {
                constexpr static std::array<std::string_view, 5> kUpdateCommands{
                        "CREATE TABLE general_dg_tmp(`id` INTEGER NOT NULL PRIMARY KEY [AUTO_INCREMENT], `key` VARCHAR(256), `value` TEXT);",
                        "INSERT INTO general_dg_tmp(`key`, `value`) SELECT `key`, `value` FROM `general`;",
                        "DROP TABLE `general`;",
                        "ALTER TABLE `general_dg_tmp` RENAME TO `general`;",
                        "CREATE UNIQUE INDEX `general_id_uindex` ON `general` (`id`);"
                };

                if(!execute_commands(this->sql(), error, kUpdateCommands))
                    return false;

                db_version(15);
            }

            case 15: {
                constexpr static std::array<std::string_view, 1> kUpdateCommands{
                        "UPDATE `properties` SET `key` = 'channel_conversation_mode' WHERE `key` = 'channel_flag_conversation_private';",
                };

                if(!execute_commands(this->sql(), error, kUpdateCommands))
                    return false;

                db_version(16);
            }

            case 16: {
                constexpr static std::array<std::string_view, 1> kUpdateCommands{
                        "UPDATE `properties` SET `value` = '4' WHERE `key` = 'channel_codec' AND (`value` != '4' OR `value` != '5');",
                };

                if(!execute_commands(this->sql(), error, kUpdateCommands))
                    return false;

                db_version(17);
            }

            case 17: {
                constexpr static std::array<std::string_view, 11> kUpdateCommands{
                        "CREATE TABLE tokens_new("
                        "  `token_id` INTEGER NOT NULL PRIMARY KEY [AUTO_INCREMENT],"
                        "  `server_id` INT,"
                        "  `token` VARCHAR(32),"
                        "  `description` VARCHAR(255),"
                        "  `issuer_database_id` BIGINT,"
                        "  `max_uses` INT,"
                        "  `use_count` INT DEFAULT 0,"
                        "  `timestamp_created` INT,"
                        "  `timestamp_expired` INT"
                        ");",

                        "CREATE TABLE token_actions("
                        "  `action_id` INTEGER NOT NULL PRIMARY KEY [AUTO_INCREMENT],"
                        "  `server_id` INTEGER NOT NULL,"
                        "  `token_id` INTEGER NOT NULL,"
                        "  `type` INTEGER,"
                        "  `id1` BIGINT,"
                        "  `id2` BIGINT,"
                        "  `text` TEXT"
                        ");",

                        "INSERT INTO `tokens_new`(`server_id`, `token`, `description`, `issuer_database_id`, `max_uses`, `timestamp_created`, `timestamp_expired`)"
                        "  SELECT `serverId`, `token`, `description`, 0, 1, `created`, 0 FROM `tokens`;",

                        "INSERT INTO `token_actions`(`server_id`, `token_id`, `type`, `id1`, `id2`, `text`)"
                        "  SELECT `serverId`, `token_id`, 1, `targetGroup`, 0, \"\" FROM `tokens` LEFT JOIN `tokens_new` ON `tokens_new`.`token` = `tokens`.`token` WHERE `type` = 0;",

                        "INSERT INTO `token_actions`(`server_id`, `token_id`, `type`, `id1`, `id2`, `text`)"
                        "  SELECT `serverId`, `token_id`, 3, `targetGroup`, `targetChannel`, \"\" FROM `tokens` LEFT JOIN `tokens_new` ON `tokens_new`.`token` = `tokens`.`token` WHERE `type` = 1;",

                        "DROP TABLE `tokens`;",
                        "ALTER TABLE `tokens_new` RENAME TO `tokens`;",

                        "CREATE UNIQUE INDEX `tokens_token` ON `tokens` (`token`);",
                        "CREATE INDEX `tokens_client` ON `tokens` (`issuer_database_id`);",
                        "CREATE INDEX `tokens_server` ON `tokens` (`server_id`);",
                        "CREATE INDEX `token_actions_token_id` ON `token_actions` (`token_id`);",
                };

                if(!execute_commands(this->sql(), error, kUpdateCommands))
                    return false;

                db_version(18);
            }

            default:
                break;
        }

        auto timestamp_end = system_clock::now();
        logMessage(LOG_GENERAL, "Database upgrade took {}ms", duration_cast<milliseconds>(timestamp_end - timestamp_start).count());
    }

    return true;
}

bool SqlDataManager::update_permissions(std::string &error) {
    if(this->_permissions_version != CURRENT_PERMISSION_VERSION) {
        sql::result result;
        auto timestamp_start = system_clock::now();

        logMessage(LOG_GENERAL, "Upgrading permissions from version " + to_string(this->_permissions_version) + " to " + to_string(CURRENT_PERMISSION_VERSION) + ". This could take a moment!");

        const auto auto_update = [&](permission::update::GroupUpdateType update_type, const std::string& permission, permission::v2::PermissionFlaggedValue value, bool skip, bool negate, permission::v2::PermissionFlaggedValue granted) {
            /*
            INSERT [OR IGNORE | IGNORE] INTO `perms` (serverId, type, id, channelId, permId, value, grant, flag_skip, flag_negate)
                SELECT DISTINCT `permissions`.`serverId`, 0, `groupId`, 0, :name, :value, :grant, :skip, :negate FROM groups
                    INNER JOIN `permissions`
                        ON permissions.permId = 'i_group_auto_update_type' AND permissions.channelId = 0 AND permissions.id = groups.groupId AND permissions.serverId = groups.serverId AND permissions.value = :update_type;
            */

            std::string query = "INSERT ";
            if(this->sql()->getType() == sql::TYPE_MYSQL)
                query += "IGNORE ";
            else
                query += "OR IGNORE ";
            query += "INTO `permissions` (`serverId`, `type`, `id`, `channelId`, `permId`, `value`, `grant`, `flag_skip`, `flag_negate`) ";
            query += string() + "SELECT DISTINCT `permissions`.`serverId`, `permissions`.`type`, `groupId`, `permissions`.`channelId`, "
                    + "'" + permission + "', "
                    + to_string(value.has_value ? value.value : -2) + ", "
                    + to_string(granted.has_value ? granted.value : -2) + ", "
                    + to_string(skip) + ", "
                    + to_string(negate) + " FROM groups ";
            query += "INNER JOIN `permissions` ";
            query += "ON permissions.permId = 'i_group_auto_update_type' AND permissions.id = groups.groupId AND permissions.serverId = groups.serverId AND permissions.value = " + to_string(update_type);

            logTrace(LOG_GENERAL, "Executing sql update: {}", query);
            auto result = sql::command(this->sql(), query).execute();
            if(!result) {
                error = "failed to auto update permission " + permission + " for type " + to_string(update_type) + ": " + result.fmtStr();
                return false;
            }

            return true;
        };

        switch (this->_permissions_version) {
            case -1:
                /* initial setup, or first introduce of 1.4.0. Default stuff will be loaded from the template file so we only run updates here */
                if(!auto_update(permission::update::QUERY_ADMIN, "b_client_is_priority_speaker", {-2, false}, false, false, {100, true}))
                    return false;
                if(!auto_update(permission::update::QUERY_ADMIN, "b_virtualserver_modify_country_code", {1, true}, false, false, {100, true}))
                    return false;
                if(!auto_update(permission::update::QUERY_ADMIN, "b_channel_ignore_subscribe_power", {1, true}, false, false, {100, true}))
                    return false;
                if(!auto_update(permission::update::QUERY_ADMIN, "b_channel_ignore_description_view_power", {1, true}, false, false, {100, true}))
                    return false;
                if(!auto_update(permission::update::QUERY_ADMIN, "i_max_playlist_size", {1, false}, false, false, {100, true}))
                    return false;
                if(!auto_update(permission::update::QUERY_ADMIN, "i_max_playlists", {1, false}, false, false, {100, true}))
                    return false;
                if(!auto_update(permission::update::QUERY_ADMIN, "i_channel_create_modify_conversation_history_length", {1, false}, false, false, {100, true}))
                    return false;
                if(!auto_update(permission::update::QUERY_ADMIN, "b_channel_create_modify_conversation_history_unlimited", {1, true}, false, false, {100, true}))
                    return false;
                if(!auto_update(permission::update::QUERY_ADMIN, "b_channel_create_modify_conversation_private", {1, true}, false, false, {100, true}))
                    return false;
                if(!auto_update(permission::update::QUERY_ADMIN, "b_channel_conversation_message_delete", {1, true}, false, false, {100, true}))
                    return false;

                if(!auto_update(permission::update::SERVER_ADMIN, "b_client_is_priority_speaker", {-2, false}, false, false, {75, true}))
                    return false;
                if(!auto_update(permission::update::SERVER_ADMIN, "b_virtualserver_modify_country_code", {1, true}, false, false, {75, true}))
                    return false;
                if(!auto_update(permission::update::SERVER_ADMIN, "b_channel_ignore_subscribe_power", {1, true}, false, false, {75, true}))
                    return false;
                if(!auto_update(permission::update::SERVER_ADMIN, "b_channel_ignore_description_view_power", {1, true}, false, false, {75, true}))
                    return false;
                if(!auto_update(permission::update::SERVER_ADMIN, "i_max_playlist_size", {1, false}, false, false, {75, true}))
                    return false;
                if(!auto_update(permission::update::SERVER_ADMIN, "i_max_playlists", {1, false}, false, false, {75, true}))
                    return false;
                if(!auto_update(permission::update::SERVER_ADMIN, "i_channel_create_modify_conversation_history_length", {1, false}, false, false, {75, true}))
                    return false;
                if(!auto_update(permission::update::SERVER_ADMIN, "b_channel_create_modify_conversation_history_unlimited", {1, true}, false, false, {75, true}))
                    return false;
                if(!auto_update(permission::update::SERVER_ADMIN, "b_channel_create_modify_conversation_private", {1, true}, false, false, {75, true}))
                    return false;
                if(!auto_update(permission::update::SERVER_ADMIN, "b_channel_conversation_message_delete", {1, true}, false, false, {75, true}))
                    return false;

                if(!auto_update(permission::update::SERVER_NORMAL, "i_max_playlist_size", {50, true}, false, false, {-2, false}))
                    return false;
                if(!auto_update(permission::update::SERVER_NORMAL, "i_channel_create_modify_conversation_history_length", {15000, true}, false, false, {-2, false}))
                    return false;

                if(!auto_update(permission::update::SERVER_GUEST, "i_max_playlist_size", {10, true}, false, false, {-2, false}))
                    return false;

                perm_version(0);

            case 0:
                result = sql::command(this->sql(), "DELETE FROM `permissions` WHERE `permId` = :permid", variable{":permid", "b_client_music_create"}).execute();
                if(!result) {
                    LOG_SQL_CMD(result);
                    return false;
                }
                result = sql::command(this->sql(), "DELETE FROM `permissions` WHERE `permId` = :permid", variable{":permid", "b_client_music_delete_own"}).execute();
                if(!result) {
                    LOG_SQL_CMD(result);
                    return false;
                }
                perm_version(1);

            case 1:
                if(!auto_update(permission::update::SERVER_ADMIN, "i_playlist_song_move_power", {75, true}, false, false, {75, true}))
                    return false;
                if(!auto_update(permission::update::QUERY_ADMIN, "i_playlist_song_move_power", {100, true}, false, false, {100, true}))
                    return false;

                if(!auto_update(permission::update::SERVER_ADMIN, "i_playlist_song_needed_move_power", {0, false}, false, false, {75, true}))
                    return false;
                if(!auto_update(permission::update::QUERY_ADMIN, "i_playlist_song_needed_move_power", {0, false}, false, false, {100, true}))
                    return false;


                if(!auto_update(permission::update::SERVER_ADMIN, "b_channel_conversation_message_delete", {1, true}, false, false, {75, true}))
                    return false;
                if(!auto_update(permission::update::QUERY_ADMIN, "b_channel_conversation_message_delete", {1, true}, false, false, {100, true}))
                    return false;
                perm_version(2);

            case 2:
                if(!auto_update(permission::update::SERVER_ADMIN, "b_client_query_create_own", {1, true}, false, false, {75, true}))
                    return false;
                if(!auto_update(permission::update::QUERY_ADMIN, "b_client_query_create_own", {1, true}, false, false, {100, true}))
                    return false;

                /* for some reason some users haven't received these updates from last time */
                if(!auto_update(permission::update::SERVER_ADMIN, "i_playlist_song_move_power", {75, true}, false, false, {75, true}))
                    return false;
                if(!auto_update(permission::update::QUERY_ADMIN, "i_playlist_song_move_power", {100, true}, false, false, {100, true}))
                    return false;
                if(!auto_update(permission::update::SERVER_ADMIN, "i_playlist_song_needed_move_power", {0, false}, false, false, {75, true}))
                    return false;
                if(!auto_update(permission::update::QUERY_ADMIN, "i_playlist_song_needed_move_power", {0, false}, false, false, {100, true}))
                    return false;

                perm_version(3);

            case 3:
                if(!auto_update(permission::update::SERVER_ADMIN, "i_client_poke_max_clients", {20, true}, false, false, {75, true}))
                    return false;
                if(!auto_update(permission::update::QUERY_ADMIN, "i_client_poke_max_clients", {50, true}, false, false, {100, true}))
                    return false;
                if(!auto_update(permission::update::SERVER_NORMAL, "i_client_poke_max_clients", {5, true}, false, false, {0, false}))
                    return false;

                perm_version(4);

            case 4:
                if(!auto_update(permission::update::QUERY_ADMIN, "b_channel_create_modify_conversation_mode_private", {1, true}, false, false, {100, true}))
                    return false;
                if(!auto_update(permission::update::QUERY_ADMIN, "b_channel_create_modify_conversation_mode_private", {1, true}, false, false, {75, true}))
                    return false;
                if(!auto_update(permission::update::CHANNEL_ADMIN, "b_channel_create_modify_conversation_mode_private", {1, true}, false, false, {0, false}))
                    return false;

                if(!auto_update(permission::update::QUERY_ADMIN, "b_channel_create_modify_conversation_mode_public", {1, true}, false, false, {100, true}))
                    return false;
                if(!auto_update(permission::update::QUERY_ADMIN, "b_channel_create_modify_conversation_mode_public", {1, true}, false, false, {75, true}))
                    return false;
                if(!auto_update(permission::update::CHANNEL_ADMIN, "b_channel_create_modify_conversation_mode_public", {1, true}, false, false, {0, false}))
                    return false;

                if(!auto_update(permission::update::QUERY_ADMIN, "b_channel_create_modify_conversation_mode_none", {1, true}, false, false, {100, true}))
                    return false;
                if(!auto_update(permission::update::QUERY_ADMIN, "b_channel_create_modify_conversation_mode_none", {1, true}, false, false, {75, true}))
                    return false;
                if(!auto_update(permission::update::CHANNEL_ADMIN, "b_channel_create_modify_conversation_mode_none", {1, true}, false, false, {0, false}))
                    return false;
                perm_version(5);

            case 5:
                if(!auto_update(permission::update::QUERY_ADMIN, "b_virtualserver_modify_maxchannels", {1, true}, false, false, {100, true}))
                    return false;

                if(!auto_update(permission::update::SERVER_ADMIN, "b_virtualserver_modify_maxchannels", {1, true}, false, false, {75, true}))
                    return false;

                if(!auto_update(permission::update::QUERY_ADMIN, "b_channel_create_modify_conversation_mode_private", {1, true}, false, false, {100, true}))
                    return false;

                if(!auto_update(permission::update::SERVER_ADMIN, "b_channel_create_modify_conversation_mode_private", {1, true}, false, false, {75, true}))
                    return false;

                if(!auto_update(permission::update::QUERY_ADMIN, "b_channel_create_modify_conversation_mode_public", {1, true}, false, false, {100, true}))
                    return false;

                if(!auto_update(permission::update::SERVER_ADMIN, "b_channel_create_modify_conversation_mode_public", {1, true}, false, false, {75, true}))
                    return false;

                if(!auto_update(permission::update::QUERY_ADMIN, "b_channel_create_modify_conversation_mode_none", {1, true}, false, false, {100, true}))
                    return false;

                if(!auto_update(permission::update::SERVER_ADMIN, "b_channel_create_modify_conversation_mode_none", {1, true}, false, false, {75, true}))
                    return false;

                if(!auto_update(permission::update::QUERY_ADMIN, "i_client_poke_max_clients", {-2, false}, false, false, {100, true}))
                    return false;

                if(!auto_update(permission::update::SERVER_ADMIN, "i_client_poke_max_clients", {50, true}, false, false, {75, true}))
                    return false;

                if(!auto_update(permission::update::QUERY_ADMIN, "b_client_query_create_own", {1, true}, false, false, {100, true}))
                    return false;

                if(!auto_update(permission::update::SERVER_ADMIN, "b_client_query_create_own", {1, true}, false, false, {75, true}))
                    return false;

                if(!auto_update(permission::update::QUERY_ADMIN, "i_playlist_song_move_power", {100, true}, false, false, {100, true}))
                    return false;

                if(!auto_update(permission::update::SERVER_ADMIN, "i_playlist_song_move_power", {75, true}, false, false, {75, true}))
                    return false;

                if(!auto_update(permission::update::QUERY_ADMIN, "i_playlist_song_needed_move_power", {100, true}, false, false, {100, true}))
                    return false;

                if(!auto_update(permission::update::SERVER_ADMIN, "i_playlist_song_needed_move_power", {75, true}, false, false, {75, true}))
                    return false;

                if(!auto_update(permission::update::QUERY_ADMIN, "i_ft_max_bandwidth_download", {-2, false}, false, false, {100, true}))
                    return false;

                if(!auto_update(permission::update::SERVER_ADMIN, "i_ft_max_bandwidth_download", {-2, false}, false, false, {75, true}))
                    return false;

                if(!auto_update(permission::update::QUERY_ADMIN, "i_ft_max_bandwidth_upload", {-2, false}, false, false, {100, true}))
                    return false;

                if(!auto_update(permission::update::SERVER_ADMIN, "i_ft_max_bandwidth_upload", {-2, false}, false, false, {75, true}))
                    return false;

                if(!auto_update(permission::update::QUERY_ADMIN, "b_channel_create_modify_sidebar_mode", {1, true}, false, false, {100, true}))
                    return false;

                if(!auto_update(permission::update::SERVER_ADMIN, "b_channel_create_modify_sidebar_mode", {1, true}, false, false, {75, true}))
                    return false;

                perm_version(6);

            case 6:

#define do_auto_update(type, name, value, granted) \
if(!auto_update(permission::update::type, name, {value, value != permNotGranted}, false, false, {granted, granted != permNotGranted})) \
    return false;

                /* Attention: Due to a mistake the "i_video_max_kbps" permission has bit and not kbit values. In version 7 we divide by 1000. */
                do_auto_update(QUERY_ADMIN, "b_video_screen", 1, 100);
                do_auto_update(QUERY_ADMIN, "b_video_camera", 1, 100);
                do_auto_update(QUERY_ADMIN, "i_video_max_kbps", 20 * 1000 * 1000, 100);
                do_auto_update(QUERY_ADMIN, "i_video_max_streams", permNotGranted, 100);
                do_auto_update(QUERY_ADMIN, "i_video_max_screen_streams", permNotGranted, 100);
                do_auto_update(QUERY_ADMIN, "i_video_max_camera_streams", permNotGranted, 100);

                do_auto_update(SERVER_ADMIN, "b_video_screen", 1, 75);
                do_auto_update(SERVER_ADMIN, "b_video_camera", 1, 75);
                do_auto_update(SERVER_ADMIN, "i_video_max_kbps", 20 * 1000 * 1000, 75);
                do_auto_update(SERVER_ADMIN, "i_video_max_streams", permNotGranted, 75);
                do_auto_update(SERVER_ADMIN, "i_video_max_screen_streams", permNotGranted, 75);
                do_auto_update(SERVER_ADMIN, "i_video_max_camera_streams", permNotGranted, 75);

                do_auto_update(SERVER_NORMAL, "b_video_screen", 1, permNotGranted);
                do_auto_update(SERVER_NORMAL, "b_video_camera", 1, permNotGranted);
                do_auto_update(SERVER_NORMAL, "i_video_max_kbps", 5 * 1000 * 1000, permNotGranted);
                do_auto_update(SERVER_NORMAL, "i_video_max_streams", 20, permNotGranted);
                do_auto_update(SERVER_NORMAL, "i_video_max_screen_streams", 2, permNotGranted);
                do_auto_update(SERVER_NORMAL, "i_video_max_camera_streams", 20, permNotGranted);

                do_auto_update(SERVER_GUEST, "b_video_screen", 1, permNotGranted);
                do_auto_update(SERVER_GUEST, "b_video_camera", 1, permNotGranted);
                do_auto_update(SERVER_GUEST, "i_video_max_kbps", 2500 * 1000, permNotGranted);
                do_auto_update(SERVER_GUEST, "i_video_max_streams", 8, permNotGranted);
                do_auto_update(SERVER_GUEST, "i_video_max_screen_streams", 1, permNotGranted);
                do_auto_update(SERVER_GUEST, "i_video_max_camera_streams", 8, permNotGranted);

                perm_version(7);

            case 7:
                result = sql::command(this->sql(), "UPDATE `permissions` SET `value` = `value` / 1000 WHERE permId = 'i_video_max_kbps'").execute();
                if(!result) {
                    LOG_SQL_CMD(result);
                    return false;
                }

                perm_version(8);

            case 8:
                do_auto_update(QUERY_ADMIN, "b_virtualserver_token_list_all", 1, 100);
                do_auto_update(SERVER_ADMIN, "b_virtualserver_token_list_all", 1, 75);

                do_auto_update(QUERY_ADMIN, "b_virtualserver_token_edit_all", 1, 100);
                do_auto_update(SERVER_ADMIN, "b_virtualserver_token_edit_all", 1, 75);

                do_auto_update(QUERY_ADMIN, "b_virtualserver_token_delete_all", 1, 100);
                do_auto_update(SERVER_ADMIN, "b_virtualserver_token_delete_all", 1, 75);

                do_auto_update(QUERY_ADMIN, "i_virtualserver_token_limit", -1, 100);
                do_auto_update(SERVER_ADMIN, "i_virtualserver_token_limit", -1, 75);
                do_auto_update(SERVER_NORMAL, "i_virtualserver_token_limit", 100, permNotGranted);
                do_auto_update(SERVER_GUEST, "i_virtualserver_token_limit", 50, permNotGranted);

                perm_version(9);
            default:
                break;
        }

        auto timestamp_end = system_clock::now();
        logMessage(LOG_GENERAL, "Permission upgrade took {}ms", duration_cast<milliseconds>(timestamp_end - timestamp_start).count());
    }
    return true;
}

void SqlDataManager::finalize() {
    if(this->manager) this->manager->disconnect();
    delete this->manager;
    this->manager = nullptr;
}

bool SqlDataManager::detect_versions() {
    auto result = sql::command(this->manager, "SELECT `value` FROM `general` WHERE `key`= :key", variable{":key", "data_version"}).query([&](int length, char** values, char**){
        this->_database_version = atoi(values[0]);
        this->database_version_present = true;
        return 0;
    });

    result = sql::command(this->manager, "SELECT `value` FROM `general` WHERE `key`= :key", variable{":key", "permissions_version"}).query([&](int length, char** values, char**){
        this->_permissions_version = atoi(values[0]);
        this->permissions_version_present = true;
        return 0;
    });

    return true;
}

bool SqlDataManager::change_database_version(int version) {
    string command;
    if(this->database_version_present)
        command = "UPDATE `general` SET `value`= :version WHERE `key`= :key;";
    else
        command = "INSERT INTO `general` (`key`, `value`) VALUES (:key, :version);";

    auto result = sql::command(this->manager, command, variable{":version", version}, variable{":key", "data_version"}).execute();
    if(!result) {
        logError(LOG_INSTANCE, "Could not update SQL database version. (" + result.fmtStr() + ")");
        return false;
    }
    this->database_version_present = true;
    this->_database_version = version;

    return true;
}

bool SqlDataManager::change_permission_version(int version) {
    string command;
    if(this->permissions_version_present)
        command = "UPDATE `general` SET `value`= :version WHERE `key`= :key;";
    else
        command = "INSERT INTO `general` (`key`, `value`) VALUES (:key, :version);";

    auto result = sql::command(this->manager, command, variable{":version", version}, variable{":key", "permissions_version"}).execute();
    if(!result) {
        logError(LOG_INSTANCE, "Could not update SQL permissions version. (" + result.fmtStr() + ")");
        return false;
    }
    this->permissions_version_present = true;
    this->_permissions_version = version;

    return true;
}