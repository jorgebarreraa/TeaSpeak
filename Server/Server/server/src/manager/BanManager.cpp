#include <regex>
#include <utility>
#include <log/LogUtils.h>
#include "BanManager.h"
#include "src/VirtualServer.h"
#include "src/client/ConnectedClient.h"

using namespace std;
using namespace std::chrono;
using namespace ts;
using namespace ts::server;

BanManager::BanManager(sql::SqlManager* handle) : sql(handle) {}

BanManager::~BanManager() {}

bool BanManager::loadBans() {
    this->current_ban_index.store(0);
    try {
        if(config::server::delete_old_bans) {
            debugMessage(LOG_INSTANCE, "Deleting old bans");
            LOG_SQL_CMD(sql::command(this->sql, "DELETE FROM `bannedClients` WHERE `until` < :now AND `until` > 0", variable{":now", duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count()}).execute());
        }
        LOG_SQL_CMD(sql::command(this->sql, "SELECT `banId` FROM `bannedClients` ORDER BY `banId` DESC LIMIT 1").query([](atomic<BanId>& counter, int, string* values, string*) {
            counter.store(stoll(values[0]));
        }, this->current_ban_index));
    } catch(std::exception& ex) {
        logCritical(LOG_INSTANCE, "Failed to setup ban manager!");
        return false;
    }
    return true;
}

inline deque<std::shared_ptr<BanRecord>> resolveBansByQuery(sql::command& command){
    deque<std::shared_ptr<BanRecord>> result;

    auto res = command.query([](deque<std::shared_ptr<BanRecord>>& list, int count, string* values, string* columnNames){
        auto res = std::make_shared<BanRecord>();
        res->banId = 0;
        res->uid = "";
        res->hwid = "";
        res->ip = "";
        res->name = "";
        res->reason = "undefined";
        res->created = time_point<system_clock>();
        res->until = time_point<system_clock>();
        res->triggered = 0;

        int index;
        try {
            for(index = 0; index < count; index++){
                string key = columnNames[index];
                if(key == "invokerDbId")
                    res->invokerDbId = stoull(values[index]);
                else if(key == "hwid")
                    res->hwid = values[index];
                else if(key == "uid")
                    res->uid = values[index];
                else if(key == "name")
                    res->name = values[index];
                else if(key == "ip")
                    res->ip = values[index];
                else if(key == "strType")
                    res->strType = static_cast<BanStringType>(stol(values[index]));
                else if(key == "created")
                    res->created = time_point<system_clock>() + milliseconds(stol(values[index]));
                else if(key == "until")
                    res->until = time_point<system_clock>() + milliseconds(stol(values[index]));
                else if(key == "banId")
                    res->banId = stoull(values[index]);
                else if(key == "reason")
                    res->reason = values[index];
                else if(key == "invName")
                    res->invokerName = values[index];
                else if(key == "invUid")
                    res->invokerUid = values[index];
                else if(key == "serverId")
                    res->serverId = stoll(values[index]);
                else if(key == "triggered")
                    res->triggered = stoull(values[index]);
                else cerr << "Invalid ban column " << key << endl;
            }
        } catch(std::exception& ex) {
            logError(LOG_GENERAL, "Failed to parse ban {}. (Exception while parsing row {} ({}): {})", res->banId, columnNames[index], values[index], ex.what());
            return 0;
        }

        list.push_back(std::move(res));
        return 0;
    }, result);

    if(!res) cerr << res << endl;

    debugMessage(LOG_GENERAL, "Query: {} -> {}", command.sqlCommand(), result.size());
    return result;
}

inline deque<shared_ptr<BanRecord>> resolveBanByQuery(sql::command& command){
    auto list = resolveBansByQuery(command);
    if(list.empty()) return {nullptr};
    std::sort(list.begin(), list.end(), [](const std::shared_ptr<BanRecord>& a, const std::shared_ptr<BanRecord>& b){ return a->created < b->created; });
    return list;
}

