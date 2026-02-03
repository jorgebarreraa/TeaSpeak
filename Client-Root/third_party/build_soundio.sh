#!/usr/bin/env bash

[[ -z "${build_helper_file}" ]] && {
	echo "Missing build helper file. Please define \"build_helper_file\""
	exit 1
}
source ${build_helper_file}
[[ $build_helpers_defined -ne 1 ]] && {
    echo "Failed to include build helpers."
    exit 1
}


library_path="soundio"
requires_rebuild ${library_path}
[[ $? -eq 0 ]] && exit 0

_fpic=""
[[ ${build_os_type} == "linux" ]] && _fpic="-fPIC"

cmake_build ${library_path} -DBUILD_DYNAMIC_LIBS=OFF -DBUILD_STATIC_LIBS=ON -DBUILD_EXAMPLE_PROGRAMS=OFF -DBUILD_TESTS=OFF -DCMAKE_BUILD_TYPE="Debug"
check_err_exit ${library_path} "Failed to build libsoundio!"
set_build_successful ${library_path}
