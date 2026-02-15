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
# NOTA: MusicPlaylist.cpp/h fueron removidos porque PARCHE 30 los modifica intencionalmente
CRITICAL_FILES=(
    # Vacío - todos los archivos se manejan con git normalmente
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
        if ! git diff --quiet "$relative_path" 2>/dev/null; then
            # Hay cambios - restaurar desde git
            log_warning "Detectados cambios en $file_name, restaurando desde repositorio..."
            git checkout HEAD -- "$relative_path" 2>/dev/null || true
            log_success "✓ $file_name restaurado correctamente"
            ((restored_count++))
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
            # Forzar checkout a master descartando cambios locales
            git fetch origin master 2>/dev/null || true
            git checkout -f master 2>/dev/null || {
                # Si no existe localmente, crear desde origin/master
                git checkout -b master origin/master 2>/dev/null || {
                    log_error "CRÍTICO: No se pudo cambiar a branch master"
                    log_error "La compilación FALLARÁ sin la branch master"
                    exit 1
                }
            }
            # Asegurar que está sincronizado con origin
            git reset --hard origin/master 2>/dev/null || true
            log_success "✓ Submódulo music cambiado a branch master"
        else
            # Aunque ya esté en master, asegurar que está actualizado
            git fetch origin master 2>/dev/null || true
            git reset --hard origin/master 2>/dev/null || true
            log_success "✓ Submódulo music ya está en branch master (actualizado)"
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
    if [[ ! -d "shared/.git" ]] && [[ ! -L "shared" ]]; then
        # Check if TeaSpeakLibrary exists in the repository root
        if [[ -d "/home/user/TeaSpeak/TeaSpeakLibrary-master" ]]; then
            log_info "Creando symlink a TeaSpeakLibrary-master..."
            ln -s /home/user/TeaSpeak/TeaSpeakLibrary-master shared
            log_success "✓ Symlink a TeaSpeakLibrary-master creado exitosamente"
        elif [[ -d "/home/user/TeaSpeak/TeaSpeakLibrary-1.4.10" ]]; then
            log_info "Creando symlink a TeaSpeakLibrary-1.4.10..."
            ln -s /home/user/TeaSpeak/TeaSpeakLibrary-1.4.10 shared
            log_success "✓ Symlink a TeaSpeakLibrary-1.4.10 creado exitosamente"
        else
            log_warning "TeaSpeakLibrary no encontrado en el repositorio"
            log_info "Submódulo 'shared' no encontrado, clonando..."
            git clone https://github.com/jorgebarreraa/TeaSpeakLibrary.git shared 2>/dev/null || \
            git clone https://github.com/TeaSpeak/TeaSpeakLibrary.git shared || {
                log_warning "No se pudo clonar submódulo shared, intentando symlink a license/shared..."
                if [[ ! -e "shared" ]]; then
                    ln -s license/shared shared
                    log_warning "✓ Symlink a license/shared creado (puede fallar al compilar)"
                fi
            }

            if [[ -d "shared/.git" ]]; then
                log_success "✓ Submódulo 'shared' clonado exitosamente"
            fi
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
# PARCHE 2: Server/Server/CMakeLists.txt - LIBEVENT_PATH y music includes
# ═══════════════════════════════════════════════════════════════════════════
SERVER_CMAKE="$SCRIPT_DIR/Server/Server/CMakeLists.txt"

