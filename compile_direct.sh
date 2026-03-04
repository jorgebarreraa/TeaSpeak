#!/bin/bash
#
# Script de Compilación Directa - Omite descarga de librerías
# Las librerías ya están en Server/Root/libraries/
#

set -e

cd "$(dirname "$0")"
SCRIPT_DIR="$(pwd)"

# Colores
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

log_step() {
    echo ""
    echo -e "${CYAN}╔═══════════════════════════════════════════════════════════╗${NC}"
    echo -e "${CYAN}║ $1${NC}"
    echo -e "${CYAN}╚═══════════════════════════════════════════════════════════╝${NC}"
}

log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[✓]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[⚠]${NC} $1"
}

log_substep() {
    echo -e "${YELLOW}▸${NC} $1"
}

echo ""
echo -e "${CYAN}╔═══════════════════════════════════════════════════════════╗${NC}"
echo -e "${CYAN}║     COMPILACIÓN TEASPEAK - LIBRERÍAS YA DESCARGADAS       ║${NC}"
echo -e "${CYAN}╚═══════════════════════════════════════════════════════════╝${NC}"
echo ""

# Variables
export build_os_type=linux
export build_os_arch=amd64
export CMAKE_MAKE_OPTIONS="-j$(nproc)"
export MAKE_OPTIONS="$CMAKE_MAKE_OPTIONS"
export build_helper_file="$SCRIPT_DIR/Server/Root/build-helpers/build_helper.sh"
export CXX_FLAGS="-fPIC"
export C_FLAGS="-fPIC"

LOG_FILE="/tmp/teaspeak_compile_$(date +%Y%m%d_%H%M%S).log"

log_info "Directorio: $SCRIPT_DIR"
log_info "CPU cores: $(nproc)"
log_info "Log file: $LOG_FILE"

# ═══════════════════════════════════════════════════════════════
# PASO 1: Compilar Librerías
# ═══════════════════════════════════════════════════════════════
log_step "PASO 1: Compilando Librerías"

cd "$SCRIPT_DIR/Server/Root/libraries"

compiled=0
failed=0
declare -a failed_libs

# Función de compilación
compile_lib() {
    local name="$1"
    local script="$2"
    local verify="$3"

    log_substep "Compilando $name..."
    if library_path="$name" bash "$script" >> "$LOG_FILE" 2>&1; then
        if [[ -f "$verify" ]]; then
            log_success "$name compilada"
            compiled=$((compiled + 1))
            return 0
        else
            log_warning "$name compiló pero archivo $verify no encontrado"
            failed=$((failed + 1))
            failed_libs+=("$name")
            return 1
        fi
    else
        log_warning "$name falló"
        failed=$((failed + 1))
        failed_libs+=("$name")
        return 1
    fi
}

# Compilar librerías en orden
compile_lib "tommath" \
    "../build-helpers/libraries/build_tommath.sh" \
    "tommath/out/linux_amd64/lib/libtommathStatic.a" || true

# TomCrypt necesita que tommath esté compilado primero
# Exportar tommath_path para que tomcrypt lo encuentre
export tommath_path="$(pwd)/tommath/out/linux_amd64"
compile_lib "tomcrypt" \
    "../build-helpers/libraries/build_tomcrypt.sh" \
    "tomcrypt/out/linux_amd64/lib/libtomcrypt.a" || true

compile_lib "Thread-Pool" \
    "../build-helpers/libraries/build_threadpool.sh" \
    "Thread-Pool/out/linux_amd64/lib/libThreadPoolStatic.a" || true

compile_lib "event" \
    "../build-helpers/libraries/build_libevent.sh" \
    "event/_build/linux_amd64/lib/libevent.a" || true

compile_lib "boringssl" \
    "../build-helpers/libraries/build_boringssl.sh" \
    "boringssl/lib/libssl.a" || true

# CXXTerminal necesita libevent
# Exportar libevent_path para que CXXTerminal lo encuentre
export libevent_path="event"
compile_lib "CXXTerminal" \
    "../build-helpers/libraries/build_cxxterminal.sh" \
    "CXXTerminal/out/linux_amd64/lib/libCXXTerminalStatic.a" || true

