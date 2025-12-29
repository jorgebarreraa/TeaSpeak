#!/bin/bash
#
# ═══════════════════════════════════════════════════════════════════════════
#  Script de Instalación Completa de TeaSpeak - VERSIÓN MEJORADA
# ═══════════════════════════════════════════════════════════════════════════
#
#  Este script configura automáticamente el entorno completo para compilar
#  TeaSpeak Server con soporte para repositorios personales en GitHub.
#
#  Características:
#  ✓ Instalación automática de todas las dependencias
#  ✓ Soporte para usar forks personales en GitHub
#  ✓ Detección y corrección de problemas comunes
#  ✓ Compilación optimizada multi-núcleo
#  ✓ Verificación paso a paso
#  ✓ Logs detallados
#
#  Uso:
#    ./setup_teaspeak.sh [opciones]
#
#  Opciones:
#    --github-user <username>  : Usar forks de este usuario de GitHub
#    --skip-deps              : Saltar instalación de dependencias del sistema
#    --build-type <type>      : Tipo de build (stable|optimized|debug|nightly)
#    --help                   : Mostrar ayuda
#
#  Ejemplos:
#    ./setup_teaspeak.sh
#    ./setup_teaspeak.sh --github-user jorgebarreraa
#    ./setup_teaspeak.sh --github-user jorgebarreraa --build-type stable
#    ./setup_teaspeak.sh --skip-deps --build-type optimized
#
# ═══════════════════════════════════════════════════════════════════════════

set -e  # Salir si hay error (excepto donde lo manejemos)

# ═══════════════════════════════════════════════════════════════════════════
# Colores para output
# ═══════════════════════════════════════════════════════════════════════════
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
MAGENTA='\033[0;35m'
NC='\033[0m' # No Color

# ═══════════════════════════════════════════════════════════════════════════
# Funciones de logging
# ═══════════════════════════════════════════════════════════════════════════
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[✓]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[⚠]${NC} $1"
}

log_error() {
    echo -e "${RED}[✗]${NC} $1"
}

log_step() {
    echo ""
    echo -e "${CYAN}╔═══════════════════════════════════════════════════════════╗${NC}"
    echo -e "${CYAN}║ $1"
    echo -e "${CYAN}╚═══════════════════════════════════════════════════════════╝${NC}"
}

log_substep() {
    echo -e "${MAGENTA}▸${NC} $1"
}

# ═══════════════════════════════════════════════════════════════════════════
# Variables globales
# ═══════════════════════════════════════════════════════════════════════════
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GITHUB_USER=""
SKIP_DEPS=false
BUILD_TYPE="stable"
REQUIRED_OPENSSL_VERSION="3.0"
MIN_GCC_VERSION="9"
LOG_FILE="/tmp/teaspeak_setup_$(date +%Y%m%d_%H%M%S).log"

# ═══════════════════════════════════════════════════════════════════════════
# Parsear argumentos
# ═══════════════════════════════════════════════════════════════════════════
parse_args() {
    while [[ $# -gt 0 ]]; do
        case $1 in
            --github-user)
                GITHUB_USER="$2"
                shift 2
                ;;
            --skip-deps)
                SKIP_DEPS=true
                shift
                ;;
            --build-type)
                BUILD_TYPE="$2"
                shift 2
                ;;
            --help)
                show_help
                exit 0
                ;;
            *)
                log_error "Opción desconocida: $1"
                show_help
                exit 1
                ;;
        esac
    done
}

show_help() {
    cat << EOF
Uso: $0 [opciones]

Opciones:
  --github-user <username>  Usar forks de este usuario de GitHub
  --skip-deps              Saltar instalación de dependencias del sistema
  --build-type <type>      Tipo de build: stable, optimized, debug, nightly (default: stable)
  --help                   Mostrar esta ayuda

Ejemplos:
  $0
  $0 --github-user jorgebarreraa
  $0 --github-user jorgebarreraa --build-type stable
  $0 --skip-deps --build-type optimized

Tipos de build:
  stable     - Build de producción estable (recomendado)
  optimized  - Build optimizado (más rápido)
  debug      - Build con información de debugging
  nightly    - Build nightly con optimizaciones y debug info

EOF
}

# ═══════════════════════════════════════════════════════════════════════════
# Función para logging dual (pantalla + archivo)
# ═══════════════════════════════════════════════════════════════════════════
log_dual() {
    echo "$1" | tee -a "$LOG_FILE"
}

