#pragma once

#include <mutex>
#include "../misc/spin_mutex.h"

namespace lookup {
    template <typename T, typename addr_t, typename addr_storage_t, typename addr_converter, typename addr_cmp, typename hash_fn>
    struct ip_vx {
            constexpr static auto kBukkitListSize{128}; /* must be a power of two */
            constexpr static auto kBukkitSize{32};

            struct bucket_entry {
                addr_storage_t address{};
                std::shared_ptr<T> entry{};
            };

            struct bucket_t {
                std::array<bucket_entry, kBukkitSize> entries{};

                uint8_t entry_count{0};
                bucket_t* next{nullptr};
            };

        public:
            inline void insert(const addr_t& address, const std::shared_ptr<T>& value) {
                auto hash = (uint8_t) hash_fn{}(address);
                hash &= (uint8_t) (kBukkitListSize - 1);

                bucket_t* bucket = &this->buckets[hash];

                std::lock_guard lock{this->bucket_locks[hash]};
                while(bucket->entry_count == kBukkitSize && bucket->next) {
                    bucket = bucket->next;
                }

                if(bucket->entry_count == kBukkitSize) {
                    bucket = (bucket->next = new bucket_t{});
                }

                auto& entry = bucket->entries[bucket->entry_count++];
                addr_converter{}(entry.address, address);
                entry.entry = value;
            }

            inline std::shared_ptr<T> remove(const addr_t& address) {
                auto hash = (uint8_t) hash_fn{}(address);
                hash &= (uint8_t) (kBukkitListSize - 1);

                bucket_t *bucket = &this->buckets[hash], *next_bucket;

                addr_storage_t addr{};
                addr_converter{}(addr, address);

                addr_cmp cmp{};

                std::lock_guard lock{this->bucket_locks[hash]};

                size_t entry_index;
                do {
                    for(entry_index = 0; entry_index < bucket->entry_count; entry_index++) {
                        if(auto& entry{bucket->entries[entry_index]}; cmp(entry.address, addr)) {
                            goto entry_found;
                        }
                    }
                } while((bucket = bucket->next));

                /* entry hasn't been found */
                return nullptr;

                entry_found:

                next_bucket = bucket;
                while(next_bucket->next && next_bucket->next->entry_count > 0) {
                    next_bucket = next_bucket->next;
                }

                /* swap the entry with the last entry and just remove the value */
                next_bucket->entry_count--;
                std::exchange(bucket->entries[entry_index], next_bucket->entries[next_bucket->entry_count]);
                return std::exchange(next_bucket->entries[next_bucket->entry_count].entry, nullptr);
            }

            inline void cleanup() {
                for(size_t bucket_index{0}; bucket_index < kBukkitListSize; bucket_index++) {
                    cleanup_bucket(bucket_index);
                }
            }

            inline void cleanup_bucket(size_t index) {
                bucket_t* delete_head;
                {
                    std::lock_guard lock{this->bucket_locks[index]};
                    auto& bucket = this->buckets[index];

                    bucket_t* prev{nullptr}, curr{&bucket};
                    while(curr.entry_count > 0 && curr.next) {
                        prev = std::exchange(curr, curr.next);
                    }

                    if(curr.entry_count == 0) {
                        prev->next = nullptr;
                        delete_head = curr;
                    } else {
                        return;
                    }
                }

                while(delete_head) {
                    auto next = delete_head->next;
                    delete delete_head;
                    delete_head = next;
                }
            }

            [[nodiscard]] inline std::shared_ptr<T> lookup(const addr_t& address) const {
                auto hash = (uint8_t) hash_fn{}(address);
                hash &= (uint8_t) (kBukkitListSize - 1);

                const bucket_t* bucket = &this->buckets[hash], next_bucket;

                addr_storage_t addr{};
                addr_converter{}(addr, address);

                addr_cmp cmp{};

                {
                    std::lock_guard lock{this->bucket_locks[hash]};
                    do {
                        for(size_t index{0}; index < bucket->entry_count; index++) {
                            if(auto& entry{bucket->entries[index]}; cmp(entry.address, addr)) {
                                return entry.entry;
                            }
                        }
                    } while((bucket = bucket->next));
                }

                return nullptr;
            }
        private:
            mutable std::array<spin_mutex, kBukkitListSize> bucket_locks{};
            std::array<bucket_t, kBukkitListSize> buckets{};
    };
}