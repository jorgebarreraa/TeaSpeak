//
// Created by wolverindev on 08.05.18.
//

#include <netinet/tcp.h>
#include <misc/endianness.h>
#include <LicenseRequest.pb.h>
#include <LicenseManager.pb.h>
#include <misc/std_unique_ptr.h>
#include "ServerConnection.h"

using namespace std;
using namespace std::chrono;
using namespace license;
using namespace license::manager;

threads::Future<bool> ServerConnection::login(const std::string& username, const std::string& password) {
	this->listener.future_login = unique_ptr<threads::Future<bool>>(new threads::Future<bool>());

	ts::proto::license::AuthorizationRequest request;
	request.set_username(username);
	request.set_password(password);
	this->sendPacket({protocol::PACKET_CLIENT_AUTH_REQUEST, request});

	if(this->network.state != ConnectionState::CONNECTED)
		this->listener.future_login->executionFailed("not connected");

	return *this->listener.future_login;
}

threads::Future<std::pair<std::shared_ptr<license::License>, std::shared_ptr<license::LicenseInfo>>> ServerConnection::registerLicense(
		const std::string &first_name,
		const std::string &last_name,
		const std::string &username,
		const std::string &email,
		license::LicenseType type,
		const std::chrono::system_clock::time_point& end,
		const std::chrono::system_clock::time_point& start,
        const std::string& old_license
) {

	this->listener.future_register = std::make_unique<threads::Future<std::pair<std::shared_ptr<license::License>, std::shared_ptr<license::LicenseInfo>>>>();

	ts::proto::license::LicenseCreateRequest request;
	request.set_issuer_first_name(first_name);
	request.set_issuer_last_name(last_name);
	request.set_issuer_username(username);
	request.set_issuer_email(email);
	request.set_type(type);
	request.set_begin(duration_cast<milliseconds>(start.time_since_epoch()).count());
    request.set_end(duration_cast<milliseconds>(end.time_since_epoch()).count());
    if(!old_license.empty() && old_license != "none")
        request.set_old_key(old_license);

	this->sendPacket({protocol::PACKET_CLIENT_LICENSE_CREATE_REQUEST, request});

	if(this->network.state != ConnectionState::CONNECTED)
		this->listener.future_register->executionFailed("not connected");

	return *this->listener.future_register;
}

threads::Future<std::map<std::string, std::shared_ptr<license::LicenseInfo>>> ServerConnection::list(int offset,
																									 int count) {
	this->listener.future_list = std::make_unique<threads::Future<std::map<std::string, std::shared_ptr<license::LicenseInfo>>>>();

    ts::proto::license::LicenseListRequest request;
    request.set_offset(offset);
    request.set_count(count);
    this->sendPacket({protocol::PACKET_CLIENT_LIST_REQUEST, request});

	if(this->network.state != ConnectionState::CONNECTED)
		this->listener.future_register->executionFailed("not connected");

	return *this->listener.future_list;
}

threads::Future<bool> ServerConnection::deleteLicense(const std::string &key, bool full) {
    this->listener.future_delete = make_unique<threads::Future<bool>>();

    ts::proto::license::LicenseDeleteRequest request;
    request.set_key(key);
    request.set_full(full);
    this->sendPacket({protocol::PACKET_CLIENT_DELETE_REQUEST, request});

	if(this->network.state != ConnectionState::CONNECTED)
		this->listener.future_register->executionFailed("not connected");

    return *this->listener.future_delete;
}

void ServerConnection::ping() {
	this->list(0, 1);
	return; //FIXME
	cout << "Sending ping" << endl;
	this->sendPacket({protocol::PACKET_PING, nullptr});
}