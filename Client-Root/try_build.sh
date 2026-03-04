#!/bin/bash

#'/g/Program Files (x86)/Microsoft Visual Studio/2019/Community/Common7/Tools/VsDevCmd.bat' -arch=amd64 -host_arch=amd64


export build_os_type=win32
export build_os_arch=amd64
export build_cmake_generator="-Ax64"

./third_party/build.sh || exit 1
./build_shared.sh
./build_client.sh
