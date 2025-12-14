#!/bin/bash
set -e

# Colores para output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}   Actualizar Submódulos a Fork${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Obtener nombre de usuario de GitHub
if command -v gh &> /dev/null && gh auth status &> /dev/null; then
    GITHUB_USER=$(gh api user -q .login)
    echo -e "${GREEN}✓ Usuario de GitHub detectado: ${GITHUB_USER}${NC}"
else
    echo -e "${YELLOW}Ingresa tu nombre de usuario de GitHub:${NC}"
    read -p "> " GITHUB_USER
fi

echo ""
echo -e "${BLUE}Actualizando referencias de submódulos...${NC}"
echo ""

# ========================================
# Actualizar build-helpers
# ========================================

echo -e "${BLUE}1. Actualizando build-helpers${NC}"

cd Root

# Verificar que el submódulo existe
if [ ! -d "build-helpers" ]; then
    echo -e "${RED}❌ Directorio build-helpers no encontrado${NC}"
    exit 1
fi

# Cambiar URL del submódulo
echo "  Cambiando URL a: https://github.com/${GITHUB_USER}/build-helpers.git"
git config -f .gitmodules submodule.build-helpers.url "https://github.com/${GITHUB_USER}/build-helpers.git"
git config -f .gitmodules submodule.build-helpers.branch master

# Sincronizar y actualizar
git submodule sync build-helpers
cd build-helpers
git remote set-url origin "https://github.com/${GITHUB_USER}/build-helpers.git"
git fetch origin
git checkout master
git pull origin master
cd ..

echo -e "${GREEN}  ✓ build-helpers actualizado${NC}"
echo ""

# ========================================
# Actualizar shared (TeaSpeak-shared)
# ========================================

echo -e "${BLUE}2. Actualizando TeaSpeak/shared (TeaSpeak-shared)${NC}"

cd ..

# Este submódulo está en Server/Server/shared
if [ ! -d "Server/shared" ]; then
    echo -e "${YELLOW}  ⚠️  Submódulo shared no encontrado como submódulo git${NC}"
    echo -e "${YELLOW}  Parece ser un directorio directo, actualizando configuración...${NC}"

    # Opción 1: Convertir a submódulo
    if [ -d "Server/shared/.git" ]; then
        cd Server
        echo "  Configurando como submódulo..."

        # Guardar cambios actuales si los hay
        cd shared
        SHARED_COMMIT=$(git rev-parse HEAD)
        cd ..

        # Remover directorio actual
        rm -rf shared

        # Agregar como submódulo
        git submodule add -f "https://github.com/${GITHUB_USER}/TeaSpeak-shared.git" shared

        cd shared
        git checkout 1.4.10-jorgebarreraa
        cd ../..

        echo -e "${GREEN}  ✓ TeaSpeak-shared agregado como submódulo${NC}"
    else
        echo -e "${RED}  ❌ shared no es un repositorio git${NC}"
        echo -e "${YELLOW}  Necesitas ejecutar los pasos manuales en MIGRATION_GUIDE.md${NC}"
    fi
else
    # Ya es un submódulo, solo actualizar URL
    cd Server
    echo "  Cambiando URL a: https://github.com/${GITHUB_USER}/TeaSpeak-shared.git"
    git config -f ../.gitmodules submodule.Server/shared.url "https://github.com/${GITHUB_USER}/TeaSpeak-shared.git"
    git config -f ../.gitmodules submodule.Server/shared.branch 1.4.10-jorgebarreraa

    cd shared
    git remote set-url origin "https://github.com/${GITHUB_USER}/TeaSpeak-shared.git"
    git fetch origin
    git checkout 1.4.10-jorgebarreraa
    git pull origin 1.4.10-jorgebarreraa
    cd ../..

    echo -e "${GREEN}  ✓ TeaSpeak-shared actualizado${NC}"
fi

echo ""

# ========================================
# Commit cambios
# ========================================

echo -e "${BLUE}3. Commiteando cambios${NC}"

git add .gitmodules Root/build-helpers

# Verificar si hay cambios
if git diff --cached --quiet; then
    echo -e "${YELLOW}  No hay cambios para commitear${NC}"
else
    git commit -m "Update submodules to personal forks

- build-helpers: https://github.com/${GITHUB_USER}/build-helpers
- TeaSpeak-shared: https://github.com/${GITHUB_USER}/TeaSpeak-shared

This gives full control over compilation dependencies."

    echo -e "${GREEN}  ✓ Cambios commiteados${NC}"
fi

echo ""

# ========================================
# Push cambios
# ========================================

echo -e "${BLUE}4. Pushing a remote${NC}"

CURRENT_BRANCH=$(git branch --show-current)
echo "  Rama actual: ${CURRENT_BRANCH}"

read -p "¿Hacer push a origin/${CURRENT_BRANCH}? (y/n): " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    git push origin "${CURRENT_BRANCH}"
    echo -e "${GREEN}  ✓ Push completado${NC}"
else
    echo -e "${YELLOW}  ⚠️  Push omitido${NC}"
    echo "  Puedes hacer push manualmente con:"
    echo "    git push origin ${CURRENT_BRANCH}"
fi

echo ""

# ========================================
# Resumen
# ========================================

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}   ✓ Actualización Completada${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""
echo "Submódulos ahora apuntan a tus forks:"
echo -e "  • build-helpers → ${BLUE}https://github.com/${GITHUB_USER}/build-helpers${NC}"
echo -e "  • shared → ${BLUE}https://github.com/${GITHUB_USER}/TeaSpeak-shared${NC}"
echo ""
echo -e "${YELLOW}Próximos pasos:${NC}"
echo "  1. Verifica que compile correctamente:"
echo -e "     ${BLUE}cd Root && bash build_teaspeak.sh optimized${NC}"
echo ""
echo "  2. Si todo funciona, los cambios ya están en tu repo"
echo "  3. Ahora tienes control total sobre las dependencias"
echo ""
