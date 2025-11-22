#pragma once

#include <string>
#include <typeinfo>

namespace memtrack {
#define TRACK_OBJECT_ALLOCATION
#if defined(FEATURE_MEMTRACK) && defined(TRACK_OBJECT_ALLOCATION)
    extern void allocated(const char* name, void* address);
    extern void freed(const char* name, void* address);
    template <typename T>
    void allocated(void* address) { allocated(typeid(T).name(), address); }

    template <typename T>
    void freed(void* address) { freed(typeid(T).name(), address); }

    void statistics();
#else
    template <typename... T>
    inline void __empty(...) { }

    #define freed __empty
    #define allocated __empty

    #define allocated_mangled __empty
    #define freed_mangled __empty

    inline void statistics() {}
#endif
}