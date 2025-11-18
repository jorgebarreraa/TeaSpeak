#!/usr/bin/env bash

BASEDIR=$(dirname "$0")
cd "$BASEDIR/"

machine="$(uname -s)"
case "${machine}" in
    Linux*)     machine=Linux;;
#    Darwin*)    machine=Mac;;
    MINGW*)     machine=MinGW;;
    *)          machine="UNKNOWN:${machine}"
esac

if [[ ${machine} == "UNKNOWN"* ]]; then
    echo "Unknown platform ${machine}"
    exit 1
fi

if [[ ${machine} == "MinGW" ]]; then
    mkdir -p  build/win32_64/
    cd build/win32_64

    rm teaclient_codec.node; ln -s ../../codec/cmake-build-debug/teaclient_codec.node
    rm teaclient_ppt.node; ln -s ../../ppt/cmake-build-debug/teaclient_ppt.node
fi
if [[ ${machine} == "Linux" ]]; then
    mkdir -p  build/linux_amd64/
    cd build/linux_amd64

    rm teaclient_codec.node; ln -s ../../codec/cmake-build-debug/Debug/teaclient_codec.node
    rm teaclient_ppt.node; ln -s ../../ppt/cmake-build-debug/Debug/teaclient_ppt.node
fi

#/home/wolverindev/.config/TeaClient/crash_dumps/crash_dump_renderer_04a85069-9d30-48ec-e2fd5e9e-846c5305.dmp
