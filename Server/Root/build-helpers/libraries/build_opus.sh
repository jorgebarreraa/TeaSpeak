#!/bin/bash
# Build opus for TeaSpeak server.
# opus uses autotools (autogen.sh + configure + make).

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
    echo "ERROR: opus source directory '${library_path}' not found."
    exit 1
}

local_root="$(pwd)"
out_dir="${local_root}/${library_path}/out/${build_os_type:-linux}_${build_os_arch:-amd64}"

mkdir -p "${out_dir}"

cd "${library_path}"
check_err_exit "${library_path}" "Failed to enter opus directory"

if [[ -f "autogen.sh" ]]; then
    ./autogen.sh
    check_err_exit "${library_path}" "opus autogen.sh failed"
fi

mkdir -p build
cd build

../configure \
    CFLAGS="-fPIC ${C_FLAGS}" \
    CXXFLAGS="-fPIC ${CXX_FLAGS}" \
    --prefix="${out_dir}" \
    --disable-shared \
    --enable-static
check_err_exit "${library_path}" "opus configure failed"

make ${CMAKE_MAKE_OPTIONS:-"-j$(nproc)"}
check_err_exit "${library_path}" "opus build failed"

make install
check_err_exit "${library_path}" "opus install failed"

cd "${local_root}"

echo "opus built successfully."

set_build_successful "${library_path}"
