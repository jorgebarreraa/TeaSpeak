#include <misc/endianness.h>
#include <misc/base64.h>
#include <misc/hex.h>
#include <log/LogUtils.h>
#include <LicenseManager.pb.h>
#include <shared/include/license/license.h>
#include "LicenseRequest.pb.h"
#include "shared/include/license/license.h"
#include "LicenseServer.h"
#include "WebAPI.h"
#include "StatisticManager.h"
#include "UserManager.h"

using namespace std;
using namespace std::chrono;
using namespace license;
using namespace ts;


inline void generate(char* buffer, size_t length){
	for(int index = 0; index < length; index++)
		buffer[index] = rand();
}

#define _str(x) #x

#define TEST_PROTOCOL_STATE(expected)               \
if(client->protocol.state != protocol::expected) {  \
	error = "invalid protocol state";               \
	return false;                                   \
}

#define ENSURE_PACKET_SIZE(expected)                \
if(packet.data.length() < (expected)) {             \
	error = "too small packet!";                    \
	return false;                                   \
}

#define PARSE_PROTO(class, var)                     \
ts::proto::license::class var;                      \
if(!var.ParseFromString(packet.data)) {             \
	error = "invalid data (" _str(class) ")!";      \
	return false;                                   \
}

#define CRYPT_KEY_LENGTH 32
bool LicenseServer::handleHandshake(shared_ptr<ConnectedClient>& client, protocol::packet& packet, std::string &error) {
	TEST_PROTOCOL_STATE(HANDSCHAKE);
	ENSURE_PACKET_SIZE(4);
	if((uint8_t) packet.data[0] != 0xC0 || (uint8_t) packet.data[1] != 0xFF || (uint8_t) packet.data[2] != 0xEE) {
		error = "invalid magic!";
		return false;
	}

	client->protocol.version = (uint8_t) packet.data[3];
	if(client->protocol.version < 2 || client->protocol.version > 3) {
	    error = "unsupported version";
	    return false;
	}

	bool manager = false;
	if(packet.data.length() >= 4 && (uint8_t) packet.data[4] == 1) {
		manager = true;
		//Its a manager!
	}

	char buffer_cryptkey[CRYPT_KEY_LENGTH];
	generate(buffer_cryptkey, CRYPT_KEY_LENGTH);

	uint8_t buffer[128];
	size_t buffer_index = 0;
	buffer[buffer_index++] = 0xAF;
	buffer[buffer_index++] = 0xFE;
	buffer[buffer_index++] = client->protocol.version;
	le2be16(CRYPT_KEY_LENGTH, buffer, buffer_index, &buffer_index);
	memcpy(&buffer[buffer_index], buffer_cryptkey, CRYPT_KEY_LENGTH);
	buffer_index += CRYPT_KEY_LENGTH;
	if(manager)
		buffer[buffer_index++] = 0x01; //Manager accepted

	client->sendPacket({protocol::PACKET_SERVER_HANDSHAKE, string((char*) buffer, buffer_index)});
	client->protocol.cryptKey = string(buffer_cryptkey, 32);

	if(manager) {
		client->protocol.state = protocol::MANAGER_AUTHORIZATION;
		client->type = ClientType::MANAGER;
	} else {
		client->protocol.state = protocol::SERVER_VALIDATION;
		client->type = ClientType::SERVER;
	}

	return true;
}

bool LicenseServer::handleDisconnect(shared_ptr<ConnectedClient>& client, protocol::packet& packet, std::string &error) {
	logMessage(LOG_GENERAL, "[CLIENT][" + client->address()  + "] Remote disconnect. Reason: " + packet.data);
	this->closeConnection(client);
	return true;
}

