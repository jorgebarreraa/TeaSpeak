#include "buffers.h"
#include <unistd.h>

using namespace std;
using namespace ts;
using namespace ts::protocol;
using namespace ts::buffer;

#pragma GCC optimize ("O3")

#define BLOCK_BUFFER_MASK 0xFFFFFFFF
#define BLOCK_BUFFERS 32

#define MEM_BUFFER_MAGIC 0xABCD
#define MEM_BLOCK_MAGIC 0xCDEF

class spin_lock {
        std::atomic_flag locked = ATOMIC_FLAG_INIT;
    public:
        void lock() {
            uint8_t round = 0;
            while (locked.test_and_set(std::memory_order_acquire)) {
                //Yield when we're using this lock for a longer time, which we usually not doing
                if(round++ % 8 == 0)
                    std::this_thread::yield();
            }
        }

        inline bool try_lock() {
            return !locked.test_and_set(std::memory_order_acquire);
        }

        void unlock() {
            locked.clear(std::memory_order_release);
        }
};
typedef spin_lock fast_lock_t;

#pragma pack(push, 1)
struct buffer_entry_head {
#ifdef MEM_BUFFER_MAGIC
    uint16_t magic;
#endif
    uint32_t base_offset : 24;
    uint8_t base_index : 8;
};

struct buffer_entry : buffer_entry_head {
    char buffer[0];
};

struct block_chain;

/* 32 buffers/block */
struct buffer_block {
#ifdef MEM_BLOCK_MAGIC
    uint16_t magic;
#endif
    uint8_t block_index; /* block index within the chain */
    block_chain* chain_entry;
    fast_lock_t block_lock{};
    size::value type = size::unset;
    union {
        uint32_t flag_free = 0;
        uint8_t    flag_used8[4];
    };

    buffer_entry buffers[0];
};
#pragma pack(pop)

struct block_chain {
    uint8_t type_index[buffer::size::max - buffer::size::min];
    block_chain* previous = nullptr;
    block_chain* next = nullptr;

    uint8_t block_count = 0;
    buffer_block* blocks[0];
};

fast_lock_t chain_lock;
block_chain* chain_head = nullptr;

inline void destroy_block(buffer_block* block) {
    block->block_lock.~fast_lock_t();
    free(block);
}

inline buffer_block* allocate_block(size::value type) {
    auto base_size = sizeof(buffer_block);
    auto buffer_size = size::byte_length(type);
    auto base_entry_size = sizeof(buffer_entry) + buffer_size;
    auto size = base_size + BLOCK_BUFFERS * base_entry_size;

    auto block = (buffer_block*) malloc(size);
    new (&block->block_lock) fast_lock_t(); /* initialize spin lock */
#ifdef MEM_BLOCK_MAGIC
    block->magic = MEM_BLOCK_MAGIC;
#endif
    block->type = type;

    for(uint8_t index = 0; index < BLOCK_BUFFERS; index++) {
        auto entry_ptr = (uintptr_t) block->buffers + index * (uintptr_t) base_entry_size;
        ((buffer_entry*) entry_ptr)->base_offset = index * (uintptr_t) base_entry_size + sizeof(buffer_block);
        ((buffer_entry*) entry_ptr)->base_index = index;
    }
    block->flag_free = BLOCK_BUFFER_MASK;
    return block;
}

inline void destroy_chain_entry(block_chain* chain) {
    free(chain);
}

inline block_chain* allocate_chain_entry(uint8_t entries) {
    auto base_size = sizeof(block_chain);
    auto chain = (block_chain*) malloc(base_size + sizeof(void*) * entries);

    chain->next = nullptr;
    chain->previous = nullptr;
    chain->block_count = entries;
    for(auto& index : chain->type_index)
        index = 0;
    for(uint8_t index = 0; index < entries; index++)
        chain->blocks[index] = nullptr;

    return chain;
}

