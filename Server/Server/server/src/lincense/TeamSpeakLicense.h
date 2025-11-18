#pragma once

#include <string>
#include <License.h>

namespace ts {
    struct LicenseChainData {
        std::shared_ptr<license::teamspeak::LicenseChain> chain;
        license::teamspeak::LicensePublicKey  public_key;
        license::teamspeak::LicensePublicKey  root_key;
        int root_index;
    };

    class TeamSpeakLicense {
        public:
            TeamSpeakLicense(const std::string&);
            bool load(std::string&);

            inline std::string file() { return this->file_name; }
            std::shared_ptr<LicenseChainData> license(bool copy = true);
        private:
            std::string file_name;
            std::shared_ptr<LicenseChainData> data;
    };
}