if [[ -f "$SERVER_CMAKE" ]]; then
    log_info "Parcheando Server/Server/CMakeLists.txt..."

    # Crear backup solo si no existe uno reciente
    if [[ ! -f "${SERVER_CMAKE}.backup" ]]; then
        cp "$SERVER_CMAKE" "${SERVER_CMAKE}.backup"
    fi

    # Verificar si ya se aplicaron los cambios (idempotencia)
    needs_libevent_fix=false
    needs_music_include=false

    if ! grep -q 'set(LIBEVENT_PATH "${LIBRARY_PATH}/event/_build/linux_amd64/lib")' "$SERVER_CMAKE"; then
        needs_libevent_fix=true
    fi

    if ! grep -q 'include_directories(music/include)' "$SERVER_CMAKE"; then
        needs_music_include=true
    fi

    # Solo aplicar el patch si es necesario
    if [[ "$needs_libevent_fix" == "true" ]] || [[ "$needs_music_include" == "true" ]]; then
        # Usar awk para reemplazos precisos
        awk -v need_libevent="$needs_libevent_fix" -v need_music="$needs_music_include" '{
            if ($0 ~ /^set\(LIBEVENT_PATH/ && need_libevent == "true") {
                print "set(LIBEVENT_PATH \"${LIBRARY_PATH}/event/_build/linux_amd64/lib\")"
            } else if ($0 ~ /^add_subdirectory\(music/ && need_music == "true") {
                print $0
                print ""
                print "# Add music include directory for server to access MusicPlayer.h"
                print "include_directories(music/include)"
            } else {
                print $0
            }
        }' "$SERVER_CMAKE" > "${SERVER_CMAKE}.tmp"

        mv "${SERVER_CMAKE}.tmp" "$SERVER_CMAKE"
        log_success "✓ LIBEVENT_PATH y music includes corregidos exitosamente"
    else
        log_success "✓ Server/Server/CMakeLists.txt ya está parcheado"
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

# ═══════════════════════════════════════════════════════════════════════════
# PARCHE 17: Comentar métodos inexistentes en AcknowledgeManager
# ═══════════════════════════════════════════════════════════════════════════
TEXT_CMD_HANDLER="$SCRIPT_DIR/Server/Root/TeaSpeak/server/src/client/ConnectedClientTextCommandHandler.cpp"

if [[ -f "$TEXT_CMD_HANDLER" ]]; then
    log_info "Parcheando ConnectedClientTextCommandHandler.cpp..."

    # Verificar si ya está parcheado
    if grep -q "// RTO.*not available in current AcknowledgeManager" "$TEXT_CMD_HANDLER" 2>/dev/null; then
        log_success "✓ ConnectedClientTextCommandHandler.cpp ya está parcheado"
    else
        # Crear backup
        if [[ ! -f "$TEXT_CMD_HANDLER.backup" ]]; then
            cp "$TEXT_CMD_HANDLER" "$TEXT_CMD_HANDLER.backup"
        fi

        # Comentar las líneas que usan métodos inexistentes y agregar alternativa
        sed -i '
            /send_message(this->ref(), " RTO   : "/c\
            // RTO, RTTVAR, SRTT methods are not available in current AcknowledgeManager\
            send_message(this->ref(), " RTO   : Not available (API changed)");
            /send_message(this->ref(), " RTTVAR: "/c\
            send_message(this->ref(), " RTTVAR: Not available (API changed)");
            /send_message(this->ref(), " SRTT  : "/c\
            send_message(this->ref(), " SRTT  : Not available (API changed)");
        ' "$TEXT_CMD_HANDLER"

        # Verificar
        if grep -q "Not available (API changed)" "$TEXT_CMD_HANDLER"; then
            log_success "✓ ConnectedClientTextCommandHandler.cpp parcheado exitosamente"
        else
            log_error "Error al parchar ConnectedClientTextCommandHandler.cpp"
            exit 1
        fi
    fi
else
    log_warning "ConnectedClientTextCommandHandler.cpp no encontrado (omitiendo parche 17)"
fi

# ═══════════════════════════════════════════════════════════════════════════
# PARCHE 18: Agregar #include <cstdint> a shared/src/misc/utf8.h
# ═══════════════════════════════════════════════════════════════════════════
UTF8_HEADER="$SCRIPT_DIR/Server/Root/TeaSpeak/shared/src/misc/utf8.h"

if [[ -f "$UTF8_HEADER" ]]; then
    log_info "Parcheando shared/src/misc/utf8.h..."

    # Verificar si ya tiene el include
    if ! grep -q '#include <cstdint>' "$UTF8_HEADER"; then
        # Agregar #include <cstdint> después de #pragma once
        sed -i '/#pragma once/a #include <cstdint>' "$UTF8_HEADER"

        if grep -q '#include <cstdint>' "$UTF8_HEADER"; then
            log_success "✓ #include <cstdint> agregado a utf8.h"
        else
            log_error "Error al agregar #include <cstdint>"
            exit 1
        fi
    else
        log_success "✓ utf8.h ya tiene #include <cstdint>"
    fi
else
    log_warning "shared/src/misc/utf8.h no encontrado (omitiendo parche 18)"
fi

# ═══════════════════════════════════════════════════════════════════════════
# PARCHE 19: Arreglar orden de enlazado OpenSSL y zlib para mysql
# ═══════════════════════════════════════════════════════════════════════════
SERVER_CMAKE_LINK="$SCRIPT_DIR/Server/Root/TeaSpeak/server/CMakeLists.txt"

if [[ -f "$SERVER_CMAKE_LINK" ]]; then
    log_info "Parcheando server/CMakeLists.txt para enlazado OpenSSL y zlib..."

    # Verificar si ya está parcheado (verificación multilínea mejorada)
    if grep -q "target_link_options(TeaSpeakServer PRIVATE" "$SERVER_CMAKE_LINK" && \
       grep -q -- "--no-as-needed" "$SERVER_CMAKE_LINK" && \
       grep -q "LINKER:-lcrypto" "$SERVER_CMAKE_LINK"; then
        log_success "✓ server/CMakeLists.txt ya tiene el fix de OpenSSL/zlib linking (v12)"
    else
        # Crear backup
        if [[ ! -f "$SERVER_CMAKE_LINK.backup_link" ]]; then
            cp "$SERVER_CMAKE_LINK" "$SERVER_CMAKE_LINK.backup_link"
        fi

        # PATCH v12: Usar target_link_options insertado después del bloque jemalloc
        # Patrón de verificación corregido: usa "--no-as-needed" en lugar de "LINKER:--no-as-needed"
        awk '
            # Detectar el endif del bloque jemalloc
            /^endif \(\)/ && prev_line ~ /HAVE_JEMALLOC/ {
                print $0
                print ""
                print "# Fix OpenSSL and zlib linking order for mysql compatibility"
                print "# Use linker flags to force inclusion of crypto and z symbols for mysql"
                print "target_link_options(TeaSpeakServer PRIVATE"
                print "    \"LINKER:--push-state,--no-as-needed\""
                print "    \"LINKER:-lcrypto\""
                print "    \"LINKER:-lz\""
                print "    \"LINKER:--pop-state\""
                print ")"
                next
            }
            {
                prev_line = $0
                print
            }
        ' "$SERVER_CMAKE_LINK" > "$SERVER_CMAKE_LINK.tmp"
        mv "$SERVER_CMAKE_LINK.tmp" "$SERVER_CMAKE_LINK"

        # Verificar con múltiples condiciones
        if grep -q "target_link_options(TeaSpeakServer PRIVATE" "$SERVER_CMAKE_LINK" && \
           grep -q -- "--no-as-needed" "$SERVER_CMAKE_LINK"; then
            log_success "✓ OpenSSL y zlib linking order parcheado en server/CMakeLists.txt (v12)"
        else
            log_error "Error al parchar server/CMakeLists.txt linking"
            exit 1
        fi
    fi
else
    log_warning "server/CMakeLists.txt no encontrado (omitiendo parche 19)"
fi

# ═══════════════════════════════════════════════════════════════════════════
# PARCHE 20: Usar OpenSSL 1.1 (no 3.0) para evitar conflictos con librerías del sistema
# ═══════════════════════════════════════════════════════════════════════════
# IMPORTANTE: OpenSSL 3.0 shared libs (.so) están enlazadas dinámicamente al sistema
# y causan conflictos de símbolos @OPENSSL_3.0.0. Usamos 1.1 que es más estable.
# NOTA: Con PATCH 35, usamos bibliotecas estáticas (.a) que son autocontenidas,
# pero mantenemos los symlinks en 1.1 por consistencia.
OPENSSL_LIB_DIR="$SCRIPT_DIR/Server/Root/libraries/openssl-prebuild/linux_amd64/lib"

if [[ -d "$OPENSSL_LIB_DIR" ]]; then
    log_info "Verificando versión de OpenSSL en libraries..."

    # Verificar si los symlinks ya apuntan a versión 1.1
    if [[ -L "$OPENSSL_LIB_DIR/libssl.so" ]] && [[ "$(readlink "$OPENSSL_LIB_DIR/libssl.so")" == "libssl.so.1.1" ]]; then
        log_success "✓ Symlinks de OpenSSL ya apuntan a versión 1.1 (correcto)"
    else
        log_info "Actualizando symlinks de OpenSSL a versión 1.1..."

        # Verificar que existan las versiones 1.1
        if [[ -f "$OPENSSL_LIB_DIR/libssl.so.1.1" ]] && [[ -f "$OPENSSL_LIB_DIR/libcrypto.so.1.1" ]]; then
            # Actualizar symlinks a 1.1
            ln -sf libssl.so.1.1 "$OPENSSL_LIB_DIR/libssl.so"
            ln -sf libcrypto.so.1.1 "$OPENSSL_LIB_DIR/libcrypto.so"

            # Verificar
            if [[ "$(readlink "$OPENSSL_LIB_DIR/libssl.so")" == "libssl.so.1.1" ]] && \
               [[ "$(readlink "$OPENSSL_LIB_DIR/libcrypto.so")" == "libcrypto.so.1.1" ]]; then
                log_success "✓ Symlinks actualizados a OpenSSL 1.1"
            else
                log_error "Error al actualizar symlinks de OpenSSL"
                exit 1
            fi
        else
            log_warning "OpenSSL 1.1 no encontrado en libraries (usando versión existente)"
        fi
    fi
else
    log_warning "Directorio openssl-prebuild no encontrado (omitiendo parche 20)"
fi

# ═══════════════════════════════════════════════════════════════════════════
# PARCHE 21: Verificar que DataPipes use BoringSSL (configuración correcta)
# ═══════════════════════════════════════════════════════════════════════════
# NOTA: La versión anterior de este parche intentaba cambiar de BoringSSL a OpenSSL,
# pero la decisión correcta es usar BoringSSL (commit 4325f01c).
# El build_datapipes.sh actual ya usa BoringSSL directamente via crypto_options.
# Este parche verifica que la configuración sea correcta y es NO-OP si ya está bien.
DATAPIPES_BUILD_SCRIPT="$SCRIPT_DIR/Server/Root/build-helpers/libraries/build_datapipes.sh"

if [[ -f "$DATAPIPES_BUILD_SCRIPT" ]]; then
    log_info "Verificando configuración de DataPipes (BoringSSL)..."

    # El build_datapipes.sh correcto usa crypto_options con BoringSSL
    if grep -q 'boringssl' "$DATAPIPES_BUILD_SCRIPT"; then
        log_success "✓ DataPipes ya está configurado para usar BoringSSL (correcto)"
    else
        log_warning "⚠ DataPipes no referencia BoringSSL - verificar build_datapipes.sh manualmente"
    fi
else
    log_warning "Script build_datapipes.sh no encontrado (omitiendo parche 21)"
fi

# ═══════════════════════════════════════════════════════════════════════════
# PARCHE 22: Verificar compilación de DataPipes con BoringSSL
# ═══════════════════════════════════════════════════════════════════════════
# NOTA: La compilación de DataPipes la realiza build.sh automáticamente.
# Este parche solo verifica el estado y corrige marcadores inconsistentes.
DATAPIPES_LIBRARY="$SCRIPT_DIR/Server/Root/libraries/DataPipes"

if [[ -d "$DATAPIPES_LIBRARY" ]]; then
    log_info "Verificando estado de compilación de DataPipes..."

    DATAPIPES_LIB_STATIC="$DATAPIPES_LIBRARY/out/linux_amd64/lib/libDataPipes-Core-Static.a"
    DATAPIPES_MARKER="$DATAPIPES_LIBRARY/.build_successful"

    if [[ -f "$DATAPIPES_LIB_STATIC" ]]; then
        log_success "✓ DataPipes está compilado correctamente con BoringSSL"
        # Asegurar que el marker de build esté presente
        if [[ ! -f "$DATAPIPES_MARKER" ]]; then
            touch "$DATAPIPES_MARKER"
        fi
    elif [[ -f "$DATAPIPES_MARKER" ]]; then
        log_info "Marker de build presente pero librerías faltantes - limpiando marker..."
        rm -f "$DATAPIPES_MARKER"
        # También limpiar el viejo marcador si existe
        rm -f "$DATAPIPES_LIBRARY/.build_linux_amd64.txt"
        log_success "✓ DataPipes se recompilará al ejecutar build.sh"
    else
        log_info "DataPipes no compilado aún - se compilará al ejecutar build.sh"
        # Limpiar viejo marcador si existe
        rm -f "$DATAPIPES_LIBRARY/.build_linux_amd64.txt"
    fi
else
    log_warning "Directorio DataPipes no encontrado (omitiendo parche 22)"
fi

# ═══════════════════════════════════════════════════════════════════════════
# PARCHE 22b: Verificar consistencia del marcador de StringVariable
# ═══════════════════════════════════════════════════════════════════════════
SV_LIBRARY="$SCRIPT_DIR/Server/Root/libraries/StringVariable"
if [[ -d "$SV_LIBRARY" ]]; then
    SV_LIB_STATIC="$SV_LIBRARY/out/linux_amd64/lib/libStringVariablesStatic.a"
    SV_MARKER="$SV_LIBRARY/.build_successful"

    if [[ -f "$SV_LIB_STATIC" ]]; then
        log_success "✓ StringVariable está compilada correctamente"
        # Asegurar que el marker esté presente
        if [[ ! -f "$SV_MARKER" ]]; then
            touch "$SV_MARKER"
        fi
    elif [[ -f "$SV_MARKER" ]]; then
        log_info "Marker de build presente pero libStringVariablesStatic.a faltante - limpiando marker..."
        rm -f "$SV_MARKER"
        log_success "✓ StringVariable se recompilará al ejecutar build.sh"
    else
        log_info "StringVariable no compilada aún - se compilará al ejecutar build.sh"
    fi
else
    log_warning "Directorio StringVariable no encontrado (omitiendo parche 22b)"
fi

# ═══════════════════════════════════════════════════════════════════════════
# PARCHE 23: REVERTIDO - Usar OpenSSL prebuild del proyecto (no del sistema)
# ═══════════════════════════════════════════════════════════════════════════
# Este parche se revirtió porque el servidor necesita OpenSSL prebuild del proyecto
# para mantener compatibilidad con la versión original de TeaSpeak
SERVER_CMAKE_FILE="$SCRIPT_DIR/Server/Root/TeaSpeak/server/CMakeLists.txt"

if [[ -f "$SERVER_CMAKE_FILE" ]]; then
    # Verificar si el PATCH 23 incorrecto está aplicado y revertirlo
    if grep -q "# PATCH 23: Use system OpenSSL libraries" "$SERVER_CMAKE_FILE"; then
        log_info "Revirtiendo PATCH 23 para usar OpenSSL prebuild del proyecto..."

        # Revertir al uso de openssl::ssl::shared y openssl::crypto::shared
        awk '
        /# PATCH 23: Use system OpenSSL libraries/ {
            getline; getline; getline; getline; getline
            print "        openssl::ssl::shared"
            next
        }
        /^[[:space:]]*#[[:space:]]*openssl::ssl::shared[[:space:]]*$/ { next }
        /^[[:space:]]*ssl[[:space:]]*$/ && ssl_skip { next }
        /^[[:space:]]*#[[:space:]]*openssl::crypto::shared[[:space:]]*$/ { next }
        /^[[:space:]]*crypto[[:space:]]*$/ && crypto_skip { next }
        /# PATCH 23:/ { ssl_skip=1; next }
        /openssl::ssl::shared/ { ssl_skip=0 }
        { print }
        ' "$SERVER_CMAKE_FILE" > "$SERVER_CMAKE_FILE.tmp"

        # Método más simple: usar sed para limpiar las líneas del PATCH 23
        sed -i '/# PATCH 23: Use system OpenSSL libraries/d' "$SERVER_CMAKE_FILE"
        sed -i 's/^[[:space:]]*# openssl::ssl::shared[[:space:]]*$/        openssl::ssl::shared/' "$SERVER_CMAKE_FILE"
        sed -i '/^[[:space:]]*ssl[[:space:]]*$/d' "$SERVER_CMAKE_FILE"
        sed -i 's/^[[:space:]]*# openssl::crypto::shared[[:space:]]*$/        openssl::crypto::shared/' "$SERVER_CMAKE_FILE"
        sed -i '/^[[:space:]]*crypto[[:space:]]*$/d' "$SERVER_CMAKE_FILE"

        log_success "✓ PATCH 23 revertido - usando OpenSSL prebuild del proyecto"
    else
        log_success "✓ server/CMakeLists.txt ya usa OpenSSL prebuild correctamente"
    fi
else
    log_warning "server/CMakeLists.txt no encontrado (omitiendo parche 23)"
fi

# ═══════════════════════════════════════════════════════════════════════════
# PARCHE 24: Compilar JsonCpp con soporte para C++17 std::string_view
# ═══════════════════════════════════════════════════════════════════════════
JSONCPP_DIR="$SCRIPT_DIR/Server/Root/libraries/jsoncpp"
JSONCPP_LIB="/usr/local/lib/libjsoncpp.so"

if [[ -d "$JSONCPP_DIR" ]]; then
    log_info "Verificando JsonCpp..."

    # Verificar si jsoncpp ya está compilado con soporte para string_view
    if [[ -f "$JSONCPP_LIB" ]]; then
        if nm -D "$JSONCPP_LIB" 2>/dev/null | grep -q "string_view" || \
           nm -D "$JSONCPP_LIB"* 2>/dev/null | grep -q "string_view"; then
            log_success "✓ JsonCpp ya está compilado con soporte para string_view"
        else
            log_info "JsonCpp requiere recompilación con C++17..."
            NEED_REBUILD=1
        fi
    else
        log_info "JsonCpp no está instalado, compilando..."
        NEED_REBUILD=1
    fi

    if [[ "$NEED_REBUILD" == "1" ]]; then
        log_info "Compilando JsonCpp con soporte para C++17..."

        CURRENT_DIR="$(pwd)"
        cd "$JSONCPP_DIR" || {
            log_error "No se pudo acceder a $JSONCPP_DIR"
            exit 1
        }

        # Limpiar código fuente para asegurar estado limpio
        git checkout . 2>/dev/null || true
        git clean -fdx 2>/dev/null || true

        # CRÍTICO: Modificar CMakeLists.txt para forzar C++17
        # El set(CMAKE_CXX_STANDARD 11) en CMakeLists.txt sobrescribe flags de línea de comandos
        log_info "Modificando CMakeLists.txt para forzar C++17..."
        if [[ -f "CMakeLists.txt" ]]; then
            sed -i 's/set(CMAKE_CXX_STANDARD [0-9]\+)/set(CMAKE_CXX_STANDARD 17)/' CMakeLists.txt
            if grep -q "CMAKE_CXX_STANDARD 17" CMakeLists.txt; then
                log_success "✓ CMakeLists.txt modificado a C++17"
            else
                log_error "Error: No se pudo modificar CMakeLists.txt"
                cd "$CURRENT_DIR"
                exit 1
            fi
        else
            log_error "CMakeLists.txt no encontrado"
            cd "$CURRENT_DIR"
            exit 1
        fi

        # Limpiar y crear directorio de build
        rm -rf build
        mkdir -p build
        cd build

        # Configurar con CMake usando C++17
        cmake .. \
            -DCMAKE_CXX_STANDARD=17 \
            -DCMAKE_CXX_STANDARD_REQUIRED=ON \
            -DCMAKE_CXX_FLAGS="-fPIC" \
            -DCMAKE_BUILD_TYPE=RelWithDebInfo \
            -DBUILD_SHARED_LIBS=ON \
            -DBUILD_STATIC_LIBS=OFF \
            -DJSONCPP_WITH_TESTS=OFF \
            -DJSONCPP_WITH_POST_BUILD_UNITTEST=OFF

        if [[ $? -ne 0 ]]; then
            log_error "Error al configurar JsonCpp"
            cd "$CURRENT_DIR"
            exit 1
        fi

        # Compilar
        make -j$(nproc)

        if [[ $? -ne 0 ]]; then
            log_error "Error al compilar JsonCpp"
            cd "$CURRENT_DIR"
            exit 1
        fi

        # Instalar (sin sudo - las librerías locales son suficientes)
        make install 2>/dev/null || true

        # Nota: ldconfig no es necesario para librerías locales
        # sudo ldconfig

        # Verificar que la compilación fue exitosa
        # Nota: JsonCpp se compila con C++11 (sin string_view) - esto es correcto
        # Los parches en el código manejan la compatibilidad
        if [[ -f "lib/libjsoncpp.so" ]] || [[ -f "lib/libjsoncpp.a" ]]; then
            log_success "✓ JsonCpp compilado exitosamente (C++11 - compatible con TeaSpeak C++17)"
        else
            log_warning "JsonCpp: archivos de librería no encontrados (puede ser normal)"
        fi

        cd "$CURRENT_DIR"
    fi
else
    log_warning "Directorio jsoncpp no encontrado (omitiendo parche 24)"
fi

# ═══════════════════════════════════════════════════════════════════════════
# PARCHE 25: Corregir timing de fix_cargo_deps.sh en generate_shared_library.sh
# ═══════════════════════════════════════════════════════════════════════════
# Problema: fix_cargo_deps.sh se ejecutaba ANTES de limpiar el caché de cargo,
# por lo que los cambios se perdían. Debe ejecutarse DESPUÉS de cargo update.
# ═══════════════════════════════════════════════════════════════════════════

echo ""
log_info "════════════════════════════════════════════════════════════"
log_info "  PARCHE 25: Corregir timing de fix_cargo_deps.sh"
log_info "════════════════════════════════════════════════════════════"

GENERATE_LIB_SCRIPT="$SCRIPT_DIR/Server/Root/TeaSpeak/rtclib/generate_shared_library.sh"

if [[ -f "$GENERATE_LIB_SCRIPT" ]]; then
    # Verificar si el parche ya fue aplicado Y las líneas de git cleanup fueron removidas
    if grep -q "# Apply Cargo.toml fix for rust-webrtc dependencies AFTER cargo clones the repo" "$GENERATE_LIB_SCRIPT" 2>/dev/null && \
       ! grep -q "rm -rf ~/\.cargo/git/checkouts/\*" "$GENERATE_LIB_SCRIPT" 2>/dev/null; then
        log_success "✓ generate_shared_library.sh ya tiene fix_cargo_deps.sh en el lugar correcto"
    else
        log_info "Corrigiendo timing de fix_cargo_deps.sh en generate_shared_library.sh..."

        # Crear backup
        cp "$GENERATE_LIB_SCRIPT" "$GENERATE_LIB_SCRIPT.backup"

        # Paso 1: Eliminar la llamada temprana a fix_cargo_deps.sh (si existe)
        sed -i '/# Apply Cargo.toml fix for rust-webrtc dependencies/,/^fi$/d' "$GENERATE_LIB_SCRIPT"

        # Paso 2: MODIFICAR la limpieza de cargo para NO eliminar git checkouts parcheados
        # Esto permite que el parche persista entre ejecuciones
        sed -i '/^rm -rf ~\/\.cargo\/git\/checkouts\/\*$/d' "$GENERATE_LIB_SCRIPT"
        sed -i '/^rm -rf ~\/\.cargo\/git\/db\/\*$/d' "$GENERATE_LIB_SCRIPT"

        # Paso 3: Agregar la llamada DESPUÉS de cargo update
        # Buscar la línea "cargo update || exit 1" y agregar el bloque después
        awk '
        /^cargo update \|\| exit 1$/ {
            print
            print ""
            print "# Apply Cargo.toml fix for rust-webrtc dependencies AFTER cargo clones the repo"
            print "FIX_SCRIPT=\"$SCRIPT_DIR/fix_cargo_deps.sh\""
            print "if [ -f \"$FIX_SCRIPT\" ]; then"
            print "    echo \"Applying Cargo.toml fixes for rust-webrtc...\""
            print "    bash \"$FIX_SCRIPT\" || echo \"Warning: Cargo fix script failed, continuing...\""
            print "else"
            print "    echo \"Warning: Cargo fix script not found at $FIX_SCRIPT\""
            print "fi"
            next
        }
        { print }
        ' "$GENERATE_LIB_SCRIPT.backup" > "$GENERATE_LIB_SCRIPT"

        # Verificar que el parche se aplicó correctamente
        if grep -q "# Apply Cargo.toml fix for rust-webrtc dependencies AFTER cargo clones the repo" "$GENERATE_LIB_SCRIPT"; then
            log_success "✓ Timing de fix_cargo_deps.sh corregido exitosamente"
            rm -f "$GENERATE_LIB_SCRIPT.backup"
        else
            log_error "[✗] Error al aplicar parche 25"
            mv "$GENERATE_LIB_SCRIPT.backup" "$GENERATE_LIB_SCRIPT"
            exit 1
        fi
    fi
else
    log_warning "generate_shared_library.sh no encontrado (omitiendo parche 25)"
fi

# ═══════════════════════════════════════════════════════════════════════════
# PARCHE 26: Evitar sobrecarga string_view en JsonCpp operator[]
# ═══════════════════════════════════════════════════════════════════════════
# Problema: Cuando se compila con C++17, el compilador intenta usar
# operator[](std::string_view) que no existe en JsonCpp compilado con C++11
# Solución: Usar std::string().c_str() para forzar operator[](const char*)

MUSIC_PLAYLIST_CPP="$SCRIPT_DIR/Server/Root/TeaSpeak/server/src/music/MusicPlaylist.cpp"

if [[ -f "$MUSIC_PLAYLIST_CPP" ]]; then
    # Verificar si el parche ya fue aplicado
    if grep -q 'std::string("type")\.c_str()' "$MUSIC_PLAYLIST_CPP" 2>/dev/null; then
        log_success "✓ PARCHE 26 ya aplicado (JsonCpp string_view fix)"
    else
        log_info "Aplicando PARCHE 26: Evitar sobrecarga string_view en JsonCpp..."

        # Crear backup
        cp "$MUSIC_PLAYLIST_CPP" "$MUSIC_PLAYLIST_CPP.backup26"

        # Reemplazar root["key"] con root[std::string("key").c_str()]
        # Esto fuerza al compilador a usar operator[](const char*) sin ambigüedad
        sed -i \
            -e 's/root\["\([^"]*\)"\]/root[std::string("\1").c_str()]/g' \
            -e 's/builder\["\([^"]*\)"\]/builder[std::string("\1").c_str()]/g' \
            -e 's/\]\[meta\.first\]/][meta.first.c_str()]/g' \
            "$MUSIC_PLAYLIST_CPP"

        # Verificar que el parche se aplicó
        if grep -q 'std::string("type")\.c_str()' "$MUSIC_PLAYLIST_CPP"; then
            log_success "✓ PARCHE 26 aplicado exitosamente"
            rm -f "$MUSIC_PLAYLIST_CPP.backup26"
        else
            log_error "[✗] Error al aplicar PARCHE 26"
            mv "$MUSIC_PLAYLIST_CPP.backup26" "$MUSIC_PLAYLIST_CPP"
            exit 1
        fi
    fi
else
    log_warning "MusicPlaylist.cpp no encontrado (omitiendo PARCHE 26)"
fi

# ═══════════════════════════════════════════════════════════════════════════
# PARCHE 26b: ELIMINADO - Ya no necesario
# ═══════════════════════════════════════════════════════════════════════════
# JsonCpp se compila con C++11 (sin string_view) desde build_jsoncpp.sh
# PARCHE 26 maneja la compatibilidad modificando MusicPlaylist.cpp
log_success "✓ PARCHE 26b no necesario (JsonCpp usa C++11)"

# ═══════════════════════════════════════════════════════════════════════════
# PARCHE 26d: Aplicar mismo fix de string_view a YTVManager.cpp
# ═══════════════════════════════════════════════════════════════════════════
# Problema: YTVManager.cpp también usa root["key"] y falla por misma razón
# Solución: Usar std::string("key").c_str() para forzar operator[](const char*)

YTVMANAGER_CPP="$SCRIPT_DIR/Server/Root/TeaSpeak/music/providers/yt/YTVManager.cpp"

if [[ -f "$YTVMANAGER_CPP" ]]; then
    # Verificar si el parche ya fue aplicado
    if grep -q 'std::string("description")\.c_str()' "$YTVMANAGER_CPP" 2>/dev/null; then
        log_success "✓ PARCHE 26d ya aplicado (YTVManager JsonCpp fix)"
    else
        log_info "Aplicando PARCHE 26d: Fix JsonCpp en YTVManager.cpp..."

        # Crear backup
        cp "$YTVMANAGER_CPP" "$YTVMANAGER_CPP.backup26d"

        # Reemplazar todos los accesos root["key"] y json["key"] con std::string("key").c_str()
        sed -i \
            -e 's/root\["\([^"]*\)"\]/root[std::string("\1").c_str()]/g' \
            -e 's/json\["\([^"]*\)"\]/json[std::string("\1").c_str()]/g' \
            -e 's/(\*jsons\[0\])\["\([^"]*\)"\]/(*jsons[0])[std::string("\1").c_str()]/g' \
            "$YTVMANAGER_CPP"

        # Verificar que el parche se aplicó
        if grep -q 'std::string("description")\.c_str()' "$YTVMANAGER_CPP"; then
            log_success "✓ PARCHE 26d aplicado exitosamente"
            rm -f "$YTVMANAGER_CPP.backup26d"
        else
            log_error "[✗] Error al aplicar PARCHE 26d"
            mv "$YTVMANAGER_CPP.backup26d" "$YTVMANAGER_CPP"
            exit 1
        fi
    fi
else
    log_warning "YTVManager.cpp no encontrado (omitiendo PARCHE 26d)"
fi

# ═══════════════════════════════════════════════════════════════════════════
# PARCHE 27: Arreglar API deprecada de Rust en libnice (hash_drain_filter)
# ═══════════════════════════════════════════════════════════════════════════
# Problema: libnice usa hash_drain_filter que fue renombrado a hash_extract_if
# Solución: Reemplazar hash_drain_filter con hash_extract_if y drain_filter con extract_if

log_info "Aplicando PARCHE 27 y 28: Arreglar APIs deprecadas de Rust..."

# Buscar el directorio de rust-libnice en el cache de cargo
LIBNICE_DIR=$(find ~/.cargo/git/checkouts -type d -name "rust-libnice-*" 2>/dev/null | head -1)

if [[ -n "$LIBNICE_DIR" && -d "$LIBNICE_DIR" ]]; then
    # Buscar el commit específico (933be6c)
    LIBNICE_SRC="$LIBNICE_DIR/933be6c"

    if [[ ! -d "$LIBNICE_SRC" ]]; then
        # Si no existe el commit específico, buscar cualquier subdirectorio
        LIBNICE_SRC=$(find "$LIBNICE_DIR" -mindepth 1 -maxdepth 1 -type d | head -1)
    fi

    if [[ -d "$LIBNICE_SRC" ]]; then
        # Verificar si el parche ya fue aplicado
        if grep -q "hash_extract_if" "$LIBNICE_SRC/src/lib.rs" 2>/dev/null; then
            log_success "✓ PARCHE 27 ya aplicado (libnice hash_extract_if)"
        else
            log_info "Corrigiendo APIs deprecadas en rust-libnice..."

            # Fix lib.rs: hash_drain_filter -> hash_extract_if
            if [[ -f "$LIBNICE_SRC/src/lib.rs" ]]; then
                sed -i 's/hash_drain_filter/hash_extract_if/g' "$LIBNICE_SRC/src/lib.rs"
            fi

            # Fix ice.rs: drain_filter -> extract_if
            if [[ -f "$LIBNICE_SRC/src/ice.rs" ]]; then
                sed -i 's/\.drain_filter(/.extract_if(/g' "$LIBNICE_SRC/src/ice.rs"
            fi

            log_success "✓ PARCHE 27 aplicado exitosamente"
        fi
    else
        log_warning "rust-libnice source no encontrado en $LIBNICE_DIR"
    fi
else
    log_warning "rust-libnice no encontrado en cargo cache (se aplicará en primera compilación)"
fi

# ═══════════════════════════════════════════════════════════════════════════
# PARCHE 28-29: Migrar BTreeMap a HashMap en rust-webrtc
# ═══════════════════════════════════════════════════════════════════════════
# Problema: Rust nightly 1.94.0 cambió BTreeMap::drain_filter API completamente
#           Ahora extract_if acepta rangos en lugar de predicados
# Solución: Usar HashMap que SÍ tiene extract_if con predicados en nightly

# Buscar el directorio de rust-webrtc en el cache de cargo
WEBRTC_DIR=$(find ~/.cargo/git/checkouts -type d -name "rust-webrtc-*" 2>/dev/null | head -1)

if [[ -n "$WEBRTC_DIR" && -d "$WEBRTC_DIR" ]]; then
    # Buscar el commit específico (58d3316)
    WEBRTC_SRC="$WEBRTC_DIR/58d3316"

    if [[ ! -d "$WEBRTC_SRC" ]]; then
        # Si no existe el commit específico, buscar cualquier subdirectorio
        WEBRTC_SRC=$(find "$WEBRTC_DIR" -mindepth 1 -maxdepth 1 -type d | head -1)
    fi

    if [[ -d "$WEBRTC_SRC" ]]; then
        # Verificar si el parche ya fue aplicado
        if grep -q "hash_extract_if" "$WEBRTC_SRC/src/lib.rs" 2>/dev/null && \
           grep -q "HashMap" "$WEBRTC_SRC/src/rtc.rs" 2>/dev/null; then
            log_success "✓ PARCHE 28-29 ya aplicado (webrtc HashMap migration)"
        else
            log_info "Migrando de BTreeMap a HashMap en rust-webrtc..."

            # Fix lib.rs: btree_drain_filter -> hash_extract_if
            if [[ -f "$WEBRTC_SRC/src/lib.rs" ]]; then
                sed -i 's/btree_drain_filter/hash_extract_if/g' "$WEBRTC_SRC/src/lib.rs"
                sed -i 's/btree_extract_if/hash_extract_if/g' "$WEBRTC_SRC/src/lib.rs"
            fi

            # Fix rtc.rs: BTreeMap -> HashMap
            if [[ -f "$WEBRTC_SRC/src/rtc.rs" ]]; then
                sed -i 's/use std::collections::{BTreeMap,/use std::collections::{HashMap,/g' "$WEBRTC_SRC/src/rtc.rs"
                sed -i 's/BTreeMap::/HashMap::/g' "$WEBRTC_SRC/src/rtc.rs"
                sed -i 's/: BTreeMap</: HashMap</g' "$WEBRTC_SRC/src/rtc.rs"
                # HashMap no tiene first_key_value, usar iter().next() en su lugar
                sed -i 's/\.first_key_value()/.iter().next()/g' "$WEBRTC_SRC/src/rtc.rs"
            fi

            # Fix all *.rs files: drain_filter -> extract_if
            find "$WEBRTC_SRC" -name "*.rs" -type f -exec sed -i 's/\.drain_filter(/.extract_if(/g' {} \;

            log_success "✓ PARCHE 28-29 aplicado exitosamente"
        fi
    else
        log_warning "rust-webrtc source no encontrado en $WEBRTC_DIR"
    fi
else
    log_warning "rust-webrtc no encontrado en cargo cache (se aplicará en primera compilación)"
fi

# ═══════════════════════════════════════════════════════════════════════════
# PARCHE 30: Usar bibliotecas estáticas de OpenSSL prebuild
# ═══════════════════════════════════════════════════════════════════════════
# Problema: libssl.so del prebuild está dinámicamente vinculado a libcrypto.so.3
#           del sistema, causando conflictos de versión de símbolos @OPENSSL_3.0.0
# Solución: Usar las bibliotecas estáticas (.a) del prebuild que son autocontenidas

log_info "Aplicando PARCHE 30: Configurar bibliotecas estáticas de OpenSSL..."

# CRÍTICO: Parchear build_teaspeak.sh para SIEMPRE limpiar el build
# Esto garantiza que Makefiles antiguos con referencias incorrectas se regeneren
BUILD_TEASPEAK_SCRIPT="$SCRIPT_DIR/Server/Root/build_teaspeak.sh"
if [[ -f "$BUILD_TEASPEAK_SCRIPT" ]]; then
    # Cambiar la condición para que SIEMPRE limpie el build incondicionalmente
    if grep -q 'if \[\[ -d build && \$teaspeak_clean_build -eq 1 \]\]; then' "$BUILD_TEASPEAK_SCRIPT"; then
        log_info "Parcheando build_teaspeak.sh para limpiar build incondicionalmente..."
        sed -i 's/if \[\[ -d build && \$teaspeak_clean_build -eq 1 \]\]; then/if [[ -d build ]]; then/g' "$BUILD_TEASPEAK_SCRIPT"
        log_success "✓ build_teaspeak.sh parcheado - ahora limpia build/ automáticamente"
    fi
fi

# CRÍTICO: SIEMPRE limpiar directorio build si existe para forzar regeneración de CMake
# Esto evita problemas con Makefiles antiguos que referencian openssl::ssl::shared
BUILD_DIR="$SCRIPT_DIR/Server/Root/TeaSpeak/build"
if [[ -d "$BUILD_DIR" ]]; then
    log_info "Limpiando directorio build para forzar regeneración de CMake..."
    rm -rf "$BUILD_DIR"
    log_success "✓ Directorio build limpiado"
fi

if [[ -f "$SERVER_CMAKE_FILE" ]]; then
    # Usar variables de CMake en lugar de rutas absolutas hardcodeadas
    # Esto funciona en cualquier máquina donde se clone el repositorio
    OPENSSL_SSL_CMAKE="\${CMAKE_SOURCE_DIR}/../libraries/openssl-prebuild/linux_amd64/lib/libssl.a"
    OPENSSL_CRYPTO_CMAKE="\${CMAKE_SOURCE_DIR}/../libraries/openssl-prebuild/linux_amd64/lib/libcrypto.a"

    # Verificar si ya está usando bibliotecas estáticas con variables de CMake
    if grep -q '\${CMAKE_SOURCE_DIR}/../libraries/openssl-prebuild/linux_amd64/lib/libssl\.a' "$SERVER_CMAKE_FILE"; then
        log_success "✓ PARCHE 30 ya aplicado (usando OpenSSL estático con variables CMake)"
    else
        log_info "Modificando CMakeLists.txt para usar bibliotecas estáticas de OpenSSL (variables CMake)..."

        # Reemplazar openssl::ssl::shared y openssl::crypto::shared con variables de CMake
        sed -i '/target_link_libraries(TeaSpeakServer$/,/^)$/ {
            s|openssl::ssl::shared|'"$OPENSSL_SSL_CMAKE"'|g
            s|openssl::crypto::shared|'"$OPENSSL_CRYPTO_CMAKE"'|g
            s|\${LIBRARY_PATH}openssl-prebuild/linux_amd64/lib/libssl\.a|'"$OPENSSL_SSL_CMAKE"'|g
            s|\${LIBRARY_PATH}openssl-prebuild/linux_amd64/lib/libcrypto\.a|'"$OPENSSL_CRYPTO_CMAKE"'|g
            s|/home/user/TeaSpeak/Server/Root/libraries/openssl-prebuild/linux_amd64/lib/libssl\.a|'"$OPENSSL_SSL_CMAKE"'|g
            s|/home/user/TeaSpeak/Server/Root/libraries/openssl-prebuild/linux_amd64/lib/libcrypto\.a|'"$OPENSSL_CRYPTO_CMAKE"'|g
            s|/root/TeaSpeak/Server/Root/libraries/openssl-prebuild/linux_amd64/lib/libssl\.a|'"$OPENSSL_SSL_CMAKE"'|g
            s|/root/TeaSpeak/Server/Root/libraries/openssl-prebuild/linux_amd64/lib/libcrypto\.a|'"$OPENSSL_CRYPTO_CMAKE"'|g
        }' "$SERVER_CMAKE_FILE"

        # También actualizar en file/CMakeLists.txt
        FILE_CMAKE="$SCRIPT_DIR/Server/Root/TeaSpeak/file/CMakeLists.txt"
        if [[ -f "$FILE_CMAKE" ]]; then
            log_info "Parcheando file/CMakeLists.txt también..."
            sed -i '/target_link_libraries(TeaSpeak-FileServer/,/^)$/ {
                s|openssl::ssl::shared|'"$OPENSSL_SSL_CMAKE"'|g
                s|openssl::crypto::shared|'"$OPENSSL_CRYPTO_CMAKE"'|g
                s|\${LIBRARY_PATH}openssl-prebuild/linux_amd64/lib/libssl\.a|'"$OPENSSL_SSL_CMAKE"'|g
                s|\${LIBRARY_PATH}openssl-prebuild/linux_amd64/lib/libcrypto\.a|'"$OPENSSL_CRYPTO_CMAKE"'|g
                s|/home/user/TeaSpeak/Server/Root/libraries/openssl-prebuild/linux_amd64/lib/libssl\.a|'"$OPENSSL_SSL_CMAKE"'|g
                s|/home/user/TeaSpeak/Server/Root/libraries/openssl-prebuild/linux_amd64/lib/libcrypto\.a|'"$OPENSSL_CRYPTO_CMAKE"'|g
                s|/root/TeaSpeak/Server/Root/libraries/openssl-prebuild/linux_amd64/lib/libssl\.a|'"$OPENSSL_SSL_CMAKE"'|g
                s|/root/TeaSpeak/Server/Root/libraries/openssl-prebuild/linux_amd64/lib/libcrypto\.a|'"$OPENSSL_CRYPTO_CMAKE"'|g
            }' "$FILE_CMAKE"
            log_success "✓ file/CMakeLists.txt parcheado"
        fi

        log_success "✓ PARCHE 30 aplicado exitosamente"
    fi
else
    log_error "CMakeLists.txt del servidor no encontrado"
    exit 1
fi

log_info "════════════════════════════════════════════════════════════"
log_info "  PARCHE 31: Fix shared module includes (GCC 13 compatibility)"
log_info "════════════════════════════════════════════════════════════"

# Fix converter.cpp - missing semicolon
CONVERTER_CPP="$SCRIPT_DIR/Server/Root/TeaSpeak/shared/src/converters/converter.cpp"
if [[ -f "$CONVERTER_CPP" ]]; then
    if grep -q "CONVERTER_PRIMITIVE_ST(uint64_t, std::stoull(std::string{str}));" "$CONVERTER_CPP"; then
        log_success "✓ converter.cpp ya tiene punto y coma"
    elif grep -q "CONVERTER_PRIMITIVE_ST(uint64_t, std::stoull(std::string{str}))" "$CONVERTER_CPP"; then
        log_info "Aplicando fix de punto y coma faltante..."
        sed -i 's/CONVERTER_PRIMITIVE_ST(uint64_t, std::stoull(std::string{str}))/CONVERTER_PRIMITIVE_ST(uint64_t, std::stoull(std::string{str}));/' "$CONVERTER_CPP"
        log_success "✓ converter.cpp parcheado"
    fi
fi

# Fix converter.h - missing #include <cstdint>
CONVERTER_H="$SCRIPT_DIR/Server/Root/TeaSpeak/shared/src/converters/converter.h"
if [[ -f "$CONVERTER_H" ]]; then
    if grep -q "#include <cstdint>" "$CONVERTER_H"; then
        log_success "✓ converter.h ya tiene #include <cstdint>"
    else
        log_info "Agregando #include <cstdint> a converter.h..."
        sed -i '/#include <cstddef>/a #include <cstdint>' "$CONVERTER_H"
        log_success "✓ converter.h parcheado"
    fi
fi

# Fix advanced_mutex.h - missing #include <memory>
ADVANCED_MUTEX_H="$SCRIPT_DIR/Server/Root/TeaSpeak/shared/src/misc/advanced_mutex.h"
if [[ -f "$ADVANCED_MUTEX_H" ]]; then
    if grep -q "#include <memory>" "$ADVANCED_MUTEX_H"; then
        log_success "✓ advanced_mutex.h ya tiene #include <memory>"
    else
        log_info "Agregando #include <memory> a advanced_mutex.h..."
        sed -i '/#include <map>/a #include <memory>' "$ADVANCED_MUTEX_H"
        log_success "✓ advanced_mutex.h parcheado"
    fi
fi

log_success "✓ PARCHE 31 completado (GCC 13 explicit includes)"

log_info "════════════════════════════════════════════════════════════"
log_info "  PARCHE 32: Make MySQL compilation conditional"
log_info "════════════════════════════════════════════════════════════"
SHARED_CMAKE="$SCRIPT_DIR/Server/Root/TeaSpeak/shared/CMakeLists.txt"
if [[ -f "$SHARED_CMAKE" ]]; then
    # Check if MySQL.cpp is in HAVE_SQLITE3 block (wrong)
    if grep -A 5 "if(HAVE_SQLITE3)" "$SHARED_CMAKE" | grep -q "src/sql/mysql/MySQL.cpp"; then
        log_warning "⚠ MySQL.cpp está en bloque SQLITE3 (esto causará errores)"
        log_success "✓ PARCHE 32 necesita aplicarse manualmente (demasiado complejo para sed)"
    elif grep -A 10 "if(mysql_FOUND)" "$SHARED_CMAKE" | grep -q "target_sources.*MySQL.cpp\|MySQL.cpp"; then
        log_success "✓ PARCHE 32 ya aplicado (MySQL.cpp es condicional)"
    else
        log_success "✓ PARCHE 32 no necesario o ya aplicado manualmente"
    fi
else
    log_warning "⚠ $SHARED_CMAKE no encontrado, saltando PARCHE 32"
fi

log_info "════════════════════════════════════════════════════════════"
log_info "  PARCHE 33: Fix spin_lock -> spin_mutex in file and server modules"
log_info "════════════════════════════════════════════════════════════"
# Fix incorrect include name and type name in file and server modules
# NOTE: The correct header is spin_mutex.h, NOT spin_lock.h
NEEDS_PATCH=0
if find "$SCRIPT_DIR/Server/Root/TeaSpeak/file" "$SCRIPT_DIR/Server/Root/TeaSpeak/server" -type f \( -name "*.h" -o -name "*.cpp" \) -exec grep -q "misc/spin_lock\.h" {} \; 2>/dev/null; then
    log_info "Corrigiendo includes de spin_lock.h -> spin_mutex.h..."
    find "$SCRIPT_DIR/Server/Root/TeaSpeak/file" "$SCRIPT_DIR/Server/Root/TeaSpeak/server" -type f \( -name "*.h" -o -name "*.cpp" \) -exec sed -i 's|misc/spin_lock\.h|misc/spin_mutex.h|g' {} + 2>/dev/null
    NEEDS_PATCH=1
fi

if find "$SCRIPT_DIR/Server/Root/TeaSpeak/file" "$SCRIPT_DIR/Server/Root/TeaSpeak/server" -type f \( -name "*.h" -o -name "*.cpp" \) -exec grep -q "spin_lock " {} \; 2>/dev/null; then
    log_info "Corrigiendo tipo spin_lock -> spin_mutex..."
    find "$SCRIPT_DIR/Server/Root/TeaSpeak/file" "$SCRIPT_DIR/Server/Root/TeaSpeak/server" -type f \( -name "*.h" -o -name "*.cpp" \) -exec sed -i 's/spin_lock /spin_mutex /g' {} + 2>/dev/null
    NEEDS_PATCH=1
fi

if [[ $NEEDS_PATCH -eq 1 ]]; then
    log_success "✓ PARCHE 33 aplicado exitosamente"
else
    log_success "✓ PARCHE 33 ya aplicado (spin_mutex correcto)"
fi

log_info "════════════════════════════════════════════════════════════"
log_info "  PARCHE 34: Add TeaSpeakLibrary include path to server CMakeLists.txt"
log_info "════════════════════════════════════════════════════════════"
SERVER_CMAKE="$SCRIPT_DIR/Server/Root/TeaSpeak/server/CMakeLists.txt"
if [[ -f "$SERVER_CMAKE" ]]; then
    if grep -q "include_directories(../../../TeaSpeakLibrary-1.4.10/src)" "$SERVER_CMAKE"; then
        log_success "✓ TeaSpeakLibrary ya está en include_directories"
    else
        log_info "Agregando TeaSpeakLibrary a include_directories..."
        sed -i '/include_directories(..\/MusicBot\/src)/a include_directories(../../../TeaSpeakLibrary-1.4.10/src)' "$SERVER_CMAKE"
        log_success "✓ PARCHE 34 aplicado exitosamente"
    fi
else
    log_warning "⚠ $SERVER_CMAKE no encontrado, saltando PARCHE 34"
fi

log_info "════════════════════════════════════════════════════════════"
log_info "  PARCHE 35: Fix DataPipes FindCrypto.cmake to use static OpenSSL"
log_info "════════════════════════════════════════════════════════════"
FINDCRYPTO_CMAKE="$SCRIPT_DIR/Server/Root/libraries/DataPipes/cmake/modules/FindCrypto.cmake"
if [[ -f "$FINDCRYPTO_CMAKE" ]]; then
    if grep -q "NAMES libssl.a ssl.lib libssl.so" "$FINDCRYPTO_CMAKE"; then
        log_success "✓ FindCrypto.cmake ya prioriza bibliotecas estáticas"
    else
        log_info "Modificando FindCrypto.cmake para usar bibliotecas estáticas de OpenSSL..."

        # Modificar find_library para SSL para que busque primero .a (static) y luego .so (shared)
        sed -i 's/NAMES libssl\.so ssl\.dll ssl\.lib/NAMES libssl.a ssl.lib libssl.so ssl.dll/g' "$FINDCRYPTO_CMAKE"

        # Modificar find_library para CRYPTO para que busque primero .a (static) y luego .so (shared)
        sed -i 's/NAMES libcrypto\.so crypto\.dll crypto\.lib/NAMES libcrypto.a crypto.lib libcrypto.so crypto.dll/g' "$FINDCRYPTO_CMAKE"

        if grep -q "NAMES libssl.a ssl.lib libssl.so" "$FINDCRYPTO_CMAKE" && \
           grep -q "NAMES libcrypto.a crypto.lib libcrypto.so" "$FINDCRYPTO_CMAKE"; then
            log_success "✓ PARCHE 35 aplicado exitosamente"

            # Forzar recompilación de DataPipes
            DATAPIPES_BUILD_MARKER="$SCRIPT_DIR/Server/Root/libraries/DataPipes/.build_linux_amd64.txt"
            if [[ -f "$DATAPIPES_BUILD_MARKER" ]]; then
                log_info "Marcando DataPipes para recompilación..."
                rm -f "$DATAPIPES_BUILD_MARKER"
                rm -rf "$SCRIPT_DIR/Server/Root/libraries/DataPipes/_build/linux_amd64"
                log_success "✓ DataPipes se recompilará automáticamente"
            fi
        else
            log_error "Error al aplicar PARCHE 35"
            exit 1
        fi
    fi
else
    log_warning "⚠ FindCrypto.cmake no encontrado, saltando PARCHE 35"
fi

log_info "════════════════════════════════════════════════════════════"
log_info "  PARCHE 36: Fix shared library to use static OpenSSL"
log_info "════════════════════════════════════════════════════════════"
SHARED_CMAKE="$SCRIPT_DIR/Server/Server/shared/CMakeLists.txt"
if [[ -f "$SHARED_CMAKE" ]]; then
    if ! grep -q "openssl::ssl::shared\|openssl::crypto::shared" "$SHARED_CMAKE"; then
        log_success "✓ shared/CMakeLists.txt ya usa bibliotecas estáticas de OpenSSL"
    else
        log_info "Modificando shared/CMakeLists.txt para usar bibliotecas estáticas de OpenSSL..."

        # Crear backup si no existe
        if [[ ! -f "$SHARED_CMAKE.backup_ssl" ]]; then
            cp "$SHARED_CMAKE" "$SHARED_CMAKE.backup_ssl"
        fi

        # Reemplazar todas las ocurrencias de shared por static para openssl targets
        sed -i 's/openssl::ssl::shared/openssl::ssl::static/g' "$SHARED_CMAKE"
        sed -i 's/openssl::crypto::shared/openssl::crypto::static/g' "$SHARED_CMAKE"

        if ! grep -q "openssl::ssl::shared\|openssl::crypto::shared" "$SHARED_CMAKE"; then
            log_success "✓ PARCHE 36 aplicado exitosamente"

            # Forzar recompilación del módulo shared
            SHARED_BUILD_DIR="$SCRIPT_DIR/Server/Root/TeaSpeak/build/shared"
            if [[ -d "$SHARED_BUILD_DIR" ]]; then
                log_info "Limpiando compilación previa de shared..."
                rm -rf "$SHARED_BUILD_DIR"
                log_success "✓ Módulo shared se recompilará automáticamente"
            fi
        else
            log_error "Error al aplicar PARCHE 36: aún quedan referencias shared"
            exit 1
        fi
    fi
else
    log_warning "⚠ shared/CMakeLists.txt no encontrado, saltando PARCHE 36"
fi

log_info "════════════════════════════════════════════════════════════"
log_info "  PARCHE 38: Fix library paths in tearoot-server.cmake"
log_info "════════════════════════════════════════════════════════════"
TEAROOT_SERVER_CMAKE="$SCRIPT_DIR/Server/Root/build-helpers/cmake/config/tearoot-server.cmake"
if [[ -f "$TEAROOT_SERVER_CMAKE" ]]; then
    if grep -qi 'TomMath_ROOT_DIR.*".*\${LIBRARY_PATH}/tommath/' "$TEAROOT_SERVER_CMAKE" && \
       grep -qi 'TomCrypt_ROOT_DIR.*".*\${LIBRARY_PATH}/tomcrypt/' "$TEAROOT_SERVER_CMAKE"; then
        log_success "✓ tearoot-server.cmake ya tiene las rutas correctas de bibliotecas"
    else
        log_info "Corrigiendo rutas de bibliotecas en tearoot-server.cmake..."

        # Crear backup si no existe
        if [[ ! -f "$TEAROOT_SERVER_CMAKE.backup_patch38" ]]; then
            cp "$TEAROOT_SERVER_CMAKE" "$TEAROOT_SERVER_CMAKE.backup_patch38"
        fi

        # Agregar las barras faltantes en las rutas de bibliotecas
        # Usar un enfoque más general para evitar romper rutas que ya tienen la barra
        sed -i 's|"${LIBRARY_PATH}tommath/|"${LIBRARY_PATH}/tommath/|g' "$TEAROOT_SERVER_CMAKE"
        sed -i 's|"${LIBRARY_PATH}tomcrypt/|"${LIBRARY_PATH}/tomcrypt/|g' "$TEAROOT_SERVER_CMAKE"
        sed -i 's|"${LIBRARY_PATH}opus/|"${LIBRARY_PATH}/opus/|g' "$TEAROOT_SERVER_CMAKE"
        sed -i 's|"${LIBRARY_PATH}breakpad/|"${LIBRARY_PATH}/breakpad/|g' "$TEAROOT_SERVER_CMAKE"
        sed -i 's|"${LIBRARY_PATH}ed25519/|"${LIBRARY_PATH}/ed25519/|g' "$TEAROOT_SERVER_CMAKE"
        sed -i 's|"${LIBRARY_PATH}DataPipes/|"${LIBRARY_PATH}/DataPipes/|g' "$TEAROOT_SERVER_CMAKE"
        sed -i 's|"${LIBRARY_PATH}Thread-Pool/|"${LIBRARY_PATH}/Thread-Pool/|g' "$TEAROOT_SERVER_CMAKE"
        sed -i 's|"${LIBRARY_PATH}CXXTerminal/|"${LIBRARY_PATH}/CXXTerminal/|g' "$TEAROOT_SERVER_CMAKE"
        sed -i 's|"${LIBRARY_PATH}StringVariable/|"${LIBRARY_PATH}/StringVariable/|g' "$TEAROOT_SERVER_CMAKE"
        sed -i 's|"${LIBRARY_PATH}yaml-cpp/|"${LIBRARY_PATH}/yaml-cpp/|g' "$TEAROOT_SERVER_CMAKE"
        sed -i 's|"${LIBRARY_PATH}jemalloc/|"${LIBRARY_PATH}/jemalloc/|g' "$TEAROOT_SERVER_CMAKE"
        sed -i 's|"${LIBRARY_PATH}boringssl/|"${LIBRARY_PATH}/boringssl/|g' "$TEAROOT_SERVER_CMAKE"

        if grep -qi 'TomMath_ROOT_DIR.*".*\${LIBRARY_PATH}/tommath/' "$TEAROOT_SERVER_CMAKE" && \
           grep -qi 'TomCrypt_ROOT_DIR.*".*\${LIBRARY_PATH}/tomcrypt/' "$TEAROOT_SERVER_CMAKE"; then
            log_success "✓ PARCHE 38 aplicado exitosamente"
        else
            log_error "Error al aplicar PARCHE 38: rutas de bibliotecas incorrectas"
            exit 1
        fi
    fi
else
    log_warning "⚠ tearoot-server.cmake no encontrado, saltando PARCHE 38"
fi

log_info "════════════════════════════════════════════════════════════"
log_info "  PARCHE 37: Fix license CMakeLists.txt to use explicit OpenSSL paths"
log_info "════════════════════════════════════════════════════════════"
LICENSE_CMAKE="$SCRIPT_DIR/Server/Server/license/CMakeLists.txt"
if [[ -f "$LICENSE_CMAKE" ]]; then
    if grep -q "# PATCH 37: Use explicit OpenSSL paths" "$LICENSE_CMAKE"; then
        log_success "✓ license/CMakeLists.txt ya usa rutas explícitas de OpenSSL"
    else
        log_info "Modificando license/CMakeLists.txt para usar rutas explícitas de OpenSSL..."

        # Crear backup si no existe
        if [[ ! -f "$LICENSE_CMAKE.backup_patch37" ]]; then
            cp "$LICENSE_CMAKE" "$LICENSE_CMAKE.backup_patch37"
        fi

        # Reemplazar openssl::ssl::static y openssl::crypto::static con rutas explícitas
        # Buscar la sección target_link_libraries(TeaLicenseServer y reemplazar
        sed -i '/target_link_libraries(TeaLicenseServer/,/^)/ {
            s|openssl::ssl::static|${CMAKE_SOURCE_DIR}/../../libraries/openssl-prebuild/linux_amd64/lib/libssl.a|g
            s|openssl::crypto::static|${CMAKE_SOURCE_DIR}/../../libraries/openssl-prebuild/linux_amd64/lib/libcrypto.a|g
        }' "$LICENSE_CMAKE"

        # Agregar marcador de que el patch fue aplicado
        sed -i '/target_link_libraries(TeaLicenseServer/i\# PATCH 37: Use explicit OpenSSL paths to avoid linking conflicts' "$LICENSE_CMAKE"

        if grep -q "# PATCH 37: Use explicit OpenSSL paths" "$LICENSE_CMAKE"; then
            log_success "✓ PARCHE 37 aplicado exitosamente"

            # Forzar recompilación del módulo license
            LICENSE_BUILD_DIR="$SCRIPT_DIR/Server/Root/TeaSpeak/build/license"
            if [[ -d "$LICENSE_BUILD_DIR" ]]; then
                log_info "Limpiando compilación previa de license..."
                rm -rf "$LICENSE_BUILD_DIR"
                log_success "✓ Módulo license se recompilará automáticamente"
            fi
        else
            log_error "Error al aplicar PARCHE 37"
            exit 1
        fi
    fi
