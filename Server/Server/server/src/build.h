#pragma once

#include <string>
#include <chrono>
#include <memory>

namespace build {
    enum BuildType {
        STABLE,
        BETA,
        NIGHTLY,
        PRIVATE
    };

    struct Version {
        int major;
        int minor;
        int patch;
        std::string additional;

        std::chrono::system_clock::time_point timestamp;

        inline bool valid() const { return timestamp.time_since_epoch().count() > 0; }

        inline bool operator>(const Version &other) const {
            if (other.major < this->major) return true;
            else if (other.major > this->major) return false;

            if (other.minor < this->minor) return true;
            else if (other.minor > this->minor) return false;

            if (other.patch < this->patch) return true;
            else if (other.patch > this->patch) return false;
            return false;
        }

        inline bool operator==(const Version &other) const {
            return this->major == other.major && this->minor == other.minor && this->patch == other.patch;
        }

        inline bool operator<(const Version &other) const { return other.operator>(*this); }

        inline bool operator>=(const Version &other) const { return this->operator>(other) || this->operator==(other); }

        inline bool operator<=(const Version &other) const { return this->operator<(other) || this->operator==(other); }

        Version(int major, int minor, int patch, std::string additional,
                const std::chrono::system_clock::time_point &timestamp) : patch(patch),
                                                                          additional(std::move(additional)),
                                                                          timestamp(timestamp) {
            this->major = major;
            this->minor = minor;
        }

        std::string string(bool timestamp = true);
    };

    extern const std::unique_ptr<Version>& version();
    extern BuildType type();
    extern int buildCount();

    extern std::string pattern();
    extern std::string platform();
}