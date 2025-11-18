#include "DatabaseHandler.h"
#include <misc/base64.h>
#include <log/LogUtils.h>

using namespace std;
using namespace std::chrono;
using namespace license;
using namespace license::server::database;
using namespace sql;

DatabaseHandler::DatabaseHandler(sql::SqlManager* database) : database(database){
    this->id_cache = make_shared<KeyIdCache>(this);
}
DatabaseHandler::~DatabaseHandler() { }

/*
        LicenseType type;
	    std::string username;
	    std::string first_name;
        std::string last_name;
        std::string email;
        std::chrono::system_clock::time_point start;
	    std::chrono::system_clock::time_point end;
	    std::chrono::system_clock::time_point creation;
 */

#define CTBL(cmd)                                                   \
res = command(this->database, cmd).execute();                       \
	if(!res) {                                                      \
	error = "Could not setup tables! (" + res.fmtStr() + ")";       \
	return false;                                                   \
}
#define CIDX(cmd)                                                   \
res = command(this->database, cmd).execute();                       \
	if(!res && res.msg().find("Duplicate") == string::npos && res.msg().find("exist") == string::npos) {       \
	error = "Could not setup indexes! (" + res.fmtStr() + ")";      \
	return false;                                                   \
}

#define SET_VERSION(ver) \
version = ver; \
sql::command(this->database, "UPDATE `general` SET `value` = :version WHERE `key` = 'version'", variable{":version", version}).execute();

