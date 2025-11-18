#include <algorithm>
#include <log/LogUtils.h>
#include "ComplainManager.h"
#include "src/VirtualServer.h"

using namespace std;
using namespace std::chrono;
using namespace ts;

ComplainManager::ComplainManager(server::VirtualServer* server) : server(server) { }
ComplainManager::~ComplainManager() {
    this->entries.clear();
}

bool ComplainManager::loadComplains() {
    sql::command(this->server->getSql(), "SELECT `targetId`, `reporterId`, `reason`, `created` FROM `complains` WHERE `serverId` = :sid", variable{":sid", this->server->getServerId()}).query([&](int length, string* values, string* columns){
        shared_ptr<ComplainEntry> entry = std::make_shared<ComplainEntry>();
        for(int index = 0; index < length; index++) {
            try {
                if(columns[index] == "targetId")
                    entry->target = stoull(values[index]);
                else if(columns[index] == "reporterId")
                    entry->invoker = stoull(values[index]);
                else if(columns[index] == "reason")
                    entry->reason = values[index];
                else if(columns[index] == "created")
                    entry->created = time_point<system_clock>() + milliseconds(stol(values[index]));
            } catch(const std::exception& ex) {
                logError(this->server->getServerId(), "Failed to load complain entry from database! Failed to parse value for {}. Value: {}, Exception: {}", columns[index], values[index], ex.what());
                return 0;
            }
        }
        this->entries.push_back(entry);
        return 0;
    });
    return true;
}

std::deque<std::shared_ptr<ComplainEntry>> ComplainManager::findComplainsFromTarget(ClientDbId cl) {
    deque<shared_ptr<ComplainEntry>> result;

    this->entryLock.lock();
    for(const auto &entry : this->entries)
        if(entry->target == cl)
            result.push_back(entry);
    this->entryLock.unlock();

    return result;
}

std::deque<std::shared_ptr<ComplainEntry>> ComplainManager::findComplainsFromReporter(ClientDbId cl) {
    deque<shared_ptr<ComplainEntry>> result;

    this->entryLock.lock();
    for(const auto &entry : this->entries)
        if(entry->invoker == cl)
            result.push_back(entry);
    this->entryLock.unlock();

    return result;
}

std::shared_ptr<ComplainEntry> ComplainManager::createComplain(ClientDbId target, ClientDbId reporter, std::string msg) {
    shared_ptr<ComplainEntry> entry = std::make_shared<ComplainEntry>();

    entry->invoker = reporter;
    entry->target = target;
    entry->reason = std::move(msg);
    entry->created = system_clock::now();

    this->entryLock.lock();
    this->entries.push_back(entry);
    this->entryLock.unlock();

    sql::command(this->server->getSql(), "INSERT INTO `complains` (`serverId`, `targetId`, `reporterId`, `reason`, `created`) VALUES (:sid, :target, :invoker, :reason, :created)",
                 variable{":sid", this->server->getServerId()}, variable{":target", entry->target}, variable{":reason", entry->reason}, variable{":invoker", entry->invoker}, variable{":created", duration_cast<milliseconds>(entry->created.time_since_epoch()).count()})
            .executeLater().waitAndGetLater(LOG_SQL_CMD, {1, "future failed"});

    return entry;
}

bool ComplainManager::deleteComplain(std::shared_ptr<ComplainEntry> entry) {
    this->entryLock.lock();
    auto found = std::find(this->entries.begin(), this->entries.end(), entry);
    if(found == this->entries.end()){
        this->entryLock.unlock();
        return false;
    }
    this->entries.erase(found);
    this->entryLock.unlock();

    sql::command(this->server->getSql(), "DELETE FROM `complains` WHERE `serverId` = :sid AND `targetId` = :target AND `reporterId` = :invoker AND `reason` = :reason AND `created` = :created",
                 variable{":sid", this->server->getServerId()}, variable{":target", entry->target}, variable{":reason", entry->reason}, variable{":invoker", entry->invoker}, variable{":created", duration_cast<milliseconds>(entry->created.time_since_epoch()).count()})
            .executeLater().waitAndGetLater(LOG_SQL_CMD, {1, "future failed"});

    return true;
}

bool ComplainManager::deleteComplainsFromReporter(ClientDbId cl) {
    bool flag = true;
    for(const auto& elm : this->findComplainsFromReporter(cl))
        flag &= deleteComplain(elm);
    return flag;
}

bool ComplainManager::deleteComplainsFromTarget(ClientDbId cl) {
    bool flag = true;
    for(const auto& elm : this->findComplainsFromTarget(cl))
        flag &= deleteComplain(elm);
    return flag;
}