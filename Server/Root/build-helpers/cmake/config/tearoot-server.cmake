# TeaSpeak Server CMake Configuration
# This file is included by Server/Server/CMakeLists.txt via BUILD_INCLUDE_FILE.
# It sets library ROOT_DIR variables so find_package() can locate each library.
#
# LIBRARY_PATH is already set in CMakeLists.txt to:
#   ${CMAKE_CURRENT_SOURCE_DIR}/../libraries/ (absolute)
#
# BUILD_OS_TYPE and BUILD_OS_ARCH are passed via cmake -D flags (e.g. linux, amd64).
# initialize_build_paths() sets BUILD_OUTPUT = "out/${BUILD_OS_TYPE}_${BUILD_OS_ARCH}"

# Include build path helpers (sets BUILD_OUTPUT macro)
get_filename_component(_tearoot_helper_dir "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
include("${_tearoot_helper_dir}/tearoot-helper.cmake")

# BUILD_OUTPUT is now set to e.g. "out/linux_amd64"

# ─── Libraries installed via cmake make install ───────────────────────────────

# TomMath: installed to tommath/out/linux_amd64/{include,lib}
set(TomMath_ROOT_DIR "${LIBRARY_PATH}/tommath/${BUILD_OUTPUT}")

# TomCrypt: installed to tomcrypt/out/linux_amd64/{include,lib}
set(TomCrypt_ROOT_DIR "${LIBRARY_PATH}/tomcrypt/${BUILD_OUTPUT}")

# spdlog: installs its own cmake config to spdlog/out/linux_amd64/lib/spdlog/cmake/
set(spdlog_ROOT_DIR "${LIBRARY_PATH}/spdlog/${BUILD_OUTPUT}")
set(spdlog_DIR "${LIBRARY_PATH}/spdlog/${BUILD_OUTPUT}/lib/spdlog/cmake")

# yaml-cpp: installs cmake config to yaml-cpp/out/linux_amd64/lib/cmake/yaml-cpp/
set(yaml-cpp_ROOT_DIR "${LIBRARY_PATH}/yaml-cpp/${BUILD_OUTPUT}")
set(yaml-cpp_DIR "${LIBRARY_PATH}/yaml-cpp/${BUILD_OUTPUT}/lib/cmake/yaml-cpp")

# jsoncpp: installs cmake config to jsoncpp/out/linux_amd64/lib/cmake/jsoncpp/
set(jsoncpp_ROOT_DIR "${LIBRARY_PATH}/jsoncpp/${BUILD_OUTPUT}")
set(jsoncpp_DIR "${LIBRARY_PATH}/jsoncpp/${BUILD_OUTPUT}/lib/cmake/jsoncpp")
# Explicitly define jsoncpp_lib IMPORTED target.
# server/CMakeLists.txt and license/CMakeLists.txt both reference jsoncpp_lib as
# a CMake target. If find_package(jsoncpp) fails to find the cmake config the
# target is never created and CMake degrades it to -ljsoncpp_lib (linker error).
# Only create the target if the cmake config doesn't exist to avoid conflicts.
if(NOT EXISTS "${LIBRARY_PATH}/jsoncpp/${BUILD_OUTPUT}/lib/cmake/jsoncpp/jsoncppConfig.cmake")
    if(NOT TARGET jsoncpp_lib)
        add_library(jsoncpp_lib STATIC IMPORTED GLOBAL)
        set_target_properties(jsoncpp_lib PROPERTIES
            IMPORTED_LOCATION "${LIBRARY_PATH}/jsoncpp/${BUILD_OUTPUT}/lib/libjsoncpp.a"
            INTERFACE_INCLUDE_DIRECTORIES "${LIBRARY_PATH}/jsoncpp/${BUILD_OUTPUT}/include"
        )
    endif()
endif()

# zstd: installs cmake config to zstd/out/linux_amd64/lib/cmake/zstd/
set(zstd_ROOT_DIR "${LIBRARY_PATH}/zstd/${BUILD_OUTPUT}")
set(zstd_DIR "${LIBRARY_PATH}/zstd/${BUILD_OUTPUT}/lib/cmake/zstd")

# StringVariable: cmake install to StringVariable/out/linux_amd64/
set(StringVariable_ROOT_DIR "${LIBRARY_PATH}/StringVariable/${BUILD_OUTPUT}")

# CXXTerminal: cmake install to CXXTerminal/out/linux_amd64/
set(CXXTerminal_ROOT_DIR "${LIBRARY_PATH}/CXXTerminal/${BUILD_OUTPUT}")

# DataPipes: cmake install to DataPipes/out/linux_amd64/
set(DataPipes_ROOT_DIR "${LIBRARY_PATH}/DataPipes/${BUILD_OUTPUT}")

# jemalloc: autoconf install to jemalloc/out/linux_amd64/
set(jemalloc_ROOT_DIR "${LIBRARY_PATH}/jemalloc/${BUILD_OUTPUT}")

# Opus: cmake install to opus/out/linux_amd64/
set(Opus_ROOT_DIR "${LIBRARY_PATH}/opus/${BUILD_OUTPUT}")

# Breakpad: autoconf install to breakpad/out/linux_amd64/
set(breakpad_ROOT_DIR "${LIBRARY_PATH}/breakpad/${BUILD_OUTPUT}")

# Protobuf: cmake install to protobuf/out/linux_amd64/
set(Protobuf_ROOT_DIR "${LIBRARY_PATH}/protobuf/${BUILD_OUTPUT}")
set(Protobuf_INCLUDE_DIR "${LIBRARY_PATH}/protobuf/${BUILD_OUTPUT}/include")
set(Protobuf_LIBRARIES "${LIBRARY_PATH}/protobuf/${BUILD_OUTPUT}/lib/libprotobuf.a")

# BoringSSL: built to boringssl/lib/ (special layout)
set(BoringSSL_ROOT_DIR "${LIBRARY_PATH}/boringssl")
set(Crypto_ROOT_DIR "${LIBRARY_PATH}/boringssl/lib")

# ─── Legacy path variables (used by PermMapHelper, LicenseManager Qt targets) ─

set(LIBRARY_TOM_MATH    "${LIBRARY_PATH}/tommath/${BUILD_OUTPUT}/lib/libtommathStatic.a")
set(LIBRARY_TOM_CRYPT   "${LIBRARY_PATH}/tomcrypt/${BUILD_OUTPUT}/lib/libtomcrypt.a")
set(LIBRARY_PATH_ED255  "${LIBRARY_PATH}/ed25519/${BUILD_OUTPUT}/lib/libed25519.a")
set(LIBRARY_PATH_THREAD_POOL "${LIBRARY_PATH}/Thread-Pool/${BUILD_OUTPUT}/lib/libThreadPoolStatic.a")
# CXXTerminal: the library may be built as shared (.so) or static (.a).
# PermMapHelper and LicenseManager link against this raw path variable, so we
# must detect which variant is actually present at configure time.
if(EXISTS "${LIBRARY_PATH}/CXXTerminal/${BUILD_OUTPUT}/lib/libCXXTerminal.so")
    set(LIBRARY_PATH_TERMINAL "${LIBRARY_PATH}/CXXTerminal/${BUILD_OUTPUT}/lib/libCXXTerminal.so")
else()
    set(LIBRARY_PATH_TERMINAL "${LIBRARY_PATH}/CXXTerminal/${BUILD_OUTPUT}/lib/libCXXTerminal.a")
endif()
set(LIBRARY_PATH_VARIBALES   "${LIBRARY_PATH}/StringVariable/${BUILD_OUTPUT}/lib/libStringVariablesStatic.a")
set(LIBRARY_PATH_YAML        "${LIBRARY_PATH}/yaml-cpp/${BUILD_OUTPUT}/lib/libyaml-cpp.a")
set(LIBRARY_PATH_JSON        "${LIBRARY_PATH}/jsoncpp/${BUILD_OUTPUT}/lib/libjsoncpp.a")
set(LIBRARY_PATH_PROTOBUF    "${LIBRARY_PATH}/protobuf/${BUILD_OUTPUT}/lib/libprotobuf.a")
set(LIBRARY_PATH_BREAKPAD    "${LIBRARY_PATH}/breakpad/${BUILD_OUTPUT}/lib/libbreakpad_client.a")
set(LIBRARY_PATH_DATA_PIPES  "${LIBRARY_PATH}/DataPipes/${BUILD_OUTPUT}/lib/libDataPipes-Core-Static.a")
set(LIBRARY_PATH_BORINGSSL_SSL    "${LIBRARY_PATH}/boringssl/lib/libssl.a")
set(LIBRARY_PATH_BORINGSSL_CRYPTO "${LIBRARY_PATH}/boringssl/lib/libcrypto.a")
set(LIBRARY_PATH_OPUS        "${LIBRARY_PATH}/opus/${BUILD_OUTPUT}/lib/libopus.a")

# libevent path (used in some places)
set(LIBEVENT_PATH "${LIBRARY_PATH}/event/${BUILD_OUTPUT}/lib")