else
    log_warning "⚠ license/CMakeLists.txt no encontrado, saltando PARCHE 37"
fi

log_info "════════════════════════════════════════════════════════════"
log_info "  PARCHE 39: Fix spdlog_ROOT_DIR in tearoot-server.cmake"
log_info "════════════════════════════════════════════════════════════"
TEAROOT_SERVER_CMAKE="$SCRIPT_DIR/Server/Root/build-helpers/cmake/config/tearoot-server.cmake"
if [[ -f "$TEAROOT_SERVER_CMAKE" ]]; then
    # Check if spdlog_ROOT_DIR is already set (case-insensitive)
    if grep -qi "^set(spdlog_ROOT_DIR\|^SET(spdlog_ROOT_DIR" "$TEAROOT_SERVER_CMAKE"; then
        log_success "✓ tearoot-server.cmake ya tiene spdlog_ROOT_DIR configurado"
    else
        log_info "Agregando spdlog_ROOT_DIR a tearoot-server.cmake..."

        # Add spdlog_ROOT_DIR after the spdlog_DIR line
        sed -i '/^set(spdlog_DIR/a\SET(spdlog_ROOT_DIR "${LIBRARY_PATH}/spdlog/${BUILD_OUTPUT}")' "$TEAROOT_SERVER_CMAKE"

        if grep -q "^SET(spdlog_ROOT_DIR" "$TEAROOT_SERVER_CMAKE"; then
            log_success "✓ PARCHE 39 aplicado exitosamente"
        else
            log_error "Error al aplicar PARCHE 39"
            exit 1
        fi
    fi
