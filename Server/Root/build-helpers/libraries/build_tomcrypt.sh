#!/bin/bash
# Build TomCrypt for TeaSpeak server.
# Called by Server/Root/libraries/build.sh via exec_script_external.
# Requires: tommath_path env var pointing to tommath output dir (e.g. tommath/out/linux_amd64)

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
    echo "ERROR: TomCrypt source directory '${library_path}' not found."
    exit 1
}

local_root="$(pwd)"
out_dir="${local_root}/${library_path}/out/${build_os_type:-linux}_${build_os_arch:-amd64}"

# Determine tommath paths
if [[ -n "${tommath_path}" ]]; then
    # tommath_path points to the install prefix (e.g. tommath/out/linux_amd64)
    tommath_lib="${tommath_path}/lib/libtommathStatic.a"
    # Headers: prefer installed include dir, fall back to source dir
    if [[ -d "${tommath_path}/include" ]]; then
        tommath_inc="${tommath_path}/include"
    else
        tommath_inc="${local_root}/tommath"
    fi
else
    # Default: look in sibling tommath directory
    tommath_lib="${local_root}/tommath/_cmake_build/libtommathStatic.a"
    tommath_inc="${local_root}/tommath"
fi

echo "Building TomCrypt with:"
echo "  tommath_library=${tommath_lib}"
echo "  tommath_include=${tommath_inc}"

# TomCrypt uses a custom makefile via create_build.sh
cd "${library_path}"
check_err_exit "${library_path}" "Failed to enter TomCrypt directory"

mkdir -p "out/${build_os_type:-linux}_${build_os_arch:-amd64}"

export tommath_library="${tommath_lib}"
export tommath_include="${tommath_inc}"
export CMAKE_MAKE_OPTIONS="${CMAKE_MAKE_OPTIONS:--j$(nproc)}"

if [[ -f "create_build.sh" ]]; then
    chmod +x create_build.sh
    bash create_build.sh
    check_err_exit "${library_path}" "TomCrypt build failed"
else
    # Fallback: direct make
    make -f makefile \
        CFLAGS="-fPIC ${C_FLAGS} -DUSE_LTM -DLTM_DESC -I${tommath_inc}" \
        EXTRALIBS="${tommath_lib}" \
        ${CMAKE_MAKE_OPTIONS}
    check_err_exit "${library_path}" "TomCrypt make failed"
    make -f makefile PREFIX="${out_dir}" install
    check_err_exit "${library_path}" "TomCrypt install failed"
fi

cd "${local_root}"

echo "TomCrypt built successfully."

set_build_successful "${library_path}"