bool DatabaseHandler::setup(std::string& error) {
	result res;
	int version = -1;
	sql::command(this->database, "SELECT `value` FROM `general` WHERE `key` = 'version'").query([](int* version, int lnegth, string* values, string* names) {
	    *version = stoll(values[0]);
	    return 0;
	}, &version);


	switch (version) {
	    case -1:
            CTBL("CREATE TABLE IF NOT EXISTS `license` (`key` BINARY(64) NOT NULL PRIMARY KEY, `type` INT, `deleted` BOOL, `issuer` TEXT)");
            CTBL("CREATE TABLE IF NOT EXISTS `license_info` (`key` BINARY(64) NOT NULL PRIMARY KEY, `username` VARCHAR(128), `first_name` TEXT, `last_name` TEXT, `email` TEXT, `begin` BIGINT, `end` BIGINT, `generated` BIGINT)");
            CTBL("CREATE TABLE IF NOT EXISTS `license_request` (`key` BINARY(64) NOT NULL, `ip` TEXT, `timestamp` BIGINT, `result` INT)");
            CTBL("CREATE TABLE IF NOT EXISTS `general` (`key` VARCHAR(64), `value` TEXT)");
            CTBL("INSERT INTO `general` (`key`, `value`) VALUES ('version', '0');");

	        SET_VERSION(0);
	    case 0:
	        logMessage(LOG_GENERAL, "Updating database! To version 1");
            CTBL("CREATE TABLE IF NOT EXISTS `history_speach` (`keyId` INT, `timestamp` BIGINT, `total` BIGINT, `dead` BIGINT, `online` BIGINT, `varianz` BIGINT)");
            CTBL("CREATE TABLE IF NOT EXISTS `history_online` (`keyId` INT, `timestamp` BIGINT, `server` INT, `clients` INT, `web` INT, `music` INT, `queries` INT)");

            CIDX("CREATE INDEX `license_key` ON `license` (`key`)");
            CIDX("CREATE INDEX `license_info_key` ON `license_info` (`key`)");

	        CTBL("ALTER TABLE `license` DROP PRIMARY KEY, ADD `keyId` INTEGER NOT NULL AUTO_INCREMENT PRIMARY KEY FIRST;");
            CTBL("ALTER TABLE `license_info` DROP PRIMARY KEY, ADD `keyId` INT NOT NULL PRIMARY KEY AUTO_INCREMENT FIRST;");
            //Fixing key id's
            CTBL("UPDATE `license_info` LEFT JOIN `license` ON `license`.`key` = `license_info`.`key` SET `license_info`.`keyId` = `license`.`keyId`");
            //Deleting the key blob and removing auto
            CTBL("ALTER TABLE `license_info` CHANGE `keyId` `keyId` INT NOT NULL UNIQUE, DROP COLUMN `key`");

            //Fixing request table
            CTBL("ALTER TABLE license_request ADD COLUMN `keyId` INT(0) NOT NULL;");
            CTBL("UPDATE license_request LEFT JOIN license ON license.key = license_request.key SET license_request.keyId = license.keyId;");
            CTBL("ALTER TABLE license_request DROP COLUMN `key`;");

            //IDK why but i failed stuff :D
            CTBL("ALTER TABLE license DROP PRIMARY KEY, CHANGE `keyId` `keyId` INT NOT NULL AUTO_INCREMENT PRIMARY KEY");
            SET_VERSION(1);

		case 1:
			CTBL("ALTER TABLE history_online ADD COLUMN `ip` TEXT AFTER `keyId`;");
			CTBL("ALTER TABLE history_speach ADD COLUMN `ip` TEXT AFTER `keyId`;");
			SET_VERSION(2);

		case 2:
			CTBL("CREATE TABLE `history_version` (`keyId` INT, `ip` VARCHAR(64), `timestamp` BIGINT, `version` VARCHAR(126));");
			SET_VERSION(3);

		case 3:
			/*
			CTBL("CREATE TABLE `save_history_online` AS SELECT * FROM `history_online`;");
			CTBL("CREATE TABLE `save_history_speach` AS SELECT * FROM `history_speach`;");
			CTBL("CREATE TABLE `save_history_version` AS SELECT * FROM `history_version`;");
			*/

			CTBL("ALTER TABLE `history_online` CHANGE `ip` `unique_id` VARCHAR(64) NOT NULL;");
			CTBL("ALTER TABLE `history_speach` CHANGE `ip` `unique_id` VARCHAR(64) NOT NULL;");
			CTBL("ALTER TABLE `history_version` CHANGE `ip` `unique_id` VARCHAR(64) NOT NULL;");
			CTBL("ALTER TABLE `license_request` ADD `unique_id` VARCHAR(64) NOT NULL AFTER `ip`;");
			SET_VERSION(4);

		case 4:
			CTBL("CREATE TABLE IF NOT EXISTS `users` (`username` VARCHAR(64) NOT NULL PRIMARY KEY, `password_hash` VARCHAR(128), `status` INT, `owner` VARCHAR(64))");
            SET_VERSION(5);

	    case 5:
            CTBL("CREATE TABLE `license_upgrades` (`upgrade_id` INT PRIMARY KEY NOT NULL, `old_key_id` INT, `new_key_id` INT, `timestamp_begin` BIGINT, `timestamp_end` BIGINT, `valid` INT, `use_count` INT, `license` BLOB);");
            CTBL("ALTER TABLE `license_upgrades` ADD INDEX(`old_key_id`)");
            CTBL("ALTER TABLE `license` ADD INDEX(`keyId`);");
            CTBL("ALTER TABLE `license` ADD COLUMN `upgrade_id` INT DEFAULT 0;");
            SET_VERSION(6);

	    case 6:
	        CTBL("CREATE TABLE license_upgrade_log (`upgrade_id` INT, `timestamp` INT, `unique_id` VARCHAR(64), `server_ip` VARCHAR(32), `succeeded` TINYINT);");
            CIDX("CREATE INDEX `upgrade_id_timestamp` ON `license_upgrade_log` (`upgrade_id`, `timestamp`)");
            SET_VERSION(7);

        default:;
    }
	return true;
}

bool DatabaseHandler::register_license(const std::string& key, const shared_ptr<LicenseInfo>& info, const std::string& issuer) {
	result res;
	res = command(this->database, "INSERT INTO `license` (`key`, `type`, `deleted`, `issuer`) VALUES (:key, :type, :deleted, :issuer)", variable{":key", key}, variable{":type", (uint32_t) info->type}, variable{":deleted", false}, variable{":issuer", issuer}).execute();
	if(!res) {
		logError(LOG_GENERAL, "Could not register new license (" + res.fmtStr() + ")");
		return false;
	}
	auto keyId = this->id_cache->get_key_id_from_key(key);
	if(keyId == 0) return false;

	res = command(this->database, "INSERT INTO `license_info` (`keyId`, `username`, `first_name`, `last_name`, `email`, `begin`, `end`, `generated`) VALUES (:key, :username, :first_name, :last_name, :email, :begin, :end, :generated)",
	              variable{":key", keyId},
	              variable{":username", info->username},
	              variable{":first_name", info->first_name},
	              variable{":last_name", info->last_name},
	              variable{":email", info->email},
	              variable{":generated", duration_cast<milliseconds>(info->creation.time_since_epoch()).count()},
	              variable{":begin", duration_cast<milliseconds>(info->start.time_since_epoch()).count()},
	              variable{":end", duration_cast<milliseconds>(info->end.time_since_epoch()).count()}
	).execute();
	if(!res) {
		logError(LOG_GENERAL, "Could not register new license info (" + res.fmtStr() + ")");
		return false;
	}
	return true;
}

