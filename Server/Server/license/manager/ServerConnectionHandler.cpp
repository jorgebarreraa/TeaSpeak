#include <netinet/tcp.h>
#include <misc/endianness.h>
#include <LicenseRequest.pb.h>
#include <LicenseManager.pb.h>
#include <shared/src/crypt.h>
#include <shared/include/license/license.h>
#include <misc/std_unique_ptr.h>
#include "ServerConnection.h"

using namespace std;
using namespace std::chrono;
using namespace license;
using namespace license::manager;

#define LERROR(message) \
do { \
	FLERROR(this->listener.future_connect, message); \
	return; \
} while(0)


void ServerConnection::handleMessage(const std::string& message) {
    auto& packet = this->network.current_packet;
	if(packet) {
        auto left = packet->header.length - packet->data.size();
        if(left >= message.length()) {
            packet->data += message;
        } else {
            packet->data += message.substr(0, left);
            this->network.overhead = message.substr(left);
        }
    } else {
        if(message.length() < sizeof(protocol::packet::header)) {
	        if(this->verbose)
                cout << "Invalid packet header size!" << endl;
            return;
        }

        packet = std::make_unique<protocol::packet>(protocol::PACKET_DISCONNECT, "");
        memcpy(packet.get(), message.data(), sizeof(protocol::packet::header));
        packet->data = message.substr(sizeof(protocol::packet::header));
    }
    if(packet->data.length() < packet->header.length)
    	return;

	if(!this->protocol.crypt_key.empty())
		xorBuffer((char*) packet->data.data(), packet->data.length(), this->protocol.crypt_key.data(), this->protocol.crypt_key.length());

	switch (packet->header.packetId) {
		case protocol::PACKET_SERVER_HANDSHAKE:
			this->handlePacketHandshake(packet->data);
			break;
		case protocol::PACKET_SERVER_AUTH_RESPONSE:
			this->handlePacketAuthResponse(packet->data);
			break;
		case protocol::PACKET_DISCONNECT:
			this->handlePacketDisconnect(packet->data);
			break;
	    case protocol::PACKET_SERVER_LICENSE_CREATE_RESPONSE:
	        this->handlePacketCreateResponse(packet->data);
	        break;
		case protocol::PACKET_SERVER_LIST_RESPONSE:
			this->handlePacketListResponse(packet->data);
			break;
        case protocol::PACKET_CLIENT_DELETE_RESPONSE:
            this->handlePacketDeleteResponse(packet->data);
            break;
		default:
			if(this->verbose)
				cout << "Invalid packet type: " << packet->header.packetId << endl;
	}
    packet.reset();
	if(!this->network.overhead.empty()) {
		auto oh = this->network.overhead;
		this->network.overhead = "";
		this->handleMessage(oh);
	}
}

void ServerConnection::handlePacketDisconnect(const std::string& message) {
	if(this->verbose)
		cout << "Got disconnect: " << message << endl;
	this->closeConnection();
}

void ServerConnection::handlePacketHandshake(const std::string& data) {
	if(this->protocol.state != protocol::HANDSCHAKE) LERROR("Protocol state mismatch");
	if(data.length() < 3) LERROR("Invalid packet size");

	if((uint8_t) data[0] != 0xAF || (uint8_t) data[1] != 0xFE) LERROR("Invalid handshake");
	if((uint8_t) data[2] != 2) LERROR("Invalid license protocol version. Please update this client!");

	auto key_length = be2le16(data.data(), 3);
	if(data.length() < key_length + 3) LERROR("Invalid packet size");
	this->protocol.crypt_key = data.substr(5, key_length);

	FLSUCCESS(this->listener.future_connect, true);
	this->protocol.state = protocol::MANAGER_AUTHORIZATION;
}

void ServerConnection::handlePacketAuthResponse(const std::string& data) {
	if(this->protocol.state != protocol::MANAGER_AUTHORIZATION) {
		FLERROR(this->listener.future_login, "Invalid state");
		return;
	}

	ts::proto::license::AuthorizationResponse pkt;
	if(!pkt.ParseFromString(data)) {
		FLERROR(this->listener.future_login, "Invalid response");
		return;
	}

	if(pkt.has_success() && pkt.success()) {
		FLSUCCESS(this->listener.future_login, true);
	} else {
		FLERROR(this->listener.future_login, pkt.has_message() ? "Login error: " + pkt.message() : "invalid login");
	}
}

void ServerConnection::handlePacketCreateResponse(const std::string& data) {
	ts::proto::license::LicenseCreateResponse pkt;
	if(!pkt.ParseFromString(data)) {
		FLERROR(this->listener.future_register, "Invalid response");
		return;
	}

	if(!pkt.has_error()) {
	    auto info = make_shared<LicenseInfo>();
	    info->first_name = pkt.license().first_name();
	    info->last_name = pkt.license().last_name();
	    info->username = pkt.license().username();
        info->email = pkt.license().email();
	    info->type = static_cast<LicenseType>(pkt.license().type());
	    info->start = system_clock::time_point() + milliseconds(pkt.license().begin());
        info->creation = system_clock::time_point() + milliseconds(pkt.license().created());
        info->end = system_clock::time_point() + milliseconds(pkt.license().end());

        string error;
        auto license = license::readLocalLicence(pkt.exported_key(), error);
        if(!license) FLERROR(this->listener.future_register, "Failed to read license!");

        if(this->listener.future_register) {
            auto l = this->listener.future_register.release();
            l->executionSucceed({license, info});
            delete l;
        }
	} else {
		FLERROR(this->listener.future_register, pkt.error());
	}
}

void ServerConnection::handlePacketListResponse(const std::string& data) {
	ts::proto::license::LicenseListResponse pkt;
	if(!pkt.ParseFromString(data)) {
		FLERROR(this->listener.future_list, "Invalid response");
		return;
	}

	std::map<std::string, std::shared_ptr<license::LicenseInfo>> response;
	for(const auto& entry : pkt.entries()) {
		auto info = make_shared<LicenseInfo>();
		info->first_name = entry.first_name();
		info->last_name = entry.last_name();
		info->username = entry.username();
		info->email = entry.email();
		info->type = static_cast<LicenseType>(entry.type());
		info->start = system_clock::time_point() + milliseconds(entry.begin());
		info->creation = system_clock::time_point() + milliseconds(entry.created());
		info->end = system_clock::time_point() + milliseconds(entry.end());

		response[entry.key()] = info;
	}

	FLSUCCESS(this->listener.future_list, response);
}

void ServerConnection::handlePacketDeleteResponse(const std::string& data) {
    ts::proto::license::LicenseDeleteResponse pkt;
    if(!pkt.ParseFromString(data)) {
        FLERROR(this->listener.future_list, "Invalid response");
        return;
    }

    FLSUCCESS(this->listener.future_delete, pkt.succeed());
}