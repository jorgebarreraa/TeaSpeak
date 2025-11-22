//
// Created by wolverindev on 14.01.19.
//

#include <misc/net.h>
#include <protocol/buffers.h>
#include <protocol/ringbuffer.h>
#include <iostream>
#include <array>

using namespace std;
using namespace std::chrono;
using namespace ts;
using namespace ts::protocol;

template <typename E, size_t S, typename T>
void print_queue(RingBuffer<E, S, T>& buffer) {
    cout << "Buffer size: " << buffer.capacity() << endl;
    for(size_t index = 0; index < buffer.capacity(); index++) {
        auto set = buffer.slot_set((T) index);
        if(buffer.current_slot() == index)
            cout << "   ";
        else
            cout << "   ";
        cout << index << " => " << set;
        if(set)
            cout << " " << buffer.slot_value(index);
        cout << endl;
    }
}

void print_meminfo() {
    auto meminfo = buffer::buffer_memory();
    printf("Used buffer bytes: %zu, Allocated buffer bytes %zu Internal: %zu. Nodes: %zu Nodes full: %zu\n", meminfo.bytes_buffer_used, meminfo.bytes_buffer, meminfo.bytes_internal, meminfo.nodes,meminfo.nodes_full);
}

void print_cleanup_memory(buffer::cleanmode::value mode) {
    auto info = buffer::cleanup_buffers(mode);
    printf("Cleanuped %zu, Buffer: %zu, Internal: %zu\n", info.bytes_freed_buffer + info.bytes_freed_internal, info.bytes_freed_buffer, info.bytes_freed_internal);
}

#define TEST_COUNT (1000000)
void malloc_speedtest() {
    auto alloc_begin = system_clock::now();
    deque<pipes::buffer> buffers;
    for(size_t i = 0; i < TEST_COUNT; i++)
        buffers.push_back(buffer::allocate_buffer(((buffer::size::value) i % 3) + 1));
    auto alloc_end = system_clock::now();
    buffers.clear();
    auto free_end = system_clock::now();
    printf("Alloc: %li Free: %li\n", duration_cast<nanoseconds>(alloc_end - alloc_begin).count() / TEST_COUNT, duration_cast<nanoseconds>(free_end - alloc_end).count() / TEST_COUNT);
}

int main() {
    /*
    RingBuffer<int, 16, uint16_t> buffer;

    buffer.insert_index(0, 1);
    buffer.insert_index(1, 2);
    buffer.insert_index(2, 3);
    buffer.pop_front();
    buffer.insert_index(0, 1);


    //assert(buffer.front_set());

    print_queue(buffer);
    cout << " => " << buffer.current_index() << " = " << buffer.pop_front() << endl;
    cout << " => " << buffer.current_index() << " = " << buffer.pop_front() << endl;
    cout << " => " << buffer.current_index() << " = " << buffer.pop_front() << endl;
    /
    assert(buffer.accept_index(0));
    assert(buffer.accept_index(1));
    assert(!buffer.accept_index(2));

    print_queue(buffer);
    assert(buffer.insert_index(1, 2));
    print_queue(buffer);
    assert(buffer.insert_index(0, 1));
    print_queue(buffer);
    assert(!buffer.insert_index(3, 2));
    print_queue(buffer);

    cout << buffer.pop_front() << endl;
    print_queue(buffer);
    assert(buffer.insert_index(2, 3));
    print_queue(buffer);
    cout << buffer.pop_front() << endl;
    print_queue(buffer);

    assert(buffer.accept_index(3));
    assert(!buffer.accept_index(4));
    assert(!buffer.accept_index(5));
    buffer.push_front(2);
    buffer.push_front(1);
    print_queue(buffer);


     */

    /*
        [2019-07-05 13:49:02] [DEBUG]   162 | [127.0.0.1:60793/Another TeaSpeak user | 1] Cant decrypt packet of type Voice. Packet ID: 65527, Estimated generation: 2, Full counter: 65536. Dropping packet. Error: memory verify failed!
        [2019-07-05 13:49:02] [DEBUG]   162 | [127.0.0.1:60793/Another TeaSpeak user | 1] Cant decrypt packet of type Voice. Packet ID: 65528, Estimated generation: 2, Full counter: 65536. Dropping packet. Error: memory verify failed!
        [2019-07-05 13:49:02] [DEBUG]   162 | [127.0.0.1:60793/Another TeaSpeak user | 1] Cant decrypt packet of type Voice. Packet ID: 65529, Estimated generation: 2, Full counter: 65536. Dropping packet. Error: memory verify failed!
        [2019-07-05 13:49:02] [DEBUG]   162 | [127.0.0.1:60793/Another TeaSpeak user | 1] Cant decrypt packet of type Voice. Packet ID: 65530, Estimated generation: 2, Full counter: 65536. Dropping packet. Error: memory verify failed!
        [2019-07-05 13:49:02] [DEBUG]   162 | [127.0.0.1:60793/Another TeaSpeak user | 1] Cant decrypt packet of type Voice. Packet ID: 65531, Estimated generation: 2, Full counter: 65536. Dropping packet. Error: memory verify failed!
        [2019-07-05 13:49:02] [DEBUG]   162 | [127.0.0.1:60793/Another TeaSpeak user | 1] Cant decrypt packet of type Voice. Packet ID: 65532, Estimated generation: 2, Full counter: 65536. Dropping packet. Error: memory verify failed!
        [2019-07-05 13:49:02] [DEBUG]   162 | [127.0.0.1:60793/Another TeaSpeak user | 1] Cant decrypt packet of type Voice. Packet ID: 65533, Estimated generation: 2, Full counter: 65536. Dropping packet. Error: memory verify failed!
        [2019-07-05 13:49:02] [DEBUG]   162 | [127.0.0.1:60793/Another TeaSpeak user | 1] Cant decrypt packet of type Voice. Packet ID: 65534, Estimated generation: 2, Full counter: 65536. Dropping packet. Error: memory verify failed!
        [2019-07-05 13:49:02] [DEBUG]   162 | [127.0.0.1:60793/Another TeaSpeak user | 1] Cant decrypt packet of type Voice. Packet ID: 65535, Estimated generation: 2, Full counter: 65536. Dropping packet. Error: memory verify failed!
     */
    PacketRingBuffer<int, 16> buffer;
    buffer.set_generation_packet(0, 65500);

    while(buffer.current_index() < 100 || (buffer.full_index() >> 16) == 0) {
        buffer.push_back(nullptr);
        buffer.pop_front();
        cout << buffer.full_index() << " | " << buffer.current_index() << " | " << buffer.generation(buffer.current_index()) << endl;
    }
    __asm__("nop");
}