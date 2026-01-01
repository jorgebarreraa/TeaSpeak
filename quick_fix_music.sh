#!/bin/bash

# Script de parche rápido para corregir errores de compilación del módulo music
# Este script corrige las rutas de headers sin tener que recompilar todo desde cero

set -e

echo "════════════════════════════════════════════════════════════"
echo "  PARCHE RÁPIDO: Corrección de Rutas del Módulo Music"
echo "════════════════════════════════════════════════════════════"
echo ""

# Colores
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# 1. Corregir music/CMakeLists.txt
echo "1. Parcheando music/CMakeLists.txt..."
if [[ ! -f "/root/TeaSpeak/Server/Root/TeaSpeak/music/CMakeLists.txt" ]]; then
    echo -e "${RED}✗ music/CMakeLists.txt no encontrado${NC}"
    exit 1
fi

cd /root/TeaSpeak/Server/Root/TeaSpeak/music

# Eliminar los include directories incorrectos
sed -i '/Include directories agregados automáticamente/d' CMakeLists.txt
sed -i '/libraries\/Thread-Pool\/src/d' CMakeLists.txt
sed -i '/libraries\/event\/_build\/linux_amd64\/include/d' CMakeLists.txt

# Agregar los include directories correctos
sed -i '/^include_directories(include)/a \
# Include directories agregados automáticamente - CORREGIDOS\
include_directories(../libraries/Thread-Pool/out/linux_amd64/include)\
include_directories(../libraries/event/include)' CMakeLists.txt

echo -e "${GREEN}✓ music/CMakeLists.txt parcheado${NC}"

# 2. Corregir Server/Server/CMakeLists.txt (LIBEVENT_PATH)
echo ""
echo "2. Parcheando Server/Server/CMakeLists.txt..."
cd /root/TeaSpeak/Server/Server

# Eliminar barra al final de LIBEVENT_PATH
sed -i 's|event/_build/linux_amd64/lib/"|event/_build/linux_amd64/lib"|g' CMakeLists.txt

echo -e "${GREEN}✓ LIBEVENT_PATH corregido (barra final eliminada)${NC}"

# 3. Verificar los parches
echo ""
echo "3. Verificando parches aplicados..."
echo ""

cd /root/TeaSpeak/Server/Root/TeaSpeak/music
echo "   Include directories en music/CMakeLists.txt:"
grep "include_directories" CMakeLists.txt | grep -E "(Thread-Pool|event)"

echo ""
cd /root/TeaSpeak/Server/Server
echo "   LIBEVENT_PATH en Server/Server/CMakeLists.txt:"
grep "LIBEVENT_PATH" CMakeLists.txt

# 4. Limpiar build anterior
echo ""
echo "4. Limpiando build anterior..."
cd /root/TeaSpeak/Server/Root/TeaSpeak
rm -rf build
mkdir -p build
echo -e "${GREEN}✓ Build directory limpio${NC}"

# 5. Instrucciones finales
echo ""
echo "════════════════════════════════════════════════════════════"
echo -e "${GREEN}✓ PARCHES APLICADOS EXITOSAMENTE${NC}"
echo "════════════════════════════════════════════════════════════"
echo ""
echo "Ahora ejecuta:"
echo "  cd /root/TeaSpeak/Server/Root"
echo "  bash build_teaspeak.sh"
echo ""
echo "Los errores de compilación deberían estar resueltos."
echo ""
