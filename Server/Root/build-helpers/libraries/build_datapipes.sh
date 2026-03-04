#!/bin/bash
# Build DataPipes for TeaSpeak server.
# Called by Server/Root/libraries/build.sh via exec_script_external.
# DataPipes is compiled against BoringSSL.

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
    echo "ERROR: DataPipes source directory '${library_path}' not found."
    exit 1
}

[[ -z "${datapipes_webrtc}" ]] && datapipes_webrtc=1
[[ "${datapipes_webrtc}" -eq 1 ]] && _datapipes_webrtc="ON" || _datapipes_webrtc="OFF"

local_root="$(pwd)"
out_dir="${local_root}/${library_path}/out/${build_os_type:-linux}_${build_os_arch:-amd64}"
build_dir="${library_path}/_cmake_build"

_cxx_options="-fPIC -static-libgcc -static-libstdc++"
[[ "${build_os_type}" == "win32" ]] && _cxx_options="-DWIN32"

general_options="-DCMAKE_C_FLAGS=\"-fPIC\" -DCMAKE_CXX_FLAGS=\"${_cxx_options}\" -DBUILD_EXAMPLES=OFF -DBUILD_STATIC=1 -DBUILD_SHARED=1 -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE:-Release}"

# DataPipes' FindCrypto.cmake only searches for shared (.so) library names and derives
# the include path as ${Crypto_ROOT_DIR}/include.  We built BoringSSL as static (.a),
# so we must point ROOT_DIR at the BoringSSL source root (where include/ lives) and
# explicitly pass the library paths to prevent find_library from failing to locate .a files.
boringssl_root="${local_root}/boringssl"
crypto_options="-DCRYPTO_TYPE=\"boringssl\""
crypto_options="${crypto_options} -DCrypto_ROOT_DIR=\"${boringssl_root}\""
crypto_options="${crypto_options} -DCrypto_INCLUDE_DIR=\"${boringssl_root}/include\""
crypto_options="${crypto_options} -DCrypto_SSL_LIBRARY=\"${boringssl_root}/lib/libssl.a\""
crypto_options="${crypto_options} -DCrypto_CRYPTO_LIBRARY=\"${boringssl_root}/lib/libcrypto.a\""

web_cmake_flags="-DBUILD_WEBRTC=${_datapipes_webrtc}"
if [[ "${build_os_type}" != "win32" && "${_datapipes_webrtc}" == "ON" ]]; then
    glib20_dir=$(realpath "${local_root}/glibc/linux_${build_os_arch:-amd64}/")
    glib20_lib_path=$(realpath "${glib20_dir}/lib/"*/"/")
    web_cmake_flags="${web_cmake_flags} -DGLIB_PREBUILD_INCLUDES=\"${glib20_dir}/include;${glib20_dir}/include/glib-2.0/;${glib20_lib_path}/glib-2.0/include/\""
    web_cmake_flags="${web_cmake_flags} -DGLIB_PREBUILD_LIBRARIES=\"${glib20_lib_path}/libgio-2.0.so;z;resolv;${glib20_lib_path}/libgmodule-2.0.so;${glib20_lib_path}/libgobject-2.0.so;${glib20_lib_path}/libffi.so;${glib20_lib_path}/libglib-2.0.so;pcre\""
    web_cmake_flags="${web_cmake_flags} -DLIBNICE_PREBUILD_PATH=\"${local_root}/libnice/linux_${build_os_arch:-amd64}\""
fi

mkdir -p "${build_dir}"
check_err_exit "${library_path}" "Failed to create DataPipes build directory"
mkdir -p "${out_dir}"

cd "${build_dir}"
check_err_exit "${library_path}" "Failed to enter DataPipes build directory"

eval cmake "../../${library_path}" ${general_options} ${crypto_options} ${web_cmake_flags} -DCMAKE_INSTALL_PREFIX="${out_dir}" ${CMAKE_OPTIONS}
check_err_exit "${library_path}" "DataPipes CMake configuration failed"

make ${CMAKE_MAKE_OPTIONS:-"-j$(nproc)"}
check_err_exit "${library_path}" "DataPipes build failed"

make install
check_err_exit "${library_path}" "DataPipes install failed"

cd "${local_root}"

echo "DataPipes built successfully."

set_build_successful "${library_path}"
