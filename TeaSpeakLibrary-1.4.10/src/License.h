#pragma once

#include <sstream>
#include <chrono>
#include <memory>
#include <deque>

namespace license {
    namespace teamspeak {
        class LicenseChain;
        enum LicenseType {
            INTERMEDIATE = 0x00,
            WEBSIDE = 0x01,
            SERVER = 0x02,
            CODE = 0x03,
            TOKEN = 0x04,
            LICENSE_SIGN = 0x05,
            MY_TS_ID_SIGN = 0x06,
            EPHEMERAL = 0x20
        };
        inline std::string type_name(LicenseType type) {
            switch (type) {
                case LicenseType::INTERMEDIATE:
                    return "Intermediate";
                case LicenseType::WEBSIDE:
                    return "Website";
                case LicenseType::SERVER:
                    return "Server";
                case LicenseType::CODE:
                    return "Code";
                case LicenseType::LICENSE_SIGN:
                    return "LicenseSign";
                case LicenseType::MY_TS_ID_SIGN:
                    return "MyTsIdSign";
                case LicenseType::EPHEMERAL:
                    return "Ephemeral";
                case LicenseType::TOKEN:
                    return "Token";
                default:
                    return "Unknown";
            }
        }

        enum ServerLicenseType : uint8_t {
            SERVER_LICENSE_NONE,
            SERVER_LICENSE_OFFLINE,
            SERVER_LICENSE_SDK,
            SERVER_LICENSE_SDKOFFLINE,
            SERVER_LICENSE_NPL,
            SERVER_LICENSE_ATHP,
            SERVER_LICENSE_AAL,
            SERVER_LICENSE_DEFAULT,
        };

        struct LicenseKey {
            bool privateKey = false;
            uint8_t privateKeyData[32] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
            uint8_t publicKeyData[32] =  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        };

        typedef uint8_t LicensePublicKey[32];
        class LicenseEntry {
                static constexpr int64_t TIMESTAMP_OFFSET = 0x50e22700;
                friend class LicenseChain;
            public:
                LicenseEntry(LicenseType type) : _type(type) {}
                ~LicenseEntry() {}

                virtual LicenseType type() const { return this->_type; }

                static std::shared_ptr<LicenseEntry> read(std::istream&, std::string&);
                virtual bool write(std::ostream&, std::string&) const ;

                std::string hash() const;

                std::chrono::system_clock::time_point begin() const { return this->_begin; }
                std::chrono::system_clock::time_point end() const { return this->_end; }

                void begin(const std::chrono::system_clock::time_point& begin) { this->_begin = begin; }
                void end(const std::chrono::system_clock::time_point& end) { this->_end = end; }

                template <typename Unit = std::chrono::seconds>
                Unit lifetime() { return std::chrono::duration_cast<Unit>(this->_end - this->_begin); }

                template <typename Unit = std::chrono::seconds>
                void lifetime(const Unit& lifetime, const std::chrono::system_clock::time_point& begin = std::chrono::system_clock::now()) {
                    this->_begin = begin;
                    this->_end = begin + lifetime;
                }
            protected:
                virtual bool readContent(std::istream&, std::string&) = 0;
                virtual bool writeContent(std::ostream&, std::string&) const = 0;

                LicenseKey key;
                LicenseType _type;
                std::chrono::system_clock::time_point _begin;
                std::chrono::system_clock::time_point _end;
        };

        class IntermediateLicenseEntry : public LicenseEntry {
            public:
                IntermediateLicenseEntry();

                std::string issuer;
                union {
                    uint32_t unknown;
                    char dummy[4];
                };
            protected:
                bool readContent(std::istream &istream1, std::string &string1) override;
                bool writeContent(std::ostream &ostream1, std::string &string1) const override;
        };

        class ServerLicenseEntry : public LicenseEntry {
            public:
                ServerLicenseEntry();

                ServerLicenseType licenseType;
                std::string issuer;
                uint32_t slots;
            protected:
                bool readContent(std::istream &stream, std::string &error) override;
                bool writeContent(std::ostream &stream, std::string &error) const override;
        };

        class CodeLicenseEntry : public LicenseEntry {
            public:
                CodeLicenseEntry();

                std::string issuer;
            protected:
                bool readContent(std::istream &stream, std::string &error) override;
                bool writeContent(std::ostream &stream, std::string &error) const override;
        };

        class LicenseSignLicenseEntry : public LicenseEntry {
            public:
                LicenseSignLicenseEntry();

            protected:
                bool readContent(std::istream &stream, std::string &error) override;
                bool writeContent(std::ostream &stream, std::string &error) const override;
        };

        class EphemeralLicenseEntry : public LicenseEntry {
            public:
                EphemeralLicenseEntry();
            protected:
                bool readContent(std::istream &stream, std::string &error) override;
                bool writeContent(std::ostream &stream, std::string &error) const override;
        };

        extern LicensePublicKey public_root;
        extern LicensePublicKey public_tea_root;
        extern LicensePublicKey private_tea_root;

        class LicenseChain {
            public:
                static std::shared_ptr<LicenseChain> parse(std::istream&, std::string&, bool return_on_error = false);
                inline static std::shared_ptr<LicenseChain> parse(const std::string& license, std::string& error, bool return_on_error = false) {
                    std::istringstream s(license);
                    return LicenseChain::parse(s, error, return_on_error);
                }

                LicenseChain() {}
                ~LicenseChain() {}

                std::shared_ptr<LicenseChain> copy() {
                    auto result = std::make_shared<LicenseChain>();
                    result->entries = this->entries;
                    return result;
                }

                void print();
                std::string exportChain();

                void addEphemeralEntry();
                void addIntermediateEntry();
                std::shared_ptr<LicenseEntry> addServerEntry(ServerLicenseType, const std::string& issuer, uint32_t slots);
                void addEntry(const std::shared_ptr<LicenseEntry>& entry) { this->entries.push_back(entry); }

                /*
                 * Attention! Root must be compressd
                 */
                std::string generatePublicKey(LicensePublicKey = public_root, int length = -1) const;
                std::string generatePrivateKey(LicensePublicKey = private_tea_root, int begin = 0) const;

                std::deque<std::shared_ptr<LicenseEntry>> entries;
            private:
        };

        namespace Anonymous {
            extern std::shared_ptr<LicenseChain> chain; //Thu Jun  1 00:00:00 2017 - Sat Sep  1 00:00:00 2018
            extern LicensePublicKey root_key;
            extern size_t root_index;
        }

    }
}