inline void fill_info(proto::license::LicenseInfo* proto, const shared_ptr<LicenseInfo>& info, const std::string& key) {
    proto->set_key(key);
    if(info) {
	    proto->set_username(info->username);
	    proto->set_first_name(info->first_name);
	    proto->set_last_name(info->last_name);
	    proto->set_email(info->email);
	    proto->set_type(info->type);
	    proto->set_created(duration_cast<milliseconds>(info->creation.time_since_epoch()).count());
	    proto->set_begin(duration_cast<milliseconds>(info->start.time_since_epoch()).count());
	    proto->set_end(duration_cast<milliseconds>(info->end.time_since_epoch()).count());
    } else {
	    proto->set_username("invalid (null)");
	    proto->set_first_name("invalid (null)");
	    proto->set_last_name("invalid (null)");
	    proto->set_email("invalid (null)");
	    proto->set_type(0);
	    proto->set_created(0);
	    proto->set_begin(0);
	    proto->set_end(0);
    }
}

std::string string_to_hex(const std::string& input)
{
	static const char* const lut = "0123456789ABCDEF";
	size_t len = input.length();

	std::string output;
	output.reserve(2 * len);
	for (size_t i = 0; i < len; ++i)
	{
		const unsigned char c = input[i];
		output.push_back(lut[c >> 4]);
		output.push_back(lut[c & 15]);
	}
	return output;
}

bool LicenseServer::handleServerValidation(shared_ptr<ConnectedClient> &client, protocol::packet &packet, std::string &error) {
    if(client->protocol.state != protocol::LICENSE_UPGRADE) /* server may wants to verify new license */
        TEST_PROTOCOL_STATE(SERVER_VALIDATION);

	PARSE_PROTO(ServerValidation, pkt);

	std::shared_ptr<License> remote_license{nullptr};
	if(pkt.licensed() && (!pkt.has_license() || !pkt.has_license_info())) {
	    error = "invalid/missing license data";
	    return false;
	}

	if(!pkt.has_info()) {
		error = "invalid data or missing data";
		return false;
	}

	if(pkt.licensed()){ //Client has license
		remote_license = readLocalLicence(pkt.license(), error);
		if(!remote_license) {
			error = "could not parse license (" + error + ")";
			return false;
		}

		logMessage(LOG_GENERAL, "[CLIENT][{}] Got remote license. Registered to {}. Key: {} (0x{})", client->address(), remote_license->owner(), base64::encode(remote_license->key()), string_to_hex(remote_license->key()));
		client->key = remote_license->key();
	}

    logMessage(LOG_GENERAL, "[CLIENT][{}] Got some server information. TeaSpeak-Version: {} uname: {}", client->address(), pkt.info().version(), pkt.info().uname());


	ts::proto::license::LicenseResponse response{};
	client->unique_identifier = pkt.info().has_unique_id() ? pkt.info().unique_id() : client->address();

	if(remote_license) {
        auto info = this->manager->query_license_info(remote_license->key());

        if(!info) {
            response.set_invalid_reason("license has not been found");
	        response.set_valid(false);

	        logMessage(LOG_GENERAL, "[CLIENT][{}] Remote license hasn't been found in database. Shutting down server!", client->address());
        } else {
            response.set_update_pending(info->upgrade_id > 0);
            client->key_pending_upgrade = info->upgrade_id;
	        if(info->deleted) {
                response.set_invalid_reason("license has been deleted");
		        response.set_valid(false);
		        logMessage(LOG_GENERAL, "[CLIENT][{}] Remote license has been deleted! Shutting down server!", client->address());
	        } else {
		        fill_info(response.mutable_license_info(), info, remote_license->data.licenceKey);
		        auto is_invalid = !info->isNotExpired();
		        if(is_invalid) {
                    response.set_invalid_reason("license is invalid");
                    response.set_valid(false);
		        } else {
		            response.set_valid(true);
		        }
	        }
        }
		this->manager->logRequest(remote_license->key(), client->unique_identifier, client->address(), pkt.info().version(), response.valid());
	}  else {
	    /* shall never happen, by default each server has the default license */
	    response.set_valid(true);
	}
    if(pkt.has_memory_valid() && !pkt.memory_valid()) {
        response.set_invalid_reason("server memory seems to be invalid");
        response.set_valid(false);
        logError(LOG_GENERAL, "Server {} has patched license memory!", client->address());
    }

	if(client->protocol.version == 2) {
        if(response.valid())
            response.mutable_blacklist()->set_state(ts::proto::license::VALID);
        else {
            response.mutable_blacklist()->set_reason(response.invalid_reason());
            response.mutable_blacklist()->set_state(ts::proto::license::BLACKLISTED); /* "Hack" for all old clients */

            if(!response.has_license_info()) fill_info(response.mutable_license_info(), nullptr, ""); /* "Hack" for old clients which require a license. Else the server would not be stopped */
            client->invalid_license = true;
        }
	} else {
	    if(!response.has_blacklist())
	        response.mutable_blacklist()->set_state(ts::proto::license::VALID);
	}
	client->invalid_license = !response.valid();

	client->sendPacket(protocol::packet{protocol::PACKET_SERVER_VALIDATION_RESPONSE, response});
	client->protocol.state = protocol::PROPERTY_ADJUSTMENT;
	return true;
}

