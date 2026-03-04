#!/bin/bash

# ═══════════════════════════════════════════════════════════════════════════
# Script de Corrección Inmediata - Aplica Parches a Archivos Existentes
# ═══════════════════════════════════════════════════════════════════════════
# Este script FUERZA la reescritura de los archivos problemáticos sin importar
# si ya existen o no. Úsalo cuando ya tengas el repositorio clonado.
# ═══════════════════════════════════════════════════════════════════════════

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

echo -e "${CYAN}════════════════════════════════════════════════════════════${NC}"
echo -e "${CYAN}  CORRECCIÓN INMEDIATA: Parches de Compilación Music Module${NC}"
echo -e "${CYAN}════════════════════════════════════════════════════════════${NC}"
echo ""

# Verificar que estamos en el directorio correcto
if [[ ! -f "setup_teaspeak.sh" ]]; then
    echo -e "${RED}[✗] Error: Ejecuta este script desde el directorio raíz de TeaSpeak${NC}"
    echo "Ejemplo: cd /root/TeaSpeak && bash apply_music_fix_now.sh"
    exit 1
fi

MUSIC_CMAKE="Server/Root/TeaSpeak/music/CMakeLists.txt"
SERVER_CMAKE="Server/Server/CMakeLists.txt"

# ═══════════════════════════════════════════════════════════════════════════
# PASO 1: Reescribir music/CMakeLists.txt
# ═══════════════════════════════════════════════════════════════════════════
echo -e "${YELLOW}[1/3]${NC} Reescribiendo music/CMakeLists.txt..."

if [[ ! -f "$MUSIC_CMAKE" ]]; then
    echo -e "${RED}[✗] Error: $MUSIC_CMAKE no encontrado${NC}"
    echo "Asegúrate de haber clonado los submódulos primero"
    exit 1
fi

# Crear backup
cp "$MUSIC_CMAKE" "${MUSIC_CMAKE}.backup.$(date +%s)"
echo -e "${GREEN}[✓]${NC} Backup creado"

# REESCRIBIR archivo completo
cat > "$MUSIC_CMAKE" << 'EOFMUSIC'
cmake_minimum_required(VERSION 3.6)
project(TeaMusic-Provider)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -fpermissive -Wall -Wno-sign-compare -static-libgcc -static-libstdc++ -fPIC")
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin/providers")

set(HEADERS include/MusicPlayer.h)

option(BUILD_PROVIDER_YT "Build the Youtube-dl provider. (You requre extra headers)" ON)
option(BUILD_PROVIDER_FFMPEG "Build the FFMpeg provider. (You requre extra headers)" ON)
option(BUILD_HELPERS "Build the development helper classes" ON)

if(NOT EXISTS ../shared/src/)
	set(LIBRARY_PATH_THREAD_POOL "ThreadPoolStatic")
	set(LIBRARY_PATH_JSON "jsoncpp_static")
	set(LIBRARY_PATH_VARIBALES "StringVariablesStatic")
endif()

include_directories(include)
# Include directories - CORREGIDOS AUTOMÁTICAMENTE
include_directories(../libraries/Thread-Pool/out/linux_amd64/include)
include_directories(../libraries/event/include)

if (BUILD_PROVIDER_YT)
	message("Building YouTube provider")
	add_library(ProviderYT SHARED ${HEADERS} providers/yt/YTProvider.cpp providers/yt/YTVManager.cpp providers/yt/YoutubeMusicPlayer.cpp providers/yt/YTRegex.cpp)
	target_link_libraries(ProviderYT ${LIBRARY_PATH_VARIBALES} ${LIBRARY_PATH_VARIBALES} ${LIBRARY_PATH_JSON} ${LIBRARY_PATH_THREAD_POOL} ProviderFFMpeg)
	#The Youtube provider requires this libraries:
	#- TeaMusic
	#- ProviderOpus
	#- stdc++fs.a
	set_target_properties(ProviderYT
			PROPERTIES
			PREFIX "001" #Library load order (Requires opus provider to load)
	)
endif ()

if(BUILD_PROVIDER_FFMPEG)
	message("Building FFMpeg provider")
	add_library(ProviderFFMpeg SHARED ${HEADERS} providers/ffmpeg/FFMpegProvider.cpp providers/ffmpeg/FFMpegMusicPlayer.cpp providers/ffmpeg/FFMpegMusicProcess.cpp)
	target_link_libraries(ProviderFFMpeg ${LIBRARY_PATH_VARIBALES} ${LIBRARY_PATH_THREAD_POOL} ${LIBEVENT_PATH}/libevent.a ${LIBEVENT_PATH}/libevent_pthreads.a)
	set_target_properties(ProviderFFMpeg
			PROPERTIES
			PREFIX "000" #Library load order (Requires nothink to load)
	)
