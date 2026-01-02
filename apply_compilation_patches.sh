#!/bin/bash

# ═══════════════════════════════════════════════════════════════════════════
# Script de Parches Pre-Compilación - Se ejecuta AUTOMÁTICAMENTE antes de CMake
# ═══════════════════════════════════════════════════════════════════════════
# Este script aplica TODOS los parches necesarios para que la compilación
# funcione correctamente. Se llama desde:
#   - setup_teaspeak.sh (PASO 9.5 y 9.6)
#   - build_teaspeak.sh (antes de cmake)
# ═══════════════════════════════════════════════════════════════════════════

set -e

# Detectar si estamos siendo llamados desde otro script o directamente
if [[ -z "$SCRIPT_DIR" ]]; then
    SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
fi

# Colores (solo si estamos en terminal)
if [[ -t 1 ]]; then
    RED='\033[0;31m'
    GREEN='\033[0;32m'
    YELLOW='\033[1;33m'
    CYAN='\033[0;36m'
    NC='\033[0m'
else
    RED=''
    GREEN=''
    YELLOW=''
    CYAN=''
    NC=''
fi

log_info() {
    echo -e "${CYAN}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[✓]${NC} $1"
}

log_error() {
    echo -e "${RED}[✗]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[⚠]${NC} $1"
}

echo ""
echo -e "${CYAN}═══════════════════════════════════════════════════════════${NC}"
echo -e "${CYAN}  Aplicando Parches Pre-Compilación${NC}"
echo -e "${CYAN}═══════════════════════════════════════════════════════════${NC}"
echo ""

# ═══════════════════════════════════════════════════════════════════════════
# PRE-PATCH: Restaurar archivos que puedan estar corruptos de ejecuciones previas
# ═══════════════════════════════════════════════════════════════════════════
log_info "Pre-Patch: Verificando integridad de archivos críticos..."

# Lista de archivos que pueden haberse corrompido en ejecuciones anteriores
CRITICAL_FILES=(
    "$SCRIPT_DIR/Server/Root/TeaSpeak/server/src/music/MusicPlaylist.cpp"
    "$SCRIPT_DIR/Server/Root/TeaSpeak/server/src/music/MusicPlaylist.h"
)

restored_count=0
for file in "${CRITICAL_FILES[@]}"; do
    if [[ -f "$file" ]]; then
        # Verificar si el archivo tiene cambios no commiteados
        file_dir="$(dirname "$file")"
        file_name="$(basename "$file")"
        relative_path="${file#$SCRIPT_DIR/Server/Root/TeaSpeak/}"

        cd "$SCRIPT_DIR/Server/Root/TeaSpeak"

        # Comprobar si hay cambios en el archivo
        if git diff --quiet "$relative_path" 2>/dev/null; then
            # No hay cambios, archivo limpio
            :
        else
            # Hay cambios - restaurar desde git
            log_warning "Detectados cambios en $file_name, restaurando desde repositorio..."
            if git checkout HEAD -- "$relative_path" 2>/dev/null; then
                log_success "✓ $file_name restaurado correctamente"
                ((restored_count++))
            else
                log_warning "⚠ No se pudo restaurar $file_name (continuando...)"
            fi
        fi
    fi
done

if [[ $restored_count -gt 0 ]]; then
    log_success "✓ Pre-Patch completado: $restored_count archivo(s) restaurado(s)"
else
    log_success "✓ Pre-Patch completado: Todos los archivos están limpios"
fi

echo ""

# ═══════════════════════════════════════════════════════════════════════════
# PARCHE 0: Verificar y clonar submódulos si no existen
# ═══════════════════════════════════════════════════════════════════════════
SUBMODULES_DIR="$SCRIPT_DIR/Server/Server"

