#include "buffers.h"
#include <unistd.h>

using namespace std;
using namespace ts;
using namespace ts::protocol;
using namespace ts::buffer;

#pragma GCC optimize ("O3")

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

struct BufferAllocator {
    bool operator()(size_t& length, void*& result) {
        __throw_logic_error("Cant reallocate a fixed buffer");
    }
};

struct Freelist {
    struct Buffer {
        Buffer* next_buffer;
        char data_ptr[0];
    };

    std::function<bool(void*)> deallocator;
    BufferAllocator allocator{};

    fast_lock_t lock{};

    Buffer* head = nullptr;
    Buffer* tail = nullptr;
    ssize_t length = 0;

    size_t buffer_size;
    ssize_t max_length;

#ifdef DEBUG_FREELIST
    std::atomic<size_t> extra_alloc = 0;
    std::atomic<size_t> extra_free = 0;

    std::atomic<size_t> total_alloc = 0;
    std::atomic<size_t> total_free = 0;
#endif

    pipes::buffer next_buffer() {
#ifdef DEBUG_FREELIST
        this->total_alloc++;
#endif
        Buffer* buffer = nullptr;
        {
            lock_guard lock(this->lock);
            if(this->head) {
                buffer = this->head;
                if(buffer == this->tail) {
                    this->tail = nullptr;
                    this->head = nullptr;
                    assert(this->length == 0);
                } else {
                    this->head = buffer->next_buffer;
                    assert(this->length > 0);
                }
                this->length--;
            }
        }
        if(!buffer) {
#ifdef DEBUG_FREELIST
            this->extra_alloc++;
#endif
            buffer = (Buffer*) malloc(sizeof(Buffer) + this->buffer_size);
        }

        return pipes::buffer{(void*) buffer, this->buffer_size + sizeof(Buffer), false, allocator, deallocator}.range(sizeof(Buffer));
    }

    bool release_buffer(void* ptr) {
#ifdef DEBUG_FREELIST
        this->total_free++;
#endif
        auto buffer = (Buffer*) ptr;

        if(this->max_length > 0 && this->length > this->max_length) {
            /* dont push anymore stuff into the freelist! */
#ifdef DEBUG_FREELIST
            this->extra_free++;
#endif
            free(ptr);
            return true;
        }

        {
            lock_guard lock(this->lock);
            if(this->tail) {
                this->tail->next_buffer = buffer;
            } else {
                this->head = buffer;
            }
            this->tail = buffer;
            buffer->next_buffer = nullptr;

            this->length++;
        }

        return true;
    }

    cleaninfo cleanup() {
        size_t buffers_deallocated = 0;

        {
            lock_guard lock(this->lock);
            while(this->head) {
                auto buffer = this->head;
                this->head = buffer->next_buffer;
                free(buffer);

                buffers_deallocated++;
            }

            this->tail = nullptr;
            this->head = nullptr;
            this->length = 0;
        }

        return {buffers_deallocated * 8, buffers_deallocated * this->buffer_size};
    }

    explicit Freelist(size_t size) {
        this->buffer_size = size;
        this->max_length = 1024 * 1024 * 1024; /* 1GB */
        this->deallocator = bind(&Freelist::release_buffer, this, placeholders::_1);
    }
};

Freelist freelists[size::max - size::min] = {
        Freelist(size::byte_length(size::Bytes_512)),
        Freelist(size::byte_length(size::Bytes_1024)),
        Freelist(size::byte_length(size::Bytes_1536))
};

buffer_t buffer::allocate_buffer(size::value size) {
    assert((size - size::min) > 0 && (size - size::min) < (size::max - size::min));
    return freelists[size - size::min].next_buffer();
}

cleaninfo& operator+=(cleaninfo& self, const cleaninfo& other) {
    self.bytes_freed_internal += other.bytes_freed_internal;
    self.bytes_freed_buffer += other.bytes_freed_buffer;
    return self;
}

cleaninfo buffer::cleanup_buffers(cleanmode::value mode) {
    cleaninfo info{0, 0};
    info += freelists[0].cleanup();
    info += freelists[1].cleanup();
    info += freelists[2].cleanup();
    return info;
}
//cleanup_buffers
//$4 = {bytes_freed_internal = 8389144, bytes_freed_buffer = 536948736}