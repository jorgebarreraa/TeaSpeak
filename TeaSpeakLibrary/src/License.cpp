#include <cstring>
#include "misc/endianness.h"
#include <misc/digest.h>
#define FIXEDINT_H_INCLUDED
#include <ed25519/ge.h>
#include <ed25519/sc.h>
#include <ed25519/ed25519.h>
#include <iomanip>
#include "License.h"

using namespace license::teamspeak;
using namespace std;
using namespace std::chrono;

LicensePublicKey license::teamspeak::public_root =
        {0xcd, 0x0d, 0xe2, 0xae, 0xd4, 0x63, 0x45, 0x50,
         0x9a, 0x7e, 0x3c, 0xfd, 0x8f, 0x68, 0xb3, 0xdc,
         0x75, 0x55, 0xb2, 0x9d, 0xcc, 0xec, 0x73, 0xcd,
         0x18, 0x75, 0x0f, 0x99, 0x38, 0x12, 0x40, 0x8a};
//0DCB3688FFD3C7E9A504563D4AEE20DCD
//8A401238990F7518CD73ECCC9DB25575
/*
 *
Public key: 0x41, 0x47, 0xeb, 0x8b, 0xab, 0xaa, 0x89, 0xb9, 0x34, 0x86, 0x76, 0x25, 0x5f, 0x9b, 0xaf, 0x10, 0xfb, 0x2b, 0x03, 0x62, 0x10, 0xd0, 0x18, 0x59, 0x04, 0x42, 0x39, 0x5b, 0x2c, 0x22, 0xc3, 0x6a,
Private key: 0xa8, 0xcf, 0x96, 0xe8, 0xa8, 0xce, 0x33, 0x3b, 0x80, 0xb5, 0xd4, 0x27, 0x25, 0x62, 0xa6, 0x3a, 0x4a, 0x9e, 0x81, 0xf3, 0x05, 0xda, 0xb5, 0xf7, 0xe9, 0x35, 0x3d, 0x02, 0x81, 0x39, 0x8d, 0x64,
 */
LicensePublicKey license::teamspeak::public_tea_root = {
        0x41, 0x47, 0xeb, 0x8b, 0xab, 0xaa, 0x89, 0xb9,
        0x34, 0x86, 0x76, 0x25, 0x5f, 0x9b, 0xaf, 0x10,
        0xfb, 0x2b, 0x03, 0x62, 0x10, 0xd0, 0x18, 0x59,
        0x04, 0x42, 0x39, 0x5b, 0x2c, 0x22, 0xc3, 0x6a,
};
LicensePublicKey license::teamspeak::private_tea_root = {
        0xa8, 0xcf, 0x96, 0xe8, 0xa8, 0xce, 0x33, 0x3b, 0x80, 0xb5, 0xd4, 0x27, 0x25, 0x62, 0xa6, 0x3a, 0x4a, 0x9e, 0x81, 0xf3, 0x05, 0xda, 0xb5, 0xf7, 0xe9, 0x35, 0x3d, 0x02, 0x81, 0x39, 0x8d, 0x64,
};

/*
//Could be found at linux x64 3.1.3 (1621edab12fd2cfc74aee73258fec2f1435e70f2): 0x9BDFE0
LicensePublicKey license::teamspeak::Anonymous::root_key = {
        0xA0, 0x23, 0xE6, 0x30, 0x0B, 0xDF, 0x91, 0x2E, 0xB3, 0xFD, 0x45, 0xA0, 0x86, 0x97, 0x1F, 0x97,
        0xE6, 0xBB, 0x10, 0xED, 0x75, 0x14, 0xC1, 0xA1, 0xFC, 0x31, 0x8A, 0x49, 0x58, 0x09, 0xDF, 0x78
};
size_t license::teamspeak::Anonymous::root_index = 1;

//01 00 35 85 41 49 8A 24 AC D3
//Could be found at linux x64 3.1.3 (1621edab12fd2cfc74aee73258fec2f1435e70f2): 0x9BDFA0
uint8_t default_chain[] = {
        0x01, 0x00, 0x35, 0x85, 0x41, 0x49, 0x8A, 0x24, 0xAC, 0xD3, 0x01, 0x57, 0x91, 0x8B, 0x8F, 0x50,
        0x95, 0x5C, 0x0D, 0xAE, 0x97, 0x0A, 0xB6, 0x53, 0x72, 0xCB, 0xE4, 0x07, 0x41, 0x5F, 0xCF, 0x3E,
        0x02, 0x9B, 0x02, 0x08, 0x4D, 0x15, 0xE0, 0x0A, 0xA7, 0x93, 0x60, 0x07, 0x00, 0x00, 0x00, 0x20,
        0x41, 0x6E, 0x6F, 0x6E, 0x79, 0x6D, 0x6F, 0x75, 0x73
};
 */

