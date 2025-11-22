//
// Created by WolverinDEV on 08/03/2020.
//

#include <cassert>
#include <cstring>
#include <netinet/in.h>
#include "ip_router.h"

using namespace ts::network;


constexpr static ip_router::route_entry generate_empty_end_node(void*) {
    ip_router::route_entry result{};
    for(auto& ptr : result.data)
        ptr = nullptr;
    return result;
}

template <size_t N>
constexpr std::array<ip_router::route_entry, N> generate_default_table() noexcept {
    std::array<ip_router::route_entry, N> result{};

    for(size_t index{0}; index < result.size(); index++) {
        result[index].use_count = ip_router::route_entry::const_flag_mask | 0xFFU;
        result[index].deep = index + 1;
    }

    return result;
}

std::array<ip_router::route_entry, 16> ip_router::recursive_ends = generate_default_table<16>();

struct sockaddr_storage_info {
    size_t address_offset{0};
    size_t address_length{0};
    size_t chunk_offset{0};
};

constexpr std::array<sockaddr_storage_info, 16> generate_storage_info() noexcept {
    std::array<sockaddr_storage_info, 16> result{};
    for(size_t type{0}; type < result.size(); type++) {
        if(type == AF_INET) {
            result[type].address_length = 4;
            result[type].chunk_offset = 12;

            sockaddr_in address{};
            result[type].address_offset = (uintptr_t) &address.sin_addr.s_addr - (uintptr_t) &address;
        } else if(type == AF_INET6) {
            result[type].address_length = 16;
            result[type].chunk_offset = 0;

            sockaddr_in6 address{};
            result[type].address_offset = (uintptr_t) &address.sin6_addr.__in6_u.__u6_addr8 - (uintptr_t) &address;
        }
    }

    return result;
}

std::array<sockaddr_storage_info, 16> storage_infos = generate_storage_info();

inline void address_to_chunks(uint8_t* chunks, const sockaddr_storage &address) {
    if constexpr (AF_INET < 16 && AF_INET6 < 16) {
        /* converter without branches (only one within the memcpy) */
        const auto& info = storage_infos[address.ss_family & 0xFU];

        memset(chunks, 0, 16);
        memcpy(chunks + info.chunk_offset, ((uint8_t*) &address) + info.address_offset, info.address_length); /* we could do this memcpy more performant */
    } else {
        /* converter with branches */
        if(address.ss_family == AF_INET) {
            auto address4 = (sockaddr_in*) &address;

            memset(chunks, 0, 12);
            memcpy(chunks + 12, &address4->sin_addr.s_addr, 4);
        } else if(address.ss_family == AF_INET6) {
            auto address6 = (sockaddr_in6*) &address;
            memcpy(chunks, address6->sin6_addr.__in6_u.__u6_addr8, 16);
        } else {
            memset(chunks, 0, 16);
        }
    }
}

ip_router::ip_router() {
    for(auto& data : this->root_entry.data)
        data = &ip_router::recursive_ends[14];
    this->root_entry.deep = 1;
    this->root_entry.use_count = ip_router::route_entry::const_flag_mask | 0xFFU;
}

inline void delete_route_entry(ip_router::route_entry* entry, size_t level) {
    level -= entry->deep;
    if(level != 0) {
        for(auto& data : entry->data) {
            auto e = (ip_router::route_entry*) data;
            if(e->is_const_entry()) continue;

            delete_route_entry(e, level);
        }
    }
    delete entry;
}

ip_router::~ip_router() {
    for(auto& entry : this->unused_nodes)
        delete entry;

    for(auto& data : this->root_entry.data) {
        auto entry = (ip_router::route_entry*) data;
        if(entry->is_const_entry()) continue;

        delete_route_entry(entry, 16 - this->root_entry.deep);
    }
}

/*
 * Because we're only reading memory here, and that even quite fast we do not need to lock the register lock.
 * Even if a block gets changed, it will not be deleted immediately. So we should finish reading first before that memory get freed.
 */
void* ip_router::resolve(const sockaddr_storage &address) const {
    uint8_t address_chunks[16];
    address_to_chunks(address_chunks, address);
    const ip_router::route_entry* current_chunk = &this->root_entry;

    std::lock_guard lock{this->entry_lock};
    //std::shared_lock lock{this->entry_lock};

    size_t byte_index{0};
    while(true) {
        byte_index += current_chunk->deep;
        if(byte_index == 16) break;
        assert(byte_index < 16);

        current_chunk = (ip_router::route_entry*) current_chunk->data[address_chunks[byte_index - 1]];
    };

    if(memcmp(address_chunks, current_chunk->previous_chunks, 15) != 0)
        return nullptr; /* route does not match */

    return current_chunk->data[address_chunks[15]];
}

