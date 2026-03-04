#!/bin/bash
#
# Script de Parches Automáticos para TeaSpeak
# Aplica fixes necesarios a archivos que están en .gitignore
#
# Este script se ejecuta automáticamente durante la instalación
# para asegurar que todos los fixes estén aplicados correctamente.
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BREAKPAD_FILE="${SCRIPT_DIR}/Server/Root/build-helpers/libraries/build_breakpad.sh"

echo "════════════════════════════════════════════════════════════"
echo "  Aplicando parches automáticos a archivos de build"
echo "════════════════════════════════════════════════════════════"

# ═══════════════════════════════════════════════════════════════
# FIX 0: Limpiar cache corrupto de Cargo COMPLETAMENTE
# ═══════════════════════════════════════════════════════════════
echo ""
echo "[1/3] Limpiando cache corrupto de Cargo..."

# Limpiar checkouts
if [[ -d "$HOME/.cargo/git/checkouts" ]]; then
    echo "  Eliminando TODOS los checkouts de rust-webrtc..."
    find "$HOME/.cargo/git/checkouts" -type d -name "rust-webrtc-*" -exec rm -rf {} + 2>/dev/null || true
    echo "  Eliminando TODOS los checkouts de rust-libnice..."
    find "$HOME/.cargo/git/checkouts" -type d -name "rust-libnice-*" -exec rm -rf {} + 2>/dev/null || true
    echo "✓ Checkouts limpiados"
fi

# Limpiar db completo
if [[ -d "$HOME/.cargo/git/db" ]]; then
    echo "  Limpiando git db de rust-webrtc..."
    find "$HOME/.cargo/git/db" -type d -name "rust-webrtc-*" -exec rm -rf {} + 2>/dev/null || true
    find "$HOME/.cargo/git/db" -type d -name "rust-libnice-*" -exec rm -rf {} + 2>/dev/null || true
    echo "✓ Git DB limpiado"
fi

# Copiar script de fix a rtc
if [[ -f "${SCRIPT_DIR}/Server/rtc/fix_cargo_deps.sh" ]]; then
    chmod +x "${SCRIPT_DIR}/Server/rtc/fix_cargo_deps.sh"
    echo "✓ Script de fix de Cargo instalado"
fi

# ═══════════════════════════════════════════════════════════════
# FIX 1: Breakpad C++17
# ═══════════════════════════════════════════════════════════════
echo ""
echo "[2/3] Aplicando fix de C++17 a breakpad..."

if [[ ! -f "$BREAKPAD_FILE" ]]; then
    echo "⚠️  WARNING: $BREAKPAD_FILE no existe todavía"
    echo "   Se aplicará cuando se descarguen las librerías"
    exit 0
fi

# Verificar si ya está aplicado
if grep -q "CXXFLAGS=\"-std=c++17" "$BREAKPAD_FILE" 2>/dev/null; then
    echo "✓ Fix de C++17 ya está aplicado"
    exit 0
fi

# Verificar si tiene el problema
if ! grep -q 'make CXXFLAGS="-std=c++11' "$BREAKPAD_FILE" 2>/dev/null; then
    echo "✓ Archivo no tiene el problema de C++11"
    exit 0
fi

echo "  Aplicando parche de C++17..."

# Hacer backup
cp "$BREAKPAD_FILE" "$BREAKPAD_FILE.backup-$(date +%Y%m%d-%H%M%S)"

# Aplicar el parche usando un archivo temporal
cat > /tmp/build_breakpad_fixed.sh << 'ENDOFFILE'
#!/usr/bin/env bash
set -e  # Exit immediately if any command fails

[[ -z "${build_helper_file}" ]] && {
    echo "Missing build helper file. Please define \"build_helper_file\""
    exit 1
}
source ${build_helper_file}
[[ $build_helpers_defined -ne 1 ]] && {
    echo "Failed to include build helpers."
    exit 1
}


if [[ ! ${build_os_type} == "linux" ]]; then
    echo "Linux support only!"
    exit 1
fi

requires_rebuild "${library_path}"
[[ $? -eq 0 ]] && exit 0

cd ${library_path} || { echo "failed to enter library path"; exit 1; }
git clone https://chromium.googlesource.com/linux-syscall-support src/third_party/lss
cd .. || { echo "failed to exit library path"; exit 1; }

generate_build_path "${library_path}"
if [[ -d ${build_path} ]]; then
    echo "Removing old build directory"
    rm -r ${build_path}
fi
mkdir -p ${build_path}
check_err_exit ${library_path} "Failed to create build directory"
cd ${build_path}
check_err_exit ${library_path} "Failed to enter build directory"

# CRITICAL: Pass C++17 to configure so Makefile is generated with correct flags
# Breakpad requires C++14+ for std::make_unique and std::string_view
CXXFLAGS="-std=c++17 ${CXX_FLAGS} -static-libgcc -static-libstdc++" \
CFLAGS="${C_FLAGS}" \
../../configure --prefix=`pwd` || { echo "ERROR: Breakpad configure failed!"; exit 1; }

# Build with the flags from configure
make ${MAKE_OPTIONS} || { echo "ERROR: Breakpad compilation failed with C++17!"; exit 1; }

make install || { echo "ERROR: Breakpad install failed!"; exit 1; }
cd ../../../

# cmake_build ${library_path} -DCMAKE_C_FLAGS="-fPIC -I../../boringssl/include/" -DEVENT__DISABLE_TESTS=ON -DEVENT__DISABLE_OPENSSL=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo
# check_err_exit ${library_path} "Failed to build libevent!"
set_build_successful "${library_path}"
ENDOFFILE

# Copiar el archivo corregido
cp /tmp/build_breakpad_fixed.sh "$BREAKPAD_FILE"
chmod +x "$BREAKPAD_FILE"
rm /tmp/build_breakpad_fixed.sh

echo "✓ Parche de C++17 aplicado exitosamente"

# ═══════════════════════════════════════════════════════════════
# FIX 2: Fix rust-webrtc Cargo.toml before rtc build
# ═══════════════════════════════════════════════════════════════
echo ""
echo "[3/3] Preparando fix para dependencias Rust..."

RTC_FIX_SCRIPT="${SCRIPT_DIR}/Server/rtc/fix_cargo_deps.sh"
if [[ -f "$RTC_FIX_SCRIPT" ]]; then
    chmod +x "$RTC_FIX_SCRIPT"
    echo "✓ Script de fix de Cargo.toml instalado en Server/rtc/"
    echo "  Se ejecutará antes de compilar rtclib"
else
    echo "⚠️  WARNING: fix_cargo_deps.sh no encontrado"
fi

echo ""
echo "════════════════════════════════════════════════════════════"
echo "  Todos los parches aplicados correctamente"
echo "════════════════════════════════════════════════════════════"
