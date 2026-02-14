#!/bin/bash
# Build Breakpad for TeaSpeak server.
# Breakpad uses autotools (configure + make).

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
    echo "ERROR: breakpad source directory '${library_path}' not found."
    exit 1
}

local_root="$(pwd)"
out_dir="${local_root}/${library_path}/out/${build_os_type:-linux}_${build_os_arch:-amd64}"

# Clone LSS (linux-syscall-support) if not present
if [[ ! -d "${library_path}/src/third_party/lss" ]]; then
    echo "Cloning linux-syscall-support into breakpad..."
    git clone https://chromium.googlesource.com/linux-syscall-support \
        "${library_path}/src/third_party/lss" --depth 1
    check_err_exit "${library_path}" "Failed to clone linux-syscall-support"
fi

mkdir -p "${out_dir}"

cd "${library_path}"
check_err_exit "${library_path}" "Failed to enter breakpad directory"

if [[ ! -f "configure" ]]; then
    ./autogen.sh 2>/dev/null || true
fi

mkdir -p build
cd build

../configure \
    CXXFLAGS="-std=c++11 -fPIC ${CXX_FLAGS}" \
    CFLAGS="${C_FLAGS} -fPIC" \
    --prefix="${out_dir}"
check_err_exit "${library_path}" "breakpad configure failed"

make ${CMAKE_MAKE_OPTIONS:-"-j$(nproc)"}
check_err_exit "${library_path}" "breakpad build failed"

make install
check_err_exit "${library_path}" "breakpad install failed"

cd "${local_root}"

echo "breakpad built successfully."

set_build_successful "${library_path}"