bool DatabaseHandler::delete_license(const std::string& key, bool full) {
    if(full) {
        auto keyId = this->id_cache->get_key_id_from_key(key);
        if(keyId == 0) return false; //Never exists

        auto res = command(this->database, "DELETE FROM `license` WHERE `key` = :key", variable{":key", key}).execute();
        if(!res) logError(LOG_GENERAL, "Could not delete license (" + res.fmtStr() + ")");
        res = command(this->database, "DELETE FROM `license_info` WHERE `keyId` = :key", variable{":keyId", keyId}).execute();
        if(!res) logError(LOG_GENERAL, "Could not delete license (" + res.fmtStr() + ")");
        return !!res;
    } else {
        auto res = command(this->database, "UPDATE `license` SET `deleted` = :true WHERE `key` = :key", variable{":true", true}, variable{":key", key}).execute();
        if(!res) logError(LOG_GENERAL, "Could not delete license (" + res.fmtStr() + ")");
        return !!res;
    }
}

bool DatabaseHandler::validLicenseKey(const std::string& key) {
	bool valid = false;
	auto res = command(this->database, "SELECT * FROM `license` WHERE `key` = :key AND `deleted` = :false LIMIT 1", variable{":false", false}, variable{":key", key}).query([](bool* flag, int, char**, char**) {
		*flag = true;
		return 0;
	}, &valid);
	if(!res) logError(LOG_GENERAL, "Could not validate license (" + res.fmtStr() + ")");
	return !!res && valid;
}

inline std::map<std::string, std::shared_ptr<LicenseInfo>> query_license(SqlManager* mgr, const std::string& key, int offset, int length) {
    std::map<std::string, std::shared_ptr<LicenseInfo>> result;

    auto query = string() + "SELECT `key`, `username`, `first_name`, `last_name`, `email`, `begin`, `end`, `generated`, `deleted`, `upgrade_id` FROM `license_info` INNER JOIN `license` ON `license_info`.`keyId` = `license`.`keyId`";
    if(!key.empty())
	    query += "WHERE `key` = :key";
    else
    	query += "WHERE `deleted` = :false";

    if(offset > 0 || length > 0)
        query += " LIMIT " + to_string(offset) + ", " + to_string(length);

    logTrace(LOG_GENERAL, "Executing query {}", query);
    auto res = command(mgr, query, variable{":key", key}, variable{":false", false}).query([](std::map<std::string, std::shared_ptr<LicenseInfo>>* list, int length, std::string* values, std::string* names) {
        auto info = make_shared<LicenseInfo>();
	    info->deleted = false;
        string k;
        for(int index = 0; index < length; index++) {
            try {
                if(names[index] == "username")
                    info->username = values[index];
                else if(names[index] == "first_name")
                    info->first_name = values[index];
                else if(names[index] == "key")
                    k = values[index];
                else if(names[index] == "last_name")
                    info->last_name = values[index];
                else if(names[index] == "email")
                    info->email = values[index];
                else if(names[index] == "begin")
                    info->start = system_clock::time_point() + milliseconds(stoll(values[index]));
                else if(names[index] == "end")
                    info->end = system_clock::time_point() + milliseconds(stoll(values[index]));
                else if(names[index] == "generated")
                    info->creation = system_clock::time_point() + milliseconds(stoll(values[index]));
                else if(names[index] == "deleted")
	                info->deleted = values[index] == "1" || values[index] == "true";
                else if(names[index] == "upgrade_id")
                    info->upgrade_id = std::stol(values[index]);
                else
                    logError(LOG_GENERAL, "Unknown field {}", names[index]);
            } catch (std::exception& ex) {
                logError(LOG_GENERAL, "Failed to parse field {} ({}). Message: {}", names[index], values[index], ex.what());
                return 1;
            }
        }
        (*list)[k] = info;
        return 0;
    }, &result);
	logTrace(LOG_GENERAL, "Query returned {} results", result.size());
    if(!res) logError(LOG_GENERAL, "Could not query license (" + res.fmtStr() + ")");
    return result;
}