# ═══════════════════════════════════════════════════════════════════════════
# PASO 1: Verificar sistema operativo
# ═══════════════════════════════════════════════════════════════════════════
check_system() {
    log_step "PASO 1: Verificando Sistema Operativo"

    if [[ ! -f /etc/os-release ]]; then
        log_error "No se pudo detectar el sistema operativo"
        exit 1
    fi

    . /etc/os-release

    log_info "Sistema: $NAME $VERSION"
    log_info "Arquitectura: $(uname -m)"
    log_info "Kernel: $(uname -r)"
    log_info "CPU cores: $(nproc)"

    # Verificar que sea Linux
    if [[ "$(uname -s)" != "Linux" ]]; then
        log_error "Este script solo funciona en Linux"
        exit 1
    fi

    # Verificar Ubuntu/Debian
    if [[ "$ID" == "ubuntu" ]] || [[ "$ID" == "debian" ]]; then
        log_success "Sistema operativo compatible detectado"
    else
        log_warning "Sistema $ID no probado oficialmente"
        log_warning "El script está optimizado para Ubuntu/Debian"
    fi
}

# ═══════════════════════════════════════════════════════════════════════════
# PASO 2: Instalar dependencias del sistema
# ═══════════════════════════════════════════════════════════════════════════
install_dependencies() {
    if [[ "$SKIP_DEPS" == true ]]; then
        log_step "PASO 2: Saltando Instalación de Dependencias (--skip-deps)"
        return
    fi

    log_step "PASO 2: Instalando Dependencias del Sistema"

    # Detectar si necesitamos sudo
    if [[ $EUID -ne 0 ]]; then
        SUDO="sudo"
        log_info "Usando sudo para instalar paquetes"
    else
        SUDO=""
    fi

    log_substep "Actualizando lista de paquetes..."
    $SUDO apt-get update -qq

    log_substep "Instalando herramientas de compilación..."
    $SUDO apt-get install -y -qq \
        build-essential \
        gcc \
        g++ \
        cmake \
        make \
        git \
        pkg-config \
        curl \
        wget \
        meson \
        ninja-build \
        autoconf \
        automake \
        libtool \
        gettext \
        coreutils \
        software-properties-common

    log_substep "Instalando librerías del sistema..."
    # Instalar librerías del sistema (ignorar errores de paquetes no disponibles)
    $SUDO apt-get install -y -qq \
        libssl-dev \
        libmysqlclient-dev \
        libcurl4-openssl-dev \
        libpcre3-dev \
        libsqlite3-dev \
        libjemalloc-dev \
        zlib1g-dev \
        python3 \
        python3-dev \
        python3-pip 2>/dev/null || true

    # Intentar instalar default-libmysqlclient-dev si está disponible
    $SUDO apt-get install -y -qq default-libmysqlclient-dev 2>/dev/null || true

    # Intentar instalar libncurses5-dev o libncurses-dev
    $SUDO apt-get install -y -qq libncurses5-dev 2>/dev/null || \
    $SUDO apt-get install -y -qq libncurses-dev 2>/dev/null || true

    log_success "Todas las dependencias del sistema instaladas"
}

# ═══════════════════════════════════════════════════════════════════════════
# PASO 3: Instalar Rust (cargo, rustc)
# ═══════════════════════════════════════════════════════════════════════════
install_rust() {
    log_step "PASO 3: Verificando e Instalando Rust NIGHTLY"

    # IMPORTANTE: TeaSpeak requiere Rust NIGHTLY (no stable)
    # El código usa features inestables como: backtrace, core_intrinsics,
    # array_methods, drain_filter, box_syntax, etc.

    # Verificar si Rust ya está instalado
    if command -v cargo &> /dev/null && command -v rustc &> /dev/null; then
        RUST_VERSION=$(rustc --version)
        if [[ "$RUST_VERSION" == *"nightly"* ]]; then
            log_info "Rust nightly ya está instalado:"
            log_info "  cargo: $(cargo --version)"
            log_info "  rustc: $RUST_VERSION"
            log_success "Rust nightly disponible"
            return
        else
            log_warning "Rust está instalado pero NO es nightly"
            log_warning "  Versión actual: $RUST_VERSION"
            log_substep "Instalando Rust nightly..."
        fi
    else
        log_substep "Descargando e instalando Rust nightly..."
        log_info "Esto puede tomar unos minutos..."

        # Descargar y ejecutar rustup
        curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y
    fi

    # Cargar el entorno de Rust
    if [[ -f "$HOME/.cargo/env" ]]; then
        source "$HOME/.cargo/env"
    fi

    # Instalar y configurar nightly como default
    log_substep "Configurando Rust nightly como versión predeterminada..."
    rustup install nightly
    rustup default nightly

    # Verificar instalación de nightly
    RUST_VERSION=$(rustc --version)
    if [[ "$RUST_VERSION" == *"nightly"* ]]; then
        log_success "Rust nightly instalado correctamente"
        log_info "  cargo: $(cargo --version)"
        log_info "  rustc: $RUST_VERSION"
    else
        log_error "Error: No se pudo configurar Rust nightly"
        log_error "  Versión actual: $RUST_VERSION"
        exit 1
    fi
}

