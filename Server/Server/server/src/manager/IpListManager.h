#pragma once

#include <netinet/in.h>
#include <deque>
#include <string>

namespace ts {
    class IpListManager {
            struct IPEntry {
                sockaddr_storage address;
                uint8_t range; /* [0;32] or [0;128] */
            };
        public:
            IpListManager(std::string , const std::deque<std::string>&);

            bool reload(std::string&);

            std::string file_name() { return this->file; }
            std::deque<IPEntry> addresses() { return this->entries; }

            bool contains(const sockaddr_storage& address);
        private:
            std::string file;
            std::deque<IPEntry> entries;
            std::deque<std::string> default_entries;

            bool parse_entry(IPEntry& result, const std::string& /* line */);
    };
}