std::shared_ptr<LicenseInfo> DatabaseHandler::query_license_info(const std::string& key) {
    auto result = query_license(this->database, key, 0, 0);
    if(result.empty()) return nullptr;
    return result.begin()->second;
}

std::map<std::string, std::shared_ptr<LicenseInfo>> DatabaseHandler::list_licenses(int offset, int limit) {
	return query_license(this->database, "", offset, limit);
}

bool DatabaseHandler::logRequest(const std::string& key, const std::string& unique_id, const std::string& ip, const std::string& version, int state) {
    result res;

    auto keyId = this->id_cache->get_key_id_from_key(key);
    if(keyId == 0) {
		logError(LOG_GENERAL, "Failed to log license request (could not resolve key id)");
	    return false;
    }

    auto timestamp = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    //SELECT * FROM `license_info` WHERE `keyId` IN (SELECT `keyId` FROM `license` WHERE `key` = '000')
    res = command(this->database, "INSERT INTO `license_request` (`keyId`, `ip`, `unique_id`, `timestamp`, `result`) VALUES (:key, :ip, :unique_id, :timestamp, :result)",
            variable{":key", keyId},
                  variable{":ip", ip},
                  variable{":timestamp", timestamp},
                  variable{":unique_id", unique_id},
                  variable{":result", state}).execute();
    if(!res) {
        logError(LOG_GENERAL, "Could not log license validation (" + res.fmtStr() + ")");
        return false;
    }

	{ //Log version
		res = command(this->database, "INSERT INTO `history_version`(`keyId`, `unique_id`, `timestamp`, `version`) VALUES (:key, :unique_id, :time, :version)",
		              variable{":key", keyId},
		              variable{":time", timestamp},
		              variable{":unique_id", unique_id},
		              variable{":version", version}
		).execute();
		if(!res)
			logError(LOG_GENERAL, "Could not log license version statistic (" + res.fmtStr() + ")");
		res = {};
	}
    return true;
}

bool DatabaseHandler::logStatistic(const std::string &key, const std::string& unique_id, const std::string &ip,
                                   const ts::proto::license::PropertyUpdateRequest &data) {
	result res;

	auto keyId = this->id_cache->get_key_id_from_key(key);
	if(keyId == 0) return false;

	auto time = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
	{ //Log online
		res = command(this->database, "INSERT INTO `history_online`(`keyId`, `unique_id`, `timestamp`, `server`, `clients`, `web`, `music`, `queries`) VALUES (:key, :unique_id, :time, :server, :client, :web, :music, :query)",
		              variable{":key", keyId},
		              variable{":unique_id", unique_id},
		              variable{":time", time},
		              variable{":server", data.servers_online()},
		              variable{":client", data.clients_online()},
		              variable{":web", data.web_clients_online()},
		              variable{":music", data.bots_online()},
		              variable{":query", data.queries_online()}
				).execute();
		if(!res)
			logError(LOG_GENERAL, "Could not log license statistics (" + res.fmtStr() + ")");
		res = {};
	}
	//SELECT * FROM `license_info` WHERE `keyId` IN (SELECT `keyId` FROM `license` WHERE `key` = '000')
	{
		res = command(this->database, "INSERT INTO `history_speach`(`keyId`, `unique_id`, `timestamp`, `total`, `dead`, `online`, `varianz`) VALUES (:key, :unique_id, :time, :total, :dead, :online, :varianz)",
		              variable{":key", keyId},
		              variable{":unique_id", unique_id},
		              variable{":time", time},
		              variable{":total", data.speach_total()},
		              variable{":dead", data.speach_dead()},
		              variable{":online", data.speach_online()},
		              variable{":varianz", data.speach_varianz()}
	              ).execute();
		if(!res)
			logError(LOG_GENERAL, "Could not log license statistics (" + res.fmtStr() + ")");
		res = {};
	}
    return true;
}

