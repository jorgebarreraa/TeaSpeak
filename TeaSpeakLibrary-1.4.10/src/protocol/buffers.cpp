#include "buffers.h"

using namespace std;
using namespace ts;
using namespace ts::protocol;
using namespace ts::buffer;

#ifndef WIN32
    #pragma GCC optimize ("O3")
#endif

meminfo buffer::buffer_memory() {
    size_t bytes_buffer = 0;
    size_t bytes_buffer_used = 0;
    size_t bytes_internal = 0;
    size_t nodes = 0;
    size_t nodes_full = 0;

    return {bytes_buffer, bytes_buffer_used, bytes_internal, nodes, nodes_full};
}