#!/usr/bin/env bash

cd $(dirname $0)
#Depends on a lot so rebuild as soon anything changed! We still have to implement that dependecy stuff!

tearoot_cmake_config="`pwd`/build-helpers/cmake/config/tearoot-client.cmake"
tearoot_cmake_module="`pwd`/build-helpers/cmake/"
traroot_library="`pwd`/third_party"

shared_path="shared"
source `pwd`/build-helpers/build_helper.sh
begin_task "build_shared" "Building shared library"
requires_rebuild ${shared_path}
[[ $? -eq 0 ]] && {
    end_task "build_shared" "Shared build finished"
    exit 0
}

generate_build_path shared
echo "Build path: $build_path"
if [[ -d ${build_path} ]]; then
    echo "Deleting old build folder"
    rm -r ${build_path}
    check_err_exit ${shared_path} "Failed to delete old build folder"
fi

#set(CMAKE_MODULE_PATH "C:/Users/WolverinDEV/TeaSpeak/cmake")
#include(${CMAKE_MODULE_PATH}/libraries_wolverin_lap.cmake)

cmake_build ${shared_path} -DCMAKE_MODULE_PATH="$tearoot_cmake_module" -DCMAKE_PLATFORM_INCLUDE="$tearoot_cmake_config" -DLIBRARY_PATH="$traroot_library" -DBUILD_TESTS=OFF
check_err_exit ${shared_path} "Failed to build TeaSpeak shared library!"
set_build_successful ${shared_path}

end_task "build_shared" "Shared build finished"