# ═══════════════════════════════════════════════════════════════════════════
# PASO 4: Deshabilitar ld.gold
# ═══════════════════════════════════════════════════════════════════════════
disable_ld_gold() {
    log_step "PASO 4: Verificando Linker (ld.gold)"

    if [[ -f /usr/bin/ld.gold ]] && [[ ! -f /usr/bin/NOT_USED_ld.gold ]]; then
        log_warning "ld.gold detectado - puede causar problemas"
        log_substep "Deshabilitando ld.gold..."

        if [[ $EUID -ne 0 ]]; then
            SUDO="sudo"
        else
            SUDO=""
        fi

        $SUDO mv /usr/bin/ld.gold /usr/bin/NOT_USED_ld.gold 2>/dev/null || true
        log_success "ld.gold deshabilitado"
    elif [[ -f /usr/bin/NOT_USED_ld.gold ]]; then
        log_success "ld.gold ya está deshabilitado"
    else
        log_success "ld.gold no está presente (OK)"
    fi

    log_info "Linker actual: $(ld --version | head -1)"
}

# ═══════════════════════════════════════════════════════════════════════════
# PASO 5: Verificar versiones de herramientas
# ═══════════════════════════════════════════════════════════════════════════
verify_tools() {
    log_step "PASO 5: Verificando Versiones de Herramientas"

    local all_ok=true

    # Verificar GCC
    if command -v gcc &> /dev/null; then
        gcc_version=$(gcc -dumpversion | cut -d. -f1)
        log_info "GCC versión: $(gcc --version | head -1)"

        if [[ $gcc_version -ge $MIN_GCC_VERSION ]]; then
            log_success "GCC versión OK (>= $MIN_GCC_VERSION)"
        else
            log_warning "GCC versión $gcc_version puede ser antigua (se recomienda >= $MIN_GCC_VERSION)"
        fi
    else
        log_error "GCC no encontrado"
        all_ok=false
    fi

    # Verificar CMake
    if command -v cmake &> /dev/null; then
        log_info "CMake: $(cmake --version | head -1)"
        log_success "CMake instalado"
    else
        log_error "CMake no encontrado"
        all_ok=false
    fi

    # Verificar Meson
    if command -v meson &> /dev/null; then
        log_info "Meson: $(meson --version)"
        log_success "Meson instalado"
    else
        log_error "Meson no encontrado"
        all_ok=false
    fi

    # Verificar Git
    if command -v git &> /dev/null; then
        log_info "Git: $(git --version)"
        log_success "Git instalado"
    else
        log_error "Git no encontrado"
        all_ok=false
    fi

    # Verificar Rust
    if command -v cargo &> /dev/null; then
        log_info "Cargo: $(cargo --version)"
        log_success "Cargo instalado"
    else
        log_error "Cargo no encontrado"
        all_ok=false
    fi

    if command -v rustc &> /dev/null; then
        log_info "Rustc: $(rustc --version)"
        log_success "Rustc instalado"
    else
        log_error "Rustc no encontrado"
        all_ok=false
    fi

    # Verificar librerías con pkg-config
    log_substep "Verificando librerías del sistema..."

    for lib in openssl mysqlclient zlib; do
        if pkg-config --exists $lib 2>/dev/null; then
            log_success "$lib disponible"
        else
            log_warning "$lib no detectado (puede no afectar la compilación)"
        fi
    done

    if [[ "$all_ok" == false ]]; then
        log_error "Algunas verificaciones fallaron"
        exit 1
    fi

    log_success "Todas las herramientas esenciales verificadas"
}

