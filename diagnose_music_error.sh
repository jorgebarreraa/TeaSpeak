#!/bin/bash

echo "═══════════════════════════════════════════════════════════"
echo "  DIAGNÓSTICO DE ERRORES DE COMPILACIÓN - MUSIC MODULE"
echo "═══════════════════════════════════════════════════════════"
echo ""

# Colores
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

cd /root/TeaSpeak/Server/Root/TeaSpeak

echo "1. Verificando existencia de music/CMakeLists.txt..."
if [[ -f "music/CMakeLists.txt" ]]; then
    echo -e "${GREEN}✓ Archivo existe${NC}"
else
    echo -e "${RED}✗ Archivo NO existe${NC}"
    exit 1
fi

echo ""
echo "2. Verificando rutas en music/CMakeLists.txt..."
echo ""
echo "   a) Buscando include_directories:"
grep -n "include_directories" music/CMakeLists.txt

echo ""
echo "   b) Buscando LIBEVENT_PATH:"
grep -n "LIBEVENT_PATH" music/CMakeLists.txt || echo "      No se encontró LIBEVENT_PATH"

echo ""
echo "   c) Buscando referencias a Thread-Pool:"
grep -n "Thread-Pool" music/CMakeLists.txt || echo "      No se encontró Thread-Pool"

echo ""
echo "   d) Buscando referencias a event/:"
grep -n "event/" music/CMakeLists.txt || echo "      No se encontró event/"

echo ""
echo "3. Verificando Server/Server/CMakeLists.txt..."
echo ""
cd /root/TeaSpeak/Server/Server

echo "   Buscando LIBEVENT_PATH:"
grep -n "LIBEVENT_PATH" CMakeLists.txt

echo ""
echo "4. Verificando que las librerías existen físicamente..."
echo ""

cd /root/TeaSpeak/Server/Root/libraries

echo -n "   libevent.a en _build/linux_amd64/lib/: "
if [[ -f "event/_build/linux_amd64/lib/libevent.a" ]]; then
    echo -e "${GREEN}✓${NC}"
    ls -lh event/_build/linux_amd64/lib/libevent.a
else
    echo -e "${RED}✗${NC}"
fi

echo ""
echo -n "   libevent_pthreads.a en _build/linux_amd64/lib/: "
if [[ -f "event/_build/linux_amd64/lib/libevent_pthreads.a" ]]; then
    echo -e "${GREEN}✓${NC}"
    ls -lh event/_build/linux_amd64/lib/libevent_pthreads.a
else
    echo -e "${RED}✗${NC}"
fi

echo ""
echo -n "   event2/thread.h en _build/linux_amd64/include/: "
if [[ -f "event/_build/linux_amd64/include/event2/thread.h" ]]; then
    echo -e "${GREEN}✓${NC}"
else
    echo -e "${RED}✗${NC}"
    echo "      Buscando en otros lugares..."
    find event/ -name "thread.h" 2>/dev/null
fi

echo ""
echo -n "   event2/thread.h en include/: "
if [[ -f "event/include/event2/thread.h" ]]; then
    echo -e "${GREEN}✓${NC}"
else
    echo -e "${RED}✗${NC}"
fi

echo ""
echo -n "   ThreadPool/Mutex.h: "
if [[ -f "Thread-Pool/src/Mutex.h" ]]; then
    echo -e "${GREEN}✓${NC}"
    ls -lh Thread-Pool/src/Mutex.h
else
    echo -e "${RED}✗${NC}"
    echo "      Buscando Mutex.h..."
    find Thread-Pool/ -name "Mutex.h" 2>/dev/null || echo "      No encontrado"
fi

echo ""
echo "5. Mostrando contenido completo de music/CMakeLists.txt..."
echo "═══════════════════════════════════════════════════════════"
cat /root/TeaSpeak/Server/Root/TeaSpeak/music/CMakeLists.txt
echo "═══════════════════════════════════════════════════════════"

echo ""
echo "6. Mostrando LIBEVENT_PATH del CMakeLists.txt principal..."
echo "═══════════════════════════════════════════════════════════"
grep -A2 -B2 "LIBEVENT_PATH" /root/TeaSpeak/Server/Server/CMakeLists.txt
echo "═══════════════════════════════════════════════════════════"

echo ""
echo "Diagnóstico completo."
