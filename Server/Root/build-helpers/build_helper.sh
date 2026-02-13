#!/bin/bash
# Minimal build helper script for TeaSpeak server library builds.
# Provides helper functions used by library build scripts.

build_helpers_defined=1

# Colors
color_green="\033[0;32m"
color_red="\033[0;31m"
color_yellow="\033[1;33m"
color_normal="\033[0m"

# requires_rebuild <library_path>
# Returns 0 if the library is already built (skip rebuild), 1 if rebuild is needed.
requires_rebuild() {
    local lib="$1"
    local marker="${lib}/.build_successful"
    if [[ -f "${marker}" ]]; then
        echo "Library ${lib} is already built. Skipping."
        return 0
    fi
    return 1
}

# set_build_successful <library_path>
# Marks the library as successfully built.
set_build_successful() {
    local lib="$1"
    touch "${lib}/.build_successful"
}

# check_err_exit <library_path> <message>
# Exits with an error if the last command failed.
check_err_exit() {
    local code=$?
    if [[ $code -ne 0 ]]; then
        echo -e "${color_red}ERROR${color_normal}: $2 (exit code: $code)"
        exit $code
    fi
}

# generate_build_path <library_path>
# Sets the global ${build_path} variable for the cmake build directory.
generate_build_path() {
    local lib="$1"
    build_path="${lib}/build"
}

# cmake_build <library_path> [cmake_args...]
# Configures and builds a library using CMake.
cmake_build() {
    local lib="$1"
    shift
    local cmake_args="$@"

    local out_dir="${lib}/out/${build_os_type}_${build_os_arch}"
    mkdir -p "${out_dir}"
    check_err_exit "${lib}" "Failed to create output directory"

    local build_dir="${lib}/_cmake_build"
    mkdir -p "${build_dir}"
    check_err_exit "${lib}" "Failed to create cmake build directory"

    cd "${build_dir}"
    check_err_exit "${lib}" "Failed to enter cmake build directory"

    eval cmake "../../${lib}" ${cmake_args} -DCMAKE_INSTALL_PREFIX="$(realpath ../${out_dir})" -DBUILD_OS_TYPE="${build_os_type}" -DBUILD_OS_ARCH="${build_os_arch}"
    check_err_exit "${lib}" "CMake configuration failed"

    make ${CMAKE_MAKE_OPTIONS:-"-j$(nproc)"}
    check_err_exit "${lib}" "Make failed"

    make install
    check_err_exit "${lib}" "Make install failed"

    cd ..
}

# begin_task / end_task: optional task logging
begin_task() {
    echo -e "${color_green}==> START${color_normal}: $2"
}

end_task() {
    echo -e "${color_green}==> DONE${color_normal}: $2"
}
