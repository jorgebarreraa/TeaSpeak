#!/bin/bash
set -e

# ==============================================================================
# Script de Actualización de Referencias de Submódulos
# ==============================================================================
#
# Este script actualiza TODAS las referencias de submódulos en .gitmodules
# para que apunten a los repositorios en tu cuenta de GitHub.
#
# Ejecutar DESPUÉS de migrate_all_repos_to_github.sh
#
# ==============================================================================

# Colores
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${BLUE}════════════════════════════════════════════════════════════${NC}"
echo -e "${BLUE}   Actualizar Referencias de Submódulos${NC}"
echo -e "${BLUE}════════════════════════════════════════════════════════════${NC}"
echo ""

GITHUB_USER="jorgebarreraa"
PROJECT_ROOT="/home/user/TeaSpeak/Server/Root"

cd "$PROJECT_ROOT"

# Backup del .gitmodules original
echo "Creando backup de .gitmodules..."
cp .gitmodules .gitmodules.backup
echo -e "${GREEN}✓ Backup creado: .gitmodules.backup${NC}"
echo ""

# ==============================================================================
# Mapeo de URLs antiguas a nuevas
# ==============================================================================

declare -A URL_MAPPING=(
    # GitHub repos
    ["https://github.com/open-source-parsers/jsoncpp.git"]="https://github.com/${GITHUB_USER}/jsoncpp.git"
    ["https://github.com/WolverinDEV/CXXTerminal.git"]="https://github.com/${GITHUB_USER}/CXXTerminal.git"
    ["https://github.com/xiph/opus"]="https://github.com/${GITHUB_USER}/opus.git"
    ["https://github.com/xiph/opusfile.git"]="https://github.com/${GITHUB_USER}/opusfile.git"
    ["https://github.com/jbeder/yaml-cpp.git"]="https://github.com/${GITHUB_USER}/yaml-cpp.git"
    ["https://github.com/libevent/libevent.git"]="https://github.com/${GITHUB_USER}/libevent.git"
    ["https://github.com/WolverinDEV/StringVariable.git"]="https://github.com/${GITHUB_USER}/StringVariable.git"
    ["https://github.com/WolverinDEV/ed25519.git"]="https://github.com/${GITHUB_USER}/ed25519.git"
    ["https://github.com/google/protobuf.git"]="https://github.com/${GITHUB_USER}/protobuf.git"
    ["https://github.com/WolverinDEV/DataPipes.git"]="https://github.com/${GITHUB_USER}/DataPipes.git"
    ["https://github.com/jemalloc/jemalloc.git"]="https://github.com/${GITHUB_USER}/jemalloc.git"
    ["https://github.com/facebook/zstd.git"]="https://github.com/${GITHUB_USER}/zstd.git"
    ["https://github.com/WolverinDEV/build-helpers.git"]="https://github.com/${GITHUB_USER}/build-helpers.git"

    # git.did.science repos
    ["https://git.did.science/WolverinDEV/ThreadPool.git"]="https://github.com/${GITHUB_USER}/Thread-Pool.git"
    ["https://git.did.science/TeaSpeak/libraries/tomcrypt.git"]="https://github.com/${GITHUB_USER}/tomcrypt.git"
    ["https://git.did.science/TeaSpeak/libraries/tommath.git"]="https://github.com/${GITHUB_USER}/tommath.git"
    ["https://git.did.science/TeaSpeak/Server/Server"]="https://github.com/${GITHUB_USER}/TeaSpeak-Server.git"
    ["https://git.did.science/TeaSpeak/libraries/spdlog.git"]="https://github.com/${GITHUB_USER}/spdlog.git"
    ["https://git.did.science/TeaSpeak/libraries/libnice-prebuild.git"]="https://github.com/${GITHUB_USER}/libnice-prebuild.git"
    ["https://git.did.science/TeaSpeak/libraries/glib2.0.git"]="https://github.com/${GITHUB_USER}/glib2.0.git"
    ["https://git.did.science/TeaSpeak/libraries/openssl-prebuild.git"]="https://github.com/${GITHUB_USER}/openssl-prebuild.git"
    ["https://git.did.science/TeaSpeak/WebDNS.git"]="https://github.com/${GITHUB_USER}/TeaDNS.git"

    # Google repos
    ["https://chromium.googlesource.com/breakpad/breakpad"]="https://github.com/${GITHUB_USER}/breakpad.git"
    ["https://boringssl.googlesource.com/boringssl"]="https://github.com/${GITHUB_USER}/boringssl.git"
)

