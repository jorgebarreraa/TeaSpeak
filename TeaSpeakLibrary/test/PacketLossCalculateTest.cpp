//
// Created by WolverinDEV on 06/04/2020.
//
#include <iostream>
#include <bitset>
#include <src/protocol/PacketLossCalculator.h>
#include <cassert>
#include <vector>
#include <cmath>

using UnorderedPacketLossCalculator = ts::protocol::UnorderedPacketLossCalculator;

inline void print_unordered_stats(UnorderedPacketLossCalculator& generator) {
    std::cout << "Valid data: " << generator.valid_data() << ". (Received{local: " << generator.received_packets() << ", unconfirmed: " << generator.unconfirmed_received_packets() << ", total: " << generator.received_packets_total() << "}" <<
                                           " Lost: {local: " << generator.lost_packets() << ", unconfirmed: " << generator.unconfirmed_lost_packets() << ", total: " << generator.lost_packets_total() << "} Current ID: " << generator.last_packet_id() << ")\n";
}

template <typename vector_t>
vector_t swap_elements(vector_t vector, int per, int max_distance) {
    for(size_t index = 0; index < vector.size() - max_distance; index++) {
        if ((rand() % 100) < per) {
            //lets switch
            auto offset = rand() % max_distance;
            std::swap(vector[index], vector[index + offset]);
        }
    }

    return vector;
}

inline void generate_unordered(UnorderedPacketLossCalculator& generator, int loss, size_t count) {
    size_t id{generator.last_packet_id()};
    while(count--) {
        if(rand() % 100 >= loss)
            generator.packet_received(id);
        id++;
    }
}

void test_const_unordered_0() {
    UnorderedPacketLossCalculator generator{};
    for(size_t pid{0}; pid < 100; pid++)
        generator.packet_received(pid);

    assert(generator.received_packets_total() == 68);
    assert(generator.unconfirmed_received_packets() == 32);
    assert(generator.received_packets() == 68);

    assert(generator.lost_packets_total() == 0);
    assert(generator.lost_packets() == 0);
    assert(generator.unconfirmed_lost_packets() == 0);

    assert(generator.valid_data());
}


void test_const_unordered_1() {
    UnorderedPacketLossCalculator generator{};
    generator.packet_received(100); /* we lost the first 99 packets */

    assert(generator.received_packets_total() == 0);
    assert(generator.unconfirmed_received_packets() == 1);
    assert(generator.received_packets() == 0);

    assert(generator.lost_packets_total() == 68);
    assert(generator.lost_packets() == 68);
    assert(generator.unconfirmed_lost_packets() == 31);

    assert(generator.valid_data());
}

void test_const_unordered_2() {
    UnorderedPacketLossCalculator generator{};
    for(size_t pid{0}; pid < 100; pid++)
        if(pid % 2)
            generator.packet_received(pid);

    assert(generator.received_packets_total() == 34);
    assert(generator.received_packets() == 34);
    assert(generator.unconfirmed_received_packets() == 16);

    assert(generator.lost_packets_total() == 34);
    assert(generator.lost_packets() == 34);
    assert(generator.unconfirmed_lost_packets() == 16);

    assert(generator.valid_data());
}


void test_const_unordered_3() {
    UnorderedPacketLossCalculator generator{};
    for(size_t pid{0}; pid < 100; pid++)
        if(pid % 3)
            generator.packet_received(pid);

    assert(generator.received_packets_total() == 44);
    assert(generator.received_packets() == 44);
    assert(generator.unconfirmed_received_packets() == 22);

    assert(generator.lost_packets_total() == 23);
    assert(generator.lost_packets() == 23);
    assert(generator.unconfirmed_lost_packets() == 10);

    assert(generator.valid_data());
}

void test_const_unordered_4() {
    std::vector<uint32_t> received_packets{};
    for(size_t pid{0}; pid < 100; pid++)
        received_packets.push_back(pid);

    received_packets = swap_elements(received_packets, 50, 7);

    UnorderedPacketLossCalculator generator{};
    for(const auto& packet : received_packets)
        generator.packet_received(packet);

    assert(generator.received_packets_total() == 68);
    assert(generator.unconfirmed_received_packets() == 32);
    assert(generator.received_packets() == 68);

    assert(generator.lost_packets_total() == 0);
    assert(generator.lost_packets() == 0);
    assert(generator.unconfirmed_lost_packets() == 0);

    assert(generator.valid_data());
}

void test_const_unordered_5() {
    std::vector<uint32_t> received_packets{};
    for(size_t pid{0}; pid < 100; pid++)
        if(pid % 3)
            received_packets.push_back(pid);

    received_packets = swap_elements(received_packets, 30, 7);

    UnorderedPacketLossCalculator generator{};
    for(const auto& packet : received_packets)
        generator.packet_received(packet);

    assert(generator.received_packets_total() == 44);
    assert(generator.received_packets() == 44);
    assert(generator.unconfirmed_received_packets() == 22);

    assert(generator.lost_packets_total() == 23);
    assert(generator.lost_packets() == 23);
    assert(generator.unconfirmed_lost_packets() == 10);

    assert(generator.valid_data());
}

void test_unordered_6() {
    UnorderedPacketLossCalculator generator{};
    {
        const auto pid_base = generator.last_packet_id();
        for(size_t pid{0}; pid < 100; pid++)
            generator.packet_received(pid_base + pid);
    }

    print_unordered_stats(generator);
    generator.short_stats();

    {
        const auto pid_base = generator.last_packet_id();
        generator.packet_received(pid_base + 100);
    }
    print_unordered_stats(generator);
    generator.short_stats();

    {
        const auto pid_base = generator.last_packet_id();
        generator.packet_received(pid_base + 100);
    }
    print_unordered_stats(generator);
    generator.short_stats();
    {
        const auto pid_base = generator.last_packet_id();
        for(size_t pid{0}; pid < 100; pid++)
            generator.packet_received(pid_base + pid);
    }
    print_unordered_stats(generator);
    generator.short_stats();
    {
        const auto pid_base = generator.last_packet_id();
        for(size_t pid{0}; pid < 100; pid++)
            generator.packet_received(pid_base + pid);
    }
    print_unordered_stats(generator);
}

int main() {
#if 0
    test_const_unordered_0();
    test_const_unordered_1();
    test_const_unordered_2();
    test_const_unordered_3();
    test_const_unordered_4();
    test_const_unordered_5();
    test_unordered_6();
#endif
    size_t value = 0;


    std::vector<size_t> bandwidth_seconds{};
    for(size_t iteration{0}; iteration < 600; iteration++) {
        if(iteration >= 200 && iteration <= 270)
            bandwidth_seconds.push_back(0);
        else
            bandwidth_seconds.push_back(500);
    }

    constexpr auto cofactor = 0.915;
    //constexpr auto cofactor = 0.978;
    for(size_t iteration{0}; iteration < bandwidth_seconds.size(); iteration++) {
        const auto next_value = value * cofactor + bandwidth_seconds[iteration] * (1 - cofactor);
        if(next_value > value)
            value = ceil(next_value);
        else
            value = floor(next_value);
        std::cout << iteration << ": " << value << "\n";
    }
}