bool LicenseServer::handlePacketLicenseUpgrade(shared_ptr<ConnectedClient> &client, license::protocol::packet &packet,
                                               std::string &error) {
    TEST_PROTOCOL_STATE(PROPERTY_ADJUSTMENT);
    PARSE_PROTO(RequestLicenseUpgrade, pkt);

    if(!client->key_pending_upgrade) {
        error = "no update pending";
        return false;
    }
    ts::proto::license::LicenseUpgradeResponse response;
    auto license_upgrade = this->manager->query_license_upgrade(client->key_pending_upgrade);
    response.set_valid(false);
    if(license_upgrade) {
        if(!license_upgrade->valid) {
            response.set_error_message("upgrade has been invalidated");
        } else if(license_upgrade->is_expired()) {
            response.set_error_message("upgrade has been expired");
        } else if(license_upgrade->not_yet_available()) {
            response.set_error_message("upgrade is not yet active.");
        } else {
            response.set_valid(true);
            response.set_license_key(license_upgrade->license_key);

        }
        this->manager->log_license_upgrade_attempt(license_upgrade->upgrade_id, response.valid(), client->unique_identifier, client->address());
        logMessage(LOG_GENERAL, "[CLIENT][{}] Client requested license upgrade {}. Result: {}", client->key_pending_upgrade, response.valid() ? "granted" : "denied (" + response.error_message() + ")");
    } else {
        response.set_error_message("failed to find upgrade");
    }

    client->sendPacket(protocol::packet{protocol::PACKET_SERVER_LICENSE_UPGRADE_RESPONSE, response});
    client->protocol.state = protocol::LICENSE_UPGRADE;
    return true;
}

