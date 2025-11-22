//
// Created by WolverinDEV on 01/08/2020.
//

#include "ipv4.h"
#include "ipv6.h"

lookup::ip_v4<void> storage_4{};
lookup::ip_v6<void> loop6{};

int test() {
    sockaddr_in addr_4{};
    storage_4.insert(addr_4, nullptr);
    (void) storage_4.lookup(addr_4);
    storage_4.remove(addr_4);


    sockaddr_in6 addr_6{};
    loop6.insert(addr_6, nullptr);
    (void) loop6.lookup(addr_6);
    loop6.remove(addr_6);
    return 0;
}