struct BufferDeallocator {
    bool operator()(void* buffer) {
        buffer::release_buffer(buffer);
        return true;
    }
};

struct BufferAllocator {
    bool operator()(size_t& /* length */, void*& /* result ptr */) {
        __throw_logic_error("Cant reallocate a fixed buffer");
    }
};

buffer_t buffer::allocate_buffer(size::value size) {
    return pipes::buffer{size};

    fast_lock_t* block_lock = nullptr;
    buffer_block* block;

    {
        block_chain* tmp_chain;

        lock_guard lock_chain(chain_lock);
        auto head = chain_head;

        auto type_index = (size::value) (size - size::min);
        iterate_head:
        while(head) {
            uint8_t& index = head->type_index[type_index];
            while(index < head->block_count) {
                auto entry = head->blocks[index];
                if(entry) {
                    if(entry->type != size || (entry->flag_free & BLOCK_BUFFER_MASK) == 0)
                        goto next_block;

                    block_lock = &entry->block_lock;
                    if(!block_lock->try_lock() || (entry->flag_free & BLOCK_BUFFER_MASK) == 0) {
                        block_lock->unlock();
                        goto next_block;
                    }

                    block = entry;
                    /* we've found an entry with a free block */
                    goto break_head_loop;
                } else {
                    /* lets insert a new block */
                    head->blocks[index] = (entry = allocate_block(size));
                    entry->chain_entry = head;
                    entry->block_index = index;

                    block_lock = &entry->block_lock;
                    block_lock->lock();

                    block = entry;
                    /* we've a new block which has to have free slots */
                    goto break_head_loop;
                }

                next_block:
                index++;
            }

            if(!head->next)
                break;

            head = head->next;
        }

        tmp_chain = allocate_chain_entry(128);
        tmp_chain->previous = head;

        if(!head) { /* we've to create a chain head */
            chain_head = (head = tmp_chain);
        } else { /* we've to append a new entry */
            head = (head->next = tmp_chain);
        }
        goto iterate_head;
        /* insert new entry */

        break_head_loop:;
    }

    auto index = __builtin_ctz(block->flag_free);
    block->flag_free &= ~(1 << index);
    block_lock->unlock();

    auto buffer_size = size::byte_length(size);
    auto buffer_entry_size = buffer_size + sizeof(buffer_entry_head);
    auto entry = (buffer_entry_head*) ((uintptr_t) block->buffers + (uintptr_t) buffer_entry_size * index);
#ifdef MEM_BUFFER_MAGIC
    entry->magic = MEM_BUFFER_MAGIC;
#endif

    return pipes::buffer{(void*) entry, buffer_entry_size, false, BufferAllocator(), BufferDeallocator()}.range(sizeof(buffer_entry_head));
}

inline bool valid_buffer_size(size_t size) {
    return size == 512 || size == 1024 || size == 1536;
}

void buffer::release_buffer(void *buffer) {
    return;

    auto head = (buffer_entry_head*) buffer;
#ifdef MEM_BUFFER_MAGIC
    assert(head->magic == MEM_BUFFER_MAGIC);
#endif

    auto block = (buffer_block*) ((uintptr_t) head - (uintptr_t) head->base_offset);
#ifdef MEM_BLOCK_MAGIC
    assert(block->magic == MEM_BLOCK_MAGIC);
#endif

    block->flag_free |= (1 << head->base_index); /* set the slot free flag */

    auto type_index = (size::value) (block->type - size::min);
    auto& index = block->chain_entry->type_index[type_index];
    if(index > block->block_index)
        index = block->block_index;
}

void buffer::release_buffer(pipes::buffer *buffer) {
    assert(buffer->capacity_origin() > sizeof(buffer_entry_head));
    assert(valid_buffer_size(buffer->capacity_origin() - sizeof(buffer_entry_head)));

    release_buffer(buffer->data_ptr_origin());
    delete buffer;
}