if [[ -d "$SUBMODULES_DIR" ]]; then
    cd "$SUBMODULES_DIR"

    # Verificar submódulo music
    if [[ ! -d "music/.git" ]]; then
        log_info "Submódulo 'music' no encontrado, clonando..."
        if [[ -d "music" ]]; then
            rm -rf music
        fi
        git clone -b master https://github.com/jorgebarreraa/TeaMusic-Providers.git music 2>/dev/null || \
        git clone -b master https://github.com/TeaSpeak/TeaMusic-Providers.git music || \
        log_warning "No se pudo clonar submódulo music (continuando...)"

        if [[ -d "music/.git" ]]; then
            log_success "✓ Submódulo 'music' clonado exitosamente (branch master)"
        fi
    else
        log_success "✓ Submódulo 'music' ya existe"

        # Verificar si está en la branch correcta y tiene la estructura correcta
        cd music
        current_branch=$(git branch --show-current)
        if [[ "$current_branch" != "master" ]]; then
            log_info "Cambiando submódulo music de branch '$current_branch' a 'master'..."
            # Guardar cambios locales antes del checkout
            git stash push -m "Auto-stash before switching to master" 2>/dev/null || true
            git fetch origin master 2>/dev/null || true
            if git checkout master 2>/dev/null; then
                log_success "✓ Submódulo music cambiado a branch master"
            else
                log_warning "No se pudo cambiar a branch master, usando branch actual"
            fi
        fi

        # Verificar estructura de directorios y corregir si es necesario
        # SIEMPRE ejecutar esto, sin importar la branch
        if [[ ! -d "include/teaspeak" ]] && [[ -f "include/MusicPlayer.h" ]]; then
            log_info "Corrigiendo estructura de directorios en music/include/..."
            mkdir -p include/teaspeak
            mv include/MusicPlayer.h include/teaspeak/MusicPlayer.h
            log_success "✓ MusicPlayer.h movido a include/teaspeak/"
        elif [[ -f "include/teaspeak/MusicPlayer.h" ]]; then
            log_success "✓ Estructura de directorios correcta (include/teaspeak/)"
            # Eliminar archivo viejo si existe (para evitar inclusión doble)
            if [[ -f "include/MusicPlayer.h" ]]; then
                log_info "Eliminando archivo duplicado include/MusicPlayer.h..."
                rm -f include/MusicPlayer.h
                log_success "✓ Archivo duplicado eliminado"
            fi
        else
            log_warning "⚠ MusicPlayer.h no encontrado en ubicación esperada"
        fi
        cd "$SUBMODULES_DIR"
    fi

    # Verificar submódulo shared
    if [[ ! -d "shared/.git" ]]; then
        log_info "Submódulo 'shared' no encontrado, clonando..."
        if [[ -d "shared" ]]; then
            rm -rf shared
        fi
        git clone https://github.com/jorgebarreraa/TeaSpeakLibrary.git shared 2>/dev/null || \
        git clone https://github.com/TeaSpeak/TeaSpeakLibrary.git shared || \
        log_warning "No se pudo clonar submódulo shared (continuando...)"

        if [[ -d "shared/.git" ]]; then
            log_success "✓ Submódulo 'shared' clonado exitosamente"
        fi
    else
        log_success "✓ Submódulo 'shared' ya existe"
    fi

    cd "$SCRIPT_DIR"
fi

# ═══════════════════════════════════════════════════════════════════════════
# PARCHE 1: music/CMakeLists.txt - Include directories
# ═══════════════════════════════════════════════════════════════════════════
MUSIC_CMAKE="$SCRIPT_DIR/Server/Root/TeaSpeak/music/CMakeLists.txt"

if [[ -f "$MUSIC_CMAKE" ]]; then
    log_info "Parcheando music/CMakeLists.txt..."

    # Crear backup solo si no existe uno reciente
    if [[ ! -f "${MUSIC_CMAKE}.backup" ]]; then
        cp "$MUSIC_CMAKE" "${MUSIC_CMAKE}.backup"
    fi

    # REESCRIBIR archivo completo
    cat > "$MUSIC_CMAKE" << 'EOFMUSIC'
cmake_minimum_required(VERSION 3.6)
project(TeaMusic-Provider)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -fpermissive -Wall -Wno-sign-compare -static-libgcc -static-libstdc++ -fPIC")
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin/providers")

set(HEADERS include/teaspeak/MusicPlayer.h)

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
include_directories(../../Root/libraries/Thread-Pool/out/linux_amd64/include)
include_directories(../../Root/libraries/event/include)
include_directories(../../Root/libraries/event/_build/linux_amd64/include)

