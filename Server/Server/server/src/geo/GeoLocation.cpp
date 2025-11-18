#include <arpa/inet.h>
#include <log/LogUtils.h>
#include "GeoLocation.h"

using namespace std;
using namespace geoloc;

InfoProvider<CountryInfo>* geoloc::provider = nullptr;
InfoProvider<VPNInfo>* geoloc::provider_vpn = nullptr;

IpAddress_t impl::inet_addr(const std::string &ipv4) {
    return ::inet_addr(ipv4.c_str());
}

std::shared_ptr<void> RangedIPProviderBase::_resolveInfo(IpAddress_t address, bool enforce_range) {
    auto beAddr = ip_swap_order(address);
    int16_t index = this->index(beAddr);

    while(index >= 0) {
        auto &list = mapping[index];
        RangeEntryMapping<void>* closest = nullptr;
        for(auto& entry : list) {
            if(entry.startAddress > beAddr)
                continue;
            if(entry.endAddress >= entry.startAddress && enforce_range) {
                if(entry.endAddress < beAddr) continue;
            }
            if(!closest || closest->startAddress < entry.startAddress)
                closest = &entry;
        }
        if(closest)
            return closest->data;
        if(enforce_range) break;
        index--;
    }
    return nullptr;
}

void RangedIPProviderBase::_registerRange(const RangeEntryMapping<void>& entry) {
    this->mapping[this->index(entry.startAddress)].push_back(entry);
    if(entry.endAddress > 0 && this->index(entry.endAddress) != this->index(entry.startAddress))
        this->mapping[this->index(entry.endAddress)].push_back(entry);
}



/** CVSFileBasedProvider **/
CVSFileBasedProviderBase::CVSFileBasedProviderBase(const std::string &file) : fileName(file) {}

bool geoloc::CVSFileBasedProviderBase::loadCVS(std::string &err) {
    std::ifstream file(this->fileName);
    if(!file.good()) {
        err = "could not open file!";
        return false;
    }
    while(file.good() && !file.eof())
    {
        std::string line;
        std::getline(file, line);
        if(line.find_first_of('#') == std::string::npos && line.length() > 0)
            this->invoke_single_line(line);
    }
    return true;
}

std::deque<std::string> CVSFileBasedProviderBase::parseCVSLine(const std::string& line, char sep) {
    std::deque<std::string> result;
    std::stringstream buffer;

    bool quoted = false;
    for(const char c : line) {
        if(quoted) {
            if(c == '"') {
                quoted = false;
                continue;
            }
            buffer << c;
            continue;
        }
        if(c == sep) {
            result.push_back(buffer.str());
            buffer = std::stringstream();
            continue;
        }
        if(c == '"') {
            quoted = true;
            continue;
        }
        if(c == ' ' && buffer.str().empty()) continue;

        buffer << c;
    }
    result.push_back(buffer.str());
    return result;
}