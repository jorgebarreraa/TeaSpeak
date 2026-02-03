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


library_path="breakpad"
requires_rebuild ${library_path}
[[ $? -eq 0 ]] && exit 0

cd ${library_path}
git clone https://chromium.googlesource.com/linux-syscall-support src/third_party/lss

if [[ -d build ]]; then
    rm -r build
fi
mkdir build

cd build
check_err_exit ${library_path} "Failed to enter build directory!"

../configure
check_err_exit ${library_path} "Failed to configure"
make CXXFLAGS="-std=c++11 -I../../boringssl/include/ ${CXX_FLAGS} -static-libgcc -static-libstdc++" CFLAGS="${C_FLAGS}" ${MAKE_OPTIONS}
check_err_exit ${library_path} "Failed to build"
sudo make install
check_err_exit ${library_path} "Failed to install"
cd ../..

# cmake_build ${library_path} -DCMAKE_C_FLAGS="-fPIC -I../../boringssl/include/" -DEVENT__DISABLE_TESTS=ON -DEVENT__DISABLE_OPENSSL=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo
# check_err_exit ${library_path} "Failed to build libevent!"
set_build_successful ${library_path}
