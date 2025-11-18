#include "ProtocolHandler.h"
#include "Socket.h"
#include "../logger.h"
#include "Error.h"

#include <misc/endianness.h>
#include <protocol/buffers.h>
#include <thread>
#include <tomcrypt.h>
#include <tommath.h>

using namespace std;
using namespace std::chrono;
using namespace tc::connection;
using namespace ts::protocol;
using namespace ts;

inline void generate_random(uint8_t *destination, size_t length) {
	while(length-- > 0) {
        *(destination++) = (uint8_t) rand();
	}
}

inline void write_reversed(uint8_t* destination, uint8_t* source, size_t length) {
	destination += length;
	while(length-- > 0) {
        *(--destination) = *(source++);
	}
}

inline bool solve_puzzle(mp_int& x, mp_int& n, mp_int& result, uint32_t level) {
	mp_int exp{};
	mp_init(&exp);
	mp_2expt(&exp, level);


	if (mp_exptmod(&x, &exp, &n, &result) != CRYPT_OK) { //Sometimes it fails (unknown why :D)
		mp_clear(&exp);
		return false;
	}

	mp_clear(&exp);
	return true;
}

void ProtocolHandler::handlePacketInit(const ts::protocol::PacketParser &packet) {
	this->pow.last_response = system_clock::now();

	auto data = packet.payload();
	auto packet_state = static_cast<pow_state::value>(data[0]);

	if(packet_state == pow_state::COMMAND_RESET) {
	    return;
		log_trace(category::connection, tr("[POW] Received reset"));
		this->pow.retry_count++;
		if(this->pow.retry_count > 8) {
            log_trace(category::connection, tr("[POW] Retied puzzle too many times. Aborting connect."));

            this->handle->call_connect_result.call(this->handle->errors.register_error(tr("failed to solve connect puzzle")), true);
            this->handle->close_connection();
            return;
		}
		this->pow.state = pow_state::COOKIE_SET; /* next expected packet state */
		this->pow_send_cookie_get();
		return;
	}

	log_trace(category::connection, tr("[POW] State {} | {}"), packet_state, data.length());
	if(packet_state != this->pow.state) {
        return; //TODO handle error?
    }

	this->acknowledge_handler.reset(); /* we don't need an ack anymore for our init packet */

	if(packet_state == pow_state::COOKIE_SET) {
		if(data.length() != 21 && data.length() != 5) {
			log_trace(category::connection, tr("[POW] Dropping cookie packet (got {} bytes expect 21 or 5 bytes)"), data.length());
			return;
		}

		if(data.length() == 21) {
			memcpy(&this->pow.server_control_data[0], &data[1], 16);
			//TODO test client data reserved bytes
		} else {
			auto errc = ntohl(*(uint32_t*) &data[1]);
			auto err = ts::findError((uint16_t) errc);

			log_error(category::connection, tr("[POW] Received error code: {:x} ({})"), errc, err.message);
			this->handle->call_connect_result.call(this->handle->errors.register_error(tr("received error: ") + to_string(errc) + " (" + err.message + ")"), true);
			this->handle->close_connection();
			return;
		}

		/* send puzzle get request */
		{
			this->pow.state = pow_state::PUZZLE_SET; /* next expected packet state */

			uint8_t response_buffer[25];
			le2be32((uint32_t) this->pow.client_ts3_build_timestamp, &response_buffer[0]);
			response_buffer[4] = pow_state::PUZZLE_GET;

			memcpy(&response_buffer[5], this->pow.server_control_data, 16);
			memcpy(&response_buffer[21], &data[17], 4);

			this->pow.last_buffer = pipes::buffer_view{response_buffer, 25}.own_buffer();
			this->send_init1_buffer();
		}

		return;
	} else if(packet_state == pow_state::PUZZLE_SET) {
		constexpr auto expected_bytes = 1 + 64 * 2 + 4 + 100;
		if(data.length() != 1 + 64 * 2 + 4 + 100) {
			log_trace(category::connection, tr("[POW] Dropping puzzle packet (got {} bytes expect {} bytes)"), data.length(), expected_bytes);
			return;
		}

		mp_int point_x{}, point_n{}, result{};
		if(mp_read_unsigned_bin(&point_x, (u_char*) &data[1], 64) < 0)
			return; //TODO handle error

		if(mp_read_unsigned_bin(&point_n, (u_char*) &data[65], 64) < 0) {
			mp_clear_multi(&point_x, nullptr);
			return; //TODO handle error
		}

		log_trace(category::connection, tr("[POW] Received puzzle with level {}"), be2le32(&data[1 + 64 + 64]));
		if(!solve_puzzle(point_x, point_n, result, be2le32(&data[1 + 64 + 64]))) {
			mp_clear_multi(&point_x, &point_n, nullptr);
			log_trace(connection, tr("[POW] Failed to solve puzzle!"));
			return; //TODO handle error
		}

		{
			auto command = this->generate_client_initiv();
			size_t response_buffer_length = 301 + command.size();
			auto response_buffer = buffer::allocate_buffer(response_buffer_length);

			le2be32((uint32_t) this->pow.client_ts3_build_timestamp, &response_buffer[0]);
			response_buffer[4] = pow_state::PUZZLE_SOLVE;
			memcpy(&response_buffer[5], &data[1], 64 * 2 + 100 + 4);

			auto offset = 4 + 1 + 2 * 64 + 04 + 100;
			memset(&response_buffer[offset], 0, 64);
			mp_to_unsigned_bin(&result, (u_char*) &response_buffer[offset + 64 - mp_unsigned_bin_size(&result)]);

			memcpy(&response_buffer[301], command.data(), command.size());

			this->pow.last_buffer = response_buffer;
            this->send_init1_buffer();
		}

		mp_clear_multi(&point_x, &point_n, &result, nullptr);
		this->connection_state = connection_state::INIT_HIGH;
	}
}

void ProtocolHandler::pow_send_cookie_get() {
	this->pow.state = pow_state::COOKIE_SET; /* next expected packet state */

	if((this->pow.client_control_data[0] & 0x01U) == 0) {
		generate_random(this->pow.client_control_data, 4);
		this->pow.client_control_data[0] |= 0x01U;
	}

	this->pow.client_ts3_build_timestamp = (uint64_t) floor<seconds>((system_clock::now() - hours{24}).time_since_epoch()).count();

	uint8_t response_buffer[21];
	le2be32((uint32_t) this->pow.client_ts3_build_timestamp, &response_buffer[0]);
	response_buffer[4] = pow_state::COOKIE_GET;
	memset(&response_buffer[5], 0, 4);
	memcpy(&response_buffer[9], &this->pow.client_control_data, 4);
	memset(&response_buffer[13], 0, 8);

	this->pow.last_buffer = pipes::buffer_view{response_buffer, 21}.own_buffer();
    this->send_init1_buffer();
}