//
// Created by WolverinDEV on 29/07/2020.
//

#include <log/LogUtils.h>
#include <misc/base64.h>
#include <misc/digest.h>
#include <src/InstanceHandler.h>
#include <misc/endianness.h>
#include "CryptSetupHandler.h"
#include "./VoiceClientConnection.h"

using namespace ts;
using namespace ts::connection;
using namespace ts::server::server::udp;

inline void generate_random(uint8_t *destination, size_t length) {
    while(length-- > 0) {
        *(destination++) = (uint8_t) rand();
    }
}

CryptSetupHandler::CryptSetupHandler(VoiceClientConnection *connection) : connection{connection} { }

CryptSetupHandler::CommandHandleResult CryptSetupHandler::handle_command(const std::string_view &payload) {
    std::variant<ts::command_result, CommandHandleResult>(CryptSetupHandler::*command_handler)(const ts::command_parser&) = nullptr;

    if(payload.starts_with("clientinitiv ")) {
        command_handler = &CryptSetupHandler::handleCommandClientInitIv;
    } else if(payload.starts_with("clientek ")) {
        command_handler = &CryptSetupHandler::handleCommandClientEk;
    } else if(payload.starts_with("clientinit ")) {
        command_handler = &CryptSetupHandler::handleCommandClientInit;
    }

    if(!command_handler) {
        return CommandHandleResult::PASS_THROUGH;
    }

    this->last_command_ = std::chrono::system_clock::now();

    ts::command_parser parser{payload};
    try {
        std::unique_lock cmd_lock{this->command_lock};
        auto result = (this->*command_handler)(parser);

        CommandHandleResult handle_result;
        if(std::holds_alternative<CommandHandleResult>(result)) {
            handle_result = std::get<CommandHandleResult>(result);
        } else {
            auto cmd_result = std::move(std::get<ts::command_result>(result));

            ts::command_builder notify{"error"};
            cmd_result.build_error_response(notify, "id");

            if(parser.has_key("return_code")) {
                notify.put_unchecked(0, "return_code", parser.value("return_code"));
            }

            this->connection->send_command(notify.build(), false, nullptr);

            handle_result = cmd_result.has_error() ? CommandHandleResult::CLOSE_CONNECTION : CommandHandleResult::CONSUME_COMMAND;
            cmd_result.release_data();
        }
        return handle_result;
    } catch (std::exception& ex) {
        debugMessage(this->connection->virtual_server_id(), "{} Failed to handle connection command: {}. Closing connection.", this->connection->log_prefix(), ex.what());
        return CommandHandleResult::CLOSE_CONNECTION;
    }
}

