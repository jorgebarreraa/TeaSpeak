#!/bin/bash

# ═══════════════════════════════════════════════════════════════════════════
# Script de Actualización y Recompilación Automática
# ═══════════════════════════════════════════════════════════════════════════
# Este script:
# 1. Resuelve conflictos de git descartando cambios locales
# 2. Hace pull de los cambios más recientes
# 3. Recompila TeaSpeak con los parches automáticos aplicados
#
# NOTA: Si ejecutas desde /root/TeaSpeak, asegúrate de tener el symlink:
#       ln -sf /home/user/TeaSpeak /root/TeaSpeak
# ═══════════════════════════════════════════════════════════════════════════

set -e

GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

echo -e "${CYAN}════════════════════════════════════════════════════════════${NC}"
echo -e "${CYAN}  Actualización y Recompilación Automática de TeaSpeak${NC}"
echo -e "${CYAN}════════════════════════════════════════════════════════════${NC}"
echo ""

# Verificar que estamos en el directorio correcto
if [[ ! -f "setup_teaspeak.sh" ]]; then
    echo "ERROR: Ejecuta este script desde el directorio raíz de TeaSpeak"
    echo "Uso: cd <directorio-teaspeak> && bash update_and_rebuild.sh"
    echo "Ejemplo: cd /home/user/TeaSpeak && bash update_and_rebuild.sh"
    exit 1
fi

# PASO 1: Descartar cambios locales en archivos parcheados
echo -e "${YELLOW}[1/4]${NC} Descartando cambios locales en archivos parcheados..."
git checkout -- Server/Root/build_teaspeak.sh 2>/dev/null || true
git checkout -- Server/Root/TeaSpeak/server/CMakeLists.txt 2>/dev/null || true
git checkout -- Server/Root/TeaSpeak/shared/src/misc/utf8.h 2>/dev/null || true
git checkout -- Server/Root/build-helpers/libraries/build_datapipes.sh 2>/dev/null || true
echo -e "${GREEN}✓${NC} Archivos restaurados a estado original"
echo ""

# PASO 2: Pull de los cambios del repositorio
echo -e "${YELLOW}[2/4]${NC} Descargando últimos cambios del repositorio..."
git pull origin claude/fix-install-script-ULBSP
echo -e "${GREEN}✓${NC} Cambios descargados exitosamente"
echo ""

# PASO 3: Verificar que apply_compilation_patches.sh existe
echo -e "${YELLOW}[3/4]${NC} Verificando script de parches automáticos..."
if [[ -f "apply_compilation_patches.sh" ]]; then
    chmod +x apply_compilation_patches.sh
    echo -e "${GREEN}✓${NC} Script de parches encontrado y configurado"
else
    echo "ERROR: apply_compilation_patches.sh no encontrado"
    exit 1
fi
echo ""

# PASO 4: Recompilar TeaSpeak
echo -e "${YELLOW}[4/4]${NC} Recompilando TeaSpeak con parches automáticos..."
echo ""
echo -e "${CYAN}Iniciando compilación (los parches se aplicarán automáticamente)...${NC}"
echo ""

cd Server/Root
export build_os_type=linux
export build_os_arch=amd64

# Ejecutar build_teaspeak.sh que ahora aplica parches automáticamente
bash build_teaspeak.sh stable

# Verificar resultado
if [[ $? -eq 0 ]]; then
    echo ""
    echo -e "${GREEN}════════════════════════════════════════════════════════════${NC}"
    echo -e "${GREEN}  ✅ COMPILACIÓN COMPLETADA EXITOSAMENTE${NC}"
    echo -e "${GREEN}════════════════════════════════════════════════════════════${NC}"
    echo ""
    echo -e "${CYAN}Binarios compilados:${NC}"
    echo "  • TeaSpeakServer: Server/Root/TeaSpeak/server/environment/"
    echo "  • Music Providers: Server/Root/TeaSpeak/music/bin/providers/"
    echo ""
    echo -e "${GREEN}¡TeaSpeak está listo para usar!${NC}"
    echo ""
else
    echo ""
    echo -e "${YELLOW}════════════════════════════════════════════════════════════${NC}"
    echo -e "${YELLOW}  ⚠ La compilación falló${NC}"
    echo -e "${YELLOW}════════════════════════════════════════════════════════════${NC}"
    echo ""
    echo "Revisa los errores arriba para más detalles"
    exit 1
fi