bool ip_router::register_route(const sockaddr_storage &address, void *target, void ** old_target) {
    uint8_t address_chunks[16];
    address_to_chunks(address_chunks, address);

    void* _temp_old_target{};
    if(!old_target)
        old_target = &_temp_old_target;

    ip_router::route_entry* current_chunk = &this->root_entry;
    std::lock_guard rlock{this->register_lock};

    size_t byte_index{0};
    while(true) {
        byte_index += current_chunk->deep;
        if(byte_index == 16) break;
        assert(byte_index < 16);

        /* for the first iteration  no previous_chunks check for "current_chunk" is needed because it will always match! */
        auto& next_chunk = (ip_router::route_entry*&) current_chunk->data[address_chunks[byte_index - 1]];
        if(next_chunk->is_const_entry()) {
            /* perfect, lets allocate our own end and we're done */
            //assert(next_chunk == &ip_rounter::recursive_ends[15 - index - 1]);

            auto allocated_entry = this->create_8bit_entry(byte_index, true);
            if(!allocated_entry) return false;

            memcpy(allocated_entry->previous_chunks, address_chunks, 15);

            /* no lock needed here, just a pointer exchange */
            next_chunk = allocated_entry;
            current_chunk->use_count++;
            current_chunk = next_chunk;
            break; /* end chunk now */
        } else if(next_chunk->deep > 1) {
            ssize_t unmatch_index{-1};
            for(size_t i{0}; i < next_chunk->deep - 1; i++) {
                if(next_chunk->previous_chunks[byte_index + i] != address_chunks[byte_index + i]) {
                    unmatch_index = i;
                    break;
                }
            }

            if(unmatch_index >= 0) {
                auto allocated_entry = this->create_8bit_entry(byte_index + unmatch_index, false);
                if(!allocated_entry) return false;

                allocated_entry->deep = unmatch_index + 1;
                allocated_entry->use_count++;
                allocated_entry->data[next_chunk->previous_chunks[byte_index + unmatch_index]] = next_chunk;
                memcpy(allocated_entry->previous_chunks, address_chunks, 15);

                {
                    std::lock_guard elock{this->entry_lock};
                    next_chunk->deep = next_chunk->deep - unmatch_index - 1;
                    next_chunk = allocated_entry;
                }
                current_chunk = next_chunk;
                continue;
            } else {
                /* every bit matched we also have this nice jump */
            }
        }

        current_chunk = next_chunk;
    }

    *old_target = std::exchange(current_chunk->data[address_chunks[15]], target);
    if(!*old_target) current_chunk->use_count++;
    return true;
}

ip_router::route_entry *ip_router::create_8bit_entry(size_t level, bool end_entry) {
    ip_router::route_entry *result;

    if(this->unused_nodes.empty())
        result = new ip_router::route_entry{};
    else {
        result = this->unused_nodes.front();
        this->unused_nodes.pop_front();
    }

    result->use_count = 0;
    if(end_entry) {
        /* this is an end chunk now */
        result->deep = 16 - level;
        for(auto& data : result->data)
            data = nullptr;
    } else {
        assert(level <= 14);
        result->deep = 1;

        auto pointer = &ip_router::recursive_ends[15 - level - 1];
        for(auto& data : result->data)
            data = pointer;
    }

    return result;
}

void *ip_router::reset_route(const sockaddr_storage &address) {
    uint8_t address_chunks[16];
    address_to_chunks(address_chunks, address);

    ip_router::route_entry* current_chunk{&this->root_entry};
    std::lock_guard rlock{this->register_lock};

    {
        size_t byte_index{0};
        while(true) {
            byte_index += current_chunk->deep;
            if(byte_index == 16) break;
            assert(byte_index < 16);

            current_chunk = (ip_router::route_entry*) current_chunk->data[address_chunks[byte_index - 1]];
            if(current_chunk->is_const_entry()) return nullptr;
        };

        if(memcmp(address_chunks, current_chunk->previous_chunks, 15) != 0)
            return nullptr; /* route does not match */
    }

    auto old = current_chunk->data[address_chunks[15]];
    if(!old) return nullptr;

    if(--current_chunk->use_count == 0) {
        while(true) {
            size_t byte_index{0};

            current_chunk = &this->root_entry;
            while(true) {
                byte_index += current_chunk->deep;
                if(byte_index == 16) break;
                assert(byte_index < 16);

                auto& next_chunk = (ip_router::route_entry*&) current_chunk->data[address_chunks[byte_index - 1]];
                if(next_chunk->deep + byte_index == 16) {
                    assert(next_chunk->use_count == 0);
                    this->unused_nodes.push_back(next_chunk);

                    /* this is the last chunk */
                    next_chunk = &ip_router::recursive_ends[15 - byte_index];
                    if(--current_chunk->use_count > 0) goto exit_delete_loop;
                }

                current_chunk = next_chunk;
            }
        }

        exit_delete_loop:;
    }
    return old;
}

