#include "log/LogUtils.h"

#include <mutex>
#include <array>
#include <deque>
#include <map>
#include <typeindex>

#define TRACK_OBJECT_ALLOCATION
#include "memtracker.h"

#ifdef NDEBUG
    #define NO_IMPL //For fast disable (e.g. when you dont want to recompile the whole source)
#else
    #define NO_IMPL
#endif

#ifndef __GLIBC__
    #define _GLIBCXX_NOEXCEPT
#endif
#ifndef WIN32
    #include <cxxabi.h>
#endif
#ifdef WIN32
    typedef int64_t ssize_t;
#endif

//#define MEMTRACK_VERBOSE

inline bool should_track_mangled(const char* mangled) {
    if(strstr(mangled, "ViewEntry")) return true;
    if(strstr(mangled, "ViewEntry")) return true;
    if(strstr(mangled, "ClientChannelView")) return true;
    if(strstr(mangled, "LinkedTreeEntry")) return true;

    return false;
}

using namespace std;
namespace memtrack {
    struct TypeInfo {
        const char* name;
        std::string mangled;

        explicit TypeInfo(const char* name) : name(name) {}

        bool operator==(const TypeInfo& other) {
            return other.name == this->name || strcmp(other.name, this->name) == 0;
        }
        bool operator!=(const TypeInfo& other) {
            return ! this->operator==(other);
        }

        bool operator<(const TypeInfo& __rhs) const noexcept
        { return this->before(__rhs); }

        bool operator<=(const TypeInfo& __rhs) const noexcept
        { return !__rhs.before(*this); }

        bool operator>(const TypeInfo& __rhs) const noexcept
        { return __rhs.before(*this); }

        bool operator>=(const TypeInfo& __rhs) const noexcept
        { return !this->before(__rhs); }

        inline bool before(const TypeInfo& __arg) const _GLIBCXX_NOEXCEPT
        { return (name[0] == '*' && __arg.name[0] == '*')
                 ? name < __arg.name
                 : strcmp (name, __arg.name) < 0; }

        inline std::string as_mangled() {
            if(!this->mangled.empty())
                return this->mangled;
#ifndef WIN32
            int status;
            std::unique_ptr<char[], void (*)(void*)> result(abi::__cxa_demangle(name, nullptr, nullptr, &status), std::free);
            if(status != 0)
                return "error: " + to_string(status);

            this->mangled = result.get();
#else
            //FIXME Implement!
            this->mangled = this->name;
#endif
            return this->mangled;
        }
    };
    class entry {
        public:
            /* std::string name; */
            size_t type{};
            void* address = nullptr;

            entry() = default;
            entry(size_t type, void* address) : type(type), address(address) {}
            ~entry() = default;
    };

    template <int N>
    class brick {
        public:
            inline bool insert(size_t type, void* address) {
                auto slot = free_slot();
                if(slot == N) return false;
                entries[slot] = entry{type, address};
                findex = slot + 1;
                return true;
            }

            inline bool remove(size_t type, void* address) {
                for(int index = 0; index < N; index++) {
                    auto& e = entries[index];
                    if(e.address == address && e.type == type) {
                        e = entry{};
                        findex = index;
                        return true;
                    }
                }
                return false;
            }

            inline int capacity() { return N; }

            array<entry, N> entries;
        private:
            inline int free_slot() {
                while (findex < N && entries[findex].address) findex++;
                return findex;
            }
            int findex = 0;
    };
    typedef brick<1024> InfoBrick;

    template <typename T, T N>
    struct DefaultValued {
        T value = N;
    };


    map<TypeInfo, DefaultValued<ssize_t, -1>> type_indexes;
    vector<InfoBrick*> bricks;
    mutex bricks_lock;

    void allocated(const char* name, void* address) {
#ifdef NO_IMPL
        return;
#else
#ifdef MEMTRACK_VERBOSE
        logTrace(lstream << "[MEMORY] Allocated a new instance of '" << name << "' at " << address);
#endif
        if(!should_track_mangled(name)) return;

        lock_guard<mutex> lock(bricks_lock);
        TypeInfo local_info(name);
        auto& type_index = type_indexes[local_info];
        if(type_index.value == -1) {
            type_index.value = type_indexes.size() - 1;
        }

        auto _value = (size_t) type_index.value;
        for(auto it = bricks.begin(); it != bricks.end(); it++)
            if((*it)->insert(_value, address)) return;
        bricks.push_back(new InfoBrick{});
        auto success = bricks.back()->insert(type_index.value, address);
        assert(success);
#endif
    }

    void freed(const char* name, void* address) {
#ifdef NO_IMPL
        return;
#else
#ifdef MEMTRACK_VERBOSE
        logTrace(lstream << "[MEMORY] Deallocated a instance of '" << name << "' at " << address);
#endif
        if(!should_track_mangled(name)) return;

        lock_guard<mutex> lock(bricks_lock);
        TypeInfo local_info(name);
        auto& type_index = type_indexes[local_info];
        if(type_index.value == -1)
            type_index.value = type_indexes.size() - 1;

        auto _value = (size_t) type_index.value;
        for (auto &brick : bricks)
            if(brick->remove(_value, address)) return;
        logError(LOG_GENERAL, "[MEMORY] Got deallocated notify, but never the allocated! (Address: {} Name: {})", address, name);
#endif
    }

    void statistics() {
#ifdef NO_IMPL
        logError(LOG_GENERAL, "memtracker::statistics() does not work due compiler flags (NO_IMPL)");
        return;
#else
        map<size_t, deque<void*>> objects;
        map<size_t, std::string> mapping;

        {
            lock_guard<mutex> lock(bricks_lock);
            for(auto& brick : bricks)
                for(auto& entry : brick->entries)
                    if(entry.address) {
                        objects[entry.type].push_back(entry.address);
                    }
            for(auto& type : type_indexes)
                mapping[type.second.value] = type.first.as_mangled();
        }

        logMessage(LOG_GENERAL, "Allocated object types: " + to_string(objects.size()));
            for(const auto& entry : objects) {
                logMessage(LOG_GENERAL, "  " + mapping[entry.first] + ": " + to_string(entry.second.size()));
                if (entry.second.size() < 50) {
                    stringstream ss;
                    for (int index = 0; index < entry.second.size(); index++) {
                        if (index % 16 == 0) {
                            if (index + 1 >= entry.second.size()) break;
                            if (index != 0)
                                logMessage(LOG_GENERAL, ss.str());
                            ss = stringstream();
                            ss << "    ";
                        }
                        ss << entry.second[index] << "  ";
                    }
                    if (!ss.str().empty())
                        logMessage(LOG_GENERAL, ss.str());
                } else {
                    logMessage(LOG_GENERAL, "<snipped>");
                }
            }
#endif
    }
}