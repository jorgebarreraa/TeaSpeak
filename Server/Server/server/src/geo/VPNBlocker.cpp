#include <arpa/inet.h>
#include "GeoLocation.h"
#include "log/LogUtils.h"

using namespace std;
using namespace geoloc;

IPCatBlocker::IPCatBlocker(const std::string &file) : CVSFileBasedProvider<VPNInfo>(file) {}

bool IPCatBlocker::parseSingleLine(const std::string &line, geoloc::RangeEntryMapping<geoloc::VPNInfo> &mapping) {
    auto tokens = this->parseCVSLine(line, ',');
    if(tokens.size() != 4) return false;

    mapping.data = this->get_or_create_info(tokens[2], tokens[3]);
    mapping.startAddress = ip_swap_order(inet_addr(tokens[0].c_str()));
    mapping.endAddress = ip_swap_order(inet_addr(tokens[1].c_str()));

    return true;
}

std::shared_ptr<VPNInfo> IPCatBlocker::get_or_create_info(const std::string &hoster, const std::string &webside) {
    for(const auto& info : this->hoster_info)
        if(info->name == hoster && info->side == webside)
            return info;
    auto result = make_shared<VPNInfo>(hoster, webside);
    this->hoster_info.push_back(result);
    return result;
}


void IPCatBlocker::emit_line_parse_failed(const std::string &line) {
    logError(LOG_GENERAL, "IPCatBlocker: Failed to parse line \"{}\". Skipping this line", line);
}