std::shared_ptr<DatabaseHandler::UserHistory> DatabaseHandler::list_statistics_user(const system_clock::time_point &begin, const system_clock::time_point &end, const milliseconds &interval) {
	auto timeout = hours(2) + minutes(10); //TODO timeout configurable?

	/* initialize the result */
	auto allocated_records = (size_t) floor((end - begin) / interval) + 1;
	auto _result = (UserHistory*) malloc(sizeof(UserHistory) + sizeof(GlobalUserStatistics) * allocated_records);
	new (_result) UserHistory();

	auto result = shared_ptr<UserHistory>(_result, [](UserHistory* ptr){
		ptr->~UserHistory();
		free(ptr);
	});


	result->record_count = 0;
	result->begin = begin;
	result->end = end;
	result->interval = interval;

	memset(&result->history[0], 0, allocated_records * sizeof(GlobalUserStatistics));
	auto info = &_result->history[0];

	/* temp db variables */
	map<std::string, DatabaseHandler::DatabaseUserStatistics> server_statistics;

	bool have_key, have_uid;
	DatabaseHandler::DatabaseUserStatistics temp_stats; /* temp struct for stats parsing */
	DatabaseHandler::DatabaseUserStatistics* stats_ptr; /* pointer to the target stats */
	std::chrono::system_clock::time_point current_timestamp = begin + interval; /* upper limit of the current interval */

	auto state = command(this->database, "SELECT * FROM `history_online` WHERE `timestamp` >= :timestamp_start AND `timestamp` <= :timestamp_end ORDER BY `timestamp` ASC",
			variable{":timestamp_start", duration_cast<milliseconds>(begin.time_since_epoch() - timeout).count()},
			variable{":timestamp_end", duration_cast<milliseconds>(end.time_since_epoch()).count()}
	).query([&](int columns, std::string* values, std::string* names) {
		/* find the user statistic ptr */
		{
			size_t key_id = 0;
			std::string unique_id;

			have_key = false;
			have_uid = false;
			for(int index = 0; index < columns; index++) {
                if(names[index] == "keyId") {
                    key_id = strtol(values[index].c_str(), nullptr, 10);
                    if(key_id == 0) return 0; /* invalid key id */

                    have_key = true;
                    if(have_key && have_uid) goto process_tag;
                } else if(names[index] == "unique_id") {
                    unique_id = values[index];
                    have_uid = true;

                    if(have_key && have_uid) goto process_tag;
                }
			}
            return 0; /* key or uid haven't been found */

			process_tag:
			stats_ptr = &server_statistics[to_string(key_id) + "_" + unique_id];
		}

		for(int index = 0; index < columns; index++) {
            if(names[index] == "timestamp")
                temp_stats.timestamp = system_clock::time_point() + milliseconds(stoll(values[index]));
            else if(names[index] == "server")
                temp_stats.servers_online = strtol(values[index].c_str(), nullptr, 10);
            else if(names[index] == "clients")
                temp_stats.clients_online = strtol(values[index].c_str(), nullptr, 10);
            else if(names[index] == "web")
                temp_stats.web_clients_online = strtol(values[index].c_str(), nullptr, 10);
            else if(names[index] == "music")
                temp_stats.bots_online = strtol(values[index].c_str(), nullptr, 10);
            else if(names[index] == "queries")
                temp_stats.queries_online = strtol(values[index].c_str(), nullptr, 10);
		}

		/* because the query could only be oldest to newest */
		while(temp_stats.timestamp > current_timestamp) {
			assert(_result->record_count < allocated_records); /* ensure we write to valid memory */

			auto min_timestamp = current_timestamp - timeout;
			for(auto& server : server_statistics) {
				auto& second = server.second;
				if(second.timestamp < min_timestamp || second.timestamp.time_since_epoch().count() == 0)
					continue; /* last server request is too old to be counted */

				info->instance_online++;
                info->instance_empty += second.web_clients_online == 0 && second.clients_online == 0;
				info->queries_online += second.queries_online;
				info->bots_online += second.bots_online;
				info->web_clients_online += second.web_clients_online;
				info->clients_online += second.clients_online;
				info->servers_online += second.servers_online;
			}

			info++;
			_result->record_count++;
			current_timestamp += interval; /* lets gather for the next interval */
		}

		/* write the "new" statistic */
		memcpy(stats_ptr, &temp_stats, sizeof(temp_stats));
		return 0;
	});

	if(!state) {
		logError(LOG_GENERAL, "Could not read license statistics (" + state.fmtStr() + ")");
		return {};
	}

	/* flush the last record */
	do {
		assert(_result->record_count < allocated_records); /* ensure we write to valid memory */

		auto min_timestamp = current_timestamp - timeout;
		for(auto& server : server_statistics) {
			auto& second = server.second;
			if(second.timestamp < min_timestamp)
				continue; /* last server request is too old to be counted */

			info->instance_online++;
            info->instance_empty += second.web_clients_online == 0 && second.clients_online == 0;
			info->queries_online += second.queries_online;
			info->bots_online += second.bots_online;
			info->web_clients_online += second.web_clients_online;
			info->clients_online += second.clients_online;
			info->servers_online += second.servers_online;
		}

		info++;
		_result->record_count++;
		current_timestamp += interval;
	} while(current_timestamp < end);

	return result;
}