# ═══════════════════════════════════════════════════════════════════════════
# PASO 6: Configurar download_libraries.sh con GitHub user
# ═══════════════════════════════════════════════════════════════════════════
configure_github_repos() {
    log_step "PASO 6: Configurando Repositorios de Dependencias"

    cd "$SCRIPT_DIR/Server/Root/libraries"

    if [[ -n "$GITHUB_USER" ]]; then
        log_info "Configurando para usar forks de: $GITHUB_USER"
        log_substep "Modificando download_libraries.sh..."

        # Crear versión personalizada del script
        cat > download_libraries_custom.sh << 'EOFSCRIPT'
#!/bin/bash
set -e

GITHUB_USER="__GITHUB_USER__"

echo "Descargando librerías desde GitHub de: $GITHUB_USER"
cd "$(dirname "$0")"

# Función para clonar o saltar si existe
clone_lib() {
    local url="$1"
    local dir="$2"
    local branch="$3"

    if [ -d "$dir" ]; then
        echo "  ✓ $dir ya existe, omitiendo"
        return 0
    fi

    echo "  Clonando $dir..."
    if [ -n "$branch" ]; then
        git clone "$url" "$dir" --branch "$branch" --depth 1 || {
            echo "  ⚠ Error clonando $url, intentando repo original..."
            return 1
        }
    else
        git clone "$url" "$dir" --depth 1 || {
            echo "  ⚠ Error clonando $url, intentando repo original..."
            return 1
        }
    fi
}

# Intentar clonar desde GitHub del usuario, si falla usar original
clone_with_fallback() {
    local original_url="$1"
    local dir="$2"
    local branch="$3"

    # Extraer nombre del repo
    local repo_name=$(basename "$original_url" .git)

    # Intentar desde GitHub del usuario primero
    if [[ -n "$GITHUB_USER" ]]; then
        local user_url="https://github.com/$GITHUB_USER/${repo_name}.git"
        if clone_lib "$user_url" "$dir" "$branch"; then
            return 0
        fi
        echo "  → Intentando desde repositorio original..."
    fi

    # Si falla o no hay usuario, usar original
    clone_lib "$original_url" "$dir" "$branch"
}

# Librerías principales
clone_with_fallback "https://github.com/open-source-parsers/jsoncpp.git" "jsoncpp"
clone_with_fallback "https://github.com/jorgebarreraa/Thread-Pool.git" "Thread-Pool"
clone_with_fallback "https://github.com/jorgebarreraa/tomcrypt.git" "tomcrypt"
clone_with_fallback "https://github.com/jorgebarreraa/tommath.git" "tommath"
clone_with_fallback "https://github.com/WolverinDEV/CXXTerminal.git" "CXXTerminal"
clone_with_fallback "https://github.com/xiph/opus" "opus"
clone_with_fallback "https://github.com/xiph/opusfile.git" "opusfile"
clone_with_fallback "https://github.com/jbeder/yaml-cpp.git" "yaml-cpp"
clone_with_fallback "https://github.com/libevent/libevent.git" "event"

# Patch libevent para compatibilidad con CMake 3.16
if [ -f "event/cmake/AddLinkerFlags.cmake" ]; then
    echo "  Parcheando libevent para CMake 3.16..."
    cat > "event/cmake/AddLinkerFlags.cmake" << 'EOFPATCH'
if (NOT CMAKE_VERSION VERSION_LESS 3.18)
	include(CheckLinkerFlag)
endif()

macro(add_linker_flags)
	foreach(flag ${ARGN})
		string(REGEX REPLACE "[-.+/:= ]" "_" _flag_esc "${flag}")

if (NOT CMAKE_VERSION VERSION_LESS 3.18)
		check_linker_flag(C "${flag}" check_c_linker_flag_${_flag_esc})
endif()

		if (check_c_linker_flag_${_flag_esc})
			set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${flag}")
			set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${flag}")
		endif()
	endforeach()
endmacro()
EOFPATCH
fi

clone_with_fallback "https://git.did.science/TeaSpeak/libraries/spdlog.git" "spdlog"
clone_with_fallback "https://github.com/WolverinDEV/StringVariable.git" "StringVariable"
clone_with_fallback "https://github.com/WolverinDEV/ed25519.git" "ed25519"
clone_with_fallback "https://chromium.googlesource.com/breakpad/breakpad" "breakpad"

# Checkout específico para breakpad (compatible con C++17)
if [ -d "breakpad" ]; then
    echo "  Configurando breakpad..."
    (cd breakpad && git fetch --unshallow 2>/dev/null || true && git checkout f032e4c3 2>/dev/null || true)
fi

clone_with_fallback "https://boringssl.googlesource.com/boringssl" "boringssl"
clone_with_fallback "https://fuchsia.googlesource.com/third_party/protobuf" "protobuf" "v3.5.1.1"
clone_with_fallback "https://github.com/jorgebarreraa/DataPipes.git" "DataPipes"
clone_with_fallback "https://github.com/jemalloc/jemalloc.git" "jemalloc" "dev"
clone_with_fallback "https://git.did.science/TeaSpeak/libraries/libnice-prebuild.git" "libnice"
clone_with_fallback "https://git.did.science/TeaSpeak/libraries/glib2.0.git" "glibc"
clone_with_fallback "https://git.did.science/TeaSpeak/libraries/openssl-prebuild.git" "openssl-prebuild"
clone_with_fallback "https://github.com/facebook/zstd.git" "zstd"

# build-helpers
cd ..
clone_with_fallback "https://github.com/WolverinDEV/build-helpers.git" "build-helpers"

echo "✓ Todas las librerías descargadas exitosamente!"
EOFSCRIPT

        # Reemplazar __GITHUB_USER__ con el valor real
        sed -i "s/__GITHUB_USER__/$GITHUB_USER/g" download_libraries_custom.sh
        chmod +x download_libraries_custom.sh

        log_success "Script personalizado creado: download_libraries_custom.sh"
    else
        log_info "No se especificó --github-user"
        log_info "Se usarán los repositorios originales"
    fi

    cd "$SCRIPT_DIR"
}

