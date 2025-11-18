//
// Created by WolverinDEV on 02/08/2020.
//

#include <malloc.h>
#include <cstring>
#include "DatagramPacket.h"

using namespace ts::server::udp;

DatagramPacket* DatagramPacket::create(const sockaddr_storage &address, const pktinfo_storage& address_info, size_t length, const uint8_t *data) {
    auto membuf = malloc(sizeof(DatagramPacket) + length);
    auto instance = (DatagramPacket*) membuf;

    instance->next_packet = nullptr;
    memcpy(&instance->address, &address, sizeof(address));
    if(address.ss_family == AF_INET6) {
        memcpy(&instance->pktinfo, &address_info, sizeof(in6_pktinfo));
    } else {
        memcpy(&instance->pktinfo, &address_info, sizeof(in_pktinfo));
    }

    instance->data_length = length;
    if(data) {
        memcpy(&instance->data, data, length);
    }

    return instance;
}

void DatagramPacket::destroy(DatagramPacket *packet) {
    free(packet);
}

int DatagramPacket::extract_info(msghdr &message, pktinfo_storage &info) {
    for (cmsghdr* cmsg = CMSG_FIRSTHDR(&message); cmsg != nullptr; cmsg = CMSG_NXTHDR(&message, cmsg)) { // iterate through all the control headers
        if(cmsg->cmsg_type != IP_PKTINFO && cmsg->cmsg_type != IPV6_PKTINFO) {
            continue;
        }

        if(cmsg->cmsg_level == IPPROTO_IP) {
            memcpy(&info, (void*) CMSG_DATA(cmsg), sizeof(in_pktinfo));
            return 4;
        } else if(cmsg->cmsg_level == IPPROTO_IPV6) {
            memcpy(&info, (void*) CMSG_DATA(cmsg), sizeof(in6_pktinfo));
            return 6;
        }
    }
    return 0;
}