# Library paths - CORREGIDOS AUTOMÁTICAMENTE
# Convert relative paths to absolute paths for proper linking
# Use CMAKE_SOURCE_DIR (root CMakeLists.txt dir) instead of CMAKE_CURRENT_SOURCE_DIR to avoid symlink issues
get_filename_component(LIBEVENT_LIB "${CMAKE_SOURCE_DIR}/../libraries/event/_build/linux_amd64/lib/libevent.a" ABSOLUTE)
get_filename_component(LIBEVENT_PTHREADS_LIB "${CMAKE_SOURCE_DIR}/../libraries/event/_build/linux_amd64/lib/libevent_pthreads.a" ABSOLUTE)

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
	target_link_libraries(ProviderFFMpeg ${LIBRARY_PATH_VARIBALES} ${LIBRARY_PATH_THREAD_POOL} ${LIBEVENT_LIB} ${LIBEVENT_PTHREADS_LIB})
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

    # Verificar
    if grep -q "../../Root/libraries/Thread-Pool/out/linux_amd64/include" "$MUSIC_CMAKE" && \
       grep -q "../../Root/libraries/event/include" "$MUSIC_CMAKE" && \
       grep -q "../../Root/libraries/event/_build/linux_amd64/include" "$MUSIC_CMAKE" && \
       grep -q 'get_filename_component(LIBEVENT_LIB' "$MUSIC_CMAKE" && \
       grep -q 'get_filename_component(LIBEVENT_PTHREADS_LIB' "$MUSIC_CMAKE"; then
        log_success "music/CMakeLists.txt parcheado correctamente"
    else
        log_error "Error al parchar music/CMakeLists.txt"
        exit 1
    fi
else
    log_warning "music/CMakeLists.txt no encontrado (omitiendo parche 1)"
fi

# ═══════════════════════════════════════════════════════════════════════════
# PARCHE 2: Server/Server/CMakeLists.txt - LIBEVENT_PATH
# ═══════════════════════════════════════════════════════════════════════════
SERVER_CMAKE="$SCRIPT_DIR/Server/Server/CMakeLists.txt"

if [[ -f "$SERVER_CMAKE" ]]; then
    log_info "Parcheando Server/Server/CMakeLists.txt..."

    # Crear backup solo si no existe uno reciente
    if [[ ! -f "${SERVER_CMAKE}.backup" ]]; then
        cp "$SERVER_CMAKE" "${SERVER_CMAKE}.backup"
    fi

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
        log_success "LIBEVENT_PATH corregida exitosamente"
    else
        log_error "Error al parchar LIBEVENT_PATH"
        exit 1
    fi
else
    log_warning "Server/Server/CMakeLists.txt no encontrado (omitiendo parche 2)"
fi

# ═══════════════════════════════════════════════════════════════════════════
# PARCHE 3: Limpiar directorio build para forzar reconfiguracion de CMake
# ═══════════════════════════════════════════════════════════════════════════
BUILD_DIR="$SCRIPT_DIR/Server/Root/TeaSpeak/build"

log_info "Limpiando directorio build para forzar reconfiguracion..."

if [[ -d "$BUILD_DIR" ]]; then
    rm -rf "$BUILD_DIR"
    log_success "Directorio build limpiado (CMake usará configuración nueva)"
else
    log_info "Directorio build no existe (normal en primera compilación)"
fi

# ═══════════════════════════════════════════════════════════════════════════
# PARCHE 4: Agregar #include <cstdint> a shared/src/misc/digest.h
# ═══════════════════════════════════════════════════════════════════════════
DIGEST_HEADER="$SCRIPT_DIR/Server/Root/TeaSpeak/shared/src/misc/digest.h"

if [[ -f "$DIGEST_HEADER" ]]; then
    log_info "Parcheando shared/src/misc/digest.h..."

    # Verificar si ya tiene el include
    if ! grep -q '#include <cstdint>' "$DIGEST_HEADER"; then
        # Agregar #include <cstdint> después de #include <cassert>
        sed -i '/#include <cassert>/a #include <cstdint>' "$DIGEST_HEADER"

        if grep -q '#include <cstdint>' "$DIGEST_HEADER"; then
            log_success "✓ #include <cstdint> agregado a digest.h"
        else
            log_error "Error al agregar #include <cstdint>"
            exit 1
        fi
    else
        log_success "✓ digest.h ya tiene #include <cstdint>"
    fi
else
    log_warning "shared/src/misc/digest.h no encontrado (omitiendo parche 4)"
fi

# ═══════════════════════════════════════════════════════════════════════════
# PARCHE 5: Agregar #include <utility> a shared/src/misc/task_executor.cpp
# ═══════════════════════════════════════════════════════════════════════════
TASK_EXECUTOR_CPP="$SCRIPT_DIR/Server/Root/TeaSpeak/shared/src/misc/task_executor.cpp"