//9ED960 linux x64
uint8_t default_chain[] = {
        0x01, 0x00, 0x95, 0x5D, 0x39, 0x4A, 0x17, 0xE5, 0x10, 0x73, 0x4C, 0xA0, 0x6B, 0xDF, 0x5D, 0x39,
        0x0F, 0x45, 0x24, 0x2C, 0x0B, 0x68, 0x9A, 0xAD, 0x0D, 0xBD, 0x46, 0xEF, 0x16, 0x0F, 0x32, 0xF1,
        0xC5, 0x33, 0x02, 0x0A, 0x55, 0xF2, 0x80, 0x0C, 0x5E, 0xB3, 0x00, 0x07, 0x00, 0x00, 0x00, 0x20,
        0x41, 0x6E, 0x6F, 0x6E, 0x79, 0x6D, 0x6F, 0x75, 0x73
};

//9ED960 + sizeof(default_chain) + pad(16)
LicensePublicKey license::teamspeak::Anonymous::root_key = {
        0x40, 0x25, 0x08, 0x56, 0xD8, 0xDA, 0x90, 0x85, 0x9E, 0xDC, 0x1A, 0x0D, 0x58, 0x7B, 0x7D, 0x73,
        0xA0, 0x57, 0xF2, 0x55, 0x32, 0x47, 0x84, 0x0E, 0x3E, 0x2A, 0xF2, 0xC0, 0x1B, 0x8F, 0x23, 0x4B
};
size_t license::teamspeak::Anonymous::root_index = 1;

std::shared_ptr<LicenseChain> default_anonymous_chain() {
    string error;
    auto str = istringstream(string((const char*) default_chain, sizeof(default_chain)));
    auto chain = LicenseChain::parse(str, error);
    if(!chain) {
        cerr << "Failed to load default chain!" << endl;
        return nullptr;
    }

    return chain;
}

shared_ptr<LicenseChain> license::teamspeak::Anonymous::chain{default_anonymous_chain()};

#define IOERROR(message) \
do {\
    error = message;\
    return 0; \
} while(0)