#define UNTIL_SQL " AND (bannedClients.`until` > :time OR bannedClients.`until` = 0)"
#define SERVER_ID "(`serverId` = 0 OR `serverId` = :sid)"
std::shared_ptr<BanRecord> BanManager::findBanById(ServerId sid, uint64_t id) {
    auto cmd = sql::command(this->sql, "SELECT * FROM `bannedClients` WHERE " SERVER_ID " AND `banId` = :id" UNTIL_SQL,
                            variable{":time", duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count()},
                            variable{":sid", sid},
                            variable{":id", id});
    return resolveBanByQuery(cmd).back();
}

std::shared_ptr<BanRecord> BanManager::findBanByHwid(ServerId sid, std::string hwid) {
    if(hwid.empty())
        hwid = "empty";
    auto cmd = sql::command(this->sql, "SELECT * FROM `bannedClients` WHERE " SERVER_ID " AND `hwid` = :hwid" UNTIL_SQL,
                            variable{":time", duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count()},
                            variable{":sid", sid},
                            variable{":hwid", hwid});
    return resolveBanByQuery(cmd).back();
}

std::shared_ptr<BanRecord> BanManager::findBanByUid(ServerId sid, std::string uid) {
    if(uid.empty())
        uid = "empty";
    auto cmd = sql::command(this->sql, "SELECT * FROM `bannedClients` WHERE " SERVER_ID " AND `uid` = :uid" UNTIL_SQL,
                            variable{":time", duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count()},
                            variable{":sid", sid},
                            variable{":uid", uid});
    return resolveBanByQuery(cmd).back();
}

std::shared_ptr<BanRecord> BanManager::findBanByIp(ServerId sid, std::string ip) {
    if(ip.empty())
        ip = "empty";
    auto cmd = sql::command(this->sql, "SELECT * FROM `bannedClients` WHERE " SERVER_ID " AND `ip` = :ip" UNTIL_SQL,
                            variable{":time", duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count()},
                            variable{":sid", sid},
                            variable{":ip", ip});
    return resolveBanByQuery(cmd).back();
}

std::shared_ptr<BanRecord> BanManager::findBanByName(ServerId sid, std::string nickName) {
    auto cmd = sql::command(this->sql, "SELECT * FROM `bannedClients` WHERE " SERVER_ID " AND (`name` = :name OR `strType` = :type)" UNTIL_SQL,
                            variable{":time", duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count()},
                            variable{":sid", sid},
                            variable{":name", nickName},
                            variable{":type", BanStringType::BST_REGEX});

    auto av = resolveBanByQuery(cmd);
    while(!av.empty()){
        auto entry = std::move(av[av.size() - 1]);
        av.pop_back();
        if(!entry) continue;

        if(entry->strType == BST_REGEX){
            try {
                if(regex_match(nickName, regex(entry->name))) return entry;
            } catch (std::regex_error& e) { }
        } else return entry;
    }
    return nullptr;
}

std::shared_ptr<BanRecord> BanManager::findBanExact(ts::ServerId server_id, const std::string &reason, const std::string &uid, const std::string &ip, const std::string &name, const std::string &hardware_id) {
    auto cmd = sql::command(this->sql, "SELECT * FROM `bannedClients` WHERE " SERVER_ID " AND `ip` = :ip AND `strType` = :name_mode AND `name` = :name AND `uid` = :uid AND `hwid` = :hwid AND `reason` = :reason" UNTIL_SQL,
                            variable{":time", duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count()},
                            variable{":sid", server_id},
                            variable{":hwid", hardware_id},
                            variable{":uid", uid},
                            variable{":name", name},
                            variable{":name_mode", BanStringType::BST_FIXED},
                            variable{":ip", ip},
                            variable{":reason", reason}
    );
    return resolveBanByQuery(cmd).back();
}