std::deque<std::unique_ptr<DatabaseHandler::GlobalVersionsStatistic>> DatabaseHandler::list_statistics_version(const std::chrono::system_clock::time_point &begin, const std::chrono::system_clock::time_point &end, const std::chrono::milliseconds &interval) {
	map<std::string, deque<unique_ptr<DatabaseHandler::GlobalVersionsStatistic>>> server_statistics;
	auto timeout = hours(2) + minutes(10); //TODO timeout configurable?

	auto state = command(this->database, "SELECT * FROM `history_version` WHERE `timestamp` >= :timestamp_start AND `timestamp` <= :timestamp_end ORDER BY `timestamp` ASC",
	                     variable{":timestamp_start", duration_cast<milliseconds>(begin.time_since_epoch() - timeout).count()},
	                     variable{":timestamp_end", duration_cast<milliseconds>(end.time_since_epoch()).count()}
	).query([&server_statistics](int columns, std::string* values, std::string* names){
		size_t key_id = 0;
		std::string unique_id;
		auto info = make_unique<DatabaseHandler::GlobalVersionsStatistic>();
		for(int index = 0; index < columns; index++) {
			try {
				if(names[index] == "keyId")
					key_id = stoull(values[index]);
				else if(names[index] == "unique_id")
					unique_id = values[index];
				else if(names[index] == "timestamp")
					info->timestamp = system_clock::time_point() + milliseconds(stoll(values[index]));
				else if(names[index] == "version")
					info->versions[values[index]] = 1;
			} catch (std::exception& ex) {
				logError(LOG_GENERAL, "Failed to parse column " + names[index] + " => " + ex.what() + " (Value: " + values[index] + ")");
				return 0;
			}
		}
		if(key_id == 0) return 0;

		server_statistics[to_string(key_id) + "_" + unique_id].push_back(std::move(info));
		return 0;
	});

	if(!state) {
		logError(LOG_GENERAL, "Could not read license statistics (" + state.fmtStr() + ")");
		return {};
	}

	std::deque<std::unique_ptr<DatabaseHandler::GlobalVersionsStatistic>> result;
	system_clock::time_point current_timestamp = begin;
	while(current_timestamp <= end) {
		auto info = make_unique<DatabaseHandler::GlobalVersionsStatistic>();
		info->timestamp = current_timestamp;

		for(auto& server : server_statistics) {
			while(!server.second.empty()) {
				auto& first = *server.second.begin();
				if(first->timestamp > current_timestamp) break; //Entry within the future
				if(first->timestamp + timeout < current_timestamp) { //Entry within the past
					server.second.pop_front();
					continue;
				}
				if(server.second.size() > 1) {
					auto& second = *(server.second.begin() + 1);
					if(second->timestamp <= current_timestamp) {
						server.second.pop_front(); //The next entry is more up 2 date
						continue;
					}
				}

				for(const auto& entry : first->versions)
					info->versions[entry.first] += entry.second;

				break;
			}
		}

		result.push_back(std::move(info));
		current_timestamp += interval;
	}

	return result;
}