# ==============================================================================
# Actualizar URLs
# ==============================================================================

echo "Actualizando URLs en .gitmodules..."
echo ""

COUNT=0
for old_url in "${!URL_MAPPING[@]}"; do
    new_url="${URL_MAPPING[$old_url]}"

    if grep -q "$old_url" .gitmodules; then
        echo "  $old_url"
        echo "    → ${new_url}"

        # Reemplazar en .gitmodules
        sed -i "s|${old_url}|${new_url}|g" .gitmodules

        COUNT=$((COUNT + 1))
    fi
done

echo ""
echo -e "${GREEN}✓ ${COUNT} URLs actualizadas${NC}"
echo ""

# ==============================================================================
# Sincronizar submódulos
# ==============================================================================

echo "Sincronizando submódulos..."
git submodule sync --recursive

echo -e "${GREEN}✓ Submódulos sincronizados${NC}"
echo ""

# ==============================================================================
# Mostrar cambios
# ==============================================================================

echo -e "${BLUE}Cambios en .gitmodules:${NC}"
git diff .gitmodules | head -50
echo ""

# ==============================================================================
# Confirmar cambios
# ==============================================================================

echo -e "${YELLOW}¿Deseas commitear estos cambios? (y/n):${NC}"
read -n 1 -r
echo

if [[ $REPLY =~ ^[Yy]$ ]]; then
    git add .gitmodules

    git commit -m "Migrate all submodules to personal GitHub account

All external dependencies now point to https://github.com/${GITHUB_USER}/
This gives full control over all project dependencies.

Updated repositories:
- All GitHub repos (WolverinDEV, Google, etc.)
- All git.did.science repos
- All Google source repos (chromium, boringssl)

Total: ${COUNT} submodule URLs updated"

    echo -e "${GREEN}✓ Cambios commiteados${NC}"
    echo ""

    echo -e "${YELLOW}¿Deseas hacer push? (y/n):${NC}"
    read -n 1 -r
    echo

    if [[ $REPLY =~ ^[Yy]$ ]]; then
        CURRENT_BRANCH=$(git branch --show-current)
        git push origin "$CURRENT_BRANCH"
        echo -e "${GREEN}✓ Push completado${NC}"
    else
        echo -e "${YELLOW}⚠️  Push omitido${NC}"
        echo "  Puedes hacer push manualmente con:"
        echo "    git push origin $(git branch --show-current)"
    fi
else
    echo -e "${YELLOW}⚠️  Cambios no commiteados${NC}"
    echo "  Puedes revisar los cambios con:"
    echo "    git diff .gitmodules"
    echo ""
    echo "  Para revertir:"
    echo "    cp .gitmodules.backup .gitmodules"
fi

echo ""
echo -e "${GREEN}════════════════════════════════════════════════════════════${NC}"
echo -e "${GREEN}   Actualización Completada${NC}"
echo -e "${GREEN}════════════════════════════════════════════════════════════${NC}"
echo ""
echo "Siguiente paso:"
echo "  Actualizar los submódulos para usar las nuevas URLs:"
echo -e "  ${GREEN}cd /home/user/TeaSpeak/Server/Root${NC}"
echo -e "  ${GREEN}git submodule update --init --recursive --remote${NC}"
echo ""
