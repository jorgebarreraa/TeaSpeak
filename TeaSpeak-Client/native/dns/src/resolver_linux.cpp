#include "./resolver.h"
#include "./response.h"

#include <cassert>
#include <functional>
#include <event.h>
#include <unbound.h>
#include <unbound-event.h>
#include <iostream>
#include <cstring>
#include <utility>

#include <fcntl.h> /* for TSDNS */
#include <unistd.h>

using namespace std;
using namespace tc::dns;

bool Resolver::initialize_platform(std::string &error, bool hosts, bool resolv) {
	this->ub_ctx = ub_ctx_create_event(this->event.base);
	if(!this->ub_ctx) {
		this->finalize();
		error = "failed to create ub context";
		return false;
	}

	/* Add /etc/hosts */
	auto err = !hosts ? 0 : ub_ctx_hosts((struct ub_ctx*) this->ub_ctx, nullptr);
	if(err != 0) {
		cerr << "Failed to add hosts file: " << ub_strerror(err) << endl;
	}

	/* Add resolv.conf */
	err = !resolv ? 0 : ub_ctx_resolvconf((struct ub_ctx*) this->ub_ctx, nullptr);
	if(err != 0) {
		cerr << "Failed to add hosts file: " << ub_strerror(err) << endl;
	}

	return true;
}

void Resolver::finalize_platform() {
	ub_ctx_delete((struct ub_ctx*) this->ub_ctx);
	this->ub_ctx = nullptr;
}

//Call only within the event loop!
void Resolver::destroy_dns_request(Resolver::dns_request *request) {
	assert(this_thread::get_id() == this->event.loop.get_id() || !this->event.loop_active);

	{
		lock_guard lock{this->request_lock};
		this->dns_requests.erase(std::find(this->dns_requests.begin(), this->dns_requests.end(), request), this->dns_requests.end());
	}

	if(!this->event.loop_active)
        ub_cancel(this->ub_ctx, request->ub_id);

	if(request->register_event) {
		event_del_noblock(request->register_event);
		event_free(request->register_event);
		request->register_event = nullptr;
	}

	if(request->timeout_event) {
		event_del_noblock(request->timeout_event);
		event_free(request->timeout_event);
		request->timeout_event = nullptr;
	}
	delete request;
}

//--------------- DNS
void Resolver::resolve_dns(const char *name, const rrtype::value &rrtype, const rrclass::value &rrclass, const std::chrono::microseconds& timeout, const dns_callback_t& callback) {
	if(!this->event.loop_active) {
		callback(ResultState::INITIALISATION_FAILED, 3, nullptr);
		return;
	}

	auto request = new dns_request{};
	request->resolver = this;

	request->callback = callback;
	request->host = name;
	request->rrtype = rrtype;
	request->rrclass = rrclass;

	request->timeout_event = evtimer_new(this->event.base, [](evutil_socket_t, short, void *_request) {
		auto request = static_cast<dns_request*>(_request);
		request->resolver->evtimer_dns_callback(request);
	}, request);

	request->register_event = evuser_new(this->event.base, [](evutil_socket_t, short, void *_request) {
		auto request = static_cast<dns_request*>(_request);
		auto errc = ub_resolve_event(request->resolver->ub_ctx, request->host.c_str(), (int) request->rrtype, (int) request->rrclass, (void*) request, [](void* _request, int a, void* b, int c, int d, char* e, int) {
			auto request = static_cast<dns_request*>(_request);
			request->resolver->ub_callback(request, a, b, c, d, e);
		}, &request->ub_id);

		if(errc != 0) {
			request->callback(ResultState::INITIALISATION_FAILED, errc, nullptr);
			request->resolver->destroy_dns_request(request);
		}
	}, request);

	if(!request->timeout_event || !request->register_event) {
		callback(ResultState::INITIALISATION_FAILED, 2, nullptr);

		if(request->timeout_event)
			event_free(request->timeout_event);

		if(request->register_event)
			event_free(request->register_event);

		delete request;
		return;
	}

	/*
	 * Lock here all requests so the event loop cant already delete the request
	 */
	unique_lock rlock{this->request_lock};

	{
		auto errc = event_add(request->timeout_event, nullptr);
		//TODO: Check for error

		evuser_trigger(request->register_event);
	}

	{
		auto seconds = chrono::floor<chrono::seconds>(timeout);
		auto microseconds = chrono::ceil<chrono::microseconds>(timeout - seconds);

		timeval tv{seconds.count(), microseconds.count()};
		auto errc = event_add(request->timeout_event, &tv);

		//TODO: Check for error
	}

	this->dns_requests.push_back(request);
	rlock.unlock();

	/* Activate the event loop */
	this->event.condition.notify_one();
}

