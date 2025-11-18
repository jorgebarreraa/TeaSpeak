#include "ProtocolHandler.h"
#include "Socket.h"
#include <thread>
#include <iostream>
#include <tommath.h>
#include <misc/endianness.h>
#include "audio/VoiceConnection.h"
#include "../logger.h"

using namespace std;
using namespace tc::connection;
using namespace ts::protocol;
using namespace ts;

//#define LOG_PING

void ProtocolHandler::handlePacketAck(const ts::protocol::PacketParser &ack) {
    if(ack.payload_length() < 2) {
        return;
    }

	string error;
	auto id = be2le16(&ack.payload()[0]);
	//log_trace(category::connection, tr("Handle packet acknowledge for {}"), be2le16(&ack->data()[0]));
    if(!this->acknowledge_handler.process_acknowledge(ack.type(), id, error)) {
        log_warn(category::connection, tr("Failed to handle acknowledge {}: {}"), id, error);
    }
}

void ProtocolHandler::handlePacketCommand(ts::command::ReassembledCommand* packet) {
	std::unique_ptr<Command> command;
	try {
	    auto payload = packet->command_view();
		command = make_unique<Command>(Command::parse(payload, true, false));
	} catch(const std::invalid_argument& ex) {
		log_error(category::connection, tr("Failed to parse command (invalid_argument): {}"), ex.what());
        ts::command::ReassembledCommand::free(packet);
		return;
	} catch(const std::exception& ex) {
		log_error(category::connection, tr("Failed to parse command (exception): {}"), ex.what());
        ts::command::ReassembledCommand::free(packet);
		return;
	}
    ts::command::ReassembledCommand::free(packet);
    //log_trace(category::connection, tr("Handing command {}"), command->command());

	if(command->command() == "initivexpand") {
		this->handleCommandInitIVExpend(*command);
	} else if(command->command() == "initivexpand2") {
		this->handleCommandInitIVExpend2(*command);
	} else if(command->command() == "initserver") {
		this->handleCommandInitServer(*command);
	}

	{
		lock_guard lock(this->handle->pending_commands_lock);
		this->handle->pending_commands.push_back(move(command));
	}
	this->handle->execute_pending_commands();
}

void ProtocolHandler::handlePacketVoice(const ts::protocol::PacketParser &packet) {
	this->handle->voice_connection->process_packet(packet);
}

void ProtocolHandler::handlePacketPing(const ts::protocol::PacketParser &packet) {
	if(packet.type() == PacketType::PONG) {
		uint16_t id = be2le16((char*) packet.payload().data_ptr());
#ifdef LOG_PING
		cout << "Received pong (" << id << "|" << this->ping.ping_id << ")" << endl;
#endif
		if(id == this->ping.ping_id) {
			this->ping.ping_received_timestamp = chrono::system_clock::now();
			this->ping.value = chrono::duration_cast<chrono::microseconds>(this->ping.ping_received_timestamp - this->ping.ping_send_timestamp);
#ifdef LOG_PING
			cout << "Updating client ping: " << chrono::duration_cast<chrono::microseconds>(this->ping.value).count() << "us" << endl;
#endif
		}
	} else {
#ifdef LOG_PING
		cout << "Received ping, sending pong" << endl;
#endif
		auto response = allocate_outgoing_client_packet(2);
		response->type_and_flags_ = PacketType::PONG | PacketFlag::Unencrypted;
        le2be16(packet.packet_id(), response->payload);
        this->send_packet(response, false);
	}
}

void ProtocolHandler::ping_send_request() {
    auto packet = allocate_outgoing_client_packet(0);
    packet->type_and_flags_ = PacketType::PING | PacketFlag::Unencrypted;

    packet->ref();
    this->send_packet(packet, false);

	this->ping.ping_send_timestamp = chrono::system_clock::now();
	this->ping.ping_id = packet->packet_id();
    packet->unref();
}