# ═══════════════════════════════════════════════════════════════════════════
# FUNCIÓN: Modificar Cargo.toml para usar repos del usuario
# ═══════════════════════════════════════════════════════════════════════════
update_rust_cargo_dependencies() {
    local github_user="$1"

    if [[ -z "$github_user" ]]; then
        log_info "No se especificó usuario GitHub, usando repos originales en Rust"
        return 0
    fi

    log_substep "Modificando Cargo.toml para usar repos de $github_user..."

    # Modificar Server/rtc/Cargo.toml
    local rtc_cargo="$SCRIPT_DIR/Server/rtc/Cargo.toml"
    if [[ -f "$rtc_cargo" ]]; then
        log_info "  Actualizando $rtc_cargo..."

        # Backup
        cp "$rtc_cargo" "$rtc_cargo.backup"

        # Reemplazar URLs de WolverinDEV con las del usuario
        sed -i "s|https://github.com/WolverinDEV/rust-webrtc.git|https://github.com/$github_user/rust-webrtc.git|g" "$rtc_cargo"
        sed -i "s|https://github.com/WolverinDEV/rust-libnice.git|https://github.com/$github_user/rust-libnice.git|g" "$rtc_cargo"

        log_success "✓ Cargo.toml actualizado para usar repos de $github_user"

        # Limpiar cache de Cargo para forzar descarga de los nuevos repos
        log_info "  Limpiando cache de Cargo para repos Rust..."
        rm -rf "$HOME/.cargo/git/checkouts/rust-webrtc-"* 2>/dev/null || true
        rm -rf "$HOME/.cargo/git/db/rust-webrtc-"* 2>/dev/null || true
        rm -rf "$HOME/.cargo/git/checkouts/rust-libnice-"* 2>/dev/null || true
        rm -rf "$HOME/.cargo/git/db/rust-libnice-"* 2>/dev/null || true
        log_success "✓ Cache de Cargo limpiado"
    else
        log_warning "⚠️  No se encontró $rtc_cargo"
    fi
}

# ═══════════════════════════════════════════════════════════════════════════
# PASO 7: Descargar librerías
# ═══════════════════════════════════════════════════════════════════════════
download_libraries() {
    log_step "PASO 7: Descargando Librerías"

    cd "$SCRIPT_DIR/Server/Root/libraries"

    # Limpiar enlaces simbólicos rotos
    log_substep "Limpiando enlaces simbólicos rotos..."
    rm -f tomcrypt tommath spdlog ed25519 openssl-prebuild libraries 2>/dev/null || true

    # Usar script personalizado si existe, sino el original
    if [[ -f "download_libraries_custom.sh" ]]; then
        log_info "Usando script personalizado..."
        bash download_libraries_custom.sh
    elif [[ -f "download_libraries.sh" ]]; then
        log_info "Usando script original..."
        bash download_libraries.sh
    else
        log_error "No se encontró script de descarga de librerías"
        exit 1
    fi

    log_success "Librerías descargadas"

    # Aplicar parches automáticos a archivos de build
    log_substep "Aplicando parches de compilación..."
    cd "$SCRIPT_DIR"
    if [[ -f "apply_build_fixes.sh" ]]; then
        bash apply_build_fixes.sh
    else
        log_warning "apply_build_fixes.sh no encontrado, saltando parches"
    fi
}

# ═══════════════════════════════════════════════════════════════════════════
# PASO 8: Arreglar permisos
# ═══════════════════════════════════════════════════════════════════════════
fix_permissions() {
    log_step "PASO 8: Configurando Permisos"

    log_substep "Dando permisos de ejecución a scripts..."
    find "$SCRIPT_DIR" -name "*.sh" -exec chmod +x {} \; 2>/dev/null || true

    log_success "Permisos configurados"
}

