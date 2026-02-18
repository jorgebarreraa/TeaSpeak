#!/bin/bash
set -e  # Exit immediately if any command fails
_build_helper_dir="$(pwd)/build-helpers/"

# Export OS type and architecture for CMake configuration (uppercase for CMake, lowercase for bash scripts)
export BUILD_OS_TYPE="linux"
export BUILD_OS_ARCH="amd64"
export build_os_type="linux"
export build_os_arch="amd64"

_build_type="Debug"
if [[ -n "$1" ]]; then
    echo "This is an automated jenkins build. Initializing variables."

    if [[ "$1" == "debug" ]]; then
        echo "Initializing debug session."
        export _build_type="Debug"
        # shellcheck disable=SC2089
        export TEASPEAK_DEPLOY_TYPE_SPECIFIER="-DBUILD_TYPE=1 -DBUILD_TYPE_NAME=beta"
        if [[ $i_really_wanna_debug -ne 1 ]]; then
            echo "Please set the i_really_wanna_debug environment variable to one if you really want a debug build"
            exit 1
        fi
    elif [[ "$1" == "nightly" ]]; then
        echo "Initializing nightly session."
        export _build_type="RelWithDebInfo"
        # shellcheck disable=SC2089
        export TEASPEAK_DEPLOY_TYPE_SPECIFIER="-DBUILD_TYPE=2 -DBUILD_TYPE_NAME=nightly"
    elif [[ "$1" == "optimized" ]]; then
        echo "Initializing optimized session."
        export _build_type="RelWithDebInfo"
        # shellcheck disable=SC2089
        export TEASPEAK_DEPLOY_TYPE_SPECIFIER="-DBUILD_TYPE=1 -DBUILD_TYPE_NAME=beta"
    elif [[ "$1" == "stable" ]]; then
        echo "Initializing stable session."
        export _build_type="RelWithDebInfo"
        # shellcheck disable=SC2089
        export TEASPEAK_DEPLOY_TYPE_SPECIFIER="-DBUILD_TYPE=0 -DBUILD_TYPE_NAME="

    else
        echo "Unknown target build type: $1"
        exit 1
    fi
fi


# Web client is enabled by default in CMakeLists.txt
# Only disable if explicitly requested via no_web environment variable
# shellcheck disable=SC2154
if [[ "$no_web" == "1" ]]; then
    echo "Disabling web support (no_web=1)"
    _web_flag="OFF"
else
    echo "Web support enabled (use no_web=1 to disable)"
    _web_flag="ON"
fi

crypto_library_path="$(pwd)/libraries/openssl-prebuild/${build_os_type}_${build_os_arch}/" \
./TeaSpeak/rtclib/generate_shared_library.sh
if [ $? -ne 0 ]; then
	exit 1
fi

# ═══════════════════════════════════════════════════════════════════════════
# Aplicar parches de compilación automáticamente ANTES de cmake
# ═══════════════════════════════════════════════════════════════════════════
echo ""
echo "Applying compilation patches..."
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
export SCRIPT_DIR

if [[ -f "$SCRIPT_DIR/apply_compilation_patches.sh" ]]; then
    bash "$SCRIPT_DIR/apply_compilation_patches.sh" || {
        echo "Failed to apply compilation patches"
        exit 1
    }
else
    echo "WARNING: apply_compilation_patches.sh not found, skipping patches"
    echo "  Location: $SCRIPT_DIR/apply_compilation_patches.sh"
fi

cd TeaSpeak || exit 1
if [[ -d build ]]; then
	rm -r build
fi
[[ ! -d build ]] && { mkdir build || exit 1; }
cd build || exit 1

echo "Build type: $_build_type"
_command="cmake .. -DCMAKE_BUILD_TYPE=\"$_build_type\" -DCMAKE_CXX_FLAGS=\"${CXX_FLAGS}\" -DBUILD_INCLUDE_FILE=\"$_build_helper_dir/cmake/config/tearoot-server.cmake\" -DCMAKE_MODULE_PATH=\"$_build_helper_dir/cmake/\" -DBUILD_OS_TYPE=\"${BUILD_OS_TYPE}\" -DBUILD_OS_ARCH=\"${BUILD_OS_ARCH}\" -DDISABLE_QT=1 -DDISABLE_CUSTOM_OUTPUT=ON -DDISABLE_QT=ON -DCOMPILE_WEB_CLIENT=\"$_web_flag\" $TEASPEAK_DEPLOY_TYPE_SPECIFIER -Dmysql_ROOT_DIR=\"/usr\""
echo "> $_command"
eval $_command
_code=$?
if [[ $_code -ne 0 ]]; then
	echo "Failed to run CMAKE project"
	exit 1
fi

# Detect number of CPU cores for parallel builds
_cpu_cores=$(nproc 2>/dev/null || echo 4)
echo "Building with $_cpu_cores CPU cores"

# Build MusicBot providers
echo "Building MusicBot FFmpeg provider..."
cmake --build "$(pwd)" --target ProviderFFMpeg -- -j $_cpu_cores; _code=$?
if [[ $_code -ne 0 ]]; then
	echo "Failed to build ProviderFFMpeg ($_code)"
	exit 1
fi

echo "Building MusicBot YouTube provider..."
cmake --build "$(pwd)" --target ProviderYT -- -j $_cpu_cores; _code=$?
if [[ $_code -ne 0 ]]; then
	echo "Failed to build ProviderYT ($_code)"
	exit 1
fi

# Build main TeaSpeak Server (includes VoiceServer, License, FileServer, MusicBot)
echo "Building TeaSpeak Server (all components)..."
cmake --build "$(pwd)" --target TeaSpeakServer -- -j $_cpu_cores; _code=$?
if [[ $_code -ne 0 ]]; then
	echo "Failed to build TeaSpeakServer ($_code)"
	exit 1
fi

# Build PermHelper and generate resources/permissions.template
# PermHelper reads ../helpers/server_groups + ../helpers/channel_groups relative to CWD,
# so it must be run from server/environment/resources/ where ../helpers/ → server/helpers/.
echo "Building PermHelper (permission template generator)..."
cmake --build "$(pwd)" --target PermHelper -- -j "$_cpu_cores"; _perm_code=$?
# Binaries land in ../server/environment/ (relative to the cmake build dir),
# not inside the build dir itself — cmake output dirs are set in CMakeLists.txt.
_perm_binary="$(pwd)/../server/environment/PermHelper"
_resources_dir="$(pwd)/../server/environment/resources"
if [[ $_perm_code -eq 0 ]] && [[ -f "$_perm_binary" ]]; then
    echo "Generating resources/permissions.template..."
    mkdir -p "$_resources_dir"
    if (cd "$_resources_dir" && "$_perm_binary" > /dev/null 2>&1); then
        echo "✓ permissions.template generated successfully"
    else
        echo "⚠ permissions.template generation failed (non-fatal)"
    fi
else
    echo "⚠ PermHelper build failed or binary not found (code: $_perm_code, path: $_perm_binary)"
fi

echo "✓ All components built successfully!"

#${CXX_FLAGS}
#${C_FLAGS}

#CMake specific
#${CMAKE_OPTIONS}
#${CMAKE_MAKE_OPTIONS}

#Make specific (without cmake)
#${MAKE_OPTIONS}


#CXXFLAGS="${CXX_FLAGS}" CFLAGS="${C_FLAGS}"
#CXXFLAGS="${CXX_FLAGS}" CFLAGS="${C_FLAGS}" ${CMAKE_OPTIONS}