# DataPipes necesita BoringSSL
# Exportar boringssl_path para que DataPipes lo encuentre
export boringssl_path="$(pwd)/boringssl"
compile_lib "DataPipes" \
    "../build-helpers/libraries/build_datapipes.sh" \
    "DataPipes/out/linux_amd64/lib/libDataPipesStatic.a" || true

compile_lib "ed25519" \
    "../build-helpers/libraries/build_ed25519.sh" \
    "ed25519/out/linux_amd64/lib/libed25519Static.a" || true

compile_lib "jsoncpp" \
    "../build-helpers/libraries/build_jsoncpp.sh" \
    "jsoncpp/out/linux_amd64/lib/libjsoncpp.a" || true

compile_lib "opus" \
    "../build-helpers/libraries/build_opus.sh" \
    "opus/out/linux_amd64/lib/libopus.a" || true

compile_lib "spdlog" \
    "../build-helpers/libraries/build_spdlog.sh" \
    "spdlog/out/linux_amd64/lib/libspdlog.a" || true

compile_lib "StringVariable" \
    "../build-helpers/libraries/build_stringvariable.sh" \
    "StringVariable/out/linux_amd64/lib/libStringVariableStatic.a" || true

compile_lib "yaml-cpp" \
    "../build-helpers/libraries/build_yaml-cpp.sh" \
    "yaml-cpp/out/linux_amd64/lib/libyaml-cpp.a" || true

compile_lib "jemalloc" \
    "../build-helpers/libraries/build_jemalloc.sh" \
    "jemalloc/out/linux_amd64/lib/libjemalloc.a" || true

compile_lib "zstd" \
    "../build-helpers/libraries/build_zstd.sh" \
    "zstd/build/cmake/out/lib/libzstd.a" || true

compile_lib "breakpad" \
    "../build-helpers/libraries/build_breakpad.sh" \
    "breakpad/out/linux_amd64/lib/libbreakpad_client.a" || true

echo ""
log_info "Librerías compiladas exitosamente: $compiled/16"
if [[ $failed -gt 0 ]]; then
    log_warning "Librerías que fallaron: ${failed_libs[@]}"
    log_warning "Protobuf se omite - se usará el del sistema"
fi

# ═══════════════════════════════════════════════════════════════
# PASO 2: Aplicar Parches de Compilación
# ═══════════════════════════════════════════════════════════════
log_step "PASO 2: Aplicando Parches de Compilación"

cd "$SCRIPT_DIR"

if [[ -f "apply_compilation_patches.sh" ]]; then
    log_substep "Ejecutando apply_compilation_patches.sh..."
    bash apply_compilation_patches.sh >> "$LOG_FILE" 2>&1 || {
        log_warning "Algunos parches fallaron (puede ser normal si ya estaban aplicados)"
    }
    log_success "Parches aplicados"
else
    log_warning "No se encontró apply_compilation_patches.sh"
fi

# ═══════════════════════════════════════════════════════════════
# PASO 3: Compilar TeaSpeak Server
# ═══════════════════════════════════════════════════════════════
log_step "PASO 3: Compilando TeaSpeak Server"

cd "$SCRIPT_DIR/Server/Root"

log_info "Ejecutando build_teaspeak.sh stable..."
log_info "(Esto puede tardar 10-20 minutos)"
echo ""

if bash build_teaspeak.sh stable; then
    echo ""
    log_step "✓ COMPILACIÓN EXITOSA"
    log_success "TeaSpeak Server compilado correctamente"

    # Buscar el binario
    if [[ -f "TeaSpeak/Server/server/out/linux_amd64/TeaSpeakServer" ]]; then
        log_info "Binario ubicado en:"
        log_info "  $(pwd)/TeaSpeak/Server/server/out/linux_amd64/TeaSpeakServer"
        echo ""
        log_info "Verificando versión..."
        TeaSpeak/Server/server/out/linux_amd64/TeaSpeakServer --version || true
    fi
else
    echo ""
    log_step "✗ ERROR EN COMPILACIÓN"
    log_warning "La compilación de TeaSpeak falló"
    log_info "Revisar logs en: $LOG_FILE"
    exit 1
fi

echo ""
log_info "═══════════════════════════════════════════════════════════"
log_success "PROCESO COMPLETADO"
log_info "═══════════════════════════════════════════════════════════"
