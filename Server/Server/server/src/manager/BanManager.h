#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <chrono>
#include <memory>
#include <vector>
#include <Variable.h>
#include <Definitions.h>
#include <sql/SqlQuery.h>

namespace ts {
    namespace server {
        enum BanStringType {
            BST_WILD_V4,
            BST_WILD_V6,
            BST_FIXED,
            BST_REGEX
        };
    }
}
DEFINE_VARIABLE_TRANSFORM(ts::server::BanStringType, VARTYPE_INT, std::to_string((uint8_t) in), static_cast<ts::server::BanStringType>(in.as<uint8_t>()));

namespace ts {
    namespace server {
        class VirtualServer;
        class ConnectedClient;

        struct BanRecord {
            ServerId serverId;
            BanId banId;

            ClientDbId invokerDbId;
            std::string invokerUid;
            std::string invokerName;

            std::string hwid;
            std::string uid;

            std::string name;
            std::string ip;
            BanStringType strType;

            std::chrono::time_point<std::chrono::system_clock> created;
            std::chrono::time_point<std::chrono::system_clock> until;

            std::string reason;

            size_t triggered;
        };

        struct BanTrigger {
            ServerId server_id;
            BanId ban_id;
            std::chrono::system_clock::time_point timestamp;

            std::string unique_id;
            std::string hardware_id;
            std::string name;
            std::string ip;
        };

        class BanManager {
            public:
                BanManager(sql::SqlManager*);
                ~BanManager();

                bool loadBans();

                std::deque<std::shared_ptr<BanRecord>> listBans(ServerId);

                std::shared_ptr<BanRecord> findBanById(ServerId,    uint64_t banId);
                std::shared_ptr<BanRecord> findBanByHwid(ServerId,  std::string hwid);
                std::shared_ptr<BanRecord> findBanByUid(ServerId,   std::string uid);
                std::shared_ptr<BanRecord> findBanByIp(ServerId,    std::string ip);
                std::shared_ptr<BanRecord> findBanByName(ServerId,  std::string nickName);
                std::shared_ptr<BanRecord> findBanExact(ServerId,
                                                        const std::string& /* reason */,
                                                        const std::string& /* uid */,
                                                        const std::string& /* ip */,
                                                        const std::string& /* display name */,
                                                        const std::string& /* hardware id */);

                void deleteAllBans(ServerId sid);

                BanId registerBan(ServerId, ClientDbId invoker, std::string reason, std::string uid, std::string ip, std::string nickName, std::string hwid, std::chrono::time_point<std::chrono::system_clock> until);
                void unban(ServerId, BanId);
                void unban(std::shared_ptr<BanRecord>);
                void updateBan(std::shared_ptr<BanRecord>);

                void updateBanReason(std::shared_ptr<BanRecord>, std::string);
                void updateBanTimeout(std::shared_ptr<BanRecord>, std::chrono::time_point<std::chrono::system_clock>);

                void trigger_ban(const std::shared_ptr<BanRecord>& /* record */,
                                 ServerId /* server */,
                                 const std::string& /* unique id */,
                                 const std::string& /* hardware id */,
                                 const std::string& /* nickname */,
                                 const std::string& /* ip */
                );
                std::deque<std::shared_ptr<BanTrigger>> trigger_list(const std::shared_ptr<BanRecord>& /* record */, ServerId /* server id */, ssize_t /* offset */, ssize_t /* limit */);

            private:
                sql::SqlManager* sql = nullptr;
                std::atomic<BanId> current_ban_index;
        };
    }
}