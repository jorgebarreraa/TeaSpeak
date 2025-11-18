#pragma once

#include <netinet/in.h>

namespace ts::server::udp {
    union pktinfo_storage {
        in_pktinfo v4;
        in6_pktinfo v6;
    };

    struct DatagramPacket {
        public:
            DatagramPacket* next_packet;

            sockaddr_storage address;
            pktinfo_storage pktinfo;

            size_t data_length;
            uint8_t data[0];

            static void destroy(DatagramPacket*);
            static DatagramPacket* create(const sockaddr_storage& address, const pktinfo_storage& address_info, size_t length, const uint8_t* data);

            static int extract_info(msghdr& /* header */, pktinfo_storage& /* info */);

            DatagramPacket() = delete;
            DatagramPacket(const DatagramPacket&) = delete;
            DatagramPacket(DatagramPacket&&) = delete;

            ~DatagramPacket() = delete;
    };
}