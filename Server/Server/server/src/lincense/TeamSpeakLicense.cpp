//
// Created by wolverindev on 30.06.18.
//

#include <cstring>
#include <fstream>
#include <ed25519/ed25519.h>
#include <map>
#include <random>
#include <algorithm>
#include <misc/base64.h>
#include "TeamSpeakLicense.h"

using namespace license;
using namespace ts;
using namespace std;
using namespace std::chrono;

TeamSpeakLicense::TeamSpeakLicense(const std::string& file) : file_name(file) {}

extern bool file_exists(const std::string& name);
inline string strip(std::string message) {
    while(!message.empty()) {
        if(message[0] == ' ')
            message = message.substr(1);
        else if(message[message.length() - 1] == ' ')
            message = message.substr(0, message.length() - 1);
        else break;
    }
    return message;
}

#define TEST_KEY(key) \
if(properties.count(key) == 0) { \
    error = string("missing key ") + key; \
    return false; \
}

#define ERR(message) \
do { \
    this->data = nullptr; \
    error = message; \
    return false; \
} while(0)

bool TeamSpeakLicense::load(std::string& error) {
    if(!file_exists(this->file_name)) {
        ofstream os(this->file_name);
        if(!os) {
            error = "Could not create default file!";
            return false;
        }
        os << "# This is your auto generated TeaSpeak keychain." << endl;
        os << "# DO NOT SHARE THESE DATA!" << endl;

        auto chain = make_shared<teamspeak::LicenseChain>();
        auto entry = chain->addServerEntry(teamspeak::SERVER_LICENSE_OFFLINE, "TeaSpeak", 1024);
        if(!entry) {
            error = "Could not create chain default entry!";
            return false;
        }
        entry->lifetime(hours(24 * 360), system_clock::now() - hours(12));

        uint8_t root_private[32], root_public[32], root_seed[64];
        for(auto& e : root_seed) e = (uint8_t) rand();
        ed25519_create_keypair(root_public, root_private, root_seed);

        auto prv_root = chain->generatePrivateKey(root_private);

        os << "version:0" << endl;
        os << "chain:" << base64::encode(chain->exportChain()) << endl;
        os << "root_key_prv:" << base64::encode(prv_root) << endl;
        os << "root_key_pbl:" << base64::encode((const char*) root_public, 32) << endl;
        os << "root_prv_index:" << to_string(1) << endl;
        os.flush();
        os.close();
    }

    ifstream in(this->file_name);
    if(!in) {
        error = "could not open file";
        return false;
    }

    string line;
    map<string, string> properties;
    while(getline(in, line)) {
        line = strip(line);
        if(line.empty() || line[0] == '#') continue;

        auto index = line.find(':');
        if(index == string::npos) continue;

        properties[line.substr(0, index)] = strip(line.substr(index + 1));
    }

    TEST_KEY("version");
    TEST_KEY("chain");
    TEST_KEY("root_key_prv");
    TEST_KEY("root_key_pbl");
    TEST_KEY("root_prv_index");

    if(properties["version"] != "0") ERR("Invalid file version (" + properties["version"] + ")");

    auto stream = istringstream(base64::decode(properties["chain"]));
    auto chain = teamspeak::LicenseChain::parse(stream, error);
    if(!chain) ERR("Could not parse chain (" + error + ")");

    this->data.reset(new LicenseChainData{});
    this->data->chain = chain;

    {
        auto buffer = base64::decode(properties["root_key_prv"]);
        if(buffer.length() != 32) ERR("Invalid root prv key length!");
        memcpy(this->data->root_key, buffer.data(), 32);
    }
    {
        auto buffer = base64::decode(properties["root_key_pbl"]);
        if(buffer.length() != 32) ERR("Invalid root pbl key length!");
        memcpy(this->data->public_key, buffer.data(), 32);
    }

    try {
        this->data->root_index = stoi(properties["root_prv_index"]);
    } catch (std::exception&) {
        ERR("Could not parse prv root index");
    }

    {
        string message = "This is a test message for singing (WolverinDEV)";
        u_char sign[128];
        auto key_public = this->data->chain->generatePublicKey(this->data->public_key);
        auto key_private = this->data->chain->generatePrivateKey(this->data->root_key, this->data->root_index);
        key_private = key_private + key_private; //ed25519_sign requires 64 bytes (may it expects a public key in front?)

        ed25519_sign(sign, (u_char*) message.data(), message.length(), (u_char*) key_public.data(), (u_char*) key_private.data());
        if(!ed25519_verify(sign, (u_char*) message.data(), message.length(), (u_char*) key_public.data())) ERR("Failed to sign and verify a simple test message. Keypair probably broken!");
    }

    return true;
}

std::shared_ptr<LicenseChainData> TeamSpeakLicense::license(bool copy) {
    if(!copy || !this->data) {
        return this->data;
    }

    auto result = std::make_shared<LicenseChainData>();
    result->chain = this->data->chain->copy();
    result->root_index = this->data->root_index;

    memcpy(result->root_key, data->root_key, 32);
    memcpy(result->public_key, data->public_key, 32);
    return result;
}