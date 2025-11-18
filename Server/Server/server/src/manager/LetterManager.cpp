#include <sql/SqlQuery.h>
#include <log/LogUtils.h>
#include "LetterManager.h"
#include "src/VirtualServer.h"

using namespace std;
using namespace std::chrono;
using namespace ts;
using namespace ts::letter;
using namespace ts::server;

//`serverId` INT NOT NULL, `sender` TEXT, `receiver` TEXT, `created` INT, `subject` TEXT, `message` TEXT

LetterManager::LetterManager(server::VirtualServer* server) : server(server) {}
LetterManager::~LetterManager() {}

size_t LetterManager::unread_letter_count(const ts::ClientUid &client_unique_id) {
    size_t result = 0;
    auto res = sql::command(this->server->getSql(), "SELECT COUNT(*) FROM `letters` WHERE `serverId` = :sid AND `receiver` = :uid AND `read` = :false",
            variable{":sid", this->server ? this->server->getServerId() : 0},
            variable{":uid", client_unique_id},
            variable{":false", 0}
    ).query([&](int length, std::string* values, std::string* columns) {
        if(length != 1)
            return 1;

        try {
            result = stoll(values[0]);
        } catch(std::exception& ex) {
            logError(this->server ? this->server->getServerId() : 0, "Failed to parse unread letter count: {}", ex.what());
            return 1;
        }
        return 0;
    });
    (LOG_SQL_CMD)(res);
    return result;
}

std::vector<std::shared_ptr<LetterHeader>> LetterManager::avariableLetters(ClientUid cluid) {
    vector<shared_ptr<LetterHeader>> result;

    auto res = sql::command(this->server->getSql(), "SELECT `letterId`, `sender`, `created`, `subject`, `read` FROM `letters` WHERE `serverId` = :sid AND `receiver` = :uid", variable{":sid", this->server->getServerId()}, variable{":uid", cluid})
            .query([cluid](vector<shared_ptr<LetterHeader>>* list, int length, char** values, char** columns){
                shared_ptr<LetterHeader> letter = make_shared<LetterHeader>();

                for(int index = 0; index < length; index++)
                    if(strcmp(columns[index], "sender") == 0)
                        letter->sender = values[index];
                    else if(strcmp(columns[index], "created") == 0)
                        letter->created = std::chrono::system_clock::time_point{} + seconds{stoull(values[index])};
                    else if(strcmp(columns[index], "letterId") == 0)
                        letter->id = static_cast<LetterId>(stoull(values[index]));
                    else if(strcmp(columns[index], "subject") == 0)
                        letter->subject = values[index];
                    else if(strcmp(columns[index], "read") == 0)
                        letter->read = strcmp(values[index], "1") == 0;
                letter->receiver = cluid;
                list->push_back(letter);
                return 0;
            }, &result);
    (LOG_SQL_CMD)(res);

    return result;
}

void LetterManager::deleteLetter(LetterId letter) {
    sql::command(this->server->getSql(), "DELETE FROM `letters` WHERE `letterId` = :id", variable{":id", letter}).executeLater().waitAndGetLater(LOG_SQL_CMD, {1, "future failed"});
}

void LetterManager::updateReadFlag(LetterId letter, bool flag) {
    sql::command(this->server->getSql(), "UPDATE `letters` SET `read` = :flag WHERE `letterId` = :id", variable{":id", letter}, variable{":flag", flag}).executeLater().waitAndGetLater(LOG_SQL_CMD, {1, "future failed"});
}

shared_ptr<Letter> LetterManager::getFullLetter(LetterId letter) {
    shared_ptr<Letter> res = std::make_shared<Letter>();

    sql::command(this->server->getSql(), "SELECT `letterId`, `sender`, `receiver`, `created`, `subject`, `read`,`message` FROM `letters` WHERE `letterId` = :id", variable{":id", letter}).query([](Letter* letter, int length, char** values, char** columns){
        for(int index = 0; index < length; index++)
            if(strcmp(columns[index], "sender") == 0)
                letter->sender = values[index];
            else if(strcmp(columns[index], "created") == 0)
                letter->created = system_clock::now() + std::chrono::seconds{stoull(values[index])};
            else if(strcmp(columns[index], "letterId") == 0)
                letter->id = static_cast<LetterId>(stoull(values[index]));
            else if(strcmp(columns[index], "subject") == 0)
                letter->subject = values[index];
            else if(strcmp(columns[index], "read") == 0)
                letter->read = strcmp(values[index], "1") == 0;
            else if(strcmp(columns[index], "receiver") == 0)
                letter->receiver = values[index];
            else if(strcmp(columns[index], "message") == 0)
                letter->message = values[index];
        return 0;
    }, res.get());

    return res;
}

void LetterManager::createLetter(const ClientUid& sender, const ClientUid& reciver, const std::string& subject, const std::string& message) {
    sql::command(this->server->getSql(), "INSERT INTO `letters` (`serverId`, `sender`, `receiver`, `created`, `subject`, `message`, `read`) VALUES (:sid, :sender, :receiver, :created, :subject, :message, :read)",
                variable{":sid", this->server->getServerId()},
                variable{":sender", sender},
                variable{":receiver", reciver},
                variable{":created", std::chrono::floor<std::chrono::seconds>(system_clock::now().time_since_epoch()).count()},
                variable{":subject", subject},
                variable{":message", message},
                variable{":read", false})
            .executeLater().waitAndGetLater(LOG_SQL_CMD, {1, "future failed"});
}