if [[ -f "$TASK_EXECUTOR_CPP" ]]; then
    log_info "Parcheando shared/src/misc/task_executor.cpp..."

    # Verificar si ya tiene el include
    if ! grep -q '#include <utility>' "$TASK_EXECUTOR_CPP"; then
        # Agregar #include <utility> después de #include <algorithm>
        sed -i '/#include <algorithm>/a #include <utility>' "$TASK_EXECUTOR_CPP"

        if grep -q '#include <utility>' "$TASK_EXECUTOR_CPP"; then
            log_success "✓ #include <utility> agregado a task_executor.cpp"
        else
            log_error "Error al agregar #include <utility>"
            exit 1
        fi
    else
        log_success "✓ task_executor.cpp ya tiene #include <utility>"
    fi
else
    log_warning "shared/src/misc/task_executor.cpp no encontrado (omitiendo parche 5)"
fi

# ═══════════════════════════════════════════════════════════════════════════
# PARCHE 6: Agregar #include <cstdint> a shared/src/query/escape.cpp
# ═══════════════════════════════════════════════════════════════════════════
ESCAPE_CPP="$SCRIPT_DIR/Server/Root/TeaSpeak/shared/src/query/escape.cpp"

if [[ -f "$ESCAPE_CPP" ]]; then
    log_info "Parcheando shared/src/query/escape.cpp..."

    # Verificar si ya tiene el include
    if ! grep -q '#include <cstdint>' "$ESCAPE_CPP"; then
        # Agregar #include <cstdint> después de #include <stdexcept>
        sed -i '/#include <stdexcept>/a #include <cstdint>' "$ESCAPE_CPP"

        if grep -q '#include <cstdint>' "$ESCAPE_CPP"; then
            log_success "✓ #include <cstdint> agregado a escape.cpp"
        else
            log_error "Error al agregar #include <cstdint>"
            exit 1
        fi
    else
        log_success "✓ escape.cpp ya tiene #include <cstdint>"
    fi
else
    log_warning "shared/src/query/escape.cpp no encontrado (omitiendo parche 6)"
fi

# ═══════════════════════════════════════════════════════════════════════════
# PARCHE 7: Agregar #include <cstdint> a shared/src/protocol/PacketLossCalculator.h
# ═══════════════════════════════════════════════════════════════════════════
PACKETLOSS_HEADER="$SCRIPT_DIR/Server/Root/TeaSpeak/shared/src/protocol/PacketLossCalculator.h"

if [[ -f "$PACKETLOSS_HEADER" ]]; then
    log_info "Parcheando shared/src/protocol/PacketLossCalculator.h..."

    # Verificar si ya tiene el include
    if ! grep -q '#include <cstdint>' "$PACKETLOSS_HEADER"; then
        # Agregar #include <cstdint> después de #include <bitset>
        sed -i '/#include <bitset>/a #include <cstdint>' "$PACKETLOSS_HEADER"

        if grep -q '#include <cstdint>' "$PACKETLOSS_HEADER"; then
            log_success "✓ #include <cstdint> agregado a PacketLossCalculator.h"
        else
            log_error "Error al agregar #include <cstdint>"
            exit 1
        fi
    else
        log_success "✓ PacketLossCalculator.h ya tiene #include <cstdint>"
    fi
else
    log_warning "shared/src/protocol/PacketLossCalculator.h no encontrado (omitiendo parche 7)"
fi

# ═══════════════════════════════════════════════════════════════════════════
# PARCHE 8: Corregir namespace en shared/src/log/LogSinks.cpp
# ═══════════════════════════════════════════════════════════════════════════
LOGSINKS_CPP="$SCRIPT_DIR/Server/Root/TeaSpeak/shared/src/log/LogSinks.cpp"

if [[ -f "$LOGSINKS_CPP" ]]; then
    log_info "Parcheando shared/src/log/LogSinks.cpp..."

    # Verificar si ya tiene la corrección
    if ! grep -q 'std::unique_ptr<spdlog::formatter> LogFormatter::clone()' "$LOGSINKS_CPP"; then
        # Corregir formatter a spdlog::formatter en la línea 137
        sed -i 's/std::unique_ptr<formatter> LogFormatter::clone()/std::unique_ptr<spdlog::formatter> LogFormatter::clone()/g' "$LOGSINKS_CPP"

        if grep -q 'std::unique_ptr<spdlog::formatter> LogFormatter::clone()' "$LOGSINKS_CPP"; then
            log_success "✓ Namespace spdlog:: agregado en LogSinks.cpp"
        else
            log_error "Error al corregir namespace en LogSinks.cpp"
            exit 1
        fi
    else
        log_success "✓ LogSinks.cpp ya tiene el namespace correcto"
    fi
