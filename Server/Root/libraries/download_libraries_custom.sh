#!/bin/bash
set -e

GITHUB_USER="jorgebarreraa"

echo "Descargando librerías desde GitHub de: $GITHUB_USER"
cd "$(dirname "$0")"

# Función para clonar o saltar si existe
clone_lib() {
    local url="$1"
    local dir="$2"
    local branch="$3"

    if [ -d "$dir" ]; then
        echo "  ✓ $dir ya existe, omitiendo"
        return 0
    fi

    echo "  Clonando $dir..."
    if [ -n "$branch" ]; then
        git clone "$url" "$dir" --branch "$branch" --depth 1 || {
            echo "  ⚠ Error clonando $url, intentando repo original..."
            return 1
        }
    else
        git clone "$url" "$dir" --depth 1 || {
            echo "  ⚠ Error clonando $url, intentando repo original..."
            return 1
        }
    fi
}

# Intentar clonar desde GitHub del usuario, si falla usar original
clone_with_fallback() {
    local original_url="$1"
    local dir="$2"
    local branch="$3"

    # Extraer nombre del repo
    local repo_name=$(basename "$original_url" .git)

    # Intentar desde GitHub del usuario primero
    if [[ -n "$GITHUB_USER" ]]; then
        local user_url="https://github.com/$GITHUB_USER/${repo_name}.git"
        if clone_lib "$user_url" "$dir" "$branch"; then
            return 0
        fi
        echo "  → Intentando desde repositorio original..."
    fi

    # Si falla o no hay usuario, usar original
    clone_lib "$original_url" "$dir" "$branch"
}

# Librerías principales
clone_with_fallback "https://github.com/open-source-parsers/jsoncpp.git" "jsoncpp"
clone_with_fallback "https://github.com/jorgebarreraa/Thread-Pool.git" "Thread-Pool"
clone_with_fallback "https://github.com/jorgebarreraa/tomcrypt.git" "tomcrypt"
clone_with_fallback "https://github.com/jorgebarreraa/tommath.git" "tommath"
clone_with_fallback "https://github.com/jorgebarreraa/CXXTerminal.git" "CXXTerminal"
clone_with_fallback "https://github.com/xiph/opus" "opus"
clone_with_fallback "https://github.com/xiph/opusfile.git" "opusfile"
clone_with_fallback "https://github.com/jbeder/yaml-cpp.git" "yaml-cpp"
clone_with_fallback "https://github.com/libevent/libevent.git" "event"

# Patch libevent para compatibilidad con CMake 3.16
if [ -f "event/cmake/AddLinkerFlags.cmake" ]; then
    echo "  Parcheando libevent para CMake 3.16..."
    cat > "event/cmake/AddLinkerFlags.cmake" << 'EOFPATCH'
if (NOT CMAKE_VERSION VERSION_LESS 3.18)
	include(CheckLinkerFlag)
endif()

macro(add_linker_flags)
	foreach(flag ${ARGN})
		string(REGEX REPLACE "[-.+/:= ]" "_" _flag_esc "${flag}")

if (NOT CMAKE_VERSION VERSION_LESS 3.18)
		check_linker_flag(C "${flag}" check_c_linker_flag_${_flag_esc})
endif()

		if (check_c_linker_flag_${_flag_esc})
			set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${flag}")
			set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${flag}")
		endif()
	endforeach()
endmacro()
EOFPATCH
fi

clone_with_fallback "https://github.com/jorgebarreraa/spdlog.git" "spdlog"
clone_with_fallback "https://github.com/jorgebarreraa/StringVariable.git" "StringVariable"
clone_with_fallback "https://github.com/jorgebarreraa/ed25519.git" "ed25519"
clone_with_fallback "https://chromium.googlesource.com/breakpad/breakpad" "breakpad"

# Checkout específico para breakpad (compatible con C++17)
if [ -d "breakpad" ]; then
    echo "  Configurando breakpad..."
    (cd breakpad && git fetch --unshallow 2>/dev/null || true && git checkout f032e4c3 2>/dev/null || true)
fi

clone_with_fallback "https://boringssl.googlesource.com/boringssl" "boringssl"
clone_with_fallback "https://github.com/protocolbuffers/protobuf.git" "protobuf" "v21.12"
clone_with_fallback "https://github.com/jorgebarreraa/DataPipes.git" "DataPipes"
clone_with_fallback "https://github.com/jemalloc/jemalloc.git" "jemalloc" "dev"
clone_with_fallback "https://github.com/jorgebarreraa/libnice-prebuild.git" "libnice"
clone_with_fallback "https://github.com/jorgebarreraa/glibc.git" "glibc"
clone_with_fallback "https://github.com/jorgebarreraa/openssl-prebuild.git" "openssl-prebuild"
clone_with_fallback "https://github.com/facebook/zstd.git" "zstd"

# build-helpers
# NOTE: build-helpers/ already exists in the repo (with cmake/ subdirectory).
# We need to populate build-helpers/libraries/ which contains the library build scripts.
# Since the directory exists, a plain git clone would be skipped. We handle this specially:
cd ..
if [ ! -d "build-helpers/libraries" ]; then
    echo "  Populating build-helpers/libraries/ from jorgebarreraa/build-helpers..."
    if [ -d "build-helpers/.git" ]; then
        # Already a git repo, pull the libraries/ subdirectory
        (cd build-helpers && git pull origin main 2>/dev/null || git pull origin master 2>/dev/null || true)
    else
        # build-helpers/ is from the main repo (not a git clone), clone into a temp location
        # and copy the libraries/ directory
        _tmp_bh="/tmp/_build_helpers_tmp_$$"
        git clone --depth 1 "https://github.com/jorgebarreraa/build-helpers.git" "$_tmp_bh" 2>/dev/null && {
            cp -r "$_tmp_bh/." "build-helpers/"
            rm -rf "$_tmp_bh"
            echo "  ✓ build-helpers/libraries/ populated"
        } || {
            echo "  ⚠ Could not clone jorgebarreraa/build-helpers (will use built-in scripts)"
        }
    fi
else
    echo "  ✓ build-helpers/libraries/ already exists, skipping"
fi

# Also ensure our local build_helper.sh is available (in case clone failed)
if [ ! -f "build-helpers/build_helper.sh" ]; then
    echo "  ⚠ build-helpers/build_helper.sh missing - using built-in fallback"
fi

echo "✓ Todas las librerías descargadas exitosamente!"