endif()

if(BUILD_HELPERS)
	message("Building helpers")
	add_executable(YoutubedlTest helpers/YoutubedlTest.cpp)
	target_link_libraries(YoutubedlTest ProviderFFMpeg ProviderYT)
	target_link_libraries(YoutubedlTest TeaMusic TeaSpeak dl stdc++fs CXXTerminal StringVariablesStatic event_pthreads pthread)
endif()
EOFMUSIC

echo -e "${GREEN}[✓]${NC} music/CMakeLists.txt reescrito"

# Verificar
if grep -q "Thread-Pool/out/linux_amd64/include" "$MUSIC_CMAKE" && \
   grep -q "libraries/event/include" "$MUSIC_CMAKE"; then
    echo -e "${GREEN}[✓]${NC} Verificación exitosa: Rutas correctas aplicadas"
else
    echo -e "${RED}[✗]${NC} Error en la verificación"
    exit 1
fi

# ═══════════════════════════════════════════════════════════════════════════
# PASO 2: Parchar Server/Server/CMakeLists.txt - LIBEVENT_PATH
# ═══════════════════════════════════════════════════════════════════════════
echo ""
echo -e "${YELLOW}[2/3]${NC} Corrigiendo LIBEVENT_PATH en Server/Server/CMakeLists.txt..."

if [[ ! -f "$SERVER_CMAKE" ]]; then
    echo -e "${RED}[✗] Error: $SERVER_CMAKE no encontrado${NC}"
    exit 1
fi

# Crear backup
cp "$SERVER_CMAKE" "${SERVER_CMAKE}.backup.$(date +%s)"
echo -e "${GREEN}[✓]${NC} Backup creado"

# Usar awk para reemplazo preciso
awk '{
    if ($0 ~ /^set\(LIBEVENT_PATH/) {
        print "set(LIBEVENT_PATH \"${LIBRARY_PATH}/event/_build/linux_amd64/lib\")"
    } else {
        print $0
    }
}' "$SERVER_CMAKE" > "${SERVER_CMAKE}.tmp"

mv "${SERVER_CMAKE}.tmp" "$SERVER_CMAKE"

# Verificar
if grep -q 'set(LIBEVENT_PATH "${LIBRARY_PATH}/event/_build/linux_amd64/lib")' "$SERVER_CMAKE"; then
    echo -e "${GREEN}[✓]${NC} LIBEVENT_PATH corregida exitosamente (sin barra final)"
else
    echo -e "${RED}[✗]${NC} Error al parchar LIBEVENT_PATH"
    echo "Línea actual:"
    grep "LIBEVENT_PATH" "$SERVER_CMAKE" || true
    exit 1
fi

# ═══════════════════════════════════════════════════════════════════════════
# PASO 3: Limpiar build y preparar para recompilación
# ═══════════════════════════════════════════════════════════════════════════
echo ""
echo -e "${YELLOW}[3/3]${NC} Limpiando directorio build..."

BUILD_DIR="Server/Root/TeaSpeak/build"
if [[ -d "$BUILD_DIR" ]]; then
    rm -rf "$BUILD_DIR"
    echo -e "${GREEN}[✓]${NC} Directorio build limpiado"
else
    echo -e "${CYAN}[INFO]${NC} Directorio build no existe (normal en primera compilación)"
fi

# ═══════════════════════════════════════════════════════════════════════════
# RESUMEN
# ═══════════════════════════════════════════════════════════════════════════
echo ""
echo -e "${GREEN}════════════════════════════════════════════════════════════${NC}"
echo -e "${GREEN}  ✅ PARCHES APLICADOS EXITOSAMENTE${NC}"
echo -e "${GREEN}════════════════════════════════════════════════════════════${NC}"
echo ""
echo -e "${CYAN}Archivos modificados:${NC}"
echo "  • music/CMakeLists.txt → Rutas de include corregidas"
echo "  • Server/Server/CMakeLists.txt → LIBEVENT_PATH sin barra final"
echo ""
echo -e "${CYAN}Backups creados:${NC}"
echo "  • ${MUSIC_CMAKE}.backup.*"
echo "  • ${SERVER_CMAKE}.backup.*"
echo ""
echo -e "${YELLOW}Próximo paso:${NC}"
echo "  Recompilar TeaSpeak con:"
echo ""
echo "    cd Server/Root"
echo "    export build_os_type=linux"
echo "    export build_os_arch=amd64"
echo "    bash build_teaspeak.sh stable"
echo ""
echo -e "${GREEN}¡Los errores de compilación deberían estar resueltos!${NC}"
echo ""
