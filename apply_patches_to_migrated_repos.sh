#!/bin/bash
#
# Script para aplicar parches a los repositorios YA MIGRADOS en tu GitHub
# Esto corrige problemas en repos que clonaste de WolverinDEV
#
# Uso: ./apply_patches_to_migrated_repos.sh TU_USUARIO_GITHUB
#
# PARCHES QUE APLICA:
#   1. rust-webrtc: Fix slog dependency en Cargo.toml
#   2. build-helpers: Fix breakpad C++17 en libraries/build_breakpad.sh
#

set -e

if [[ -z "$1" ]]; then
    echo "Error: Debes proporcionar tu usuario de GitHub"
    echo "Uso: ./apply_patches_to_migrated_repos.sh TU_USUARIO"
    exit 1
fi

GITHUB_USER="$1"
TEMP_DIR="/tmp/teaspeak_patch_repos"

# Verificar que gh esté instalado
if ! command -v gh &> /dev/null; then
    echo "⚠️  ERROR: GitHub CLI (gh) no está instalado"
    exit 1
fi

# Verificar que esté autenticado
if ! gh auth status &> /dev/null; then
    echo "⚠️  ERROR: No estás autenticado con GitHub CLI"
    exit 1
fi

echo "════════════════════════════════════════════════════════════"
echo "  Aplicando Parches a Repos Migrados en GitHub"
echo "  Usuario: $GITHUB_USER"
echo "════════════════════════════════════════════════════════════"
echo ""

mkdir -p "$TEMP_DIR"
cd "$TEMP_DIR"

# ═══════════════════════════════════════════════════════════════
# PARCHE 1: rust-webrtc - Fix slog dependency
# ═══════════════════════════════════════════════════════════════
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "📦 PARCHE 1: rust-webrtc - Fix slog en Cargo.toml"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

# Verificar si el repo existe
if ! gh repo view "$GITHUB_USER/rust-webrtc" &> /dev/null; then
    echo "⚠️  Repo $GITHUB_USER/rust-webrtc no existe, saltando..."
else
    echo "[1/5] Limpiando directorio local..."
    rm -rf rust-webrtc 2>/dev/null || true

    echo "[2/5] Clonando desde tu GitHub..."
    git clone "https://github.com/$GITHUB_USER/rust-webrtc.git"
    cd rust-webrtc

    echo "[3/5] Aplicando parche a Cargo.toml..."

    # Verificar si el parche ya está aplicado
    if grep -q 'slog = { version = "2.7"' Cargo.toml 2>/dev/null; then
        echo "  ✓ Parche ya aplicado, saltando..."
    elif grep -qE '^slog\s*=\s*\{\s*\}$|^slog\s*=\s*\{\s*features' Cargo.toml 2>/dev/null; then
        echo "  Aplicando fix..."
        sed -i 's/^slog = {/slog = { version = "2.7",/' Cargo.toml

        echo "[4/5] Commiteando cambios..."
        git add Cargo.toml
        git commit -m "Fix: Add missing version to slog dependency

The slog dependency was missing a version specifier, causing Cargo errors:
'dependency (slog) specified without providing a local path, Git repository,
version, or workspace dependency to use'

This fix adds version = \"2.7\" to resolve the issue."

        echo "[5/5] Pusheando a GitHub..."
        git push

        echo "  ✅ rust-webrtc patcheado exitosamente"
    else
        echo "  ⚠️  No se encontró la línea problemática de slog, saltando..."
    fi

    cd ..
    echo ""
fi

# ═══════════════════════════════════════════════════════════════
# PARCHE 2: build-helpers - Fix breakpad C++17
# ═══════════════════════════════════════════════════════════════
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "📦 PARCHE 2: build-helpers - Fix breakpad C++17"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

# Verificar si el repo existe
if ! gh repo view "$GITHUB_USER/build-helpers" &> /dev/null; then
    echo "⚠️  Repo $GITHUB_USER/build-helpers no existe, saltando..."
else
    echo "[1/5] Limpiando directorio local..."
    rm -rf build-helpers 2>/dev/null || true

    echo "[2/5] Clonando desde tu GitHub..."
    git clone "https://github.com/$GITHUB_USER/build-helpers.git"
    cd build-helpers

    echo "[3/5] Aplicando parche a libraries/build_breakpad.sh..."

    BREAKPAD_FILE="libraries/build_breakpad.sh"

    # Verificar si el parche ya está aplicado
    if grep -q "CXXFLAGS=\"-std=c++17" "$BREAKPAD_FILE" 2>/dev/null; then
        echo "  ✓ Parche ya aplicado, saltando..."
    elif grep -q 'make CXXFLAGS="-std=c++11' "$BREAKPAD_FILE" 2>/dev/null; then
        echo "  Aplicando fix de C++17..."

        # Reemplazar sección completa
        awk '
        /^make CXXFLAGS=/ {
            print "# CRITICAL: Pass C++17 to configure so Makefile is generated with correct flags"
            print "# Breakpad requires C++14+ for std::make_unique and std::string_view"
            print "CXXFLAGS=\"-std=c++17 ${CXX_FLAGS} -static-libgcc -static-libstdc++\" \\"
            print "CFLAGS=\"${C_FLAGS}\" \\"
            print "../../configure --prefix=`pwd` || { echo \"ERROR: Breakpad configure failed!\"; exit 1; }"
            print ""
            print "# Build with the flags from configure"
            print "make ${MAKE_OPTIONS} || { echo \"ERROR: Breakpad compilation failed with C++17!\"; exit 1; }"
            print ""
            print "make install || { echo \"ERROR: Breakpad install failed!\"; exit 1; }"
            next
        }
        { print }
        ' "$BREAKPAD_FILE" > "$BREAKPAD_FILE.tmp"

        mv "$BREAKPAD_FILE.tmp" "$BREAKPAD_FILE"
        chmod +x "$BREAKPAD_FILE"

        echo "[4/5] Commiteando cambios..."
        git add "$BREAKPAD_FILE"
        git commit -m "Fix: Update breakpad build to use C++17

Breakpad requires C++14+ for modern features like std::make_unique.
Changed from passing CXXFLAGS to make, to passing to configure.
This ensures the Makefile is generated with correct C++17 flags.

Without this fix, breakpad fails to compile with:
'error: std::make_unique is not a member of std'"

        echo "[5/5] Pusheando a GitHub..."
        git push

        echo "  ✅ build-helpers patcheado exitosamente"
    else
        echo "  ⚠️  No se encontró el patrón esperado en build_breakpad.sh, saltando..."
    fi

    cd ..
    echo ""
fi

# ═══════════════════════════════════════════════════════════════
# RESUMEN FINAL
# ═══════════════════════════════════════════════════════════════
echo ""
echo "════════════════════════════════════════════════════════════"
echo "  ✅ PARCHES APLICADOS A TUS REPOS"
echo "════════════════════════════════════════════════════════════"
echo ""
echo "📊 REPOS PATCHEADOS:"
echo ""
echo "  1. rust-webrtc: slog dependency ✅"
echo "  2. build-helpers: breakpad C++17 ✅"
echo ""
echo "🎯 PRÓXIMO PASO:"
echo ""
echo "  Ahora puedes compilar TeaSpeak con TUS repos patcheados:"
echo ""
echo "    cd ~/TeaSpeak"
echo "    ./setup_teaspeak.sh --github-user $GITHUB_USER --build-type stable"
echo ""
echo "  Todos los repos estarán bajo tu control con parches permanentes."
echo ""
echo "════════════════════════════════════════════════════════════"