bool LicenseServer::handlePacketPropertyUpdate(shared_ptr<ConnectedClient> &client, protocol::packet &packet, std::string &error) {
    if(client->protocol.state != protocol::LICENSE_UPGRADE) /* LICENSE_UPGRADE could be skipped */
        TEST_PROTOCOL_STATE(PROPERTY_ADJUSTMENT);
	if(client->invalid_license) {
		ts::proto::license::PropertyUpdateResponse response;
		response.set_accepted(true);
		response.set_reset_speach(false);
		response.set_speach_total_remote(0);
		response.set_speach_varianz_corrector(0);
		client->sendPacket(protocol::packet{protocol::PACKET_SERVER_PROPERTY_ADJUSTMENT, response});
		this->disconnectClient(client, "finished");

		/* Not sure if we really want here to log these data. May put a lvalid=false within the db? */
		return true;
	}
	PARSE_PROTO(PropertyUpdateRequest, pkt);

	logMessage(LOG_GENERAL, "[CLIENT][" + client->address() + "] Got server statistics:");
	logMessage(LOG_GENERAL, "[CLIENT][" + client->address() + "]   Spoken total       : " + to_string(pkt.speach_total()));
	logMessage(LOG_GENERAL, "[CLIENT][" + client->address() + "]   Spoken dead        : " + to_string(pkt.speach_dead()));
	logMessage(LOG_GENERAL, "[CLIENT][" + client->address() + "]   Spoken online      : " + to_string(pkt.speach_online()));
	logMessage(LOG_GENERAL, "[CLIENT][" + client->address() + "]   Spoken varianz     : " + to_string(pkt.speach_varianz()));
	logMessage(LOG_GENERAL, "[CLIENT][" + client->address() + "]   -------------------------------");
	logMessage(LOG_GENERAL, "[CLIENT][" + client->address() + "]   Users online       : " + to_string(pkt.clients_online()));
	logMessage(LOG_GENERAL, "[CLIENT][" + client->address() + "]   Web Users online   : " + to_string(pkt.web_clients_online()));
	logMessage(LOG_GENERAL, "[CLIENT][" + client->address() + "]   Queries online     : " + to_string(pkt.queries_online()));
	logMessage(LOG_GENERAL, "[CLIENT][" + client->address() + "]   Bots online        : " + to_string(pkt.bots_online()));
	logMessage(LOG_GENERAL, "[CLIENT][" + client->address() + "]   Servers            : " + to_string(pkt.servers_online()));
	this->manager->logStatistic(client->key, client->unique_identifier, client->address(), pkt);
	//TODO test stuff if its possible!

	ts::proto::license::WebCertificate* proto_web_certificate{nullptr};
	if(pkt.has_web_cert_revision()) {
		logMessage(LOG_GENERAL, "[CLIENT][" + client->address() + "]   -------------------------------");
		logMessage(LOG_GENERAL, "[CLIENT][" + client->address() + "]   Web cert revision : " + hex::hex(pkt.web_cert_revision()));

		auto cert = this->web_certificate;
		if(cert && cert->revision != pkt.web_cert_revision()) {
            proto_web_certificate = new ts::proto::license::WebCertificate{};
			proto_web_certificate->set_key(cert->key);
			proto_web_certificate->set_certificate(cert->certificate);
			proto_web_certificate->set_revision(cert->revision);
		}
	}

	ts::proto::license::PropertyUpdateResponse response;
	response.set_accepted(true);
    response.set_reset_speach(pkt.speach_total() < 0);
	response.set_speach_total_remote(pkt.speach_total());
	response.set_speach_varianz_corrector(0);
	response.set_allocated_web_certificate(proto_web_certificate);
	client->sendPacket(protocol::packet{protocol::PACKET_SERVER_PROPERTY_ADJUSTMENT, response});
	this->disconnectClient(client, "finished");

	if(this->statistics)
		this->statistics->reset_cache_general();

	if(this->web_statistics)
		this->web_statistics->async_broadcast_notify_general_update();
	return true;
}

bool LicenseServer::handlePacketAuth(shared_ptr<ConnectedClient> &client, protocol::packet &packet, std::string &error) {
	TEST_PROTOCOL_STATE(MANAGER_AUTHORIZATION);
	PARSE_PROTO(AuthorizationRequest, pkt);

	logMessage(LOG_GENERAL, "[MANAGER][" + client->address() + "] Got login. User: " + pkt.username() + " Password: " + pkt.password());

	ts::proto::license::AuthorizationResponse response;
	response.set_success(false);
    if(this->user_manager) {
	    auto user_account = this->user_manager->find_user(pkt.username());
	    if(user_account) {
		    if(user_account->verify_password(pkt.password())) {
		    	switch(user_account->status()) {
		    		case User::Status::ACTIVE:
					    response.set_success(true);
					    break;
		    		case User::Status::BANNED:
					    response.set_message("you have been banned");
					    break;
				    case User::Status::DISABLED:
					    response.set_message("you have been disabled");
					    break;
		    		default:
					    response.set_message("Your account hasn't been activated");
					    break;
		    	}
		    }
	    }
    } else {
	    response.set_success(pkt.password() == "HelloWorld");
    }
    if(!response.has_message() && !response.success())
		response.set_message("username or password mismatch");

    if(response.success()) {
	    logMessage(LOG_GENERAL, "[MANAGER][" + client->address() + "] Got succeeded user login. User: " + pkt.username() + " Password: " + pkt.password());
	    client->username = pkt.username();
	    client->protocol.state = protocol::MANAGER_CONNECTED;
    } else
	    logMessage(LOG_GENERAL, "[MANAGER][" + client->address() + "] Got failed user login. User: " + pkt.username() + " Password: " + pkt.password());

	client->sendPacket(protocol::packet{protocol::PACKET_SERVER_AUTH_RESPONSE, response});
	return true;
}

