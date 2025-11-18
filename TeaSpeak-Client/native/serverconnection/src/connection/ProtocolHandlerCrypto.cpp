#include "ProtocolHandler.h"
#include "Socket.h"
#include "../logger.h"
#include <thread>
#include <tomcrypt.h>
#include <tommath.h>
#include <misc/base64.h>
#include <misc/digest.h>
#include <License.h>
#include <ed25519/ed25519.h>
#include <ed25519/sha512.h>

using namespace std;
using namespace tc::connection;
using namespace ts::protocol;
using namespace ts;

inline void generate_random(uint8_t *destination, size_t length) {
	while(length-- > 0) {
        *(destination++) = (uint8_t) rand();
	}
}

std::string ProtocolHandler::generate_client_initiv() {
	if(!this->crypto.initiv_command.empty())
		return this->crypto.initiv_command;

	/* setup basic parameters */
	if((this->crypto.alpha[0] & 0x01) == 0) {
		generate_random(this->crypto.alpha, 10);
		this->crypto.alpha[0] |= 0x01;
	}

	Command command("clientinitiv");
	command["alpha"] = base64::encode((char*) this->crypto.alpha, 10);
	command["ot"] = 1;
	command["ip"] = "unknown";

	if(this->server_type != server_type::TEAMSPEAK) {/* if TEAMSPEAK then TS3 has been enforced */
        command.enableParm("teaspeak"); /* using "old" encryption system, we expect a teaspeak=1 within the response */
	}

	{
		size_t buffer_length = 265;
		char buffer[265];
		auto result = ecc_export((unsigned char *) buffer, (unsigned long*) &buffer_length, PK_PUBLIC, &*this->crypto.identity);
		if(result == CRYPT_OK) {
            command["omega"] = base64::encode(buffer, (unsigned long) buffer_length);
		} else {
            log_error(category::connection, tr("Failed to export identiry ({})"), result);
		}
	}

	this->crypto.initiv_command = command.build(true);
	return this->crypto.initiv_command;
}

void ProtocolHandler::handleCommandInitIVExpend(ts::Command &cmd) {
	this->pow.last_buffer = pipes::buffer{};

	auto alpha = base64::decode(cmd["alpha"].string());
	auto beta = base64::decode(cmd["beta"].string());
	auto omega = base64::decode(cmd["omega"].string());

	if(alpha.length() != 10 || memcmp(alpha.data(), this->crypto.alpha, 10) != 0) {
		this->handle->call_connect_result.call(this->handle->errors.register_error(tr("alpha key miss match")), true);
		this->handle->close_connection();

		log_error(category::connection, tr("InitIVExpend contains invalid alpha"));
		return;
	}

	ecc_key server_key{};
	if(ecc_import((u_char*) omega.data(), (unsigned long) omega.length(), &server_key) != CRYPT_OK) {
		this->handle->call_connect_result.call(this->handle->errors.register_error(tr("failed to import server key")), true);
		this->handle->close_connection();

		log_error(category::connection, tr("InitIVExpend contains invalid key"));
		return;
	}

	string error;
	if(!this->crypt_handler.setupSharedSecret(alpha, beta, &server_key, &*this->crypto.identity, error)) {
		this->handle->call_connect_result.call(this->handle->errors.register_error(tr("failed to setup encryption")), true);
		this->handle->close_connection();

		log_error(category::connection, tr("Failed to setup crypto ({})"), error);
		return;
	}
	this->crypt_setupped = true;

	if(this->server_type == server_type::UNKNOWN) {
		if(cmd[0].has("teaspeak") && cmd["teaspeak"].as<bool>()) {
			this->server_type = server_type::TEASPEAK;
		} else {
			this->server_type = server_type::TEAMSPEAK;
		}
	}

	this->handle->call_connect_result.call(0, true);
	this->connection_state = connection_state::CONNECTING;
}


int __ed_sha512_init(sha512_context* ctx) {
	ctx->context = new hash_state{};
	return sha512_init((hash_state*) ctx->context) == CRYPT_OK;
}

int __ed_sha512_final(sha512_context* ctx, unsigned char *out) {
	assert(ctx->context);

	auto result = sha512_done((hash_state*) ctx->context, out) == CRYPT_OK;
	delete (hash_state*) ctx->context;
	return result;
}
int __ed_sha512_update(sha512_context* ctx, const unsigned char *msg, size_t len) {
	assert(ctx->context);
	return sha512_process((hash_state*) ctx->context, msg, (unsigned long) len) == CRYPT_OK;
}