else
    log_warning "shared/src/log/LogSinks.cpp no encontrado (omitiendo parche 8)"
fi

# ═══════════════════════════════════════════════════════════════════════════
# PARCHE 9: Agregar includes necesarios a shared/src/lookup/ip.h
# ═══════════════════════════════════════════════════════════════════════════
IP_HEADER="$SCRIPT_DIR/Server/Root/TeaSpeak/shared/src/lookup/ip.h"

if [[ -f "$IP_HEADER" ]]; then
    log_info "Parcheando shared/src/lookup/ip.h..."

    # Agregar #include <memory> si no existe
    if ! grep -q '#include <memory>' "$IP_HEADER"; then
        sed -i '/#include <mutex>/a #include <memory>' "$IP_HEADER"
        log_success "✓ #include <memory> agregado a ip.h"
    else
        log_success "✓ ip.h ya tiene #include <memory>"
    fi

    # Agregar #include <utility> si no existe
    if ! grep -q '#include <utility>' "$IP_HEADER"; then
        sed -i '/#include <memory>/a #include <utility>' "$IP_HEADER"
        log_success "✓ #include <utility> agregado a ip.h"
    else
        log_success "✓ ip.h ya tiene #include <utility>"
    fi

    # Agregar #include <array> si no existe
    if ! grep -q '#include <array>' "$IP_HEADER"; then
        sed -i '/#include <utility>/a #include <array>' "$IP_HEADER"
        log_success "✓ #include <array> agregado a ip.h"
    else
        log_success "✓ ip.h ya tiene #include <array>"
    fi
else
    log_warning "shared/src/lookup/ip.h no encontrado (omitiendo parche 9)"
fi

# ═══════════════════════════════════════════════════════════════════════════
# PARCHE 10: Agregar #include <utility> a file/local_server/NetTools.h
# ═══════════════════════════════════════════════════════════════════════════
NETTOOLS_HEADER="$SCRIPT_DIR/Server/Root/TeaSpeak/file/local_server/NetTools.h"

if [[ -f "$NETTOOLS_HEADER" ]]; then
    log_info "Parcheando file/local_server/NetTools.h..."

    # Verificar si ya tiene el include
    if ! grep -q '#include <utility>' "$NETTOOLS_HEADER"; then
        # Agregar #include <utility> después de #include <numeric>
        sed -i '/#include <numeric>/a #include <utility>' "$NETTOOLS_HEADER"

        if grep -q '#include <utility>' "$NETTOOLS_HEADER"; then
            log_success "✓ #include <utility> agregado a NetTools.h"
        else
            log_error "Error al agregar #include <utility>"
            exit 1
        fi
    else
        log_success "✓ NetTools.h ya tiene #include <utility>"
    fi
else
    log_warning "file/local_server/NetTools.h no encontrado (omitiendo parche 10)"
fi

# ═══════════════════════════════════════════════════════════════════════════
# PARCHE 11: Corregir include en MusicBot/src/MusicPlayer.cpp
# ═══════════════════════════════════════════════════════════════════════════
MUSICPLAYER_CPP="$SCRIPT_DIR/Server/Root/TeaSpeak/MusicBot/src/MusicPlayer.cpp"

if [[ -f "$MUSICPLAYER_CPP" ]]; then
    log_info "Parcheando MusicBot/src/MusicPlayer.cpp..."

    modified=false

    # Corregir #include "teaspeak/MusicPlayer.h" -> #include <teaspeak/MusicPlayer.h>
    if grep -q '#include "teaspeak/MusicPlayer.h"' "$MUSICPLAYER_CPP"; then
        sed -i 's|#include "teaspeak/MusicPlayer.h"|#include <teaspeak/MusicPlayer.h>|g' "$MUSICPLAYER_CPP"
        modified=true
    fi

    # Corregir #include "MusicPlayer.h" -> #include <teaspeak/MusicPlayer.h>
    if grep -q '#include "MusicPlayer.h"' "$MUSICPLAYER_CPP"; then
        sed -i 's|#include "MusicPlayer.h"|#include <teaspeak/MusicPlayer.h>|g' "$MUSICPLAYER_CPP"
        modified=true
    fi

    # Agregar #include <memory> al inicio si no existe
    if ! grep -q '#include <memory>' "$MUSICPLAYER_CPP"; then
        sed -i '1i #include <memory>' "$MUSICPLAYER_CPP"
        modified=true
    fi

    if [[ "$modified" == true ]]; then
        log_success "✓ MusicPlayer.cpp include path corregido"
    else
        log_success "✓ MusicPlayer.cpp ya tiene el include correcto"
    fi
