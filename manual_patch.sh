#!/bin/bash

set -e

echo "=== Aplicando Parches Manuales a TeaSpeak ==="
echo ""

# Colores para output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# PARCHE 1: Server/Server/CMakeLists.txt (LIBEVENT_PATH)
echo "1. Parcheando Server/Server/CMakeLists.txt..."
if [[ -f "/root/TeaSpeak/Server/Server/CMakeLists.txt" ]]; then
    # Hacer backup
    cp /root/TeaSpeak/Server/Server/CMakeLists.txt /root/TeaSpeak/Server/Server/CMakeLists.txt.backup

    # Aplicar parche
    sed -i 's|event/build/lib|event/_build/linux_amd64/lib|g' /root/TeaSpeak/Server/Server/CMakeLists.txt

    if grep -q "event/_build/linux_amd64/lib" /root/TeaSpeak/Server/Server/CMakeLists.txt; then
        echo -e "${GREEN}✓ LIBEVENT_PATH corregido exitosamente${NC}"
    else
        echo -e "${RED}✗ Error al corregir LIBEVENT_PATH${NC}"
        exit 1
    fi
else
    echo -e "${RED}✗ Server/Server/CMakeLists.txt no encontrado${NC}"
    exit 1
fi

echo ""

# PARCHE 2: music/CMakeLists.txt (rutas e includes)
echo "2. Parcheando music/CMakeLists.txt..."
if [[ -f "/root/TeaSpeak/Server/Root/TeaSpeak/music/CMakeLists.txt" ]]; then
    # Hacer backup
    cp /root/TeaSpeak/Server/Root/TeaSpeak/music/CMakeLists.txt \
       /root/TeaSpeak/Server/Root/TeaSpeak/music/CMakeLists.txt.backup

    cd /root/TeaSpeak/Server/Root/TeaSpeak

    # Aplicar parches de rutas
    sed -i 's|event/build/lib|event/_build/linux_amd64/lib|g' music/CMakeLists.txt
    sed -i 's|event/build/include|event/_build/linux_amd64/include|g' music/CMakeLists.txt
    sed -i 's|Thread-Pool/build|Thread-Pool/out/linux_amd64|g' music/CMakeLists.txt

    # Agregar include directories si no existen
    if ! grep -q "libraries/Thread-Pool/src" music/CMakeLists.txt; then
        echo "   Agregando include directories..."
        sed -i '/^include_directories(include)/a \
# Include directories agregados automáticamente\
include_directories(../libraries/Thread-Pool/src)\
include_directories(../libraries/event/include)\
include_directories(../libraries/event/_build/linux_amd64/include)' music/CMakeLists.txt
        echo -e "${GREEN}✓ Include directories agregados${NC}"
    else
        echo -e "${YELLOW}⚠ Include directories ya existen${NC}"
    fi

    # Verificar parches
    if grep -q "event/_build/linux_amd64" music/CMakeLists.txt && \
       grep -q "Thread-Pool/src" music/CMakeLists.txt; then
        echo -e "${GREEN}✓ music/CMakeLists.txt parcheado exitosamente${NC}"
    else
        echo -e "${RED}✗ Error al parchar music/CMakeLists.txt${NC}"
        exit 1
    fi
else
    echo -e "${RED}✗ music/CMakeLists.txt no encontrado${NC}"
    echo "   Verifica que los submódulos estén clonados"
    exit 1
fi

echo ""
echo -e "${GREEN}=== Parches aplicados exitosamente ===${NC}"
echo ""
echo "Ahora ejecuta:"
echo "  cd /root/TeaSpeak/Server/Root"
echo "  bash build_teaspeak.sh"
echo ""