std::deque<std::shared_ptr<BanRecord>> BanManager::listBans(ServerId sid) {
    auto command = sql::command(this->sql, "SELECT `bannedClients`.*, clients_server.`client_unique_id` AS `invUid`, clients_server.`client_nickname` AS `invName` FROM `bannedClients` INNER JOIN clients_server ON clients_server.client_database_id = bannedClients.invokerDbId AND clients_server.server_id = :sid WHERE bannedClients.`serverId` = :sid" UNTIL_SQL,
                                variable{":time", duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count()},
                                variable{":sid", sid});
    return resolveBansByQuery(command);
}

void BanManager::deleteAllBans(ServerId sid) {
    sql::command(this->sql, "DELETE FROM `bannedClients` WHERE `serverId` = :sid",
                 variable{":sid", sid}
    ).executeLater().waitAndGetLater(LOG_SQL_CMD, {1, "future failed"});
}

BanId BanManager::registerBan(ServerId id, uint64_t invoker, string reason, std::string uid, std::string ip, std::string nickName, std::string hwid, std::chrono::time_point<std::chrono::system_clock> until) {
    if(ip.empty() && uid.empty() && hwid.empty() && nickName.empty()) {
        logError(0, "Tried to register an empty ban. Reject ban.");
        return 0;
    }
    const BanId ban_id = ++this->current_ban_index;
    sql::command(this->sql, "INSERT INTO `bannedClients` (`banId`, `serverId`, `invokerDbId`, `reason`, `hwid`, `uid`, `name`, `ip`, `strType`, `created`, `until`) VALUES (:bid, :sid, :invoker, :reason, :hwid, :uid, :name, :ip, :strType, :create, :until)",
                 variable{":sid", id},
                 variable{":bid", ban_id},
                 variable{":invoker", invoker},
                 variable{":reason", std::move(reason)},
                 variable{":hwid", std::move(hwid)},
                 variable{":uid", std::move(uid)},
                 variable{":name", std::move(nickName)},
                 variable{":ip", std::move(ip)},
                 variable{":strType", BST_REGEX},
                 variable{":create", duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count()},
                 variable{":until", duration_cast<milliseconds>(until.time_since_epoch()).count()}
    ).executeLater().waitAndGetLater(LOG_SQL_CMD, {1, "future failed"});
    return ban_id;
}

void BanManager::unban(ServerId sid, BanId record) {
    sql::command(this->sql, "DELETE FROM `bannedClients` WHERE `serverId` = :sid AND `banId` = :bid", variable{":sid", sid}, variable{":bid", record}).executeLater().waitAndGetLater(LOG_SQL_CMD, {1, "future failed"});
}

void BanManager::unban(std::shared_ptr<BanRecord> record) {
    this->unban(record->serverId, record->banId);
}

void BanManager::updateBan(std::shared_ptr<BanRecord> record) {
    sql::command(this->sql, "UPDATE `bannedClients` SET `invokerDbId` = :invokerDbId, `ip` = :ip, `triggered` = :triggered, `name` = :name, `reason` = :reason, `uid` = :uid, `hwid` = :hwid, `strType` = :strType, `until` = :until WHERE `banId` = :banId AND `serverId` = :sid",
                 variable{":sid", record->serverId},
                 variable{":banId", record->banId},
                 variable{":ip", record->ip},
                 variable{":name", record->name},
                 variable{":uid", record->uid},
                 variable{":until", duration_cast<milliseconds>(record->until.time_since_epoch()).count()},
                 variable{":strType", record->strType},
                 variable{":hwid", record->hwid},
                 variable{":invokerDbId", record->invokerDbId},
                 variable{":triggered", record->triggered},
                 variable{":reason", record->reason}
    ).executeLater().waitAndGetLater(LOG_SQL_CMD, {1, "future failed"});
}

void BanManager::updateBanReason(std::shared_ptr<BanRecord> record, std::string reason) {
    auto res = sql::command(this->sql, "UPDATE `bannedClients` SET `reason` = :reason WHERE `serverId` = :sid AND `banId` = :banId", variable{":reason", reason}, variable{":sid", record->serverId}, variable{":banId", record->banId}).execute();
    auto pf = LOG_SQL_CMD;
    pf(res);
}

