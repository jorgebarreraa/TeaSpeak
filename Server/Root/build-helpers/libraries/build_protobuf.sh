#!/bin/bash
# Build protobuf for TeaSpeak server.

[[ -z "${build_helper_file}" ]] && {
    echo "Missing build helper file. Please define \"build_helper_file\""
    exit 1
}
source ${build_helper_file}
[[ $build_helpers_defined -ne 1 ]] && {
    echo "Failed to include build helpers."
    exit 1
}

requires_rebuild "${library_path}"
[[ $? -eq 0 ]] && exit 0

[[ ! -d "${library_path}" ]] && {
    echo "ERROR: protobuf source directory '${library_path}' not found."
    exit 1
}

local_root="$(pwd)"
out_dir="${local_root}/${library_path}/out/${build_os_type:-linux}_${build_os_arch:-amd64}"
build_dir="${library_path}/_cmake_build"

mkdir -p "${build_dir}"
check_err_exit "${library_path}" "Failed to create protobuf build directory"
mkdir -p "${out_dir}"

cd "${build_dir}"
check_err_exit "${library_path}" "Failed to enter protobuf build directory"

# protobuf v21.12 uses cmake
cmake "../../${library_path}/cmake" \
    -DCMAKE_C_FLAGS="-fPIC ${C_FLAGS}" \
    -DCMAKE_CXX_FLAGS="-fPIC ${CXX_FLAGS}" \
    -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-Release}" \
    -DCMAKE_INSTALL_PREFIX="${out_dir}" \
    -Dprotobuf_BUILD_TESTS=OFF \
    -Dprotobuf_BUILD_EXAMPLES=OFF \
    -Dprotobuf_WITH_ZLIB=OFF \
    -DBUILD_SHARED_LIBS=OFF \
    ${CMAKE_OPTIONS} 2>/dev/null || \
cmake "../../${library_path}" \
    -DCMAKE_C_FLAGS="-fPIC ${C_FLAGS}" \
    -DCMAKE_CXX_FLAGS="-fPIC ${CXX_FLAGS}" \
    -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-Release}" \
    -DCMAKE_INSTALL_PREFIX="${out_dir}" \
    -Dprotobuf_BUILD_TESTS=OFF \
    -Dprotobuf_BUILD_EXAMPLES=OFF \
    -Dprotobuf_WITH_ZLIB=OFF \
    -DBUILD_SHARED_LIBS=OFF \
    ${CMAKE_OPTIONS}
check_err_exit "${library_path}" "protobuf CMake configuration failed"

make ${CMAKE_MAKE_OPTIONS:-"-j$(nproc)"}
check_err_exit "${library_path}" "protobuf build failed"

make install
check_err_exit "${library_path}" "protobuf install failed"

cd "${local_root}"

echo "protobuf built successfully."

set_build_successful "${library_path}"