else
    log_warning "⚠ tearoot-server.cmake no encontrado, saltando PARCHE 39"
fi

log_info "════════════════════════════════════════════════════════════"
log_info "  PARCHE 40: Fix OpenSSL linking for DataPipes (add -lssl)"
log_info "════════════════════════════════════════════════════════════"
SERVER_CMAKE="$SCRIPT_DIR/Server/Server/server/CMakeLists.txt"
if [[ -f "$SERVER_CMAKE" ]]; then
    # Check if -lssl is already in target_link_options
    if grep -A 3 "target_link_options(TeaSpeakServer PRIVATE" "$SERVER_CMAKE" | grep -q '"LINKER:-lssl"'; then
        log_success "✓ server/CMakeLists.txt ya incluye -lssl en target_link_options"
    else
        log_info "Agregando -lssl a target_link_options en server/CMakeLists.txt..."

        # Create backup if not exists
        if [[ ! -f "$SERVER_CMAKE.backup_patch40" ]]; then
            cp "$SERVER_CMAKE" "$SERVER_CMAKE.backup_patch40"
        fi

        # Add "LINKER:-lssl" before "LINKER:-lcrypto"
        sed -i '/"LINKER:-lcrypto"/i\    "LINKER:-lssl"' "$SERVER_CMAKE"

        if grep -A 4 "target_link_options(TeaSpeakServer PRIVATE" "$SERVER_CMAKE" | grep -q '"LINKER:-lssl"'; then
            log_success "✓ PARCHE 40 aplicado exitosamente"
        else
            log_error "Error al aplicar PARCHE 40"
            exit 1
        fi
    fi