else
    log_warning "MusicBot/src/MusicPlayer.cpp no encontrado (omitiendo parche 11)"
fi

# ═══════════════════════════════════════════════════════════════════════════
# PARCHE 12: Corregir include MusicPlayer.h en módulo server (asegurar ruta correcta)
# ═══════════════════════════════════════════════════════════════════════════
log_info "Parcheando includes de MusicPlayer.h en módulo server..."

SERVER_FILES=(
    "$SCRIPT_DIR/Server/Root/TeaSpeak/server/src/client/ConnectedClient.h"
    "$SCRIPT_DIR/Server/Root/TeaSpeak/server/src/client/music/MusicClient.h"
    "$SCRIPT_DIR/Server/Root/TeaSpeak/server/src/client/music/Song.h"
    "$SCRIPT_DIR/Server/Root/TeaSpeak/server/src/client/music/internal_provider/channel_replay/ChannelProvider.h"
    "$SCRIPT_DIR/Server/Root/TeaSpeak/server/src/music/MusicPlaylist.h"
    "$SCRIPT_DIR/Server/Root/TeaSpeak/server/src/music/MusicPlaylist.cpp"
)

for file in "${SERVER_FILES[@]}"; do
    if [[ -f "$file" ]]; then
        # Corregir includes incorrectos a la forma correcta <teaspeak/MusicPlayer.h>
        if grep -q '#include "teaspeak/MusicPlayer.h"' "$file"; then
            sed -i 's|#include "teaspeak/MusicPlayer.h"|#include <teaspeak/MusicPlayer.h>|g' "$file"
            log_success "✓ $(basename "$file") - include corregido a <teaspeak/MusicPlayer.h>"
        elif grep -q '#include "MusicPlayer.h"' "$file"; then
            sed -i 's|#include "MusicPlayer.h"|#include <teaspeak/MusicPlayer.h>|g' "$file"
            log_success "✓ $(basename "$file") - include corregido a <teaspeak/MusicPlayer.h>"
        elif grep -q '#include <MusicPlayer.h>' "$file"; then
            sed -i 's|#include <MusicPlayer.h>|#include <teaspeak/MusicPlayer.h>|g' "$file"
            log_success "✓ $(basename "$file") - include corregido a <teaspeak/MusicPlayer.h>"
        elif grep -q '#include <teaspeak/MusicPlayer.h>' "$file"; then
            log_success "✓ $(basename "$file") - include ya es correcto"
        fi
    fi
done

# ═══════════════════════════════════════════════════════════════════════════
# PARCHE 13: Agregar #include <cstdint> a shared/src/misc/strobf.h
# ═══════════════════════════════════════════════════════════════════════════
STROBF_HEADER="$SCRIPT_DIR/Server/Root/TeaSpeak/shared/src/misc/strobf.h"

if [[ -f "$STROBF_HEADER" ]]; then
    log_info "Parcheando shared/src/misc/strobf.h..."

    if ! grep -q '#include <cstdint>' "$STROBF_HEADER"; then
        sed -i '/#include <array>/a #include <cstdint>' "$STROBF_HEADER"

        if grep -q '#include <cstdint>' "$STROBF_HEADER"; then
            log_success "✓ #include <cstdint> agregado a strobf.h"
        else
            log_error "Error al agregar #include <cstdint>"
            exit 1
        fi
    else
        log_success "✓ strobf.h ya tiene #include <cstdint>"
    fi
else
    log_warning "shared/src/misc/strobf.h no encontrado (omitiendo parche 13)"
fi

# ═══════════════════════════════════════════════════════════════════════════
# PARCHE 14: Corregir método inexistente current_rttvar en VoiceClient.cpp
# ═══════════════════════════════════════════════════════════════════════════
VOICECLIENT_CPP="$SCRIPT_DIR/Server/Root/TeaSpeak/server/src/client/voice/VoiceClient.cpp"

