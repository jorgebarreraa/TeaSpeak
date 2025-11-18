#include "ProtocolHandler.h"
#include "../logger.h"


using namespace std;
using namespace tc::connection;
using namespace ts::protocol;
using namespace ts;

void ProtocolHandler::handleCommandInitServer(ts::Command &cmd) {
	this->client_id = cmd["aclid"];
	this->connection_state = connection_state::CONNECTED;

	log_info(category::connection, tr("Received own client id: {}"), this->client_id);
}