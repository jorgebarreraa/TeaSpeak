#pragma once

#include <string>
#include <fstream>
#include <utility>
#include <vector>
#include <sstream>
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <array>
#include <deque>
#include <memory>

namespace geoloc {
    typedef int64_t OptionalIpAddress_t;
    typedef uint32_t IpAddress_t;

    inline IpAddress_t ip_swap_order(const IpAddress_t& addr) {
        IpAddress_t result = 0;
        result |= ((addr >> 24) & 0xFF) << 0;
        result |= ((addr >> 16) & 0xFF) << 8;
        result |= ((addr >> 8) & 0xFF) << 16;
        result |= ((addr >> 0) & 0xFF) << 24;
        return result;
    }

    namespace impl {
        extern IpAddress_t inet_addr(const std::string&);
    }

    template <typename Info>
    class InfoProvider {
        public:
            InfoProvider() {}
            virtual ~InfoProvider() {}

            virtual bool load(std::string&) = 0;
            virtual void unload() = 0;

            std::shared_ptr<Info> resolveInfoV4(const std::string &ipv4, bool enforce_range) { return this->resolveInfo(impl::inet_addr(ipv4.c_str()), enforce_range); }
            std::shared_ptr<Info> resolveInfoV6(const std::string &ipv6, bool enforce_range) { return nullptr; }
            virtual std::shared_ptr<Info> resolveInfo(IpAddress_t addr, bool enforce_range) = 0;
        private:
    };

    template <typename Info>
    class RangeEntryMapping {
        public:
            IpAddress_t startAddress;
            OptionalIpAddress_t endAddress;
            std::shared_ptr<Info> data;
    };

    class RangedIPProviderBase {
        public:

        protected:
            inline uint8_t index(IpAddress_t address) { return (address >> (8 * 3)) & 0xFF; }

            std::shared_ptr<void> _resolveInfo(IpAddress_t address, bool enforce_range);
            void _registerRange(const RangeEntryMapping<void>&);

            std::array<std::deque<RangeEntryMapping<void>>, 256> mapping;
    };

    template <typename Info>
    class RangedIPProvider : public InfoProvider<Info>, public RangedIPProviderBase {
        public:
            std::shared_ptr<Info> resolveInfo(IpAddress_t address, bool enforce_range) override { return std::static_pointer_cast<Info>(this->_resolveInfo(address, enforce_range)); }

            void unload() override {
                for(auto& elm : this->mapping)
                    elm.clear();
            }
        protected:
            void registerRange(const RangeEntryMapping<Info>& mapping) {
                this->_registerRange(*(RangeEntryMapping<void>*) &mapping);
            }
    };

    class CVSFileBasedProviderBase {
        public:
            explicit CVSFileBasedProviderBase(const std::string& file);
            virtual ~CVSFileBasedProviderBase() = default;

            bool loadCVS(std::string &);
            std::string getFileName(){ return this->fileName; }
        protected:
            std::string fileName;
            virtual void invoke_single_line(const std::string& line) = 0;
            virtual void emit_line_parse_failed(const std::string& line) = 0;
            std::deque<std::string> parseCVSLine(const std::string& line, char sep);
    };

    template <typename Info>
    class CVSFileBasedProvider : public RangedIPProvider<Info>, public CVSFileBasedProviderBase {
        public:
            explicit CVSFileBasedProvider(const std::string& file) : CVSFileBasedProviderBase(file) {}
            ~CVSFileBasedProvider() = default;

            bool load(std::string &error) override {
                return this->loadCVS(error);
            }
        protected:
            virtual bool parseSingleLine(const std::string&, RangeEntryMapping<Info>&) = 0;

        private:
            void invoke_single_line(const std::string& line) override {
                RangeEntryMapping<Info> entry;
                if(!this->parseSingleLine(line, entry)) {
                    this->emit_line_parse_failed(line);
                } else this->registerRange(entry);
            }

    };

    /** IP to location */
    struct CountryInfo {
        CountryInfo(std::string identifier, std::string name) : identifier(std::move(identifier)), name(std::move(name)) {}

        std::string identifier;
        std::string name;
    };

    enum ProviderType {
        PROVIDER_MIN,
        PROVIDER_IP2LOCATION = PROVIDER_MIN,
        PROVIDER_SOFTWARE77,
        PROVIDER_MAX
    };

    class LocationIPProvider : public CVSFileBasedProvider<CountryInfo> {
        public:
            LocationIPProvider(const std::string& file) : CVSFileBasedProvider<CountryInfo>(file) {};
            virtual ~LocationIPProvider() = default;

            void unload() override {
                for(auto& elm : this->countryMapping)
                    elm.clear();
            }

            std::shared_ptr<CountryInfo> resolveCountryInfo(std::string code);
            std::shared_ptr<CountryInfo> createCountryInfo(std::string code, const std::string& name);
        private:
            std::array<std::deque<std::shared_ptr<CountryInfo>>, 26> countryMapping; //A-Z
    };


    class Software77Provider : public LocationIPProvider {
        public:
            explicit Software77Provider(const std::string& file);
            ~Software77Provider() override;
        protected:
            bool parseSingleLine(const std::string &string, RangeEntryMapping<CountryInfo> &mapping) override;
            void emit_line_parse_failed(const std::string &line) override;
    };

    class IP2LocationProvider : public LocationIPProvider {
        public:
            explicit IP2LocationProvider(const std::string& file);
            ~IP2LocationProvider() = default;
        protected:
            bool parseSingleLine(const std::string &string, RangeEntryMapping<CountryInfo> &mapping) override;
            void emit_line_parse_failed(const std::string &line) override;
    };

    /** VPN Blocker **/

    struct VPNInfo {
        VPNInfo(std::string name, std::string side) : name(std::move(name)), side(std::move(side)) {}

        std::string name;
        std::string side;
    };

    class IPCatBlocker : public CVSFileBasedProvider<VPNInfo> {
        public:
            explicit IPCatBlocker(const std::string& file);
            ~IPCatBlocker() = default;

        protected:
            bool parseSingleLine(const std::string &string, RangeEntryMapping<VPNInfo> &mapping) override;
            void emit_line_parse_failed(const std::string &line) override;
        private:
            std::shared_ptr<VPNInfo> get_or_create_info(const std::string &hoster, const std::string &webside);

            std::deque<std::shared_ptr<VPNInfo>> hoster_info;
    };


    extern InfoProvider<CountryInfo>* provider;
    extern InfoProvider<VPNInfo>* provider_vpn;
}