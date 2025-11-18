#pragma once

#include <array>
#include <mutex>
#include <deque>
#include <netinet/in.h>
#include "./spin_lock.h"
#include <shared_mutex>

namespace ts::network {
    class ip_router {
            /* currently its not possible to change this! */
            constexpr static auto bits_per_entry{8U}; /* must be a multiple of 2! */
            constexpr static auto total_bits{128U};
        public:
            struct route_entry {
                constexpr static auto const_flag_mask = 0x80000000ULL;
                route_entry() noexcept = default;
                ~route_entry() = default;

                struct {
                    uint8_t deep;
                    uint8_t previous_chunks[(total_bits - bits_per_entry) / 8]; /* subtract the last entry because we do not need a special check there */
                    uint32_t use_count; /* could be size_t as well :) */
                } __attribute__((packed));

                void* data[1U << (bits_per_entry + 1)];

                [[nodiscard]] inline bool is_const_entry() const { return (this->use_count & const_flag_mask) > 0; }
            };

            static_assert(std::is_trivially_destructible<route_entry>::value);
            static_assert(std::is_trivially_constructible<route_entry>::value);


            ip_router();
            ~ip_router();

            void cleanup_cache();
            [[nodiscard]] bool validate_tree() const;
            [[nodiscard]] size_t used_memory() const;
            [[nodiscard]] std::string print_as_string() const;
            /**
             * @return Whatever the route register succeeded to initialize
             */
            bool register_route(const sockaddr_storage& /* address */, void* /* target */, void** /* old route */ = nullptr);

            /**
             * @return The old pointer from the route
             */
            void* reset_route(const sockaddr_storage& /* address */);

            [[nodiscard]] void* resolve(const sockaddr_storage& /* address */) const;
        private:
            /**
             * Value 0 will be an empty end
             * Value 1 will points with all pointers to value 0
             * Value 2 will points with all pointers to value 1
             * ... and so on
             */
            static std::array<route_entry, 16> recursive_ends;

            std::mutex register_lock{};
            std::deque<route_entry*> unused_nodes{};

            //std::shared_mutex entry_lock{};
            spin_lock entry_lock{};
            route_entry root_entry{};

            route_entry* create_8bit_entry(size_t /* level */, bool /* as end entry */);
            bool validate_chunk_entry(const ip_router::route_entry* /* current entry */, size_t /* level */) const;
            size_t chunk_memory(const ip_router::route_entry* /* current entry */, size_t /* level */) const;
            void print_as_string(std::string& /* output */, const std::string& /* indent */, const ip_router::route_entry* /* current entry */, size_t /* level */) const;
    };
}