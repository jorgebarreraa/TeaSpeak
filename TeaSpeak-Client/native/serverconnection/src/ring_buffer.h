#pragma once

#include <cstddef>
#include <atomic>

namespace tc {
    class ring_buffer {
        public:
            /* Attention: Actual size may be larger than given capacity */
            explicit ring_buffer(size_t /* minimum capacity */);
            ~ring_buffer();

            [[nodiscard]] size_t capacity() const;
            [[nodiscard]] size_t fill_count() const;
            [[nodiscard]] size_t free_count() const;

            /* do not write more than the capacity! */
            char* write_ptr();
            void advance_write_ptr(size_t /* count */);

            char* calculate_advanced_write_ptr(size_t /* count */);
            char* calculate_backward_write_ptr(size_t /* count */);

            /* do not read more than the capacity! */
            [[nodiscard]] const void* read_ptr() const;
            void advance_read_ptr(size_t /* count */);

            void clear();
            [[nodiscard]] bool valid() const;

            [[nodiscard]] inline operator bool() const { return this->valid(); }
        private:
#ifndef HAVE_SOUNDIO
            struct MirroredMemory {
                size_t capacity;
                char *address;
                void *priv;
            };

            bool allocate_memory(size_t requested_capacity);
            void free_memory();

            MirroredMemory memory{};

            std::atomic_long write_offset{};
            std::atomic_long read_offset{};
            size_t _capacity{0}; /* for faster access */
#endif

            void* handle{nullptr};
    };
}