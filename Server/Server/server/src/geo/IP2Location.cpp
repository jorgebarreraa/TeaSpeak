#include "GeoLocation.h"
#include "log/LogUtils.h"

using namespace std;
using namespace geoloc;

/** LocationIPProvider **/
std::shared_ptr<CountryInfo> LocationIPProvider::resolveCountryInfo(std::string code) {
    if(code[0] == '-') code = "UNKNOWN";
    auto &list = countryMapping[code[0] - 'A'];
    for (const auto &entry : list)
        if (entry->identifier == code)
            return entry;
    return nullptr;
}

std::shared_ptr<CountryInfo> LocationIPProvider::createCountryInfo(std::string code, const std::string &name) {
    if(code[0] == '-') code = "UNKNOWN";
    auto info = resolveCountryInfo(code);
    if(info) return info;
    info = std::make_shared<CountryInfo>(code, name);
    this->countryMapping[code[0] - 'A'].push_back(info);

    return info;
}

/** Software77Provider **/
geoloc::Software77Provider::Software77Provider(const std::string &file) : LocationIPProvider(file) {}
geoloc::Software77Provider::~Software77Provider() = default;

bool Software77Provider::parseSingleLine(const std::string &line, RangeEntryMapping<CountryInfo> &mapping) {
    auto tokens = this->parseCVSLine(line, ',');

    auto code = tokens[4];
    auto cinfo = this->resolveCountryInfo(code);
    if(!cinfo)
        cinfo = this->createCountryInfo(code, tokens[6]);
    mapping.data = cinfo;
    mapping.startAddress = static_cast<IpAddress_t>(std::stoul(tokens[0]));
    mapping.endAddress = -1;
    return true;
}

void Software77Provider::emit_line_parse_failed(const std::string &line) {
    logError(LOG_GENERAL, "Software77Provider: Failed to parse line \"{}\". Skipping this line", line);
}

/** IP2LocationProvider **/
IP2LocationProvider::IP2LocationProvider(const std::string &file) : LocationIPProvider(file){}
bool IP2LocationProvider::parseSingleLine(const std::string &line, RangeEntryMapping<CountryInfo> &mapping) {
    auto tokens = this->parseCVSLine(line, ',');

    auto code = tokens[2];
    auto cinfo = this->resolveCountryInfo(code);
    if(!cinfo)
        cinfo = this->createCountryInfo(code, tokens[3]);
    mapping.data = cinfo;
    mapping.startAddress = static_cast<IpAddress_t>(std::stoul(tokens[0]));
    mapping.endAddress = static_cast<IpAddress_t>(std::stoul(tokens[1]));
    return true;
}

void IP2LocationProvider::emit_line_parse_failed(const std::string &line) {
    logError(LOG_GENERAL, "IP2LocationProvider: Failed to parse line \"{}\". Skipping this line", line);
}
