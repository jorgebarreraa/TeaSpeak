#pragma once

#include <list>
#include <chrono>
#include <memory>
#include <cstring>
#include <sstream>
#include <cassert>
#include <utility>

#include "./Packet.h"
#include "../misc/queue.h"

namespace ts::buffer {
    struct RawBuffer {
        public:
            RawBuffer() : RawBuffer(0) {}
            RawBuffer(size_t length) : index(0), length(length) {
                if(length > 0) buffer = (char *) malloc(length);
                else buffer = nullptr;
                this->length = length;
                this->index = 0;
            }

            RawBuffer(const RawBuffer &other) : RawBuffer(other.length) {
                if(other.length > 0) memcpy(this->buffer, other.buffer, this->length);
                this->index = other.index;
            }

            virtual ~RawBuffer() {
                if(buffer)
                    free(buffer);
                this->buffer = nullptr;
            }

            void slice(size_t length) {
                char *oldBuff = this->buffer;

                this->buffer = (char *) malloc(length);
                memcpy(this->buffer, oldBuff, length);
                this->length = length;

                free(oldBuff);
            }

            char *buffer = nullptr;
            size_t length = 0;
            size_t index = 0;

            TAILQ_ENTRY(ts::buffer::RawBuffer) tail;
    };

    struct size {
        enum value : uint8_t {
            unset,
            min,
            Bytes_512 = min,
            Bytes_1024,
            Bytes_1536,
            max
        };

        static inline size_t byte_length(value size) {
            switch (size) {
                case Bytes_512:
                    return 512;
                case Bytes_1024:
                    return 1024;
                case Bytes_1536:
                    return 1536;
                case unset:
                case max:
                default:
                    return 0;
            }
        }
    };

    //typedef std::unique_ptr<pipes::buffer, void(*)(pipes::buffer*)> buffer_t;
    typedef pipes::buffer buffer_t;

    extern buffer_t allocate_buffer(size::value /* size */);
    inline buffer_t allocate_buffer(size_t length) {
        pipes::buffer result;
        if(length <= 512)
            result = allocate_buffer(size::Bytes_512);
        else if(length <= 1024)
            result = allocate_buffer(size::Bytes_1024);
        else if(length <= 1536)
            result = allocate_buffer(size::Bytes_1536);
        else {
            return pipes::buffer{length};
        }
        result.resize(length);
        return result;
    }

    struct cleaninfo {
        size_t bytes_freed_internal;
        size_t bytes_freed_buffer;
    };
    struct cleanmode {
        enum value {
            CHUNKS = 0x01,
            BLOCKS = 0x02,

            CHUNKS_BLOCKS = 0x03
        };
    };
    extern cleaninfo cleanup_buffers(cleanmode::value /* mode */);

    struct meminfo {
        size_t bytes_buffer = 0;
        size_t bytes_buffer_used = 0;
        size_t bytes_internal = 0;

        size_t nodes = 0;
        size_t nodes_full = 0;
    };
    extern meminfo buffer_memory();
}