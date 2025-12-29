#!/bin/bash

# Script para compilar SOLO las librerías sin re-ejecutar todo el setup
# Usa las librerías ya descargadas de jorgebarreraa

set +e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LOG_FILE="/tmp/compile_libraries_$(date +%Y%m%d_%H%M%S).log"

# Colores
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo "╔═══════════════════════════════════════════════════════════╗"
echo "║ Compilación de Librerías TeaSpeak (Solo Librerías)       ║"
echo "╚═══════════════════════════════════════════════════════════╝"
echo ""
echo "📁 Directorio: $SCRIPT_DIR/Server/Root/libraries"
echo "📝 Log: $LOG_FILE"
echo ""

cd "$SCRIPT_DIR/Server/Root/libraries"

# Limpiar procesos zombies
echo -e "${YELLOW}▸ Limpiando procesos de compilación previos...${NC}"
pkill -9 -f "build_breakpad.sh" 2>/dev/null || true
pkill -9 -f "stackwalker" 2>/dev/null || true

# Limpiar archivos de estado
echo -e "${YELLOW}▸ Limpiando archivos de estado de compilaciones antiguas...${NC}"
find . -maxdepth 2 -name ".build_linux_amd64.txt" -delete 2>/dev/null || true

# Exportar variables
export build_os_type=linux
export build_os_arch=amd64
export CXX_FLAGS="-fPIC"
export C_FLAGS="-fPIC"
export CMAKE_BUILD_TYPE="Release"
export CMAKE_MAKE_OPTIONS="-j$(nproc)"
export build_helper_file="../build-helpers/build_helper.sh"

echo ""
echo -e "${BLUE}[INFO] Compilando con $(nproc) núcleos...${NC}"
echo -e "${YELLOW}[⚠] Esto puede tardar 10-20 minutos...${NC}"
echo ""

successful=0
failed=0
declare -a failed_libs

# Función para compilar y verificar
compile_lib() {
    local lib_name="$1"
    local build_script="$2"
    local verify_file="$3"

    echo -e "${YELLOW}▸ Compilando $lib_name...${NC}"

    if library_path="$lib_name" $build_script >> "$LOG_FILE" 2>&1; then
        if [[ -f "$verify_file" ]]; then
            echo -e "${GREEN}[✓] $lib_name compilada${NC}"
            ((successful++))
            return 0
        else
            echo -e "${RED}[⚠] $lib_name compilación reportó éxito pero archivos no encontrados${NC}"
            ((failed++))
            failed_libs+=("$lib_name")
            return 1
        fi
    else
        echo -e "${RED}[⚠] $lib_name falló${NC}"
        ((failed++))
        failed_libs+=("$lib_name")
        return 1
    fi
}

# 1. TomMath
compile_lib "tommath" \
    "../build-helpers/libraries/build_tommath.sh" \
    "tommath/out/linux_amd64/lib/libtommathStatic.a"

# 2. TomCrypt
compile_lib "tomcrypt" \
    "../build-helpers/libraries/build_tomcrypt.sh" \
    "tomcrypt/out/linux_amd64/lib/libtomcrypt.a"

# 3. Thread-Pool
compile_lib "Thread-Pool" \
    "../build-helpers/libraries/build_threadpool.sh" \
    "Thread-Pool/out/linux_amd64/lib/libThreadPoolStatic.a"

# 4. libevent
compile_lib "event" \
    "../build-helpers/libraries/build_libevent.sh" \
    "event/out/linux_amd64/lib/libevent.a"

# 5. CXXTerminal
compile_lib "CXXTerminal" \
    "../build-helpers/libraries/build_cxxterminal.sh" \
    "CXXTerminal/out/linux_amd64/include/Terminal.h"

# 6. DataPipes (CON FIX DE STDEXCEPT)
compile_lib "DataPipes" \
    "../build-helpers/libraries/build_datapipes.sh" \
    "DataPipes/out/linux_amd64/include/pipes/buffer.h"

# 7. ed25519
compile_lib "ed25519" \
    "../build-helpers/libraries/build_ed25519.sh" \
    "ed25519/out/linux_amd64/lib/libed25519.a"