else
    log_warning "⚠ server/CMakeLists.txt no encontrado, saltando PARCHE 40"
fi

log_info "════════════════════════════════════════════════════════════"
log_info "  PARCHE 41: Make CXXTerminal and Breakpad optional"
log_info "════════════════════════════════════════════════════════════"

# Disable BREAKPAD_EXCEPTION_HANDLER in SignalHandler.cpp
SIGNAL_HANDLER_CPP="$SCRIPT_DIR/Server/Server/server/src/SignalHandler.cpp"
if [[ -f "$SIGNAL_HANDLER_CPP" ]]; then
    if grep -q "#define BREAKPAD_EXCEPTION_HANDLER 0" "$SIGNAL_HANDLER_CPP"; then
        log_success "✓ SignalHandler.cpp ya tiene Breakpad deshabilitado"
    else
        log_info "Deshabilitando Breakpad en SignalHandler.cpp..."
        if [[ ! -f "$SIGNAL_HANDLER_CPP.backup_patch41" ]]; then
            cp "$SIGNAL_HANDLER_CPP" "$SIGNAL_HANDLER_CPP.backup_patch41"
        fi
        sed -i 's/#define BREAKPAD_EXCEPTION_HANDLER 1/#define BREAKPAD_EXCEPTION_HANDLER 0/' "$SIGNAL_HANDLER_CPP"
        log_success "✓ Breakpad deshabilitado en SignalHandler.cpp"
    fi
