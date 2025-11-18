#!/usr/bin/env bash

cd $(dirname $0)

export tearoot_cmake_config="`pwd`/build-helpers/cmake/config/tearoot-client.cmake"
export tearoot_cmake_module="`pwd`/build-helpers/cmake/"
export traroot_library="`pwd`/third_party"

source build-helpers/build_helper.sh
begin_task "build_client" "Building client"

./client/jenkins/create_build.sh
check_err_exit "__build_client" "Failed to build client!"

end_task "build_client" "Client build finished"