void ip_router::cleanup_cache() {
    std::lock_guard rlock{this->register_lock};
    for(auto node : this->unused_nodes)
        delete node;
    this->unused_nodes.clear();
}

bool ip_router::validate_chunk_entry(const ip_router::route_entry* current_entry, size_t level) const {
    if(level == 0)
        return true;

    if(current_entry->is_const_entry() && level != 16)
        return level == current_entry->deep;

    auto default_pointer = &ip_router::recursive_ends[level - 1];
    for(const auto& data_ptr : current_entry->data) {
        if(data_ptr == default_pointer)
            continue;

        if(!this->validate_chunk_entry((const ip_router::route_entry*) data_ptr, level - current_entry->deep))
            return false;
    }

    return true;
}

bool ip_router::validate_tree() const {
    std::lock_guard rlock{this->register_lock};

    /* first lets validate all const chunks */
    for(size_t index{0}; index < 16; index++) {
        if(!ip_router::recursive_ends[index].is_const_entry())
            return false;

        if(ip_router::recursive_ends[index].deep != index + 1)
            return false;

        for(const auto& data_ptr : ip_router::recursive_ends[index].data)
            if(data_ptr)
                return false;
    }

    /* not lets check our tree */
    return this->validate_chunk_entry(&this->root_entry, 16);
}

size_t ip_router::chunk_memory(const ip_router::route_entry *current_entry, size_t level) const {
    size_t result{sizeof(ip_router::route_entry)};

    level -= current_entry->deep;
    if(level > 0) {
        for(const auto& data_ptr : current_entry->data) {
            auto entry = (const ip_router::route_entry*) data_ptr;
            if(entry->is_const_entry()) continue;

            result += chunk_memory(entry, level);
        }
    }

    return result;
}

size_t ip_router::used_memory() const {
    return this->chunk_memory(&this->root_entry, 16);
}

std::string ip_router::print_as_string() const {
    std::string result{};
    this->print_as_string(result, "", &this->root_entry, 16);
    result += "Memory used: " + std::to_string(this->used_memory() / 1024) + "kb";
    return result;
}

template <typename I>
std::string n2hexstr(I w, size_t hex_len = sizeof(I)<<1) {
    if(w == 0) return "0x0";
    static const char* digits = "0123456789ABCDEF";
    std::string rc(hex_len,'0');
    for (size_t i=0, j=(hex_len-1)*4 ; i<hex_len; ++i,j-=4)
        rc[i] = digits[(w >> j) & 0x0f];
    size_t lz{0};
    for(;lz < rc.length() && rc[lz] == '0'; lz++);
    return "0x" + rc.substr(lz);
}

template <typename T>
inline std::string padded_num(T value) {
    auto result = std::to_string(value);
    return result.length() > 3 ? "" : std::string(3 - result.length(), '0') + result;
}

void ip_router::print_as_string(std::string& result, const std::string& indent, const ip_router::route_entry *current_entry, size_t level) const {
    level -= current_entry->deep;

    size_t range_begin{0};
    for(size_t i = 0; i <= 0xFF; i++) {
        auto entry = (const ip_router::route_entry*) current_entry->data[i];
        if(level == 0 ? !entry : entry->is_const_entry()) continue;

        if(i > 0) {
            if(range_begin < i - 1)
                result += indent + padded_num(range_begin) + ".." + padded_num(i - 1) + ": empty\n";
            else if(range_begin == i - 1)
                result += indent + padded_num(range_begin) + ": empty\n";
        }

        if(level == 0) {
            result += indent + padded_num(i) + ": " + n2hexstr((uintptr_t) entry) + "\n";
        } else {
            result += indent + padded_num(i) + ": " + n2hexstr((uintptr_t) entry) + " (used by: " + std::to_string(entry->use_count) + ", deph: " + std::to_string(entry->deep) + ")\n";
            this->print_as_string(result, indent + "  ", entry, level);
        }
        range_begin = i + 1;
    }

    if(range_begin < 0xFF)
        result += indent + padded_num(range_begin) + "..255: empty\n";
}