std::shared_ptr<LicenseEntry> LicenseEntry::read(std::istream& stream, std::string& error) {
    int baseLength = 42;
    streamsize read;
    char buffer[42];
    if((read = stream.readsome(buffer, baseLength)) != baseLength) {
        if(read == 0)
            return nullptr;
        else
            IOERROR("Could not read new license block");
    }

    if(buffer[0] != 0x00) IOERROR("Invalid entry type (" + to_string(buffer[0]) + ")");

    shared_ptr<LicenseEntry> result;
    switch ((int) buffer[33]) {
        case 0x00:
            result = make_shared<IntermediateLicenseEntry>();
            break;
        case 0x02:
            result = make_shared<ServerLicenseEntry>();
            break;
        case 0x03:
            result = make_shared<CodeLicenseEntry>();
            break;
        case 0x05:
            result = make_shared<LicenseSignLicenseEntry>();
            break;
        case 0x20:
            result = make_shared<EphemeralLicenseEntry>();
            break;
        default:
            error = "Invalid license type! (" + to_string((int) buffer[33]) + ")";
            return nullptr;
    }

    memcpy(result->key.publicKeyData, &buffer[1], 32);
    result->_begin = system_clock::time_point() + seconds(be2le32(buffer, 34) + LicenseEntry::TIMESTAMP_OFFSET);
    result->_end = system_clock::time_point() + seconds(be2le32(buffer, 38) + LicenseEntry::TIMESTAMP_OFFSET);

    if(!result->readContent(stream, error)) return nullptr;
    return result;
}
bool LicenseEntry::write(std::ostream& stream, std::string& error) const {
    stream << (uint8_t) 0x00;
    stream.write((char*) this->key.publicKeyData, 32);
    stream.write((char*) &this->_type, 1);

    char timeBuffer[8];
    le2be32((uint32_t) (duration_cast<seconds>(this->_begin.time_since_epoch()).count() - LicenseEntry::TIMESTAMP_OFFSET), timeBuffer, 0);
    le2be32((uint32_t) (duration_cast<seconds>(this->_end.time_since_epoch()).count() - LicenseEntry::TIMESTAMP_OFFSET), timeBuffer, 4);
    stream.write(timeBuffer, 8);

    return this->writeContent(stream, error);
}

std::string LicenseEntry::hash() const {
    ostringstream buffer;
    string error;
    this->write(buffer, error);

    return digest::sha512(buffer.str().substr(1));
}

IntermediateLicenseEntry::IntermediateLicenseEntry() : LicenseEntry(LicenseType::INTERMEDIATE) {}

bool IntermediateLicenseEntry::readContent(std::istream &stream, std::string &error) {
    if(stream.readsome(this->dummy, 4) != 4) IOERROR("Could not read data! (Invalid length!)");
    getline(stream, this->issuer, '\0');
    return true;
}

bool IntermediateLicenseEntry::writeContent(std::ostream &ostream, std::string &string1) const {
    ostream.write(this->dummy, 4);
    ostream << this->issuer;
    ostream << (uint8_t) 0;
    return true;
}

ServerLicenseEntry::ServerLicenseEntry() : LicenseEntry(LicenseType::SERVER) {}
bool ServerLicenseEntry::readContent(std::istream &stream, std::string &error) {
    if(stream.readsome((char*) &this->licenseType, 1) != 1) IOERROR("Could not read server license type!");
    char buffer[4];
    if(stream.readsome(buffer, 4) != 4) IOERROR("Could not read data! (Invalid length!)");
    this->slots = be2le32(buffer);
    getline(stream, this->issuer, '\0');
    return true;
}
bool ServerLicenseEntry::writeContent(std::ostream &ostream, std::string &string1) const {
    ostream.write((char*) &this->licenseType, 1);
    char buffer[4];
    le2be32(this->slots, buffer);
    ostream.write(buffer, 4);
    ostream.write(this->issuer.data(), this->issuer.length());
    ostream << (uint8_t) 0;
    return false;
}

CodeLicenseEntry::CodeLicenseEntry() : LicenseEntry(LicenseType::CODE) {}
bool CodeLicenseEntry::readContent(std::istream &stream, std::string &error) {
    getline(stream, this->issuer, '\0');
    return true;
}
bool CodeLicenseEntry::writeContent(std::ostream &ostream, std::string &error) const {
    ostream.write(this->issuer.data(), this->issuer.length());
    ostream << (uint8_t) 0;
    return true;
}

LicenseSignLicenseEntry::LicenseSignLicenseEntry() : LicenseEntry(LicenseType::LICENSE_SIGN) {}
bool LicenseSignLicenseEntry::writeContent(std::ostream &stream, std::string &error) const { return true; }
bool LicenseSignLicenseEntry::readContent(std::istream &stream, std::string &error) {
    cout << "License read: " << stream.gcount() << " -> " << stream.tellg() << endl;
    return true;
}

