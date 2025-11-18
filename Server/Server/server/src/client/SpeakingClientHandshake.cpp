#include "SpeakingClient.h"
#include <netinet/tcp.h>
#include <misc/base64.h>
#include <misc/digest.h>
#include <misc/rnd.h>
#include <log/LogUtils.h>
#include "../VirtualServerManager.h"
#include "../InstanceHandler.h"

#if defined(TCP_CORK) && !defined(TCP_NOPUSH)
    #define TCP_NOPUSH TCP_CORK
#endif

using namespace std;
using namespace std::chrono;
using namespace ts;
using namespace ts::server;
using namespace ts::protocol;

void free_ecc(ecc_key* key) {
    if(!key) return;
    ecc_free(key);
    delete key;
}

command_result SpeakingClient::handleCommandHandshakeBegin(Command& cmd) { //If !result than the connection will be closed!
    if(this->handshake.state != HandshakeState::BEGIN)
        return command_result{error::web_handshake_invalid};

    auto intention = cmd["intention"].as<int>();
    if(intention != 0)
        return command_result{error::web_handshake_unsupported};

    auto authenticationMethod = cmd["authentication_method"].as<int>();
    if(authenticationMethod == IdentityType::TEAMSPEAK) {
        this->handshake.identityType = IdentityType::TEAMSPEAK;
        
        auto identity = base64::decode(cmd["publicKey"].string());
        this->properties()[property::CLIENT_UNIQUE_IDENTIFIER] = base64::encode(digest::sha1(cmd["publicKey"].string()));

        this->handshake.identityKey = shared_ptr<ecc_key>(new ecc_key{}, free_ecc);
        if(ecc_import((u_char*) identity.data(), identity.length(), this->handshake.identityKey.get()) != CRYPT_OK) {
            this->handshake.identityKey = nullptr;
            logWarning(this->getServerId(), "{} Failed to import remote public key.", CLIENT_STR_LOG_PREFIX);
            return command_result{error::web_handshake_invalid};
        }
        
        auto message = "TeaSpeak, made with love and coffee by WolverinDEV (#" + base64::encode(rnd_string(32)) + ")";
        this->handshake.proof_message = digest::sha256(message);

        this->sendCommand(Command("handshakeidentityproof", {
                {"message",message},
                {"digest", "SHA-256"}
        }));
        this->handshake.state = HandshakeState::IDENTITY_PROOF;
    } else if(authenticationMethod == IdentityType::TEASPEAK_FORUM) {
        this->handshake.identityType = IdentityType::TEASPEAK_FORUM;
        try {
            this->handshake.identityData = make_shared<Json::Value>();
            this->handshake.proof_message = cmd["data"].string();

            std::string error{};
            Json::CharReaderBuilder rbuilder{};
            const std::unique_ptr<Json::CharReader> reader(rbuilder.newCharReader());

            auto& json_str = this->handshake.proof_message;
            if(!reader->parse(json_str.data(), json_str.data() + json_str.size(), &*this->handshake.identityData, &error)) {
                debugMessage(this->getServerId(), "[{}] Failed to parse forum account data: {}", error);
                return command_result{error::web_handshake_invalid};
            }

            auto& json_data = *this->handshake.identityData;
            if(json_data["user_id"].isNull())
                return command_result{error::web_handshake_invalid}; //{findError("web_handshake_invalid"), "Missing json data (user_id)!"};
            if(json_data["user_name"].isNull())
                return command_result{error::web_handshake_invalid}; //{findError("web_handshake_invalid"), "Missing json data (user_name)!"};
            if(json_data["user_group"].isNull())
                return command_result{error::web_handshake_invalid}; //{findError("web_handshake_invalid"), "Missing json data (user_group)!"};
            if(json_data["user_groups"].isNull())
                return command_result{error::web_handshake_invalid}; //{findError("web_handshake_invalid"), "Missing json data (user_groups)!"};
            if(json_data["data_age"].isNull())
                return command_result{error::web_handshake_invalid}; //{findError("web_handshake_invalid"), "Missing json data (data_age)!"};

            //Type test
            json_data["user_id"].asInt64();

            if(json_data["data_age"].asUInt64() < duration_cast<milliseconds>((system_clock::now() - hours(72)).time_since_epoch()).count())
                return command_result{error::web_handshake_identity_outdated}; // {findError("web_handshake_invalid"), "Provided data is too old!"};

            this->properties()[property::CLIENT_UNIQUE_IDENTIFIER] = base64::encode(digest::sha1("TeaSpeak-Forum#" + json_data["user_id"].asString()));

            this->properties()[property::CLIENT_TEAFORO_ID] = json_data["user_id"].asInt64();
            this->properties()[property::CLIENT_TEAFORO_NAME] = json_data["user_name"].asString();

            {
                ///* 0x01 := Banned | 0x02 := Stuff | 0x04 := Premium */
                uint64_t flags = 0;

                if(json_data["is_banned"].isBool() && json_data["is_banned"].asBool())
                    flags |= 0x01U;

                if(json_data["is_staff"].isBool() && json_data["is_staff"].asBool())
                    flags |= 0x02U;

                if(json_data["is_premium"].isBool() && json_data["is_premium"].asBool())
                    flags |= 0x04U;

                this->properties()[property::CLIENT_TEAFORO_FLAGS] = flags;
            }
        } catch (Json::Exception& exception) {
            debugMessage(this->getServerId(), "{} Failed to parse supplied json: {}", CLIENT_STR_LOG_PREFIX, exception.what());
            return command_result{error::web_handshake_invalid};
        }
        this->sendCommand(Command("handshakeidentityproof"));
        this->handshake.state = HandshakeState::IDENTITY_PROOF;
    } else if(authenticationMethod == IdentityType::NICKNAME) {
        if(!config::server::authentication::name)
            return command_result{error::web_handshake_unsupported};

        this->handshake.state = HandshakeState::SUCCEEDED;
        this->handshake.identityType = IdentityType::NICKNAME;
        this->properties()[property::CLIENT_UNIQUE_IDENTIFIER] = base64::encode(digest::sha1("UserName#" + cmd["client_nickname"].string()));
    } else {
        return command_result{error::web_handshake_unsupported};
    }
    return command_result{error::ok};
}

