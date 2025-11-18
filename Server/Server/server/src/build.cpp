#include <iostream>
#include <sstream>
#include <misc/strobf.h>
#include "build.h"

#ifndef BUILD_MAJOR
    #define BUILD_MAJOR 0
#endif

#ifndef BUILD_MINOR
    #define BUILD_MINOR 0
#endif

#ifndef BUILD_PATCH
    #define BUILD_PATCH 0
#endif

#ifndef BUILD_TYPE
    #define BUILD_TYPE build::BuildType::PRIVATE
#endif

#ifndef BUILD_DATA
    #define BUILD_DATA "Unknown build"
#endif

#ifndef BUILD_COUNT
    #define BUILD_COUNT 0
#endif

#define STR1(x) #x
#define STR2(x) STR1(x)

using namespace build;
using namespace std;
using namespace std::chrono;

unique_ptr<Version> local_version{[]() -> Version * {
    const char *build_timestamp = __TIME__; //23:59:01
    const char *build_date = __DATE__;      //Feb 12 1996

    cout << "Time " << build_timestamp << " date " << build_date << endl;
    tm timestamp{}, date{};
    if (!strptime(build_timestamp, "%H:%M:%S", &timestamp)) cerr << "Could not parse build timestamp!" << endl;
    if (!strptime(build_date, "%b %d %Y", &date)) cerr << "Could not parse build date!" << endl;

    system_clock::time_point time =
            system_clock::time_point() + seconds(mktime(&date)) + hours(timestamp.tm_hour) + minutes(timestamp.tm_min) +
            seconds(timestamp.tm_sec);
    return new Version{BUILD_MAJOR, BUILD_MINOR, BUILD_PATCH, STR2(BUILD_DATA), time};
}()};

namespace build {
    const std::unique_ptr<Version>& version() {
        return local_version;
    }

    std::string Version::string(bool timestamp) {
        stringstream ss;
        ss << this->major << "." << this->minor << "." << this->patch << this->additional;
        if(timestamp) ss << " [Build: " << duration_cast<seconds>(this->timestamp.time_since_epoch()).count() << "]";
        return ss.str();
    }

    BuildType type() { return static_cast<BuildType>(BUILD_TYPE); }

    std::string additionalData(){ return STR2(BUILD_DATA); }

    int buildCount(){ return BUILD_COUNT; }

    std::string pattern(){
        //return R"([0-9]{1,5}\.[0-9]{1,5}\.[0-9]{1,5}(\-.*)?)";
        return R"([0-9]{1,5}\.[0-9]{1,5}\.[0-9]{1,5}(-\S+( \[[Bb]uild: \d+\])?)?)";
    }

    std::string platform() {
#if defined(__aarch64__)
        return strobf("arm64v8").string(); /* no port yet */
#elseif defined(_M_ARM)
        return strobf("arm32v" STR2(_M_ARM)).string();
#elseif defined(WIN32)
        return strobf("Windows").string(); /* no port yet */
#else
        return strobf("Linux").string();
#endif
    }
}