# 8. jsoncpp
compile_lib "jsoncpp" \
    "../build-helpers/libraries/build_jsoncpp.sh" \
    "jsoncpp/out/linux_amd64/lib/libjsoncpp.a"

# 9. opus
compile_lib "opus" \
    "../build-helpers/libraries/build_opus.sh" \
    "opus/out/linux_amd64/lib/libopus.a"

# 10. protobuf
compile_lib "protobuf" \
    "../build-helpers/libraries/build_protobuf.sh" \
    "protobuf/out/linux_amd64/lib/libprotobuf-lite.a"

# 11. spdlog
compile_lib "spdlog" \
    "../build-helpers/libraries/build_spdlog.sh" \
    "spdlog/out/linux_amd64/include/spdlog/spdlog.h"

# 12. StringVariable
compile_lib "StringVariable" \
    "../build-helpers/libraries/build_stringvariable.sh" \
    "StringVariable/out/linux_amd64/include/StringVariable.h"

# 13. yaml-cpp
compile_lib "yaml-cpp" \
    "../build-helpers/libraries/build_yaml.sh" \
    "yaml-cpp/out/linux_amd64/lib/libyaml-cpp.a"

# 14. jemalloc
compile_lib "jemalloc" \
    "../build-helpers/libraries/build_jemalloc.sh" \
    "jemalloc/out/linux_amd64/lib/libjemalloc.a"

# 15. zstd
compile_lib "zstd" \
    "../build-helpers/libraries/build_zstd.sh" \
    "zstd/out/linux_amd64/lib/libzstd.a"

# 16. breakpad (puede tardar mucho o colgar)
echo -e "${YELLOW}▸ Compilando breakpad (puede tardar)...${NC}"
if timeout 600 bash -c 'library_path="breakpad" ../build-helpers/libraries/build_breakpad.sh' >> "$LOG_FILE" 2>&1; then
    if [[ -f "breakpad/out/linux_amd64/lib/libbreakpad.a" ]]; then
        echo -e "${GREEN}[✓] breakpad compilada${NC}"
        ((successful++))
    else
        echo -e "${RED}[⚠] breakpad compilación reportó éxito pero archivos no encontrados${NC}"
        ((failed++))
        failed_libs+=("breakpad")
    fi
else
    echo -e "${RED}[⚠] breakpad falló o excedió timeout (10 min)${NC}"
    ((failed++))
    failed_libs+=("breakpad")
fi

echo ""
echo "═══════════════════════════════════════════════════════════"
if [[ $successful -eq 16 ]]; then
    echo -e "${GREEN}[✓] ÉXITO: Todas las 16 librerías compiladas correctamente${NC}"
elif [[ $successful -ge 12 ]]; then
    echo -e "${YELLOW}[⚠] Librerías compiladas: $successful/16${NC}"
    echo -e "${RED}[✗] Librerías que FALLARON ($failed): ${failed_libs[@]}${NC}"
    echo -e "${BLUE}[INFO] Las librerías críticas están compiladas (suficiente para continuar)${NC}"
else
    echo -e "${RED}[✗] ERROR: Solo $successful/16 librerías compiladas${NC}"
    echo -e "${RED}[✗] Librerías que FALLARON ($failed): ${failed_libs[@]}${NC}"
fi
echo -e "${BLUE}[✗] Ver detalles completos en: $LOG_FILE${NC}"
echo "═══════════════════════════════════════════════════════════"
echo ""

if [[ $successful -ge 12 ]]; then
    echo -e "${GREEN}✅ Compilación completada con éxito suficiente${NC}"
    echo ""
    echo "🎯 Siguiente paso:"
    echo "   cd $SCRIPT_DIR"
    echo "   ./setup_teaspeak.sh --github-user jorgebarreraa --build-type stable"
    echo "   (Continuar desde PASO 10: Compilación del Servidor)"
    exit 0
else
    echo -e "${RED}❌ Demasiadas librerías fallaron${NC}"
    echo "   Revisa el log: $LOG_FILE"
    exit 1
fi