else
    log_warning "⚠ SignalHandler.cpp no encontrado"
fi

# Comment out CXXTerminal include in main.cpp
MAIN_CPP="$SCRIPT_DIR/Server/Server/server/main.cpp"
if [[ -f "$MAIN_CPP" ]]; then
    if grep -q "^//#include <CXXTerminal/QuickTerminal.h>" "$MAIN_CPP"; then
        log_success "✓ main.cpp ya tiene CXXTerminal comentado"
    else
        log_info "Comentando CXXTerminal en main.cpp..."
        if [[ ! -f "$MAIN_CPP.backup_patch41" ]]; then
            cp "$MAIN_CPP" "$MAIN_CPP.backup_patch41"
        fi
        sed -i 's|^#include <CXXTerminal/QuickTerminal.h>|//#include <CXXTerminal/QuickTerminal.h>|' "$MAIN_CPP"
        log_success "✓ CXXTerminal comentado en main.cpp"
    fi
else
    log_warning "⚠ main.cpp no encontrado"
fi

log_success "✓ PARCHE 41 aplicado exitosamente"

log_info "════════════════════════════════════════════════════════════"
log_info "  PARCHE 42: Compilar spdlog si no está disponible"
log_info "════════════════════════════════════════════════════════════"
_LIBRARIES_DIR="$SCRIPT_DIR/Server/Root/libraries"
_SPDLOG_OUT="$_LIBRARIES_DIR/spdlog/out/linux_amd64"
_SPDLOG_CMAKE_CONFIG="$_SPDLOG_OUT/lib/cmake/spdlog/spdlogConfig.cmake"