CryptSetupHandler::CommandResult CryptSetupHandler::handleCommandClientInitIv(const ts::command_parser &cmd) {
    auto client = this->connection->getCurrentClient();
    assert(client);

    {
        std::lock_guard state_lock{client->state_lock};
        switch(client->state) {
            case ConnectionState::INIT_LOW:
                client->state = ConnectionState::INIT_HIGH;
                break;

            case ConnectionState::INIT_HIGH:
                logTrace(client->getServerId(), "{} Received a duplicated initiv. It seems like our initivexpand2 hasn't yet reached the client. The acknowledge handler should handle this issue for us.", CLIENT_STR_LOG_PREFIX_(client));
                return CommandHandleResult::CONSUME_COMMAND; /* we don't want to send an error id=0 msg=ok */

            case ConnectionState::CONNECTED:
            case ConnectionState::DISCONNECTING:
            case ConnectionState::DISCONNECTED:
                /* That's really odd an should not happen */
                return CommandHandleResult::PASS_THROUGH;

            case ConnectionState::UNKNWON:
            default:
                assert(false);
                return CommandHandleResult::PASS_THROUGH;
        }
    }

    this->connection->reset();
    this->connection->packet_decoder().register_initiv_packet();
    this->connection->packet_statistics().reset_offsets();

    bool use_teaspeak = cmd.has_switch("teaspeak");
    if(!use_teaspeak && !config::server::clients::teamspeak) {
        return command_result{error::client_type_is_not_allowed, config::server::clients::teamspeak_not_allowed_message};
    }

    if(use_teaspeak) {
        debugMessage(this->connection->virtual_server_id(), "{} Client using TeaSpeak.", this->connection->log_prefix());
        client->properties()[property::CLIENT_TYPE_EXACT] = ClientType::CLIENT_TEASPEAK;
    }

    this->seed_client = base64::decode(cmd.value("alpha"));
    if(this->seed_client.length() != 10) {
        return ts::command_result{error::parameter_invalid, "alpha"};
    }

    std::string clientOmega = base64::decode(cmd.value("omega")); //The identity public key
    std::string ip = cmd.value("ip");
    bool ot = cmd.has_key("ot") && cmd.value_as<bool>("ot");

    {
        auto remote_key_{new ecc_key{}};
        auto state = ecc_import((const unsigned char *) clientOmega.data(), clientOmega.length(), remote_key_);
        if(state != CRYPT_OK) {
            delete remote_key_;
            return ts::command_result{error::client_could_not_validate_identity};
        }

        this->remote_key = std::shared_ptr<ecc_key>{remote_key_, [](ecc_key* key) {
            if(!key) {
                return;
            }

            /* We can only call ecc_free if we've really initialized the remote key */
            ecc_free(key);
            delete key;
        }};

        client->properties()[property::CLIENT_UNIQUE_IDENTIFIER] = base64::encode(digest::sha1(cmd.value("omega")));
    }

    this->new_protocol = !use_teaspeak && ot && config::experimental_31 && (this->client_protocol_time_ >= 173265950ULL || this->client_protocol_time_ == (uint32_t) 5680278000ULL);

    this->seed_server.resize(this->new_protocol ? 54 : 10);
    for(auto& byte : this->seed_server) {
        byte = (uint8_t) rand();
    }

    auto server_public_key = client->getServer()->publicServerKey();
    if(server_public_key.empty()) {
        return ts::command_result{error::vs_critical, "failed to export server public key"};
    }

    if(this->new_protocol) {
        //Pre setup
        //Generate chain
        debugMessage(this->connection->virtual_server_id(), "{} Got client 3.1 protocol with build timestamp {}", this->connection->log_prefix(), this->client_protocol_time_);

        this->chain_data = serverInstance->getTeamSpeakLicense()->license();
        this->chain_data->chain->addEphemeralEntry();

        auto crypto_chain = this->chain_data->chain->exportChain();
        auto crypto_chain_hash = digest::sha256(crypto_chain);

        size_t sign_buffer_size{512};
        char sign_buffer[sign_buffer_size];

        prng_state prng_state{};
        memset(&prng_state, 0, sizeof(prng_state));

        auto sign_result = ecc_sign_hash(
                (u_char*) crypto_chain_hash.data(), crypto_chain_hash.length(),
                (u_char*) sign_buffer, &sign_buffer_size,
                &prng_state, find_prng("sprng"),
                client->getServer()->serverKey());

        if(sign_result != CRYPT_OK) {
            return ts::command_result{error::vs_critical, "failed to sign crypto chain"};
        }

        ts::command_builder answer{"initivexpand2"};
        answer.put_unchecked(0, "time", std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
        answer.put_unchecked(0, "l", base64::encode(crypto_chain));
        answer.put_unchecked(0, "beta", base64::encode(this->seed_server));
        answer.put_unchecked(0, "omega", server_public_key);
        answer.put_unchecked(0, "proof", base64::encode((const char*) sign_buffer, sign_buffer_size));
        answer.put_unchecked(0, "tvd", "");
        answer.put_unchecked(0, "root", base64::encode((char*) this->chain_data->public_key, 32));
        answer.put_unchecked(0, "ot", "1");

        this->connection->send_command(answer.build(), false, nullptr);
        client->handshake.state = SpeakingClient::HandshakeState::SUCCEEDED; /* we're doing the verify via TeamSpeak */

        return CommandHandleResult::CONSUME_COMMAND; /* we don't want to send an error id=0 msg=ok */
    } else {
        debugMessage(this->connection->virtual_server_id(), "{} Got non client 3.1 protocol with build timestamp {}", this->connection->log_prefix(), this->client_protocol_time_, this->client_protocol_time_);

        {
            ts::command_builder answer{"initivexpand"};
            answer.put_unchecked(0, "alpha", base64::encode(this->seed_client));
            answer.put_unchecked(0, "beta", base64::encode(this->seed_server));
            answer.put_unchecked(0, "omega", server_public_key);

            if(use_teaspeak) {
                answer.put_unchecked(0, "teaspeak", "1");
                client->handshake.state = SpeakingClient::HandshakeState::BEGIN; /* we need to start the handshake */
            } else {
                client->handshake.state = SpeakingClient::HandshakeState::SUCCEEDED; /* we're using the provided identity as identity */
            }

            this->connection->send_command(answer.build(), false, nullptr);
            this->connection->packet_encoder().encrypt_pending_packets();
        }

        std::string error;
        if(!this->connection->getCryptHandler()->setupSharedSecret(this->seed_client, this->seed_server, &*this->remote_key, client->getServer()->serverKey(), error)){
            logError(this->connection->virtual_server_id(), "{} Failed to calculate shared secret {}. Dropping client.",
                    this->connection->log_prefix(), error);
            return ts::command_result{error::vs_critical};
        }

        return CommandHandleResult::CONSUME_COMMAND; /* we don't want to send an error id=0 msg=ok */
    }
}

CryptSetupHandler::CommandResult CryptSetupHandler::handleCommandClientEk(const ts::command_parser &cmd) {
    debugMessage(this->connection->virtual_server_id(), "{} Got client ek!", this->connection->log_prefix());

    if(!this->chain_data || !this->chain_data->chain) {
        return ts::command_result{error::vs_critical, "missing chain data"};
    }

    auto client_key = base64::decode(cmd.value("ek"));
    auto private_key = this->chain_data->chain->generatePrivateKey(this->chain_data->root_key, this->chain_data->root_index);

    this->connection->getCryptHandler()->setupSharedSecretNew(this->seed_client, this->seed_server, (char*) private_key.data(), client_key.data());
    this->connection->packet_encoder().acknowledge_manager().reset();

    {
        char buffer[2];
        le2be16(1, buffer);

        auto pflags = protocol::PacketFlag::NewProtocol;
        this->connection->send_packet(protocol::PacketType::ACK, (protocol::PacketFlags) pflags, buffer, 2);
        //We cant use the send_packet_acknowledge function since it sends the acknowledge unencrypted
    }

    return CommandHandleResult::CONSUME_COMMAND; /* we don't want to send an error id=0 msg=ok */
}

CryptSetupHandler::CommandResult CryptSetupHandler::handleCommandClientInit(const ts::command_parser &) {
    /* the client must have received everything */
    this->connection->packet_encoder().acknowledge_manager().reset();
    this->seed_client.clear();
    this->seed_server.clear();
    this->chain_data = nullptr;

    return CommandHandleResult::PASS_THROUGH;
}