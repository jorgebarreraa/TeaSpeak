#include "buffers.h"

using namespace std;
using namespace ts;
using namespace ts::protocol;
using namespace ts::buffer;

#ifndef WIN32
    #pragma GCC optimize ("O3")
#endif

buffer_t buffer::allocate_buffer(size::value size) {
    return pipes::buffer{buffer::size::byte_length(size)};
}

cleaninfo buffer::cleanup_buffers(cleanmode::value mode) {
    return {0, 0};
}