meminfo buffer::buffer_memory() {
    size_t bytes_buffer = 0;
    size_t bytes_buffer_used = 0;
    size_t bytes_internal = 0;
    size_t nodes = 0;
    size_t nodes_full = 0;

    {
        lock_guard lock_chain(chain_lock);
        auto head = chain_head;
        bool full;
        while(head) {
            full = true;
            nodes++;
            bytes_internal += sizeof(chain_head) + sizeof(void*) * head->block_count;

            for(uint8_t index = 0; index < head->block_count; index++) {
                auto block = head->blocks[index];
                if(block) {
                    bytes_internal += sizeof(buffer_block);
                    bytes_internal += sizeof(buffer_entry) * BLOCK_BUFFERS;

                    full = full && (block->flag_free & BLOCK_BUFFER_MASK) == 0;
                    auto length = size::byte_length(block->type);
                    bytes_buffer += length * BLOCK_BUFFERS;
                    bytes_buffer_used += (BLOCK_BUFFERS - __builtin_popcount(block->flag_free & BLOCK_BUFFER_MASK)) * length;
                } else
                    full = false;
            }

            head = head->next;
        }
    }

    return {bytes_buffer, bytes_buffer_used, bytes_internal, nodes, nodes_full};
}

cleaninfo buffer::cleanup_buffers(cleanmode::value mode) {
    std::deque<buffer_block*> orphaned_blocks;
    std::deque<block_chain*> orphaned_chunks;

    bool flag_blocks = (mode & cleanmode::BLOCKS) > 0;
    bool flag_chunks = (mode & cleanmode::CHUNKS) > 0;
    {
        lock_guard lock_chain(chain_lock);

        auto head = chain_head;
        uint32_t free_value = BLOCK_BUFFER_MASK;
        vector<unique_lock<fast_lock_t>> block_locks;

        while(head) {
            bool flag_used = false;

            if(flag_blocks) {
                for(uint8_t index = 0; index < head->block_count; index++) {
                    auto block = head->blocks[index];
                    if(!block) continue; /* block isn't set */

                    if(block->flag_free == free_value) {
                        lock_guard block_lock(block->block_lock);
                        if(block->flag_free != free_value) { /* block had been used while locking */
                            flag_used |= true;
                            continue;
                        }

                        head->blocks[index] = nullptr;
                        orphaned_blocks.push_back(block);
                    } else {
                        flag_used |= true;
                    }
                }
            }

            if(flag_chunks) {
                if(!flag_blocks) { /* we've to calculate flag_used */
                    block_locks.resize(head->block_count);

                    for(uint8_t index = 0; index < head->block_count; index++) {
                        auto block = head->blocks[index];
                        if(block) {
                            block_locks[index] = unique_lock{block->block_lock};
                            if(block->flag_free == free_value) {
                                /* delete that block later */
                                continue;
                            }

                            flag_used = true;
                            break;
                        }
                    }
                }

                if(!flag_used) {
                    if(head->previous)
                        head->previous->next = head->next;
                    else if(head == chain_head)
                        chain_head = head->next;

                    if(head->next)
                        head->next->previous = head->previous;

                    orphaned_chunks.push_back(head);
                }

                block_locks.clear();
            }

            head = head->next;
        }
    }

    size_t bytes_internal = 0;
    size_t bytes_buffer = 0;

    for(auto chain : orphaned_chunks) {
        for(uint8_t index = 0; index < chain->block_count; index++) {
            if(chain->blocks[index])
                orphaned_blocks.push_back(chain->blocks[index]);
        }
        bytes_internal += sizeof(block_chain);

        destroy_chain_entry(chain);
    }

    for(auto block : orphaned_blocks) {
        bytes_buffer += size::byte_length(block->type) * BLOCK_BUFFERS;
        bytes_internal += sizeof(buffer_entry_head) * BLOCK_BUFFERS;
        bytes_internal += sizeof(buffer_block);

        destroy_block(block);
    }

    return {bytes_internal, bytes_buffer};
}