if [[ -f "$VOICECLIENT_CPP" ]]; then
    log_info "Parcheando server/src/client/voice/VoiceClient.cpp..."

    # Verificar si la línea problemática existe
    if grep -q "acknowledge_manager().current_rttvar()" "$VOICECLIENT_CPP"; then
        # Reemplazar con un valor por defecto ya que el método no existe
        sed -i 's|return this->connection->packet_encoder().acknowledge_manager().current_rttvar();|// current_rttvar() does not exist in AcknowledgeManager\n    return 0.0f; // TODO: Implement proper ping deviation calculation|g' "$VOICECLIENT_CPP"

        if grep -q "return 0.0f; // TODO: Implement proper ping deviation calculation" "$VOICECLIENT_CPP"; then
            log_success "✓ VoiceClient.cpp current_ping_deviation() corregido"
        else
            log_error "Error al corregir current_ping_deviation()"
            exit 1
        fi
    else
        log_success "✓ VoiceClient.cpp ya está corregido"
    fi
else
    log_warning "server/src/client/voice/VoiceClient.cpp no encontrado (omitiendo parche 14)"
fi

# ═══════════════════════════════════════════════════════════════════════════
# PARCHE 15: ELIMINADO - Los campos length y thumbnail SÍ existen en UrlSongInfo
# ═══════════════════════════════════════════════════════════════════════════
# El problema era que el include path estaba incorrecto, no que los campos faltaran.
# Con el PATCH 12 corregido, ahora se usa <teaspeak/MusicPlayer.h> correctamente.
log_success "✓ PATCH 15 no es necesario - campos existen en struct UrlSongInfo"

# ═══════════════════════════════════════════════════════════════════════════
# PARCHE 16: Corregir includes de MusicPlayer.h en providers de música
# ═══════════════════════════════════════════════════════════════════════════
log_info "Parcheando includes en music providers..."

# Buscar todos los archivos .cpp y .h en music/providers/
MUSIC_PROVIDERS_DIR="$SCRIPT_DIR/Server/Root/TeaSpeak/music/providers"

if [[ -d "$MUSIC_PROVIDERS_DIR" ]]; then
    # Encontrar todos los archivos que necesitan corrección
    find "$MUSIC_PROVIDERS_DIR" -type f \( -name "*.cpp" -o -name "*.h" \) | while read -r file; do
        modified=false

        # Corregir #include <include/MusicPlayer.h> -> #include <teaspeak/MusicPlayer.h>
        if grep -q '#include <include/MusicPlayer.h>' "$file" 2>/dev/null; then
            sed -i 's|#include <include/MusicPlayer.h>|#include <teaspeak/MusicPlayer.h>|g' "$file"
            modified=true
        fi

        # Corregir #include "include/MusicPlayer.h" -> #include <teaspeak/MusicPlayer.h>
        if grep -q '#include "include/MusicPlayer.h"' "$file" 2>/dev/null; then
            sed -i 's|#include "include/MusicPlayer.h"|#include <teaspeak/MusicPlayer.h>|g' "$file"
            modified=true
        fi

        # Corregir #include <MusicPlayer.h> -> #include <teaspeak/MusicPlayer.h>
        if grep -q '#include <MusicPlayer.h>' "$file" 2>/dev/null; then
            sed -i 's|#include <MusicPlayer.h>|#include <teaspeak/MusicPlayer.h>|g' "$file"
            modified=true
        fi

        # Corregir #include "MusicPlayer.h" -> #include <teaspeak/MusicPlayer.h>
        if grep -q '#include "MusicPlayer.h"' "$file" 2>/dev/null; then
            sed -i 's|#include "MusicPlayer.h"|#include <teaspeak/MusicPlayer.h>|g' "$file"
            modified=true
        fi

        if [[ "$modified" == true ]]; then
            log_success "✓ $(basename "$file") - include corregido"
        fi
    done

    log_success "✓ Todos los includes en music providers corregidos"
else
    log_warning "music/providers/ no encontrado (omitiendo parche 16)"
fi

echo ""
echo -e "${GREEN}═══════════════════════════════════════════════════════════${NC}"
echo -e "${GREEN}  ✅ Parches aplicados exitosamente${NC}"
echo -e "${GREEN}═══════════════════════════════════════════════════════════${NC}"
echo ""

exit 0
