#include <log/LogUtils.h>
#include <misc/endianness.h>
#include <misc/base64.h>
#include <ThreadPool/Timer.h>
#include <openssl/sha.h>
#include <src/client/SpeakingClient.h>

#include "../../InstanceHandler.h"
#include "VoiceClient.h"

using namespace std;
using namespace std::chrono;
using namespace ts::server;
using namespace ts::protocol;
using namespace ts;

VoiceClientCommandHandler::VoiceClientCommandHandler(const std::shared_ptr<VoiceClient> &client) : client_ref{client} {}
bool VoiceClientCommandHandler::handle_command(const std::string_view &command_string) {
    auto client = this->client_ref.lock();
    if(!client) {
        return false;
    }

    std::lock_guard command_lock{client->command_lock};
    {
        std::lock_guard state_lock{client->state_lock};
        switch(client->state) {
            case ConnectionState::INIT_LOW:
            case ConnectionState::INIT_HIGH:
            case ConnectionState::CONNECTED:
                break;

            case ConnectionState::DISCONNECTING:
            case ConnectionState::DISCONNECTED:
                /* we're just dropping the command and all future commands */
                return false;

            case ConnectionState::UNKNWON:
            default:
                assert(false);
                return false;
        }
    }

    client->handlePacketCommand(command_string);
    return true;
}

void VoiceClient::handlePacketCommand(const std::string_view& command_string) {
    std::unique_ptr<Command> command;
    command_result result{};
    try {
        command = make_unique<Command>(Command::parse(command_string, true, !ts::config::server::strict_ut8_mode));
    } catch(std::invalid_argument& ex) {
        result.reset(command_result{error::parameter_convert, std::string{ex.what()}});
        goto handle_error;
    } catch(std::exception& ex) {
        result.reset(command_result{error::parameter_convert, std::string{ex.what()}});
        goto handle_error;
    }

    this->handleCommandFull(*command, true);
    return;
    handle_error:
    this->notifyError(result);
    result.release_data();
}

command_result VoiceClient::handleCommand(ts::Command &command) {
    threads::MutexLock l2(this->command_lock);
    if(this->state == ConnectionState::DISCONNECTED) return command_result{error::client_not_logged_in};
    if(!this->voice_server) return command_result{error::server_unbound};

    if(this->state == ConnectionState::INIT_HIGH && this->handshake.state == HandshakeState::SUCCEEDED) {
        if(command.command() == "clientinit") {
            return this->handleCommandClientInit(command);
        }
    } else if(command.command() == "clientdisconnect") {
        return this->handleCommandClientDisconnect(command);
    }
    return SpeakingClient::handleCommand(command);
}

inline bool calculate_security_level(int& result, ecc_key* pubKey, const std::string& offset) {
    size_t pubLength = 256;
    char pubBuffer[256];
    if((result = ecc_export(reinterpret_cast<unsigned char *>(pubBuffer), &pubLength, PK_PUBLIC, pubKey)) != CRYPT_OK)
        return false;

    std::string hashStr = base64_encode(pubBuffer, pubLength) + offset;
    char shaBuffer[SHA_DIGEST_LENGTH];
    SHA1((const unsigned char *) hashStr.data(), hashStr.length(), (unsigned char *) shaBuffer);

    //Leading zero bits
    int zeroBits = 0;
    int i;
    for(i = 0; i < SHA_DIGEST_LENGTH; i++)
        if(shaBuffer[i] == 0) zeroBits += 8;
        else break;
    if(i < SHA_DIGEST_LENGTH) {
        for(uint8_t bit = 0; bit < 8; bit++) {
            if((shaBuffer[i] & (1U << bit)) == 0) zeroBits++;
            else break;
        }
    }
    result = zeroBits;
    return true;
}

command_result VoiceClient::handleCommandClientInit(Command &cmd) {
    if(this->getType() == ClientType::CLIENT_TEAMSPEAK) {
        auto client_identity = this->connection->crypt_setup_handler().identity_key();

        auto client_key_offset = cmd["client_key_offset"].string();
        if(client_key_offset.length() > 128 || client_key_offset.find_first_not_of("0123456789") != std::string::npos) {
            return command_result{error::parameter_invalid, "client_key_offset"};
        }

        int security_level;
        if(!calculate_security_level(security_level, &*client_identity, client_key_offset)) {
            logError(this->getServerId(), "[{}] Failed to calculate security level. Error code: {}", CLIENT_STR_LOG_PREFIX, security_level);
            return command_result{error::vs_critical};
        }

        if(security_level < 8) {
            return command_result{error::client_could_not_validate_identity};
        }

        auto requiredLevel = this->getServer()->properties()[property::VIRTUALSERVER_NEEDED_IDENTITY_SECURITY_LEVEL].as_unchecked<uint8_t>();
        if(security_level < requiredLevel) {
            return command_result{error::client_could_not_validate_identity, std::to_string(requiredLevel)};
        }
    }

    return SpeakingClient::handleCommandClientInit(cmd);
}

command_result VoiceClient::handleCommandClientDisconnect(Command& cmd) {
    auto reason = cmd["reasonmsg"].size() > 0 ? cmd["reasonmsg"].as<string>() : "";

    if(reason.empty()) {
        debugMessage(this->getServerId(), "{} Received client disconnect with no custom reason.", CLIENT_STR_LOG_PREFIX);
    } else {
        debugMessage(this->getServerId(), "{} Received client disconnect with custom reason: {}", CLIENT_STR_LOG_PREFIX, reason);
    }

    this->disconnect(VREASON_SERVER_LEFT, reason, nullptr, true);

    this->postCommandHandler.push_back([&]{
        /*
         * Instantly set the disconnect acknowledged flag since we actually received it from the client.
         */
        std::lock_guard flush_lock{this->flush_mutex};
        this->disconnect_acknowledged = std::make_optional(true);
    });

    return ts::command_result{error::ok};
}