#!/bin/bash

# ═══════════════════════════════════════════════════════════════════════════
# ⚠️  SCRIPT OBSOLETO - INTEGRADO EN setup_teaspeak.sh
# ═══════════════════════════════════════════════════════════════════════════
# Este script ya NO es necesario. Sus correcciones fueron integradas en:
#   - PASO 9.5: initialize_submodules() → Reescribe music/CMakeLists.txt
#   - PASO 9.6: patch_cmake_library_paths() → Corrige LIBEVENT_PATH con awk
#
# Ejecuta directamente: ./setup_teaspeak.sh
# ═══════════════════════════════════════════════════════════════════════════

echo "⚠️  Este script está OBSOLETO"
echo ""
echo "Las correcciones de este script ya están integradas en setup_teaspeak.sh"
echo ""
echo "Para aplicar todos los parches automáticamente, ejecuta:"
echo "  ./setup_teaspeak.sh"
echo ""
exit 0

# Script definitivo para corregir errores de compilación del módulo music
# Este script REESCRIBE los archivos problemáticos en lugar de usar sed

set -e

echo "════════════════════════════════════════════════════════════"
echo "  PARCHE DEFINITIVO: Reescritura de CMakeLists.txt Files"
echo "════════════════════════════════════════════════════════════"
echo ""

# Colores
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# ============================================================================
# PASO 1: Parchar music/CMakeLists.txt
# ============================================================================
echo "1. Parcheando music/CMakeLists.txt..."

if [[ ! -f "/root/TeaSpeak/Server/Root/TeaSpeak/music/CMakeLists.txt" ]]; then
    echo -e "${RED}✗ music/CMakeLists.txt no encontrado${NC}"
    exit 1
fi

# Crear backup
cp /root/TeaSpeak/Server/Root/TeaSpeak/music/CMakeLists.txt \
   /root/TeaSpeak/Server/Root/TeaSpeak/music/CMakeLists.txt.backup.$(date +%s)

# REESCRIBIR el archivo completo con las correcciones
cat > /root/TeaSpeak/Server/Root/TeaSpeak/music/CMakeLists.txt << 'EOFMUSIC'
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

echo -e "${GREEN}✓ music/CMakeLists.txt reescrito${NC}"

# ============================================================================
# PASO 2: Parchar Server/Server/CMakeLists.txt (LIBEVENT_PATH)
# ============================================================================
echo ""
echo "2. Parcheando Server/Server/CMakeLists.txt..."

if [[ ! -f "/root/TeaSpeak/Server/Server/CMakeLists.txt" ]]; then
    echo -e "${RED}✗ Server/Server/CMakeLists.txt no encontrado${NC}"
    exit 1
fi

# Crear backup
cp /root/TeaSpeak/Server/Server/CMakeLists.txt \
   /root/TeaSpeak/Server/Server/CMakeLists.txt.backup.$(date +%s)

# Leer el archivo y reemplazar SOLO la línea de LIBEVENT_PATH
awk '{
    if ($0 ~ /^set\(LIBEVENT_PATH/) {
        print "set(LIBEVENT_PATH \"${LIBRARY_PATH}/event/_build/linux_amd64/lib\")"
    } else {
        print $0
    }
}' /root/TeaSpeak/Server/Server/CMakeLists.txt > /root/TeaSpeak/Server/Server/CMakeLists.txt.tmp

mv /root/TeaSpeak/Server/Server/CMakeLists.txt.tmp /root/TeaSpeak/Server/Server/CMakeLists.txt

echo -e "${GREEN}✓ Server/Server/CMakeLists.txt corregido${NC}"

# ============================================================================
# PASO 3: Verificar cambios
# ============================================================================
echo ""
echo "3. Verificando cambios aplicados..."
echo ""

cd /root/TeaSpeak/Server/Root/TeaSpeak/music
echo "   music/CMakeLists.txt - Include directories:"
grep -n "include_directories" CMakeLists.txt | head -5

echo ""
cd /root/TeaSpeak/Server/Server
echo "   Server/Server/CMakeLists.txt - LIBEVENT_PATH:"
grep "LIBEVENT_PATH" CMakeLists.txt

# ============================================================================
# PASO 4: Limpiar build
# ============================================================================
echo ""
echo "4. Limpiando build anterior..."
cd /root/TeaSpeak/Server/Root/TeaSpeak
rm -rf build
mkdir -p build
echo -e "${GREEN}✓ Build directory limpio${NC}"

# ============================================================================
# PASO 5: Instrucciones finales
# ============================================================================
echo ""
echo "════════════════════════════════════════════════════════════"
echo -e "${GREEN}✓ PARCHES APLICADOS DEFINITIVAMENTE${NC}"
echo "════════════════════════════════════════════════════════════"
echo ""
echo "Archivos modificados:"
echo "  - music/CMakeLists.txt: REESCRITO con rutas correctas"
echo "  - Server/Server/CMakeLists.txt: LIBEVENT_PATH corregido"
echo ""
echo "Backups creados:"
echo "  - music/CMakeLists.txt.backup.*"
echo "  - Server/Server/CMakeLists.txt.backup.*"
echo ""
echo "Ahora ejecuta:"
echo "  cd /root/TeaSpeak/Server/Root"
echo "  bash build_teaspeak.sh"
echo ""