void BanManager::updateBanTimeout(std::shared_ptr<BanRecord> record, std::chrono::time_point<std::chrono::system_clock> until) {
    auto res = sql::command(this->sql, "UPDATE `bannedClients` SET `until` = :until WHERE `serverId` = :sid AND `banId` = :banId", variable{":until", duration_cast<milliseconds>(until.time_since_epoch()).count()}, variable{":sid", record->serverId}, variable{":banId", record->banId}).execute();
    auto pf = LOG_SQL_CMD;
    pf(res);
}

//`server_id` INT, `ban_id` INT, `unique_id` VARCHAR(" CLIENT_UID_LENGTH "), `hardware_id` VARCHAR(" CLIENT_UID_LENGTH "), `name` VARCHAR(" CLIENT_NAME_LENGTH "), `ip` VARCHAR(128), `timestamp` BIGINT
void BanManager::trigger_ban(const std::shared_ptr<BanRecord>& record,
                             ServerId server,
                             const std::string& unique_id,
                             const std::string& hardware_id,
                             const std::string& nickname,
                             const std::string& ip) {
    sql::command(this->sql, "INSERT INTO `ban_trigger` (`server_id`, `ban_id`, `unique_id`, `hardware_id`, `name`, `ip`, `timestamp`) VALUES (:sid, :bid, :uid, :hid, :name, :ip, :timestamp)",
                 variable{":sid", server},
                 variable{":bid", record->banId},
                 variable{":uid", unique_id},
                 variable{":hid", hardware_id},
                 variable{":name", nickname},
                 variable{":ip", ip},
                 variable{":timestamp", duration_cast<milliseconds>(chrono::system_clock::now().time_since_epoch()).count()}
    ).executeLater().waitAndGetLater(LOG_SQL_CMD, {1, "future failed"});

    record->triggered++;
    sql::command(this->sql, "UPDATE `bannedClients` SET `triggered` = :triggered WHERE `banId` = :banId AND `serverId` = :sid",
                 variable{":sid", record->serverId},
                 variable{":banId", record->banId},
                 variable{":triggered", record->triggered}
    ).executeLater().waitAndGetLater(LOG_SQL_CMD, {1, "future failed"});
}

std::deque<std::shared_ptr<BanTrigger>> BanManager::trigger_list(const std::shared_ptr<ts::server::BanRecord> &record, ServerId server_id, ssize_t offset, ssize_t length) {
    std::deque<std::shared_ptr<BanTrigger>> result;

    if(offset < 0) offset = 0;
    if(length <= 0) length = -1;
    LOG_SQL_CMD(sql::command(this->sql, "SELECT `unique_id`, `hardware_id`, `name`, `ip`, `timestamp` FROM `ban_trigger` WHERE `server_id` = :sid AND `ban_id` = :bid LIMIT :offset, :limit",
                             variable{":sid", server_id},
                             variable{":bid", record->banId},
                             variable{":offset", offset},
                             variable{":limit", length}
    ).query([server_id, &record](std::deque<std::shared_ptr<BanTrigger>>& list, int length, string* values, string* names){
            auto entry = make_shared<BanTrigger>();
            entry->server_id = server_id;
            entry->ban_id = record->banId;
            int index;
            try {
                for(index = 0; index < length; index++) {
                    if(names[index] == "unique_id")
                        entry->unique_id = values[index];
                    else if(names[index] == "hardware_id")
                        entry->hardware_id = values[index];
                    else if(names[index] == "name")
                        entry->name = values[index];
                    else if(names[index] == "ip")
                        entry->ip = values[index];
                    else if(names[index] == "timestamp")
                        entry->timestamp = system_clock::time_point() + milliseconds(stoll(values[index]));
                }
            } catch(std::exception& ex) {
                logError(LOG_GENERAL, "Failed to parse ban trigger {}. (Exception while parsing row {} ({}): {})", entry->ban_id, names[index], values[index], ex.what());
                return 0;
            }

            list.push_back(entry);
            return 0;
    }, result));

    return result;
}