EphemeralLicenseEntry::EphemeralLicenseEntry() : LicenseEntry(LicenseType::EPHEMERAL) {}
bool EphemeralLicenseEntry::readContent(std::istream &stream, std::string &error) { return true; }
bool EphemeralLicenseEntry::writeContent(std::ostream &ostream, std::string &string1) const { return true; }

std::shared_ptr<LicenseChain> LicenseChain::parse(std::istream& stream, std::string& error, bool return_on_error) {
    error = "";

    uint8_t chainType;
    if(stream.readsome((char*) &chainType, 1) != 1) IOERROR("Invalid stream length!");
    if(chainType != 1) IOERROR("Invalid chain type! (" + to_string(chainType) + ")");

    deque<shared_ptr<LicenseEntry>> entries;
    while (stream) {
        auto entry = LicenseEntry::read(stream, error);
        if(!entry) {
            if(error.length() == 0) break;
            if(return_on_error) break;
            return nullptr;
        }
        entries.push_back(entry);
    }

    auto result = make_shared<LicenseChain>();
    result->entries = entries;
    return result;
}

inline std::string to_string(const std::chrono::system_clock::time_point& point) {
    auto tp = system_clock::to_time_t(point);
    string str = ctime(&tp);
    return str.empty() ? str : str.substr(0, str.length() - 1);
}

void LicenseChain::print() {
    int index = 1;
    for(const auto& entry : this->entries) {
        cout << "Entry " << index ++ << " (Type: " << entry->type() << " | " << type_name(entry->type()) << ")" << endl;

        cout << hex;
        cout << "  Begin   : " << to_string(entry->begin()) << " (" << (duration_cast<seconds>(entry->begin().time_since_epoch()).count() - LicenseEntry::TIMESTAMP_OFFSET) << ")" << endl;
        cout << "  End     : " << to_string(entry->end()) << " (" << (duration_cast<seconds>(entry->end().time_since_epoch()).count() - LicenseEntry::TIMESTAMP_OFFSET) << ")" << endl;
        cout << "  Public key:";
        for(const auto& e : entry->key.publicKeyData)
            cout << " " << hex << setfill('0') << setw(2) << (int) e << dec;
        cout << endl;

        if(dynamic_pointer_cast<IntermediateLicenseEntry>(entry)) {
            cout << "  Issuer  : " << dynamic_pointer_cast<IntermediateLicenseEntry>(entry)->issuer << endl;
            cout << "  Slots   : " << dynamic_pointer_cast<IntermediateLicenseEntry>(entry)->unknown << endl;
        } else if(dynamic_pointer_cast<ServerLicenseEntry>(entry)) {
            cout << "  Issuer  : " << dynamic_pointer_cast<ServerLicenseEntry>(entry)->issuer << endl;
            cout << "  Server license type  : " << (int) dynamic_pointer_cast<ServerLicenseEntry>(entry)->licenseType << endl;
            cout << "  Slots   : " << dynamic_pointer_cast<ServerLicenseEntry>(entry)->slots << endl;
        } else if(dynamic_pointer_cast<CodeLicenseEntry>(entry)){
            cout << "  Issuer  : " << dynamic_pointer_cast<CodeLicenseEntry>(entry)->issuer << endl;
        }
    }

    auto key = this->generatePublicKey();
    cout << "Public key: " << endl;
    //hexDump((char*) key.data(), (int) key.length(), (int) key.length(), (int) key.length(), [](string message) { cout << message << endl; });
}

std::string LicenseChain::exportChain() {
    string error;
    ostringstream stream;
    stream << (uint8_t) 0x01;
    for(const auto& entry : this->entries)
        entry->write(stream, error);
    return stream.str();
}

inline void _ed25519_create_keypair(uint8_t(&public_key)[32], uint8_t(&private_key)[32]) {
    uint8_t seed[32];
    ed25519_create_seed(seed);

    uint8_t buffer_private[64]; /* Because we word with SHA512 we required 64 bytes! */
    ed25519_create_keypair(public_key, buffer_private, seed);
    memcpy(private_key, buffer_private, 32);
}