# ═══════════════════════════════════════════════════════════════════════════
# PASO 9: Compilar librerías
# ═══════════════════════════════════════════════════════════════════════════
compile_libraries() {
    log_step "PASO 9: Compilando Librerías"

    cd "$SCRIPT_DIR/Server/Root/libraries"

    # Limpiar procesos zombies de compilaciones anteriores
    log_substep "Limpiando procesos de compilación previos..."
    pkill -9 -f "build_breakpad.sh" 2>/dev/null || true
    pkill -9 -f "stackwalker" 2>/dev/null || true

    # Limpiar archivos de estado de compilaciones previas para forzar recompilación
    log_substep "Limpiando archivos de estado de compilaciones antiguas..."
    find . -maxdepth 2 -name ".build_linux_amd64.txt" -delete 2>/dev/null || true
    log_info "Todas las librerías se compilarán desde cero"

    # Exportar variables de compilación
    export build_os_type=linux
    export build_os_arch=amd64
    export CXX_FLAGS="-fPIC"
    export C_FLAGS="-fPIC"
    export CMAKE_BUILD_TYPE="Release"
    export CMAKE_MAKE_OPTIONS="-j$(nproc)"
    export build_helper_file="../build-helpers/build_helper.sh"

    log_info "Compilando con $(nproc) núcleos..."
    log_warning "Esto puede tardar 10-20 minutos..."

    # Compilar librerías críticas individualmente para mejor control
    if [[ -f "../build-helpers/build_helper.sh" ]]; then
        source ../build-helpers/build_helper.sh

        # TomMath (CRÍTICA)
        log_substep "Compilando TomMath..."
        if library_path="tommath" ../build-helpers/libraries/build_tommath.sh >> "$LOG_FILE.libraries" 2>&1; then
            if [[ -f "tommath/out/linux_amd64/lib/libtommathStatic.a" ]]; then
                log_success "TomMath compilada"
            else
                log_error "TomMath compilación reportó éxito pero archivos no encontrados"
                log_error "Ver detalles en: $LOG_FILE.libraries"
                exit 1
            fi
        else
            log_error "Falló la compilación de TomMath"
            log_error "Ver detalles en: $LOG_FILE.libraries"
            exit 1
        fi

        # TomCrypt (CRÍTICA)
        log_substep "Compilando TomCrypt..."
        if tommath_path="$(pwd)/tommath/out/linux_amd64" library_path="tomcrypt" ../build-helpers/libraries/build_tomcrypt.sh >> "$LOG_FILE.libraries" 2>&1; then
            if [[ -f "tomcrypt/out/linux_amd64/lib/libtomcrypt.a" ]]; then
                log_success "TomCrypt compilada"
            else
                log_error "TomCrypt compilación reportó éxito pero archivos no encontrados"
                log_error "Ver detalles en: $LOG_FILE.libraries"
                exit 1
            fi
        else
            log_error "Falló la compilación de TomCrypt"
            log_error "Ver detalles en: $LOG_FILE.libraries"
            exit 1
        fi

        # Thread-Pool (CRÍTICA)
        log_substep "Compilando Thread-Pool..."
        if library_path="Thread-Pool" ../build-helpers/libraries/build_threadpool.sh >> "$LOG_FILE.libraries" 2>&1; then
            if [[ -f "Thread-Pool/out/linux_amd64/lib/libThreadPoolStatic.a" ]]; then
                log_success "Thread-Pool compilada"
            else
                log_error "Thread-Pool compilación reportó éxito pero archivos no encontrados"
                log_error "Ver detalles en: $LOG_FILE.libraries"
                exit 1
            fi
        else
            log_error "Falló la compilación de Thread-Pool"
            log_error "Ver detalles en: $LOG_FILE.libraries"
            exit 1
        fi

        # Compilar TODAS las librerías restantes individualmente
        local failed_libs=()
        local total_libs=16
        local compiled_libs=3  # Ya compilamos tommath, tomcrypt, Thread-Pool

        # libevent
        log_substep "Compilando libevent..."
        if library_path="event" ../build-helpers/libraries/build_libevent.sh >> "$LOG_FILE.libraries" 2>&1; then
            log_success "libevent compilada"
            ((compiled_libs++))
        else
            log_warning "libevent falló"
            failed_libs+=("libevent")
        fi

        # CXXTerminal
        log_substep "Compilando CXXTerminal..."
        if library_path="CXXTerminal" libevent_path=event ../build-helpers/libraries/build_cxxterminal.sh >> "$LOG_FILE.libraries" 2>&1; then
            log_success "CXXTerminal compilada"
            ((compiled_libs++))
        else
            log_warning "CXXTerminal falló"
            failed_libs+=("CXXTerminal")
        fi

        # DataPipes
        log_substep "Compilando DataPipes..."
        if library_path="DataPipes" ./build_datapipes.sh >> "$LOG_FILE.libraries" 2>&1; then
            log_success "DataPipes compilada"
            ((compiled_libs++))
        else
            log_warning "DataPipes falló"
            failed_libs+=("DataPipes")
        fi

        # ed25519
        log_substep "Compilando ed25519..."
        if library_path="ed25519" ../build-helpers/libraries/build_ed25519.sh >> "$LOG_FILE.libraries" 2>&1; then
            log_success "ed25519 compilada"
            ((compiled_libs++))
        else
            log_warning "ed25519 falló"
            failed_libs+=("ed25519")
        fi

        # jsoncpp
        log_substep "Compilando jsoncpp..."
        if library_path="jsoncpp" ../build-helpers/libraries/build_jsoncpp.sh >> "$LOG_FILE.libraries" 2>&1; then
            log_success "jsoncpp compilada"
            ((compiled_libs++))
        else
            log_warning "jsoncpp falló"
            failed_libs+=("jsoncpp")
        fi

        # opus
        log_substep "Compilando opus..."
        if library_path="opus" ../build-helpers/libraries/build_opus.sh >> "$LOG_FILE.libraries" 2>&1; then
            log_success "opus compilada"
            ((compiled_libs++))
        else
            log_warning "opus falló"
            failed_libs+=("opus")
        fi

        # protobuf
        log_substep "Compilando protobuf..."
        if library_path="protobuf" ./build_protobuf.sh >> "$LOG_FILE.libraries" 2>&1; then
            log_success "protobuf compilada"
            ((compiled_libs++))
        else
            log_warning "protobuf falló"
            failed_libs+=("protobuf")
        fi

        # spdlog
        log_substep "Compilando spdlog..."
        if library_path="spdlog" ../build-helpers/libraries/build_spdlog.sh >> "$LOG_FILE.libraries" 2>&1; then
            log_success "spdlog compilada"
            ((compiled_libs++))
        else
            log_warning "spdlog falló"
            failed_libs+=("spdlog")
        fi

        # StringVariable
        log_substep "Compilando StringVariable..."
        if library_path="StringVariable" ../build-helpers/libraries/build_stringvariable.sh >> "$LOG_FILE.libraries" 2>&1; then
            log_success "StringVariable compilada"
            ((compiled_libs++))
        else
            log_warning "StringVariable falló"
            failed_libs+=("StringVariable")
        fi

        # yaml-cpp
        log_substep "Compilando yaml-cpp..."
        if library_path="yaml-cpp" ../build-helpers/libraries/build_yamlcpp.sh >> "$LOG_FILE.libraries" 2>&1; then
            log_success "yaml-cpp compilada"
            ((compiled_libs++))
        else
            log_warning "yaml-cpp falló"
            failed_libs+=("yaml-cpp")
        fi

        # jemalloc
        log_substep "Compilando jemalloc..."
        if library_path="jemalloc" ../build-helpers/libraries/build_jemalloc.sh >> "$LOG_FILE.libraries" 2>&1; then
            log_success "jemalloc compilada"
            ((compiled_libs++))
        else
            log_warning "jemalloc falló"
            failed_libs+=("jemalloc")
        fi

        # zstd
        log_substep "Compilando zstd..."
        if library_path="zstd" ../build-helpers/libraries/build_zstd.sh >> "$LOG_FILE.libraries" 2>&1; then
            log_success "zstd compilada"
            ((compiled_libs++))
        else
            log_warning "zstd falló"
            failed_libs+=("zstd")
        fi

        # breakpad (último porque se cuelga - con timeout de 10 min)
        log_substep "Compilando breakpad (puede tardar)..."
        if timeout 600 bash -c 'library_path="breakpad" ../build-helpers/libraries/build_breakpad.sh' >> "$LOG_FILE.libraries" 2>&1; then
            log_success "breakpad compilada"
            ((compiled_libs++))
        else
            log_warning "breakpad falló o excedió timeout (10 min)"
            failed_libs+=("breakpad")
        fi

        # Resumen final
        echo ""
        echo "═══════════════════════════════════════════════════════════"
        if [[ ${#failed_libs[@]} -eq 0 ]]; then
            log_success "TODAS las librerías ($total_libs/$total_libs) compiladas exitosamente"
        else
            log_warning "Librerías compiladas: $compiled_libs/$total_libs"
            log_error "Librerías que FALLARON (${#failed_libs[@]}): ${failed_libs[*]}"
            log_error "Ver detalles completos en: $LOG_FILE.libraries"
            echo ""
            log_warning "¿Deseas continuar de todas formas?"
            log_info "Las librerías críticas (tommath, tomcrypt, Thread-Pool) están OK"
            read -p "Continuar con la compilación de TeaSpeak? [s/N]: " -n 1 -r
            echo
            if [[ ! $REPLY =~ ^[Ss]$ ]]; then
                log_error "Instalación abortada por el usuario"
                exit 1
            fi
        fi
        echo "═══════════════════════════════════════════════════════════"
    else
        log_error "build_helper.sh no encontrado"
        exit 1
    fi

    cd "$SCRIPT_DIR"
}

# ═══════════════════════════════════════════════════════════════════════════
# PASO 10: Compilar TeaSpeak
# ═══════════════════════════════════════════════════════════════════════════
compile_teaspeak() {
    log_step "PASO 10: Compilando TeaSpeak ($BUILD_TYPE)"

    cd "$SCRIPT_DIR/Server/Root"

    # Configurar variables de entorno
    export build_os_type=linux
    export build_os_arch=amd64
    export CMAKE_MAKE_OPTIONS="-j$(nproc)"

    log_info "Build type: $BUILD_TYPE"
    log_info "Usando $(nproc) núcleos"
    log_info "Esto puede tardar 10-30 minutos..."

    if [[ -f "build_teaspeak.sh" ]]; then
        log_substep "Ejecutando build_teaspeak.sh..."
        bash build_teaspeak.sh "$BUILD_TYPE" 2>&1 | tee "$LOG_FILE.teaspeak"

        if [[ ${PIPESTATUS[0]} -eq 0 ]]; then
            log_success "TeaSpeak compilado exitosamente!"
        else
            log_error "La compilación falló"
            log_error "Ver logs en: $LOG_FILE.teaspeak"
            exit 1
        fi
    else
        log_error "build_teaspeak.sh no encontrado"
        exit 1
    fi

    cd "$SCRIPT_DIR"
}

# ═══════════════════════════════════════════════════════════════════════════
# PASO 11: Verificar resultado
# ═══════════════════════════════════════════════════════════════════════════
verify_build() {
    log_step "PASO 11: Verificando Compilación"

    local build_dir="$SCRIPT_DIR/Server/Root/TeaSpeak/Server/server/out/linux_amd64"

    if [[ -f "$build_dir/TeaSpeakServer" ]]; then
        local size=$(du -h "$build_dir/TeaSpeakServer" | cut -f1)
        log_success "TeaSpeakServer encontrado ($size)"

        log_substep "Verificando versión..."
        "$build_dir/TeaSpeakServer" --version 2>&1 || true
    else
        log_warning "TeaSpeakServer no encontrado en la ubicación esperada"
        log_info "Buscando en otros directorios..."
        find "$SCRIPT_DIR/Server" -name "TeaSpeakServer" -type f 2>/dev/null || true
    fi
}

# ═══════════════════════════════════════════════════════════════════════════
# Mostrar resumen
# ═══════════════════════════════════════════════════════════════════════════
show_summary() {
    log_step "✅ INSTALACIÓN COMPLETADA"

    echo ""
    echo -e "${GREEN}╔═══════════════════════════════════════════════════════════╗${NC}"
    echo -e "${GREEN}║          TEASPEAK CONFIGURADO EXITOSAMENTE                ║${NC}"
    echo -e "${GREEN}╚═══════════════════════════════════════════════════════════╝${NC}"
    echo ""

    log_info "Configuración utilizada:"
    echo "  • Build type: $BUILD_TYPE"
    echo "  • GitHub user: ${GITHUB_USER:-ninguno (repos originales)}"
    echo "  • CPU cores: $(nproc)"
    echo ""

    log_info "Logs guardados en:"
    echo "  • Principal: $LOG_FILE"
    echo "  • Librerías: $LOG_FILE.libraries"
    echo "  • TeaSpeak: $LOG_FILE.teaspeak"
    echo ""

    log_info "Para ejecutar TeaSpeak:"
    echo "  cd $SCRIPT_DIR/Server/Root/TeaSpeak/Server/server/out/linux_amd64"
    echo "  ./TeaSpeakServer"
    echo ""

    log_info "Para recompilar:"
    echo "  cd $SCRIPT_DIR/Server/Root"
    echo "  export build_os_type=linux"
    echo "  export build_os_arch=amd64"
    echo "  bash build_teaspeak.sh $BUILD_TYPE"
    echo ""

    log_success "¡Listo para usar!"
}

# ═══════════════════════════════════════════════════════════════════════════
# FUNCIÓN PRINCIPAL
# ═══════════════════════════════════════════════════════════════════════════
main() {
    clear

    echo -e "${CYAN}"
    cat << "EOF"
╔═══════════════════════════════════════════════════════════╗
║                                                           ║
║     INSTALADOR AUTOMÁTICO DE TEASPEAK SERVER              ║
║                  VERSIÓN MEJORADA                         ║
║                                                           ║
║   • Instalación automática de dependencias                ║
║   • Soporte para forks personales en GitHub               ║
║   • Compilación optimizada                                ║
║   • Verificación completa paso a paso                     ║
║                                                           ║
╚═══════════════════════════════════════════════════════════╝
EOF
    echo -e "${NC}"
    echo ""

    # Parsear argumentos
    parse_args "$@"

    log_info "Iniciando instalación..."
    log_info "Directorio: $SCRIPT_DIR"
    if [[ -n "$GITHUB_USER" ]]; then
        log_info "GitHub user: $GITHUB_USER"
    fi
    log_info "Build type: $BUILD_TYPE"
    echo ""

    # Confirmar
    if [[ "$SKIP_DEPS" != true ]]; then
        read -p "¿Continuar con la instalación? (y/n): " -n 1 -r
        echo
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            log_info "Instalación cancelada"
            exit 0
        fi
    fi

    # Ejecutar pasos
    check_system
    install_dependencies
    install_rust
    disable_ld_gold
    verify_tools
    configure_github_repos
    download_libraries
    update_rust_cargo_dependencies "$GITHUB_USER"  # Modificar Cargo.toml para usar repos del usuario
    fix_permissions
    compile_libraries
    compile_teaspeak
    verify_build
    show_summary
}

# ═══════════════════════════════════════════════════════════════════════════
# EJECUCIÓN
# ═══════════════════════════════════════════════════════════════════════════
main "$@"
