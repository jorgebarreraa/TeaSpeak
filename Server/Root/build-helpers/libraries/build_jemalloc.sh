#!/bin/bash
# Build jemalloc for TeaSpeak server.
# jemalloc uses autotools.

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
    echo "ERROR: jemalloc source directory '${library_path}' not found."
    exit 1
}

local_root="$(pwd)"
out_dir="${local_root}/${library_path}/out/${build_os_type:-linux}_${build_os_arch:-amd64}"

mkdir -p "${out_dir}"

cd "${library_path}"
check_err_exit "${library_path}" "Failed to enter jemalloc directory"

if [[ -f "autogen.sh" ]]; then
    ./autogen.sh
    check_err_exit "${library_path}" "jemalloc autogen.sh failed"
fi

./configure \
    CFLAGS="-fPIC ${C_FLAGS}" \
    CXXFLAGS="-fPIC ${CXX_FLAGS}" \
    --prefix="${out_dir}" \
    --disable-initial-exec-tls
check_err_exit "${library_path}" "jemalloc configure failed"

make ${CMAKE_MAKE_OPTIONS:-"-j$(nproc)"}
check_err_exit "${library_path}" "jemalloc build failed"

make install
check_err_exit "${library_path}" "jemalloc install failed"

cd "${local_root}"

echo "jemalloc built successfully."

set_build_successful "${library_path}"