command_result SpeakingClient::handleCommandHandshakeIdentityProof(Command& cmd) {
    if(this->handshake.state != HandshakeState::IDENTITY_PROOF)
        return command_result{error::web_handshake_invalid};

    if(this->handshake.identityType == IdentityType::TEASPEAK_FORUM) {
        auto encodedProof = cmd["proof"].string();
        auto proof = base64::decode(encodedProof);

        auto key = serverInstance->sslManager()->getRsaKey("teaforo_sign");
        if(!key)
            return command_result{error::web_handshake_identity_unsupported};
        if(!serverInstance->sslManager()->verifySign(key, this->handshake.proof_message, proof))
            return command_result{error::web_handshake_identity_proof_failed};

        this->properties()[property::CLIENT_TEAFORO_ID] = (int64_t) (*this->handshake.identityData)["user_id"].asInt64();
        this->properties()[property::CLIENT_TEAFORO_NAME] = (*this->handshake.identityData)["user_name"].asString();
        this->handshake.state = HandshakeState::SUCCEEDED;
    } else if(this->handshake.identityType == IdentityType::TEAMSPEAK) {
        auto proof = base64::decode(cmd["proof"].string());

        int result;
        if(ecc_verify_hash((u_char*) proof.data(), proof.length(), (u_char*) this->handshake.proof_message.data(), this->handshake.proof_message.length(), &result, this->handshake.identityKey.get()) != CRYPT_OK)
            return command_result{error::web_handshake_identity_proof_failed};
        if(!result)
            return command_result{error::web_handshake_identity_proof_failed};
        this->handshake.state = HandshakeState::SUCCEEDED;
    } else
        return command_result{error::web_handshake_invalid};

    return command_result{error::ok};
}