bool DatabaseHandler::register_license_upgrade(license_key_id_t old_key_id, license_key_id_t new_key_id,
        const std::chrono::system_clock::time_point &begin_timestamp, const std::chrono::system_clock::time_point &end_timestamp, const std::string &license_key) {
    auto upgrade_id = std::chrono::ceil<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    auto sql_result = sql::command(this->sql(), "INSERT INTO `license_upgrades` (`upgrade_id`, `old_key_id`, `new_key_id`, `timestamp_begin`, `timestamp_end`, `valid`, `use_count`, `license`) VALUES"
                                                "(:upgrade_id, :old_key_id, :new_key_id, :timestamp_begin, :timestamp_end, 1, 0, :license)",
                                                variable{":upgrade_id", upgrade_id},
                                                variable{":old_key_id", old_key_id},
                                                variable{":new_key_id", new_key_id},
                                                variable{":timestamp_begin", std::chrono::duration_cast<std::chrono::milliseconds>(begin_timestamp.time_since_epoch()).count()},
                                                variable{":timestamp_end", std::chrono::duration_cast<std::chrono::milliseconds>(end_timestamp.time_since_epoch()).count()},
                                                variable{":license", base64::decode(license_key)}).execute();
    if(!sql_result) {
        logError(LOG_GENERAL, "Failed to insert license upgrade: {}", sql_result.fmtStr());
        return false;
    }

    sql_result = sql::command(this->sql(), "UPDATE `license` SET `upgrade_id` = :upgrade_id WHERE `keyId` = :key_id",
                              variable{":upgrade_id", upgrade_id},
                              variable{":key_id", old_key_id}).execute();
    if(!sql_result) {
        logError(LOG_GENERAL, "Failed to set license upgrade: {}", sql_result.fmtStr());
        return false;
    }

    return true;
}

std::unique_ptr<LicenseUpgrade> DatabaseHandler::query_license_upgrade(upgrade_id_t id) {
    std::unique_ptr<LicenseUpgrade> result{};
    auto sql_result = sql::command(this->sql(), "SELECT `upgrade_id`, `old_key_id`, `new_key_id`, `timestamp_begin`, `timestamp_end`, `valid`, `use_count`, `license` FROM `license_upgrades` WHERE `upgrade_id` = :upgrade_id LIMIT 1",
            variable{":upgrade_id", id}).query([&](int length, std::string* values, std::string* names) {
        result = std::make_unique<LicenseUpgrade>();
        for(size_t index = 0; index < length; index++) {
            try {
                if(names[index] == "upgrade_id")
                    result->upgrade_id = std::stoull(values[index]);
                else if(names[index] == "old_key_id")
                    result->old_license_key_id = std::stoull(values[index]);
                else if(names[index] == "new_key_id")
                    result->new_license_key_id = std::stoull(values[index]);
                else if(names[index] == "timestamp_begin")
                    result->begin_timestamp = std::chrono::system_clock::time_point{} + std::chrono::milliseconds{std::stoull(values[index])};
                else if(names[index] == "timestamp_end")
                    result->end_timestamp = std::chrono::system_clock::time_point{} + std::chrono::milliseconds{std::stoull(values[index])};
                else if(names[index] == "use_count")
                    result->update_count = std::stoull(values[index]);
                else if(names[index] == "valid")
                    result->valid = std::stoull(values[index]) > 0;
                else if(names[index] == "license")
                    result->license_key = base64::encode(values[index]);
            } catch(std::exception& ex) {
                result = nullptr;
                logWarning(LOG_GENERAL, "Failed to parse column {} for upgrade id {}. (Value: {})", names[index], id, values[index]);
                return;
            }
        }
    });
    if(!sql_result) {
        logWarning(LOG_GENERAL, "Failed to query license upgrade info for upgrade {}: {}", id, sql_result.fmtStr());
        return nullptr;
    }

    return result;
}

void DatabaseHandler::log_license_upgrade_attempt(upgrade_id_t upgrade_id, bool succeeded, const std::string &unique_id, const std::string &ip) {
    auto result = sql::command(this->sql(), "INSERT INTO `license_upgrade_log` (`upgrade_id`, `timestamp`, `unique_id`, `server_ip`, `succeeded`) VALUES (:upgrade_id, :timestamp, :unique_id, :server_ip, :succeeded);",
            variable{":upgrade_id", upgrade_id},
            variable{":timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()},
            variable{":unique_id", unique_id},
            variable{":server_ip", ip},
            variable{":succeeded", succeeded}).execute();
    if(!result)
        logWarning(LOG_GENERAL, "Failed to insert upgrade log into database ({})", result.fmtStr());

    if(succeeded) {
        result = sql::command(this->sql(), "UPDATE `license_upgrades` SET `use_count` = `use_count` + 1 WHERE `upgrade_id` = :upgrade_id",
                              variable{":upgrade_id", upgrade_id}).execute();
        if(!result)
            logWarning(LOG_GENERAL, "Failed to increase upgrade use count MySQL ({})", result.fmtStr());
    }
}