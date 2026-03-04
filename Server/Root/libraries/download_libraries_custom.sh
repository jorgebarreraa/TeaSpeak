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

    # Verificación especial para build-helpers
    if [[ "$dir" == "build-helpers" ]]; then
        # Check if library scripts are present (not just build_helper.sh which is now in git)
        if [[ -d "$dir" && ! -f "$dir/libraries/build_tommath.sh" ]]; then
            echo "  ⚠ $dir/libraries/ está incompleto (faltan scripts de compilación), eliminando para re-clonar..."
            rm -rf "$dir"
        fi
    fi

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
cd ..
clone_with_fallback "https://github.com/jorgebarreraa/build-helpers.git" "build-helpers"

# Asegurar que build-helpers tenga todos los archivos (puede haber sido parcialmente clonado antes)
if [[ -d "build-helpers/.git" ]]; then
    echo "  Verificando integridad de build-helpers..."
    cd build-helpers
    git reset --hard HEAD >/dev/null 2>&1
    cd ..
fi

# Sobrescribir build_jsoncpp.sh de build-helpers con versión C++17
# NOTE: This file no longer exists in libraries/, build-helpers has the correct version
# echo "Sobrescribiendo build_jsoncpp.sh con versión C++17..."
# cp -f libraries/build_jsoncpp.sh build-helpers/libraries/build_jsoncpp.sh

echo "✓ Todas las librerías descargadas exitosamente!"