bool LicenseServer::handlePacketLicenseCreate(shared_ptr<ConnectedClient> &client, protocol::packet &packet, std::string &error) {
	TEST_PROTOCOL_STATE(MANAGER_CONNECTED);
	PARSE_PROTO(LicenseCreateRequest, pkt);

	auto old_license = pkt.has_old_key() ? hex::hex(pkt.old_key()) : "none";
    logMessage(LOG_GENERAL, "[MANAGER][" + client->address() + "] Register new license to {} {} ({}). E-Mail: {}. Old license: {}", pkt.issuer_first_name(), pkt.issuer_last_name(), pkt.issuer_username(), pkt.issuer_email(), old_license);

    auto old_key_id{0};
    ts::proto::license::LicenseCreateResponse response{};
    if(pkt.has_old_key()) {
        old_key_id = this->manager->key_id_cache()->get_key_id_from_key(pkt.old_key());
        if(old_key_id == 0) {
            response.set_error("failed to find old license key in database");
            goto _send_response;
        }
    }

    {
        auto db_info = make_shared<LicenseInfo>();
        db_info->start = system_clock::time_point() + milliseconds(pkt.begin());
        db_info->end = system_clock::time_point() + milliseconds(pkt.end());
        db_info->last_name = pkt.issuer_last_name();
        db_info->first_name = pkt.issuer_first_name();
        db_info->username = pkt.issuer_username();
        db_info->email = pkt.issuer_email();
        db_info->creation = system_clock::now();
        db_info->type = static_cast<LicenseType>(pkt.type());

        auto license = license::createLocalLicence(db_info->type, db_info->end, db_info->first_name + db_info->last_name);
        auto parsed_license = license::readLocalLicence(license, error);
        if(!parsed_license) {
            response.set_error("failed to register license (parse)");
        } else {
            if(!this->manager->register_license(parsed_license->key(), db_info, client->username)) {
                response.set_error("failed to register license");
                goto _send_response;
            }

            if(old_key_id) {
                auto new_key_id = this->manager->key_id_cache()->get_key_id_from_key(parsed_license->key());
                if(!new_key_id)  {
                    response.set_error("failed to find new license in database");
                    goto _send_response;
                }

                if(!this->manager->register_license_upgrade(old_key_id, new_key_id, std::chrono::system_clock::now(), db_info->end, license)) {
                    response.set_error("failed to register license upgrade");
                    goto _send_response;
                }
            }
            fill_info(response.mutable_license(), db_info, parsed_license->key());
            response.set_exported_key(license);
        }
    }

	_send_response:
    client->sendPacket(protocol::packet{protocol::PACKET_SERVER_LICENSE_CREATE_RESPONSE, response});
	return true;
}

bool LicenseServer::handlePacketLicenseList(shared_ptr<ConnectedClient> &client, protocol::packet &packet, std::string &error) {
	TEST_PROTOCOL_STATE(MANAGER_CONNECTED);
	PARSE_PROTO(LicenseListRequest, pkt);

	proto::license::LicenseListResponse response;
    response.set_end(false);

	for(const auto& info : this->manager->list_licenses(pkt.offset(), pkt.count())) {
		auto entry = response.add_entries();
		fill_info(entry, info.second, info.first);
	}
	client->sendPacket(protocol::packet{protocol::PACKET_SERVER_LIST_RESPONSE, response});

	return true;
}

bool LicenseServer::handlePacketLicenseDelete(shared_ptr<ConnectedClient> &client, protocol::packet &packet, std::string &error) {
	TEST_PROTOCOL_STATE(MANAGER_CONNECTED);
	PARSE_PROTO(LicenseDeleteRequest, pkt);

	proto::license::LicenseDeleteResponse response;
	response.set_succeed(this->manager->delete_license(pkt.key(), pkt.full()));
	client->sendPacket(protocol::packet{protocol::PACKET_CLIENT_DELETE_RESPONSE, response});

	return true;
}