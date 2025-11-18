#!/usr/bin/env bash

source ../scripts/build_helper.sh

library_path="protobuf"
requires_rebuild ${library_path}
[[ $? -eq 0 ]] && exit 0

#./${library_path}/libraries/build_event.sh
#check_err_exit ${library_path} "Failed to build library libevent"

_cxx_options=""
[[ ${build_os_type} == "linux" ]] && _cxx_options="-fPIC -std=c++11"
base_path_suffix="/cmake/"
cmake_build ${library_path} -Dprotobuf_BUILD_TESTS=OFF -DCMAKE_CXX_FLAGS="${_cxx_options}" -DCMAKE_BUILD_TYPE=RelWithDebInfo
check_err_exit ${library_path} "Failed to build protobuf!"
set_build_successful ${library_path}