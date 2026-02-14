#!/bin/bash
# Build CXXTerminal for TeaSpeak server.
# Called by Server/Root/libraries/build.sh via exec_script_external.
# Requires: libevent_path env var pointing to libevent library dir (e.g. "event")

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
    echo "ERROR: CXXTerminal source directory '${library_path}' not found."
    exit 1
}

local_root="$(pwd)"
out_dir="${local_root}/${library_path}/out/${build_os_type:-linux}_${build_os_arch:-amd64}"
build_dir="${library_path}/_cmake_build"

# Determine libevent cmake dir
if [[ -n "${libevent_path}" ]]; then
    event_cmake_dir="${local_root}/${libevent_path}/out/${build_os_type:-linux}_${build_os_arch:-amd64}/lib/cmake/libevent"
    if [[ ! -d "${event_cmake_dir}" ]]; then
        # Try alternative locations
        event_cmake_dir="${local_root}/${libevent_path}/_cmake_build"
    fi
else
    event_cmake_dir="${local_root}/event/out/${build_os_type:-linux}_${build_os_arch:-amd64}/lib/cmake/libevent"
fi

mkdir -p "${build_dir}"
check_err_exit "${library_path}" "Failed to create CXXTerminal build directory"
mkdir -p "${out_dir}"
check_err_exit "${library_path}" "Failed to create CXXTerminal output directory"

cd "${build_dir}"
check_err_exit "${library_path}" "Failed to enter CXXTerminal build directory"

cmake "../../${library_path}" \
    -DCMAKE_CXX_FLAGS="${CXX_FLAGS} -fPIC" \
    -DCMAKE_C_FLAGS="${C_FLAGS} -fPIC" \
    -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-Release}" \
    -DCMAKE_INSTALL_PREFIX="${out_dir}" \
    -DNO_EMBETTED_LIBEVENT=1 \
    -DLibevent_DIR="${event_cmake_dir}" \
    ${CMAKE_OPTIONS}
check_err_exit "${library_path}" "CXXTerminal CMake configuration failed"

make ${CMAKE_MAKE_OPTIONS:-"-j$(nproc)"}
check_err_exit "${library_path}" "CXXTerminal build failed"

make install
check_err_exit "${library_path}" "CXXTerminal install failed"

cd "${local_root}"

echo "CXXTerminal built successfully."

set_build_successful "${library_path}"