void LicenseChain::addIntermediateEntry() {
    auto entry = make_shared<IntermediateLicenseEntry>();
    _ed25519_create_keypair(entry->key.publicKeyData, entry->key.privateKeyData);
    entry->key.privateKey = true;
    entry->_begin = system_clock::now() - hours(16);
    entry->_end = system_clock::now() + hours(16);
    entry->issuer = "XXXX";
    entry->unknown = 10; //Max 0x7F
    this->entries.push_back(entry);
}

std::shared_ptr<LicenseEntry> LicenseChain::addServerEntry(ServerLicenseType type, const std::string &issuer, uint32_t slots) {
    auto entry = make_shared<ServerLicenseEntry>();
    _ed25519_create_keypair(entry->key.publicKeyData, entry->key.privateKeyData);
    entry->key.privateKey = true;
    entry->issuer = issuer;
    entry->licenseType = type;
    entry->slots = slots;
    entry->_begin = system_clock::now() - hours(8);
    entry->_end = system_clock::now() + hours(8);
    this->entries.push_back(entry);
    return entry;
}

void LicenseChain::addEphemeralEntry() {
    auto entry = std::make_shared<EphemeralLicenseEntry>();
    _ed25519_create_keypair(entry->key.publicKeyData, entry->key.privateKeyData);
    entry->key.privateKey = true;
    entry->_begin = system_clock::now() - hours(6);
    entry->_end = system_clock::now() + hours(6);
    this->entries.push_back(entry);
}

inline ge_p3 import(const char* buffer) {
    ge_p3 result{};
    ge_frombytes_negate_vartime(&result, (uint8_t*) buffer);
    return result;
}

inline string importHash(const std::string& hash) {
    uint8_t buffer[64]; //We need to allocate 64 bytes (s[0]+256*s[1]+...+256^63*s[63] as input for sc_reduce (result could max be 2^256 | 32 bytes))
    memset(buffer, 0, 64);

    memcpy(buffer, (void*) hash.data(), 32);
    buffer[0]  &= (uint8_t) 0xF8;
    buffer[31] &= (uint8_t) 0x3F;
    buffer[31] |= (uint8_t) 0x40;
    sc_reduce(buffer);
    return string((char*) buffer, 32);
}

std::string LicenseChain::generatePublicKey(LicensePublicKey root, int length) const {
    auto parent = import((char*) root);
    fe_neg(parent.X, parent.X); // undo negate
    fe_neg(parent.T, parent.T); // undo negate

    for(const auto& entry : this->entries) {
        if(length-- == 0) continue;
        ge_p3 pKey{};
        ge_frombytes_negate_vartime(&pKey, entry->key.publicKeyData);


        ge_cached parentCached{};
        ge_p3_to_cached(&parentCached, &parent);

        auto clamp = importHash(entry->hash());

        ge_p3 p3_clamp_mul_pKey{};
        ge_p2 p2_clamp_mul_pKey{};
        ge_scalarmult_vartime(&p2_clamp_mul_pKey, (unsigned char *) clamp.data(), &pKey);
        ge_p2_to_p3(&p3_clamp_mul_pKey, &p2_clamp_mul_pKey);

        //----- | -----
        ge_p1p1 a{};
        ge_add(&a, &p3_clamp_mul_pKey, &parentCached);

        ge_p3 r2{};
        ge_p1p1_to_p3(&r2, &a);

        parent = r2;
     }

    char buf[32];
    ge_p3_tobytes((uint8_t*) buf, &parent);
    return string(buf, 32);
}

std::string LicenseChain::generatePrivateKey(uint8_t *root, int begin) const {
    uint8_t buffer[32];
    memcpy(buffer, root, 32);
    for(int index = begin; index < this->entries.size(); index++) {
        assert(this->entries[index]->key.privateKey);
        sc_muladd(buffer, this->entries[index]->key.privateKeyData, (uint8_t*) importHash(this->entries[index]->hash()).data(), buffer);
    }
    return string((char*) buffer, 32);
}