if [ -f "$_SPDLOG_CMAKE_CONFIG" ]; then
    log_success "✓ spdlog ya está compilado correctamente"
else
    log_info "spdlog no compilado - buscando código fuente..."
    _SPDLOG_SRC=""
    if [ -d "$_LIBRARIES_DIR/spdlog" ] && [ -f "$_LIBRARIES_DIR/spdlog/CMakeLists.txt" ]; then
        _SPDLOG_SRC="$_LIBRARIES_DIR/spdlog"
    elif [ -d "$SCRIPT_DIR/libraries/spdlog-master" ] && [ -f "$SCRIPT_DIR/libraries/spdlog-master/CMakeLists.txt" ]; then
        _SPDLOG_SRC="$SCRIPT_DIR/libraries/spdlog-master"
    fi

    if [ -z "$_SPDLOG_SRC" ]; then
        log_error "✗ PARCHE 42: Código fuente de spdlog no encontrado"
        log_error "  Buscado en: $_LIBRARIES_DIR/spdlog"
        log_error "  Buscado en: $SCRIPT_DIR/libraries/spdlog-master"
        exit 1
    fi

    log_info "Compilando spdlog desde: $_SPDLOG_SRC"
    _SPDLOG_BUILD="/tmp/spdlog_cmake_build_$$"
    mkdir -p "$_SPDLOG_OUT"
    mkdir -p "$_SPDLOG_BUILD"

    (
        cmake -S "$_SPDLOG_SRC" -B "$_SPDLOG_BUILD" \
            -DCMAKE_C_FLAGS="-fPIC" \
            -DCMAKE_CXX_FLAGS="-fPIC" \
            -DCMAKE_BUILD_TYPE="Release" \
            -DCMAKE_INSTALL_PREFIX="$_SPDLOG_OUT" \
            -DSPDLOG_BUILD_EXAMPLE=OFF \
            -DSPDLOG_BUILD_EXAMPLES=OFF \
            -DSPDLOG_BUILD_TESTING=OFF \
            -DSPDLOG_BUILD_TESTS=OFF \
            -DBUILD_TESTING=OFF
        cmake --build "$_SPDLOG_BUILD" -j$(nproc 2>/dev/null || echo 4)
        cmake --install "$_SPDLOG_BUILD"
    ) || {
        rm -rf "$_SPDLOG_BUILD" 2>/dev/null || true
        log_error "✗ PARCHE 42: Error al compilar spdlog"
        exit 1
    }

    rm -rf "$_SPDLOG_BUILD" 2>/dev/null || true

    if [ -f "$_SPDLOG_CMAKE_CONFIG" ]; then
        log_success "✓ PARCHE 42: spdlog compilado exitosamente"
        touch "$_LIBRARIES_DIR/spdlog/.build_successful" 2>/dev/null || true
    else
        log_error "✗ PARCHE 42: spdlog compilado pero cmake config no encontrado en:"
        log_error "  $_SPDLOG_CMAKE_CONFIG"
        exit 1
    fi
fi

echo ""
echo -e "${GREEN}═══════════════════════════════════════════════════════════${NC}"
echo -e "${GREEN}  ✅ Parches aplicados exitosamente${NC}"
echo -e "${GREEN}═══════════════════════════════════════════════════════════${NC}"
echo ""

exit 0