void Resolver::evtimer_dns_callback(tc::dns::Resolver::dns_request *request) {
	if(request->ub_id > 0) {
		auto errc = ub_cancel(this->ub_ctx, request->ub_id);
		if(errc != 0) {
			cerr << "Failed to cancel DNS request " << request->ub_id << " after timeout (" << errc << "/" << ub_strerror(errc) << ")!" << endl;
		}
	}

	request->callback(ResultState::DNS_TIMEOUT, 0, nullptr);
	this->destroy_dns_request(request);
}

void Resolver::ub_callback(dns_request* request, int rcode, void *packet, int packet_length, int sec, char *why_bogus) {
	if(rcode != 0) {
		request->callback(ResultState::DNS_FAIL, rcode, nullptr);
	} else {
		auto callback = request->callback;
		auto data = std::unique_ptr<DNSResponse>(new DNSResponse{(uint8_t) sec, why_bogus, packet, (size_t) packet_length});
		callback(ResultState::SUCCESS, 0, std::move(data));
	}

	this->destroy_dns_request(request);
}

thread_local std::vector<size_t> visited_links;
std::string DNSResponseData::parse_dns_dn(std::string &error, size_t &index, bool allow_compression) {
	if(allow_compression) {
		visited_links.clear();
		visited_links.reserve(8);

		if(std::find(visited_links.begin(), visited_links.end(), index) != visited_links.end()) {
			error = "circular link detected";
			return "";
		}
		visited_links.push_back(index);
	}

	error.clear();

	string result;
	result.reserve(256); //Max length is 253

	while(true) {
		if(index + 1 > this->length) {
			error = "truncated data (missing code)";
			goto exit;
		}

		auto code = this->buffer[index++];
		if(code == 0) break;

		if((code >> 6U) == 3) {
			if(!allow_compression) {
				error = "found link, but links are not allowed";
				goto exit;
			}

			auto lower_addr = this->buffer[index++];
			if(index + 1 > this->length) {
				error = "truncated data (missing lower link address)";
				goto exit;
			}

			size_t addr = ((code & 0x3FU) << 8U) | lower_addr;
			if(addr >= this->length) {
				error = "invalid link address";
				goto exit;
			}
			auto tail = this->parse_dns_dn(error, addr, true);
			if(!error.empty())
				goto exit;

			if(!result.empty())
				result += "." + tail;
			else
				result = tail;
			break;
		} else {
			if(code > 63) {
				error = "max domain label length is 63 characters";
				goto exit;
			}

			if(!result.empty())
				result += ".";

			if(index + code >= this->length) {
				error = "truncated data (domain label)";
				goto exit;
			}

			result.append((const char*) (this->buffer + index), code);
			index += code;
		}
	}

	exit:
	if(allow_compression) visited_links.pop_back();
	return result;
}

DNSResponseData::~DNSResponseData() {
	::free(this->buffer);
}

DNSResponse::DNSResponse(uint8_t secure_state, const char* bogus, void *packet, size_t size) {
	this->bogus = bogus ? std::string{bogus} : std::string{"packet is secure"};
	this->secure_state = secure_state;

	this->data = make_shared<DNSResponseData>();
	this->data->buffer = (uint8_t*) malloc(size);
	this->data->length = size;

	memcpy(this->data->buffer, packet, size);
}

