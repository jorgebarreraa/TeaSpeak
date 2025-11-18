#pragma once

#include <sql/SqlQuery.h>
#include <shared/include/license/license.h>
#include <LicenseRequest.pb.h>

namespace license::server::database {
    typedef uint64_t license_key_id_t;
    typedef uint64_t upgrade_id_t;

    class DatabaseHandler;
    class KeyIdCache {
        public:
            explicit KeyIdCache(DatabaseHandler*);

            std::string get_key_from_id(license_key_id_t keyId);
            size_t  get_key_id_from_key(const std::string &key);

            void clear_cache();
        private:
            struct CacheEntry {
                std::string key;
                size_t keyId;
                std::chrono::system_clock::time_point age;
            };

            int insert_entry(int, std::string*, std::string*);

            DatabaseHandler* handle;

            std::mutex entry_lock{};
            std::deque<std::unique_ptr<CacheEntry>> entries;
    };

    struct LicenseUpgrade {
        upgrade_id_t upgrade_id{0};

        license_key_id_t old_license_key_id{0};
        license_key_id_t new_license_key_id{0};

        std::chrono::system_clock::time_point begin_timestamp{};
        std::chrono::system_clock::time_point end_timestamp{};

        bool valid{false};
        size_t update_count{0};

        std::string license_key{};

        [[nodiscard]] inline bool not_yet_available() const {
            return std::chrono::system_clock::now() < this->begin_timestamp && this->begin_timestamp.time_since_epoch().count() != 0;
        }

        [[nodiscard]] inline bool is_expired() const {
            return std::chrono::system_clock::now() > this->end_timestamp && this->end_timestamp.time_since_epoch().count() != 0;
        }
    };

    class DatabaseHandler {
        public:
            struct UserStatistics {
                uint64_t clients_online = 0;
                uint64_t web_clients_online = 0;
                uint64_t bots_online = 0;
                uint64_t queries_online = 0;
                uint64_t servers_online = 0;
            };

            struct ExtendedUserStatistics : public UserStatistics {
                std::string ip_address;
            };

            struct GlobalUserStatistics : public UserStatistics {
                uint64_t instance_online{0};
                uint64_t instance_empty{0};
            };

            struct DatabaseUserStatistics : public UserStatistics {
                std::chrono::system_clock::time_point timestamp{};
            };

            struct GlobalVersionsStatistic {
                std::chrono::system_clock::time_point timestamp;
                std::map<std::string, uint32_t> versions;
            };

            struct UserHistory {
                std::chrono::system_clock::time_point begin{};
                std::chrono::system_clock::time_point end{};
                std::chrono::milliseconds interval{0};

                size_t record_count = 0;
                GlobalUserStatistics history[0];
            };
            static_assert(sizeof(UserHistory) == 32);

        public:
            explicit DatabaseHandler(sql::SqlManager* database);
            ~DatabaseHandler();

            bool setup(std::string&);

            [[nodiscard]] inline std::shared_ptr<KeyIdCache> key_id_cache() { return this->id_cache; }

            bool validLicenseKey(const std::string& /* key */);
            std::shared_ptr<LicenseInfo> query_license_info(const std::string&);
            std::map<std::string, std::shared_ptr<LicenseInfo>> list_licenses(int offset = 0, int limit = -1);

            bool register_license_upgrade(license_key_id_t /* old key */, license_key_id_t /* new key */, const std::chrono::system_clock::time_point& /* begin */, const std::chrono::system_clock::time_point& /* end */, const std::string& /* key */);
            std::unique_ptr<LicenseUpgrade> query_license_upgrade(upgrade_id_t /* upgrade id */);
            void log_license_upgrade_attempt(upgrade_id_t /* upgrade id */, bool /* succeeded */, const std::string& /* server unique id */, const std::string& /* ip address */);

            bool register_license(const std::string& /* key */, const std::shared_ptr<LicenseInfo>& /* info */, const std::string& /* issuer */);
            bool delete_license(const std::string& /* key */, bool /* full */ = false);

            bool logRequest(const std::string& /* key */, const std::string& /* unique_id */, const std::string& /* ip */, const std::string& /* version */, int /* result */);
            bool logStatistic(const std::string& /* key */, const std::string& /* unique_id */, const std::string& /* ip */, const ts::proto::license::PropertyUpdateRequest&);
            //std::deque<std::unique_ptr<ExtendedUserStatistics>> list_statistics_user(const std::string& key, const std::chrono::system_clock::time_point& begin, const std::chrono::system_clock::time_point& end);
            std::shared_ptr<UserHistory> list_statistics_user(const std::chrono::system_clock::time_point& begin, const std::chrono::system_clock::time_point& end, const std::chrono::milliseconds& /* interval */);
            std::deque<std::unique_ptr<GlobalVersionsStatistic>> list_statistics_version(const std::chrono::system_clock::time_point& begin, const std::chrono::system_clock::time_point& end, const std::chrono::milliseconds& /* interval */);

            inline sql::SqlManager* sql() { return this->database; }
        private:
            std::shared_ptr<KeyIdCache> id_cache;
            sql::SqlManager* database;
    };
}