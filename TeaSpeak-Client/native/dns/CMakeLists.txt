set(MODULE_NAME "teaclient_dns")

set(SOURCE_FILES ${SOURCE_FILES} src/resolver.cpp src/types.cpp src/response.cpp utils.cpp)
if (WIN32)
    set(SOURCE_FILES ${SOURCE_FILES} src/resolver_windows.cpp)
else()
    set(SOURCE_FILES ${SOURCE_FILES} src/resolver_linux.cpp)
endif()

find_package(Libevent REQUIRED)
include_directories(${LIBEVENT_INCLUDE_DIRS})

if(NOT WIN32)
    find_package(unbound REQUIRED)
endif()

add_nodejs_module(${MODULE_NAME} binding.cc ${SOURCE_FILES})
add_executable(DNS-Test ${SOURCE_FILES} test/main.cpp)

if (WIN32)
    target_link_libraries(${MODULE_NAME} libevent::core  Ws2_32.lib Ntdll.lib Dnsapi.lib)
    target_link_libraries(DNS-Test libevent::core Ws2_32.lib Ntdll.lib Dnsapi.lib)
else()
    target_link_libraries(${MODULE_NAME} unbound::static libevent::core libevent::pthreads pthread)
    target_link_libraries(DNS-Test unbound::static ssl crypto libevent::core libevent::pthreads pthread)
endif()