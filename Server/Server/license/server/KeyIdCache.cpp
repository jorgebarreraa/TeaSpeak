#include "log/LogUtils.h"
#include "DatabaseHandler.h"

using namespace license;
using namespace license::server::database;
using namespace std;
using namespace std::chrono;

KeyIdCache::KeyIdCache(DatabaseHandler *handle) : handle(handle) {}

void KeyIdCache::clear_cache() {
    std::lock_guard elock{this->entry_lock};
    this->entries.clear();
}

std::string KeyIdCache::get_key_from_id(size_t keyId) {
    {
        std::lock_guard elock{this->entry_lock};

        for(const auto& entry : this->entries)
            if(entry->keyId == keyId) return entry->key;
    }

    sql::command(this->handle->sql(), "SELECT `key`, `keyId` FROM `license` WHERE `keyId` = :key", variable{":key", keyId})
            .query(&KeyIdCache::insert_entry, this);
    {
        std::lock_guard elock{this->entry_lock};

        for(const auto& entry : this->entries)
            if(entry->keyId == keyId) return entry->key;
        return ""; //Key not found!
    }
}

size_t KeyIdCache::get_key_id_from_key(const std::string &key) {
    {
        std::lock_guard elock{this->entry_lock};

        for(const auto& entry : this->entries)
            if(entry->key == key) return entry->keyId;
    }

    auto result = sql::command(this->handle->sql(), "SELECT `key`, `keyId` FROM `license` WHERE `key` = :key", variable{":key", key})
            .query(&KeyIdCache::insert_entry, this);
    if(!result)
        logError(LOG_GENERAL, "Failed to query key id for license. Query resulted in {}", result.fmtStr());
    {
        std::lock_guard elock{this->entry_lock};

        for(const auto& entry : this->entries)
            if(entry->key == key)
                return entry->keyId;

        return 0; //Key not found!
    }
}

int KeyIdCache::insert_entry(int length, std::string *value, std::string *names) {
    string key{"unknown"};
    size_t keyId{0};

    for(int index = 0; index < length; index++) {
        if(names[index] == "key")
            key = value[index];
        else if(names[index] == "keyId")
            keyId = std::strtoll(value[index].c_str(), nullptr, 10);
    }
    if(!keyId) {
        logWarning(LOG_GENERAL, "Failed to parse key id for key {}", key);
        return 0;
    }

    {
        auto entry = new KeyIdCache::CacheEntry{key, keyId, system_clock::now()};
        std::lock_guard elock{this->entry_lock};
        this->entries.emplace_back(entry);
    }
    return 0;
}