response::DNSHeader DNSResponse::header() const {
	return response::DNSHeader{this};
}

bool DNSResponse::parse(std::string &error) {
	if(this->is_parsed) {
		error = this->parse_error;
		return error.empty();
	}
	error.clear();
	this->is_parsed = true;

	auto header = this->header();
	size_t index = 12; /* 12 bits for the header */

	{
		auto count = header.query_count();
		this->parsed_queries.reserve(count);

		for(size_t idx = 0; idx < count; idx++) {
			auto dn = this->data->parse_dns_dn(error, index, true);
			if(!error.empty()) {
				error = "failed to parse query " + to_string(idx) + " dn: " + error; // NOLINT(performance-inefficient-string-concatenation)
				goto error_exit;
			}

			if(index + 4 > this->packet_length()) {
				error = "truncated data for query " + to_string(index);
				goto error_exit;
			}

			auto type = (rrtype::value) ntohs(*(uint16_t*) (this->data->buffer + index));
			index += 2;

			auto klass = (rrclass::value) ntohs(*(uint16_t*) (this->data->buffer + index));
			index += 2;

			this->parsed_queries.emplace_back(new response::DNSQuery{dn, type, klass});
		}
	}

	{
		auto count = header.answer_count();
		this->parsed_answers.reserve(count);

		for(size_t idx = 0; idx < count; idx++) {
			this->parsed_answers.push_back(this->parse_rr(error, index, true));
			if(!error.empty()) {
				error = "failed to parse answer " + to_string(idx) + ": " + error; // NOLINT(performance-inefficient-string-concatenation)
				goto error_exit;
			}
		}
	}

	{
		auto count = header.authority_count();
		this->parsed_authorities.reserve(count);

		for(size_t idx = 0; idx < count; idx++) {
			this->parsed_authorities.push_back(this->parse_rr(error, index, true));
			if(!error.empty()) {
				error = "failed to parse authority " + to_string(idx) + ": " + error; // NOLINT(performance-inefficient-string-concatenation)
				goto error_exit;
			}
		}
	}

	{
		auto count = header.additional_count();
		this->parsed_additionals.reserve(count);

		for(size_t idx = 0; idx < count; idx++) {
			this->parsed_additionals.push_back(this->parse_rr(error, index, true));
			if(!error.empty()) {
				error = "failed to parse additional " + to_string(idx) + ": " + error; // NOLINT(performance-inefficient-string-concatenation)
				goto error_exit;
			}
		}
	}

	return true;

	error_exit:
	this->parsed_queries.clear();
	this->parsed_answers.clear();
	this->parsed_authorities.clear();
	this->parsed_additionals.clear();
	return false;
}

std::shared_ptr<response::DNSResourceRecords> DNSResponse::parse_rr(std::string &error, size_t &index, bool allow_compressed) {
	auto dn = this->data->parse_dns_dn(error, index, allow_compressed);
	if(!error.empty()) {
		error = "failed to parse rr dn: " + error; // NOLINT(performance-inefficient-string-concatenation)
		return nullptr;
	}

	if(index + 10 > this->packet_length()) {
		error = "truncated header";
		return nullptr;
	}

	auto type = (rrtype::value) ntohs(*(uint16_t*) (this->data->buffer + index));
	index += 2;

	auto klass = (rrclass::value) ntohs(*(uint16_t*) (this->data->buffer + index));
	index += 2;

	auto ttl = ntohl(*(uint32_t*) (this->data->buffer + index));
	index += 4;

	auto payload_length = ntohs(*(uint16_t*) (this->data->buffer + index));
	index += 2;

	if(index + payload_length > this->packet_length()) {
		error = "truncated body";
		return nullptr;
	}

	auto response = std::shared_ptr<response::DNSResourceRecords>(new response::DNSResourceRecords{this->data, index, payload_length, ttl, dn, type, klass});
	index += payload_length;
	return response;
}