static sha512_functions __ed_sha512_functions {
		__ed_sha512_init,
		__ed_sha512_final,
		__ed_sha512_update
};

void ProtocolHandler::handleCommandInitIVExpend2(ts::Command &cmd) {
	this->pow.last_buffer = pipes::buffer{};

	/* setup ed functions */
	if(&__ed_sha512_functions != &_ed_sha512_functions) {
        _ed_sha512_functions = __ed_sha512_functions;
	}

	auto beta = base64::decode(cmd["beta"].string());
	auto omega = base64::decode(cmd["omega"].string());
	auto proof = base64::decode(cmd["proof"].string());

	auto crypto_chain_data = base64::decode(cmd["l"].string());
	auto crypto_root = cmd[0].has("root") ? base64::decode(cmd["root"].string()) : string((char*) license::teamspeak::public_root, 32);
	auto crypto_hash = digest::sha256(crypto_chain_data);

	/* suspicious, tries the server to hide himself? We dont know */
	if(this->server_type == server_type::UNKNOWN) {
		if(cmd[0].has("root")) {
            this->server_type = server_type::TEASPEAK;
		} else {
            this->server_type = server_type::TEAMSPEAK;
		}
	}

	ecc_key server_key{};
	if(ecc_import((u_char*) omega.data(), (unsigned long) omega.length(), &server_key) != CRYPT_OK) {
		this->handle->call_connect_result.call(this->handle->errors.register_error(tr("failed to import server key")), true);
		this->handle->close_connection();

		log_error(category::connection, tr("InitIVExpend contains invalid key"));
		return;
	}

	int result, crypt_result;
	if((crypt_result = ecc_verify_hash((u_char*) proof.data(), (unsigned long) proof.length(), (u_char*) crypto_hash.data(), (unsigned long) crypto_hash.length(), &result, &server_key)) != CRYPT_OK || result != 1) {
		this->handle->call_connect_result.call(this->handle->errors.register_error(tr("failed to verify server integrity")), true);
		this->handle->close_connection();
		return;
	}

	string error;
	auto crypto_chain = license::teamspeak::LicenseChain::parse(crypto_chain_data, error, false);
	if(!crypto_chain) {
		this->handle->call_connect_result.call(this->handle->errors.register_error(tr("failed to read crypto chain")), true);
		this->handle->close_connection();
		return;
	}

	auto server_public_key = crypto_chain->generatePublicKey(*(license::teamspeak::LicensePublicKey*) crypto_root.data());
	crypto_chain->print();

	u_char seed[32];
	ed25519_create_seed(seed);
	u_char public_key[32], private_key[64]; /* We need 64 bytes because we're doing some SHA512 actions */
	ed25519_create_keypair(public_key, private_key, seed);

	/* send clientek response */
	{
		size_t sign_buffer_length = 200;
		char sign_buffer[200];

		prng_state prng_state{};
		memset(&prng_state, 0, sizeof(prng_state));

		auto proof_data = digest::sha256(string((char*) public_key, 32) + beta);
		if(ecc_sign_hash((uint8_t*) proof_data.data(), (unsigned long) proof_data.length(), (uint8_t*) sign_buffer, (unsigned long*) &sign_buffer_length, &prng_state, find_prng("sprng"), &*this->crypto.identity) != CRYPT_OK) {
			this->handle->call_connect_result.call(this->handle->errors.register_error(tr("failed to generate proof of identity")), true);
			this->handle->close_connection();
			return;
		}

		Command response("clientek");
		response["ek"] = base64::encode((char*) public_key, 32);
		response["proof"] = base64::encode(sign_buffer, (unsigned long) sign_buffer_length);
		/* no need to send this because we're sending the clientinit as the begin packet along with the POW init */
		//this->_packet_id_manager.nextPacketId(PacketTypeInfo::Command); /* skip the first because we've send our first command within the low level handshake packets */
		this->send_command(response, false, std::make_unique<std::function<void(bool)>>([&](bool success){
            if(success) {
                /* trigger connected; because the connection has been established on protocol layer */
                this->crypt_setupped = true;
                this->handle->call_connect_result.call(0, true);
                this->connection_state = connection_state::CONNECTING;
            }
        })); /* needs to be encrypted at the time! */
	}


	if(!this->crypt_handler.setupSharedSecretNew(string((char*) this->crypto.alpha, 10), beta, (char*) private_key, server_public_key.data())) {
		this->handle->call_connect_result.call(this->handle->errors.register_error(tr("failed to setup encryption")), true);
		this->handle->close_connection();
		return;
	}
}
