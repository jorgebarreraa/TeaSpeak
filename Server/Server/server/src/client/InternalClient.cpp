#include <log/LogUtils.h>
#include <misc/memtracker.h>
#include "InternalClient.h"

using namespace std;
using namespace ts;
using namespace ts::server;

InternalClient::InternalClient(sql::SqlManager* sql,const std::shared_ptr<server::VirtualServer>& handle, std::string displayName, bool generalClient) : ConnectedClient(sql, handle) {
    memtrack::allocated<InternalClient>(this);
    this->properties()[property::CLIENT_TYPE] = ClientType::CLIENT_INTERNAL;
    this->properties()[property::CLIENT_TYPE_EXACT] = ClientType::CLIENT_INTERNAL;
    this->properties()[property::CLIENT_NICKNAME] = displayName;

    if(generalClient){
        this->properties()[property::CLIENT_UNIQUE_IDENTIFIER] = displayName;
    } else {
        this->properties()[property::CLIENT_UNIQUE_IDENTIFIER] = "";
    }
}
InternalClient::~InternalClient() {
    memtrack::freed<InternalClient>(this);
}

void InternalClient::sendCommand(const ts::Command &command, bool low) { }
void InternalClient::sendCommand(const ts::command_builder &command, bool low) { }

bool InternalClient::close_connection(const std::chrono::system_clock::time_point& timeout) {
    logError(this->getServerId(), "Internal client is force to disconnect?");

    if(this->server)
        this->server->unregisterInternalClient(static_pointer_cast<InternalClient>(this->ref()));
    this->properties()[property::CLIENT_ID] = 0;
    return true;
}

void InternalClient::tick_server(const std::chrono::system_clock::time_point &time) {
    ConnectedClient::tick_server(time);
}

bool InternalClient::disconnect(const std::string &reason) {
    return false;
}
