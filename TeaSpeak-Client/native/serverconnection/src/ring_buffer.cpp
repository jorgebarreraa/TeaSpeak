//
// Created by wolverindev on 07.02.20.
//

#include "./ring_buffer.h"
#include <cassert>

#ifdef HAVE_SOUNDIO
using namespace tc;
#include <soundio/soundio.h>

ring_buffer::ring_buffer(size_t cap) {
    this->handle = soundio_ring_buffer_create(nullptr, (int) cap);
}

ring_buffer::~ring_buffer() {
    soundio_ring_buffer_destroy((SoundIoRingBuffer*) this->handle);
}

size_t ring_buffer::capacity() const {
    return soundio_ring_buffer_capacity((SoundIoRingBuffer*) this->handle);
}

size_t ring_buffer::free_count() const {
    return soundio_ring_buffer_free_count((SoundIoRingBuffer*) this->handle);
}

size_t ring_buffer::fill_count() const {
    return soundio_ring_buffer_fill_count((SoundIoRingBuffer*) this->handle);
}

char* ring_buffer::write_ptr() {
    return soundio_ring_buffer_write_ptr((SoundIoRingBuffer*) this->handle);
}

void ring_buffer::advance_write_ptr(size_t bytes) {
    soundio_ring_buffer_advance_write_ptr((SoundIoRingBuffer*) this->handle, (int) bytes);
}

const void* ring_buffer::read_ptr() const {
    return soundio_ring_buffer_read_ptr((SoundIoRingBuffer*) this->handle);
}

void ring_buffer::advance_read_ptr(size_t bytes) {
    soundio_ring_buffer_advance_read_ptr((SoundIoRingBuffer*) this->handle, (int) bytes);
}

void ring_buffer::clear() {
    soundio_ring_buffer_clear((SoundIoRingBuffer*) this->handle);
}
#else

#ifdef WIN32
    #include <windows.h>
    #include <objbase.h>
#else
    #include <cstdlib>
    #include <sys/mman.h>
    #include <zconf.h>
#endif

namespace tc {
#ifdef WIN32
    bool sysinfo_initialized{false};
    static SYSTEM_INFO win32_system_info;

    int soundio_os_page_size() {
        if(!sysinfo_initialized) {
            GetSystemInfo(&win32_system_info);
            sysinfo_initialized = true;
        }
        return win32_system_info.dwAllocationGranularity;
    }
#else
    long int soundio_os_page_size() {
        return sysconf(_SC_PAGESIZE);
    }
#endif

    static inline size_t ceil_dbl_to_size_t(double x) {
        const auto truncation = (size_t) x;
        return truncation + (truncation < x);
    }

    ring_buffer::ring_buffer(size_t min_capacity) {
        auto result = this->allocate_memory(min_capacity);
        assert(result);
    }

    ring_buffer::~ring_buffer() {
        this->free_memory();
    }

    bool ring_buffer::allocate_memory(size_t requested_capacity) {
        size_t actual_capacity = ceil_dbl_to_size_t((double) requested_capacity / (double) soundio_os_page_size()) * soundio_os_page_size();
        assert(actual_capacity > 0);

        this->memory.address = nullptr;
#ifdef WIN32
        BOOL ok;
        HANDLE hMapFile = CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, (DWORD) (actual_capacity * 2), nullptr);
        if (!hMapFile) return false;

        for (;;) {
            // find a free address space with the correct size
            char *address = (char*)MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, actual_capacity * 2);
            if (!address) {
                ok = CloseHandle(hMapFile);
                assert(ok);
                return false;
            }

            // found a big enough address space. hopefully it will remain free
            // while we map to it. if not, we'll try again.
            ok = UnmapViewOfFile(address);
            assert(ok);

            char *addr1 = (char*)MapViewOfFileEx(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, actual_capacity, address);
            if (addr1 != address) {
                DWORD err = GetLastError();
                if (err == ERROR_INVALID_ADDRESS) {
                    continue;
                } else {
                    ok = CloseHandle(hMapFile);
                    assert(ok);
                    return false;
                }
            }

