#!/bin/bash
#
# Script Simplificado para Compilar TeaSpeak
# Este script descarga librerías faltantes y compila todo
#

set -e

cd "$(dirname "$0")"
SCRIPT_DIR="$(pwd)"

echo "═══════════════════════════════════════════════════════════"
echo "  Compilación Directa de TeaSpeak Server"
echo "═══════════════════════════════════════════════════════════"
echo ""
echo "📁 Directorio base: $SCRIPT_DIR"
echo ""

# Variables
export build_os_type=linux
export build_os_arch=amd64
export CMAKE_MAKE_OPTIONS="-j$(nproc)"
export MAKE_OPTIONS="$CMAKE_MAKE_OPTIONS"

# ═══════════════════════════════════════════════════════════════
# PASO 1: Verificar Estructura
# ═══════════════════════════════════════════════════════════════
echo "[1/4] Verificando estructura de directorios..."

cd "$SCRIPT_DIR/Server/Root"

if [[ ! -d "TeaSpeak" ]]; then
    echo "  ERROR: Falta symlink TeaSpeak -> ../Server"
    exit 1
fi

if [[ ! -d "build-helpers" ]]; then
    echo "  ERROR: Falta directorio build-helpers"
    exit 1
fi

echo "  ✓ Estructura correcta"

# ═══════════════════════════════════════════════════════════════
# PASO 2: Descargar Librerías Faltantes
# ═══════════════════════════════════════════════════════════════
echo ""
echo "[2/4] Descargando librerías faltantes..."

cd "$SCRIPT_DIR/Server/Root/libraries"

# Función para clonar si no existe
clone_if_missing() {
    local url="$1"
    local dir="$2"
    local branch="$3"

    if [[ -d "$dir" ]]; then
        echo "  ✓ $dir ya existe"
        return 0
    fi

    echo "  Clonando $dir..."
    if [[ -n "$branch" ]]; then
        git clone --depth 1 --branch "$branch" "$url" "$dir" 2>/dev/null || {
            echo "  ⚠ Fallo, intentando sin branch..."
            git clone --depth 1 "$url" "$dir"
        }
    else
        git clone --depth 1 "$url" "$dir"
    fi
}

# Librerías principales
clone_if_missing "https://github.com/jorgebarreraa/jsoncpp.git" "jsoncpp"
clone_if_missing "https://github.com/jorgebarreraa/Thread-Pool.git" "Thread-Pool"
clone_if_missing "https://github.com/jorgebarreraa/tommath.git" "tommath" "develop"
clone_if_missing "https://github.com/jorgebarreraa/tomcrypt.git" "tomcrypt" "master"
clone_if_missing "https://github.com/jorgebarreraa/CXXTerminal.git" "CXXTerminal"
clone_if_missing "https://github.com/jorgebarreraa/DataPipes.git" "DataPipes"
clone_if_missing "https://github.com/jorgebarreraa/ed25519.git" "ed25519"
clone_if_missing "https://github.com/libevent/libevent.git" "event" "release-2.1.8-stable"
clone_if_missing "https://github.com/xiph/opus.git" "opus"
clone_if_missing "https://github.com/gabime/spdlog.git" "spdlog" "v1.x"
clone_if_missing "https://github.com/WolverinDEV/StringVariable.git" "StringVariable"
clone_if_missing "https://github.com/jbeder/yaml-cpp.git" "yaml-cpp"
clone_if_missing "https://github.com/jemalloc/jemalloc.git" "jemalloc" "dev"
clone_if_missing "https://github.com/facebook/zstd.git" "zstd"
clone_if_missing "https://github.com/google/boringssl.git" "boringssl"
clone_if_missing "https://chromium.googlesource.com/breakpad/breakpad" "breakpad"

# Crear symlinks si no existen
[[ ! -e "spdlog-master" ]] && ln -s spdlog spdlog-master
[[ ! -e "tommath-develop" ]] && ln -s tommath tommath-develop
[[ ! -e "tomcrypt-master" ]] && ln -s tomcrypt tomcrypt-master

echo "  ✓ Todas las librerías descargadas"

# ═══════════════════════════════════════════════════════════════
# PASO 3: Compilar Librerías
# ═══════════════════════════════════════════════════════════════
echo ""
echo "[3/4] Compilando librerías..."
echo "  (Esto puede tardar 15-20 minutos con $(nproc) núcleos)"
echo ""

export build_helper_file="../build-helpers/build_helper.sh"
export CXX_FLAGS="-fPIC"
export C_FLAGS="-fPIC"

compiled=0
failed=0

compile_lib() {
    local name="$1"
    local script="$2"
    echo -n "  Compilando $name... "
    if library_path="$name" "$script" >/dev/null 2>&1; then
        echo "✓"
        ((compiled++))
    else
        echo "✗"
        ((failed++))
    fi
}

compile_lib "tommath" "../build-helpers/libraries/build_tommath.sh"
compile_lib "tomcrypt" "../build-helpers/libraries/build_tomcrypt.sh"
compile_lib "Thread-Pool" "../build-helpers/libraries/build_threadpool.sh"
compile_lib "event" "../build-helpers/libraries/build_libevent.sh"
compile_lib "boringssl" "../build-helpers/libraries/build_boringssl.sh"
compile_lib "CXXTerminal" "../build-helpers/libraries/build_cxxterminal.sh"
compile_lib "DataPipes" "../build-helpers/libraries/build_datapipes.sh"
compile_lib "ed25519" "../build-helpers/libraries/build_ed25519.sh"
compile_lib "jsoncpp" "../build-helpers/libraries/build_jsoncpp.sh"
compile_lib "opus" "../build-helpers/libraries/build_opus.sh"
compile_lib "spdlog" "../build-helpers/libraries/build_spdlog.sh"
compile_lib "StringVariable" "../build-helpers/libraries/build_stringvariable.sh"
compile_lib "yaml-cpp" "../build-helpers/libraries/build_yaml-cpp.sh"
compile_lib "jemalloc" "../build-helpers/libraries/build_jemalloc.sh"
compile_lib "zstd" "../build-helpers/libraries/build_zstd.sh"
compile_lib "breakpad" "../build-helpers/libraries/build_breakpad.sh"

echo ""
echo "  Librerías compiladas: $compiled/16"
[[ $failed -gt 0 ]] && echo "  ⚠ Fallaron: $failed (protobuf se omite - usa sistema)"

# ═══════════════════════════════════════════════════════════════
# PASO 4: Compilar TeaSpeak Server
# ═══════════════════════════════════════════════════════════════
echo ""
echo "[4/4] Compilando TeaSpeak Server..."
echo ""

cd "$SCRIPT_DIR/Server/Root"

# Aplicar parches si no están aplicados
if [[ -f "$SCRIPT_DIR/apply_compilation_patches.sh" ]]; then
    echo "  Aplicando parches de compilación..."
    bash "$SCRIPT_DIR/apply_compilation_patches.sh" >/dev/null 2>&1 || true
fi

# Compilar
echo "  Ejecutando build_teaspeak.sh stable..."
echo ""
bash build_teaspeak.sh stable

echo ""
echo "═══════════════════════════════════════════════════════════"
echo "  ✓ Compilación completada"
echo "═══════════════════════════════════════════════════════════"
