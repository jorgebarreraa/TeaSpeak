#include "./response.h"
#include "./resolver.h"

#include <iostream>
#include <codecvt>
#ifdef WIN32
    #include <WS2tcpip.h>
    #include <in6addr.h>
    #include <ip2string.h>
    #include <inaddr.h>
    #include <WinDNS.h>
#else
	#include <arpa/inet.h>
	#include <netinet/in.h>
#endif

using namespace tc::dns::response;
using namespace tc::dns::response::rrparser;

#ifndef WIN32
uint16_t DNSHeader::field(int index) const {
	return ((uint16_t*) this->response->packet_data())[index];
}

DNSResourceRecords::DNSResourceRecords(std::shared_ptr<DNSResponseData> packet, size_t payload_offset, size_t length, uint32_t ttl, std::string name, rrtype::value type, rrclass::value klass)
		: offset{payload_offset}, length{length}, ttl{ttl}, name{std::move(name)}, type{type}, klass{klass} {
	this->data = std::move(packet);
}

const uint8_t* DNSResourceRecords::payload_data() const  {
	return this->data->buffer + this->offset;
}
#else
DNSResourceRecords::DNSResourceRecords(std::shared_ptr<tc::dns::DNSResponseData> data, PDNS_RECORDA rdata) : nrecord{rdata}, data{std::move(data)} { }
bool DNSResourceRecords::is_wide_string() const {
    return this->data->wide_string;
}
#endif

bool A::is_valid() {
#ifdef WIN32
    return true;
#else
    return this->handle->payload_length() == 4;
#endif
}

in_addr A::address() {
#ifdef WIN32
    in_addr result{};
    result.S_un.S_addr = this->handle->native_record()->Data.A.IpAddress;
    return result;
#else
    //TODO: Attention: Unaligned access
    return {*(uint32_t*) this->handle->payload_data()};
#endif
}

std::string A::address_string() {
#ifdef WIN32
    struct in_addr address = this->address();
    char buffer[17];
    RtlIpv4AddressToStringA(&address, buffer);
    return std::string{buffer};
#else
    auto _1 = this->handle->payload_data()[0],
            _2 = this->handle->payload_data()[1],
            _3 = this->handle->payload_data()[2],
            _4 = this->handle->payload_data()[3];
    return std::to_string(_1) + "." + std::to_string(_2) + "." + std::to_string(_3) + "." + std::to_string(_4);
#endif
}

//---------------- AAAA
#ifdef WIN32
bool AAAA::is_valid() {
    return true;
}

std::string AAAA::address_string() {
    struct in6_addr address = this->address();
    char buffer[47];
    RtlIpv6AddressToStringA(&address, buffer); //Supported for Win7 as well and not only above 8.1 like inet_ntop
    return std::string{buffer};
}

in6_addr AAAA::address() {
    in6_addr result{};
    memcpy(result.u.Byte, this->handle->native_record()->Data.AAAA.Ip6Address.IP6Byte, 16);
    return result;
}
#else
bool AAAA::is_valid() {
    return this->handle->payload_length() == 16;
}

std::string AAAA::address_string() {
    auto address = this->address();

    char buffer[INET6_ADDRSTRLEN];
	if(!inet_ntop(AF_INET6, (void*) &address, buffer, INET6_ADDRSTRLEN)) return "";
	return std::string(buffer);
}

in6_addr AAAA::address() {
	return {
			.__in6_u = {
				.__u6_addr32 = {
						//TODO: Attention unaligned memory access
						((uint32_t*) this->handle->payload_data())[0],
						((uint32_t*) this->handle->payload_data())[1],
						((uint32_t*) this->handle->payload_data())[2],
						((uint32_t*) this->handle->payload_data())[3]
				}
			}
	};
}
#endif

//---------------- SRV
#ifdef WIN32
bool SRV::is_valid() { return true; }
std::string SRV::target_hostname() {
    if(this->handle->is_wide_string()) {
        auto result = std::wstring{ ((PDNS_RECORDW) this->handle->native_record())->Data.Srv.pNameTarget };
        return std::string{result.begin(), result.end()};
    } else {
        return std::string{ this->handle->native_record()->Data.Srv.pNameTarget };
    }
}
uint16_t SRV::priority() { return this->handle->native_record()->Data.SRV.wPriority; }
uint16_t SRV::weight() { return this->handle->native_record()->Data.SRV.wWeight; }
uint16_t SRV::target_port() { return this->handle->native_record()->Data.SRV.wPort; }
#else
bool SRV::is_valid() {
	if(this->handle->payload_length() < 7)
		return false;
	size_t index = this->handle->payload_offset() + 6;
	std::string error{};
	this->handle->dns_data()->parse_dns_dn(error, index, true);
	return error.empty();
}

std::string SRV::target_hostname() {
	size_t index = this->handle->payload_offset() + 6;
	std::string error{};
	return this->handle->dns_data()->parse_dns_dn(error, index, true);
}


uint16_t SRV::priority() { return ntohs(((uint16_t*) this->handle->payload_data())[0]); }
uint16_t SRV::weight() { return ntohs(((uint16_t*) this->handle->payload_data())[1]); }
uint16_t SRV::target_port() { return ntohs(((uint16_t*) this->handle->payload_data())[2]); }
#endif

//---------------- All types with a name
bool named_base::is_valid() {
#ifdef WIN32
    return true;
#else
	size_t index = this->handle->payload_offset();
	std::string error{};
	this->handle->dns_data()->parse_dns_dn(error, index, true);
	return error.empty();
#endif
}

std::string named_base::name() {
#ifdef WIN32
    if(this->handle->is_wide_string()) {
        auto result = std::wstring{ ((PDNS_RECORDW) this->handle->native_record())->Data.Cname.pNameHost };
        return std::string{result.begin(), result.end()};
    } else {
        return std::string{ this->handle->native_record()->Data.Cname.pNameHost };
    }
#else
    size_t index = this->handle->payload_offset();
	std::string error{};
	return this->handle->dns_data()->parse_dns_dn(error, index, true);
#endif
}
