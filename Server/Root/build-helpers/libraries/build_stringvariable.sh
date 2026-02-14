#!/bin/bash
# Build StringVariable for TeaSpeak server.

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
if [[ $? -eq 0 ]]; then
    # Marker exists; verify the output library is actually present before skipping.
    _sv_out="${library_path}/out/${build_os_type:-linux}_${build_os_arch:-amd64}/lib/libStringVariable.a"
    if [[ -f "$_sv_out" ]]; then
        exit 0
    fi
    echo "WARNING: .build_successful marker exists but $_sv_out is missing. Rebuilding StringVariable..."
    rm -f "${library_path}/.build_successful"
fi

[[ ! -d "${library_path}" ]] && {
    echo "ERROR: StringVariable source directory '${library_path}' not found."
    exit 1
}

local_root="$(pwd)"
out_dir="${local_root}/${library_path}/out/${build_os_type:-linux}_${build_os_arch:-amd64}"
build_dir="${library_path}/_cmake_build"

mkdir -p "${build_dir}"
check_err_exit "${library_path}" "Failed to create StringVariable build directory"
mkdir -p "${out_dir}"

cd "${build_dir}"
check_err_exit "${library_path}" "Failed to enter StringVariable build directory"

cmake "../../${library_path}" \
    -DCMAKE_CXX_FLAGS="${CXX_FLAGS} -fPIC" \
    -DCMAKE_C_FLAGS="${C_FLAGS} -fPIC" \
    -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-Release}" \
    -DCMAKE_INSTALL_PREFIX="${out_dir}" \
    ${CMAKE_OPTIONS}
check_err_exit "${library_path}" "StringVariable CMake configuration failed"

make ${CMAKE_MAKE_OPTIONS:-"-j$(nproc)"}
check_err_exit "${library_path}" "StringVariable build failed"

make install
check_err_exit "${library_path}" "StringVariable install failed"

cd "${local_root}"

echo "StringVariable built successfully."

set_build_successful "${library_path}"
