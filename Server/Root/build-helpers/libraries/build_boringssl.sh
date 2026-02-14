#!/bin/bash
# Build BoringSSL for TeaSpeak server.
# Called by Server/Root/libraries/build.sh via exec_script_external.
# Expected env vars: library_path (="boringssl"), build_os_type, build_os_arch
# Expected: BoringSSL source already cloned at libraries/boringssl/

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
    echo "ERROR: BoringSSL source directory '${library_path}' not found."
    echo "Run download_libraries_custom.sh first to clone BoringSSL."
    exit 1
}

echo "Building BoringSSL from source in ${library_path}/ ..."

generate_build_path "${library_path}"

[[ -d "${build_path}" ]] && {
    echo "Removing old build directory ${build_path}"
    rm -rf "${build_path}"
}
mkdir -p "${build_path}"
check_err_exit "${library_path}" "Failed to create BoringSSL build directory"

cd "${build_path}"
check_err_exit "${library_path}" "Failed to enter BoringSSL build directory"

# BoringSSL requires Go for code generation; install if needed
if ! command -v go &>/dev/null; then
    echo "Go not found, attempting to install golang-go..."
    apt-get install -y golang-go 2>/dev/null || true
fi

# Build BoringSSL as static libraries
cmake "../../${library_path}" \
    -DOPENSSL_NO_ASM=ON \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=OFF \
    -DCMAKE_C_FLAGS="${C_FLAGS} -fPIC" \
    -DCMAKE_CXX_FLAGS="-fPIC -Wno-error=format -Wno-error=format-extra-args ${CXX_FLAGS}" \
    ${CMAKE_OPTIONS}
check_err_exit "${library_path}" "BoringSSL CMake configuration failed"

make ${CMAKE_MAKE_OPTIONS:-"-j$(nproc)"} ssl crypto
check_err_exit "${library_path}" "BoringSSL build failed"

# Install library outputs to the expected lib/ directory
cd ../..
mkdir -p "${library_path}/lib"
check_err_exit "${library_path}" "Failed to create ${library_path}/lib directory"

# BoringSSL changed output layout: older versions put libs in build/ssl/ and
# build/crypto/, newer versions put them directly in build/. Search both.
_libssl=$(find "${build_path}" -maxdepth 2 -name "libssl.a" 2>/dev/null | head -1)
[[ -z "$_libssl" ]] && { echo "ERROR: libssl.a not found under ${build_path}"; exit 1; }
cp "$_libssl" "${library_path}/lib/libssl.a"
check_err_exit "${library_path}" "Failed to copy libssl.a"

_libcrypto=$(find "${build_path}" -maxdepth 2 -name "libcrypto.a" 2>/dev/null | head -1)
[[ -z "$_libcrypto" ]] && { echo "ERROR: libcrypto.a not found under ${build_path}"; exit 1; }
cp "$_libcrypto" "${library_path}/lib/libcrypto.a"
check_err_exit "${library_path}" "Failed to copy libcrypto.a"

echo "BoringSSL built successfully. Libraries in ${library_path}/lib/"

set_build_successful "${library_path}"
