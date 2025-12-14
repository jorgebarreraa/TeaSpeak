#!/bin/bash
set -e

# Colores para output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}   Migración de Submódulos a GitHub${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Verificar que gh está instalado
if ! command -v gh &> /dev/null; then
    echo -e "${RED}❌ GitHub CLI (gh) no está instalado${NC}"
    echo ""
    echo "Instala con:"
    echo "  sudo apt install gh"
    echo ""
    echo "O sigue la guía manual en MIGRATION_GUIDE.md"
    exit 1
fi

# Verificar autenticación
if ! gh auth status &> /dev/null; then
    echo -e "${YELLOW}⚠️  No estás autenticado en GitHub${NC}"
    echo ""
    echo "Autenticando..."
    gh auth login
fi

echo -e "${GREEN}✓ Autenticación verificada${NC}"
echo ""

# Obtener nombre de usuario
GITHUB_USER=$(gh api user -q .login)
echo -e "${BLUE}Usuario de GitHub: ${GITHUB_USER}${NC}"
echo ""

# ========================================
# Repo 1: build-helpers
# ========================================

echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${BLUE}1. Creando repo: build-helpers${NC}"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"

cd build-helpers

# Crear repo en GitHub
echo "Creando repositorio en GitHub..."
gh repo create build-helpers --public \
    --description "Fork of WolverinDEV/build-helpers with ed25519 CMake targets for TeaSpeak" \
    --source=. \
    || echo -e "${YELLOW}⚠️  Repo ya existe, continuando...${NC}"

# Cambiar remote
echo "Actualizando remote origin..."
git remote set-url origin "https://github.com/${GITHUB_USER}/build-helpers.git"

# Push
echo "Pushing commits..."
git push -u origin master

echo -e "${GREEN}✓ build-helpers subido exitosamente${NC}"
echo -e "${GREEN}  URL: https://github.com/${GITHUB_USER}/build-helpers${NC}"
echo ""

cd ..

# ========================================
# Repo 2: TeaSpeak-shared
# ========================================

echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${BLUE}2. Creando repo: TeaSpeak-shared${NC}"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"

cd TeaSpeak-shared

# Crear repo en GitHub
echo "Creando repositorio en GitHub..."
gh repo create TeaSpeak-shared --public \
    --description "Fork of TeaSpeakLibrary with GCC 13.3.0 compilation fixes and MySQL linking" \
    --source=. \
    || echo -e "${YELLOW}⚠️  Repo ya existe, continuando...${NC}"

# Cambiar remote
echo "Actualizando remote origin..."
git remote set-url origin "https://github.com/${GITHUB_USER}/TeaSpeak-shared.git"

# Push
echo "Pushing commits..."
git push -u origin 1.4.10-jorgebarreraa

# También crear rama master apuntando al mismo commit
echo "Creando rama master..."
git branch -f master 1.4.10-jorgebarreraa
git push -u origin master

echo -e "${GREEN}✓ TeaSpeak-shared subido exitosamente${NC}"
echo -e "${GREEN}  URL: https://github.com/${GITHUB_USER}/TeaSpeak-shared${NC}"
echo ""

cd ..

# ========================================
# Resumen
# ========================================

echo ""
echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}   ✓ Migración Completada${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""
echo "Repos creados:"
echo -e "  1. ${BLUE}https://github.com/${GITHUB_USER}/build-helpers${NC}"
echo -e "  2. ${BLUE}https://github.com/${GITHUB_USER}/TeaSpeak-shared${NC}"
echo ""
echo -e "${YELLOW}Próximo paso:${NC}"
echo "  Actualizar las referencias de submódulos en tu repo TeaSpeak"
echo ""
echo "  Ejecuta:"
echo -e "    ${BLUE}cd /home/user/TeaSpeak/Server${NC}"
echo -e "    ${BLUE}bash update_submodules_to_fork.sh${NC}"
echo ""