            char *addr2 = (char*)MapViewOfFileEx(hMapFile, FILE_MAP_WRITE, 0, 0, actual_capacity, address + actual_capacity);
            if (addr2 != address + actual_capacity) {
                ok = UnmapViewOfFile(addr1);
                assert(ok);

                DWORD err = GetLastError();
                if (err == ERROR_INVALID_ADDRESS) {
                    continue;
                } else {
                    ok = CloseHandle(hMapFile);
                    assert(ok);
                    return false;
                }
            }

            this->memory.priv = hMapFile;
            this->memory.address = address;
            break;
        }
#else
        char shm_path[] = "/dev/shm/teaclient-XXXXXX";
        char tmp_path[] = "/tmp/teaclient-XXXXXX";
        char *chosen_path;

        int fd = mkstemp(shm_path);
        if (fd < 0) {
            fd = mkstemp(tmp_path);
            if (fd < 0) {
                return false;
            } else {
                chosen_path = tmp_path;
            }
        } else {
            chosen_path = shm_path;
        }

        if (unlink(chosen_path)) {
            close(fd);
            return false;
        }

        if (ftruncate(fd, actual_capacity)) {
            close(fd);
            return false;
        }

        char *address = (char*) mmap(nullptr, actual_capacity * 2, PROT_NONE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
        if (address == MAP_FAILED) {
            close(fd);
            return false;
        }

        char *other_address = (char*) mmap(address, actual_capacity, PROT_READ|PROT_WRITE,
                                          MAP_FIXED|MAP_SHARED, fd, 0);
        if (other_address != address) {
            munmap(address, 2 * actual_capacity);
            close(fd);
            return false;
        }

        other_address = (char*) mmap(address + actual_capacity, actual_capacity,
                                    PROT_READ|PROT_WRITE, MAP_FIXED|MAP_SHARED, fd, 0);
        if (other_address != address + actual_capacity) {
            munmap(address, 2 * actual_capacity);
            close(fd);
            return false;
        }

        this->memory.address = address;

        if (close(fd))
            return false;
#endif

        this->memory.capacity = actual_capacity;
        this->_capacity = actual_capacity;
        return true;
    }

    void ring_buffer::free_memory() {
        if(!this->memory.address) return;

#ifdef WIN32
        BOOL ok;
        ok = UnmapViewOfFile(this->memory.address);
        assert(ok);
        ok = UnmapViewOfFile(this->memory.address + this->memory.capacity);
        assert(ok);
        ok = CloseHandle((HANDLE) this->memory.priv);
        assert(ok);
#else
        int err = munmap(this->memory.address, 2 * this->memory.capacity);
        assert(!err);
#endif

        this->memory.address = nullptr;
    }

    size_t ring_buffer::capacity() const {
        return this->_capacity;
    }

    char* ring_buffer::write_ptr() {
        auto offset{this->write_offset.load()};
        return this->memory.address + (offset % this->memory.capacity);
    }

    void ring_buffer::advance_write_ptr(size_t bytes) {
        this->write_offset.fetch_add((long) bytes);
        assert(this->fill_count() >= 0);
    }

    char* ring_buffer::calculate_advanced_write_ptr(size_t bytes) {
        auto offset{this->write_offset.load() + bytes};
        return this->memory.address + (offset % this->memory.capacity);
    }

    char* ring_buffer::calculate_backward_write_ptr(size_t bytes) {
        bytes %= this->memory.capacity;
        auto offset{this->write_offset.load() + this->memory.capacity - bytes};
        return this->memory.address + (offset % this->memory.capacity);
    }

    const void* ring_buffer::read_ptr() const {
        auto offset{this->read_offset.load()};
        return this->memory.address + (offset % this->memory.capacity);
    }

    void ring_buffer::advance_read_ptr(size_t bytes) {
        this->read_offset.fetch_add((long) bytes);
        assert(this->fill_count() >= 0);
    }

    size_t ring_buffer::fill_count() const {
        // Whichever offset we load first might have a smaller value. So we load
        // the read_offset first.
        auto roffset{this->read_offset.load()};
        auto woffset{this->write_offset.load()};

        int count = woffset - roffset;
        assert(count >= 0);
        assert(count <= this->memory.capacity);
        return count;
    }

    size_t ring_buffer::free_count() const {
        return this->memory.capacity - this->fill_count();
    }

    void ring_buffer::clear() {
        this->write_offset.store(this->read_offset.load());
    }

    bool ring_buffer::valid() const {
        return this->memory.address != nullptr;
    }
}

#endif