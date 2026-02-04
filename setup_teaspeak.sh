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
CONFIG_FILE="$SCRIPT_DIR/.teaspeak.conf"
GITHUB_USER=""
GITHUB_TOKEN=""
SKIP_DEPS=false
SKIP_LIBS=false
BUILD_TYPE="stable"
REQUIRED_OPENSSL_VERSION="3.0"
MIN_GCC_VERSION="9"
LOG_FILE="/tmp/teaspeak_setup_$(date +%Y%m%d_%H%M%S).log"

# ═══════════════════════════════════════════════════════════════════════════
# Funciones de configuración
# ═══════════════════════════════════════════════════════════════════════════
load_config() {
    if [[ -f "$CONFIG_FILE" ]]; then
        source "$CONFIG_FILE"
        if [[ -n "$SAVED_GITHUB_TOKEN" ]]; then
            GITHUB_TOKEN="$SAVED_GITHUB_TOKEN"
        fi
        if [[ -n "$SAVED_GITHUB_USER" ]]; then
            GITHUB_USER="$SAVED_GITHUB_USER"
        fi
    fi
}

save_config() {
    cat > "$CONFIG_FILE" << EOF
# Configuración de TeaSpeak - Generado automáticamente
# ⚠️  NO COMPARTIR ESTE ARCHIVO (contiene token de autenticación privado)
SAVED_GITHUB_USER="$GITHUB_USER"
SAVED_GITHUB_TOKEN="$GITHUB_TOKEN"
EOF
    chmod 600 "$CONFIG_FILE"
}

# ═══════════════════════════════════════════════════════════════════════════
# Validar token de GitHub
# ═══════════════════════════════════════════════════════════════════════════
validate_github_token() {
    local token="$1"
    local user="$2"

    echo ""
    log_substep "Validando token de acceso..."

    # Lista de repositorios requeridos
    local repos=(
        "TeaSpeak"
        "TeaSpeakLibrary"
        "TeaMusic-Providers"
    )

    local all_valid=true

    for repo in "${repos[@]}"; do
        # Usar GitHub API para verificar acceso (más confiable que git ls-remote)
        local api_url="https://api.github.com/repos/${user}/${repo}"
        local http_code=$(curl -s -o /dev/null -w "%{http_code}" \
            -H "Authorization: token ${token}" \
            "${api_url}")

        if [[ "$http_code" == "200" ]]; then
            echo -e "  ${GREEN}✓${NC} Acceso verificado: ${user}/${repo}"
        else
            echo -e "  ${RED}✗${NC} Sin acceso a: ${user}/${repo} (HTTP ${http_code})"
            all_valid=false
        fi
    done

    echo ""

    if [[ "$all_valid" == "true" ]]; then
        log_success "Token válido - Acceso verificado a todos los repositorios"
        # Configurar git credential helper para cachear el token durante esta sesión
        git config --global credential.helper 'cache --timeout=86400'
        return 0
    else
        log_error "Token inválido o sin permisos suficientes"
        echo ""
        echo -e "${YELLOW}Posibles causas:${NC}"
        echo "  • El token ha expirado"
        echo "  • El token no tiene permisos 'repo'"
        echo "  • No tienes acceso a los repositorios privados de ${user}"
        echo "  • Tu licencia/acceso ha finalizado"
        echo ""
        return 1
    fi
}

request_github_credentials() {
    echo ""
    echo "╔═══════════════════════════════════════════════════════════╗"
    echo "║       🔐 AUTENTICACIÓN DE TEASPEAK REQUERIDA             ║"
    echo "╚═══════════════════════════════════════════════════════════╝"
    echo ""
    echo -e "${CYAN}Este software requiere autenticación para acceder a los${NC}"
    echo -e "${CYAN}repositorios privados necesarios para la instalación.${NC}"
    echo ""

    # Solicitar usuario de GitHub si no está configurado
    if [[ -z "$GITHUB_USER" ]]; then
        echo -e "${YELLOW}Usuario de GitHub del propietario de los repositorios:${NC}"
        read -p "Ingresa el usuario de GitHub: " GITHUB_USER

        if [[ -z "$GITHUB_USER" ]]; then
            echo -e "${RED}✗ Error: El usuario de GitHub es obligatorio${NC}"
            exit 1
        fi
    else
        echo -e "${GREEN}✓ Usuario configurado: $GITHUB_USER${NC}"
    fi

    # Solicitar token de GitHub - OBLIGATORIO
    if [[ -z "$GITHUB_TOKEN" ]]; then
        echo ""
        echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
        echo -e "${YELLOW}📋 TOKEN DE ACCESO REQUERIDO${NC}"
        echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
        echo ""
        echo "Para obtener un token de acceso válido:"
        echo ""
        echo "  🔹 Si ya tienes una licencia/acceso:"
        echo "     Usa el token que se te proporcionó al momento de la compra"
        echo ""
        echo "  🔹 Si necesitas adquirir acceso:"
        echo "     Contacta con el proveedor del software"
        echo ""
        echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
        echo ""
        read -sp "Ingresa tu token de acceso (ghp_xxxxx): " GITHUB_TOKEN
        echo ""

        if [[ -z "$GITHUB_TOKEN" ]]; then
            echo ""
            echo -e "${RED}✗ Error: El token de acceso es obligatorio para continuar${NC}"
            echo -e "${YELLOW}  La instalación no puede proceder sin autenticación válida${NC}"
            exit 1
        fi
    else
        echo -e "${GREEN}✓ Token configurado (${GITHUB_TOKEN:0:7}...)${NC}"
    fi

    echo ""

    # Validar que el token tenga acceso
    if ! validate_github_token "$GITHUB_TOKEN" "$GITHUB_USER"; then
        echo -e "${RED}✗ Autenticación fallida${NC}"
        echo ""
        echo "La instalación no puede continuar sin un token válido."
        exit 1
    fi

    # Guardar configuración solo si la validación fue exitosa
    save_config
    echo ""
    echo -e "${GREEN}✓ Autenticación exitosa - Configuración guardada${NC}"
    echo -e "${CYAN}  Archivo: $CONFIG_FILE${NC}"
    echo -e "${CYAN}  El token se reutilizará automáticamente en futuras ejecuciones${NC}"
    echo ""
}

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
            --github-token)
                GITHUB_TOKEN="$2"
                shift 2
                ;;
            --skip-deps)
                SKIP_DEPS=true
                shift
                ;;
            --skip-libs)
                SKIP_LIBS=true
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
╔═══════════════════════════════════════════════════════════════════╗
║              INSTALADOR DE TEASPEAK SERVER                        ║
║              Sistema de Autenticación Requerido                   ║
╚═══════════════════════════════════════════════════════════════════╝

Uso: $0 [opciones]

AUTENTICACIÓN:
  Este software requiere autenticación válida para acceder a los
  repositorios privados necesarios para la compilación.

  Al ejecutar el script por primera vez, se solicitará:
    • Usuario de GitHub (propietario de los repositorios)
    • Token de acceso (proporcionado con tu licencia)

  El token se guardará localmente y se reutilizará automáticamente
  en ejecuciones futuras.

OPCIONES:
  --github-user <username>  Usuario de GitHub (ej: jorgebarreraa)
  --github-token <token>    Token de acceso (formato: ghp_xxxxx)
  --skip-deps               Saltar instalación de dependencias del sistema
  --skip-libs               Saltar compilación de librerías (usar cache)
  --build-type <type>       Tipo de build (default: stable)
  --help                    Mostrar esta ayuda

TIPOS DE BUILD:
  stable     - Build de producción estable (recomendado)
  optimized  - Build optimizado con máximo rendimiento
  debug      - Build con símbolos de debugging
  nightly    - Build experimental con últimas características

EJEMPLOS:
  # Primera instalación (solicitará credenciales interactivamente)
  $0

  # Con credenciales en línea de comandos
  $0 --github-user jorgebarreraa --github-token ghp_xxxxx

  # Build optimizado reutilizando credenciales guardadas
  $0 --build-type optimized

  # Reinstalación rápida (sin recompilar librerías)
  $0 --skip-libs

NOTAS:
  • El token se guarda en: .teaspeak.conf (permisos 600)
  • NO compartas el archivo .teaspeak.conf
  • Si el token expira, el script lo detectará y solicitará uno nuevo
  • Los logs se guardan en: /tmp/teaspeak_setup_*.log

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
        software-properties-common \
        golang-go

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

    # Verificar que MySQL client dev esté instalado (crítico para TeaSpeak)
    log_substep "Verificando instalación de MySQL client dev..."
    if ! dpkg -l | grep -q "libmysqlclient-dev\|default-libmysqlclient-dev\|libmariadb-dev"; then
        log_warning "MySQL client dev no detectado, intentando instalación alternativa..."

        # Intentar diferentes paquetes en orden de prioridad
        if $SUDO apt-get install -y libmysqlclient-dev 2>/dev/null; then
            log_success "libmysqlclient-dev instalado"
        elif $SUDO apt-get install -y default-libmysqlclient-dev 2>/dev/null; then
            log_success "default-libmysqlclient-dev instalado"
        elif $SUDO apt-get install -y libmariadb-dev libmariadb-dev-compat 2>/dev/null; then
            log_success "libmariadb-dev instalado"
        else
            log_warning "No se pudo instalar MySQL client dev automáticamente"
            log_info "El módulo CMake está configurado para buscar en rutas estándar"
            log_info "Si la compilación falla, instala manualmente: sudo apt-get install libmysqlclient-dev"
        fi
    else
        log_success "MySQL client dev ya está instalado"
    fi

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

    # Verificar que el directorio existe antes de intentar cd
    if [[ ! -d "$SCRIPT_DIR/Server/Root/libraries" ]]; then
        log_error "ERROR: Directorio $SCRIPT_DIR/Server/Root/libraries no existe"
        log_info "SCRIPT_DIR=$SCRIPT_DIR"
        log_info "pwd=$(pwd)"
        exit 1
    fi

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

    # Verificación especial para build-helpers
    if [[ "$dir" == "build-helpers" ]]; then
        if [[ -d "$dir" && ! -f "$dir/build_helper.sh" ]]; then
            echo "  ⚠ $dir existe pero está incompleto, eliminando..."
            rm -rf "$dir"
        fi
    fi

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
clone_with_fallback "https://github.com/jorgebarreraa/CXXTerminal.git" "CXXTerminal"
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

clone_with_fallback "https://github.com/jorgebarreraa/spdlog.git" "spdlog"
clone_with_fallback "https://github.com/jorgebarreraa/StringVariable.git" "StringVariable"
clone_with_fallback "https://github.com/jorgebarreraa/ed25519.git" "ed25519"
clone_with_fallback "https://chromium.googlesource.com/breakpad/breakpad" "breakpad"

# Checkout específico para breakpad (compatible con C++17)
if [ -d "breakpad" ]; then
    echo "  Configurando breakpad..."
    (cd breakpad && git fetch --unshallow 2>/dev/null || true && git checkout f032e4c3 2>/dev/null || true)
fi

clone_with_fallback "https://boringssl.googlesource.com/boringssl" "boringssl"
clone_with_fallback "https://github.com/protocolbuffers/protobuf.git" "protobuf" "v21.12"
clone_with_fallback "https://github.com/jorgebarreraa/DataPipes.git" "DataPipes"
clone_with_fallback "https://github.com/jemalloc/jemalloc.git" "jemalloc" "dev"
clone_with_fallback "https://github.com/jorgebarreraa/libnice-prebuild.git" "libnice"
clone_with_fallback "https://github.com/jorgebarreraa/glibc.git" "glibc"
clone_with_fallback "https://github.com/jorgebarreraa/openssl-prebuild.git" "openssl-prebuild"
clone_with_fallback "https://github.com/facebook/zstd.git" "zstd"

# build-helpers
cd ..
clone_with_fallback "https://github.com/jorgebarreraa/build-helpers.git" "build-helpers"

# Asegurar que build-helpers tenga todos los archivos (puede haber sido parcialmente clonado antes)
if [[ -d "build-helpers/.git" ]]; then
    echo "  Verificando integridad de build-helpers..."
    cd build-helpers
    git reset --hard HEAD >/dev/null 2>&1
    cd ..
fi

# Sobrescribir build_jsoncpp.sh de build-helpers con versión C++17
# NOTE: This file no longer exists in libraries/, build-helpers has the correct version
# echo "Sobrescribiendo build_jsoncpp.sh con versión C++17..."
# cp -f libraries/build_jsoncpp.sh build-helpers/libraries/build_jsoncpp.sh

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

    # Limpiar librerías que cambiaron de URL a fork del usuario
    log_substep "Limpiando librerías con URLs actualizadas..."
    # Limpiar TODAS las librerías que cambiaron de URL (WolverinDEV y git.did.science -> jorgebarreraa)
    local libs_to_clean=(
        "DataPipes"
        "tommath"
        "tomcrypt"
        "Thread-Pool"
        "CXXTerminal"
        "spdlog"
        "StringVariable"
        "ed25519"
        "libnice"
        "glibc"
        "openssl-prebuild"
    )

    # build-helpers se maneja en el directorio padre
    local build_helpers_to_clean=(
        "build-helpers"
    )

    for lib in "${libs_to_clean[@]}"; do
        if [[ -d "$lib" ]]; then
            cd "$lib" 2>/dev/null && {
                local remote_url=$(git remote get-url origin 2>/dev/null || echo "")
                if [[ "$remote_url" != *"jorgebarreraa"* && "$remote_url" != "" ]]; then
                    cd ..
                    log_info "Borrando $lib (URL antigua: ${remote_url%%/git*})"
                    rm -rf "$lib"
                else
                    cd ..
                fi
            }
        fi
    done

    # Limpiar build-helpers en el directorio padre
    cd "$SCRIPT_DIR/Server/Root"
    for lib in "${build_helpers_to_clean[@]}"; do
        if [[ -d "$lib" ]]; then
            cd "$lib" 2>/dev/null && {
                local remote_url=$(git remote get-url origin 2>/dev/null || echo "")
                if [[ "$remote_url" != *"jorgebarreraa"* && "$remote_url" != "" ]]; then
                    cd ..
                    log_info "Borrando $lib (URL antigua: ${remote_url%%/git*})"
                    rm -rf "$lib"
                else
                    cd ..
                fi
            }
        fi
    done
    cd "$SCRIPT_DIR/Server/Root/libraries"

    log_success "Todas las URLs verificadas y limpias (12 librerías monitoreadas)"

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

    cd "$SCRIPT_DIR/Server/Root/libraries" || {
        log_error "No se pudo cambiar al directorio: $SCRIPT_DIR/Server/Root/libraries"
        log_error "SCRIPT_DIR=$SCRIPT_DIR"
        log_error "Directorio actual: $(pwd)"
        exit 1
    }

    # Archivo marker de compilación exitosa
    local success_marker="$SCRIPT_DIR/Server/Root/libraries/.libraries_compiled_successfully"

    # Si se especificó --skip-libs, saltar sin preguntar
    if [[ "$SKIP_LIBS" == true ]]; then
        log_info "Flag --skip-libs detectado, saltando compilación de librerías"
        cd "$SCRIPT_DIR"
        return 0
    fi

    # Verificar si existe marker de compilación exitosa anterior
    if [[ -f "$success_marker" ]]; then
        log_success "✓ Librerías compiladas previamente con éxito"
        log_info "Saltando compilación de librerías (usando cache automático)"
        log_info "Para forzar recompilación, elimina: $success_marker"
        cd "$SCRIPT_DIR"
        return 0
    fi

    # Si llegamos aquí, necesitamos compilar
    log_info "Iniciando compilación de librerías..."
    log_info "El éxito será registrado automáticamente para futuras ejecuciones"

    # Eliminar marker de éxito anterior (si existe por alguna razón)
    rm -f "$success_marker"

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

    # Verificar que build_helper.sh existe
    if [[ ! -f "$SCRIPT_DIR/Server/Root/build-helpers/build_helper.sh" ]]; then
        log_error "build_helper.sh no encontrado en: $SCRIPT_DIR/Server/Root/build-helpers/"
        log_error "El directorio build-helpers parece estar corrupto o incompleto"
        log_error ""
        log_error "Para solucionar esto, ejecuta:"
        log_error "  rm -rf $SCRIPT_DIR/Server/Root/build-helpers"
        log_error "  ./setup_teaspeak.sh"
        exit 1
    fi

    # Cargar build_helper.sh
    log_info "Cargando build_helper.sh..."
    source "$SCRIPT_DIR/Server/Root/build-helpers/build_helper.sh"

    # Limpiar cachés de compilaciones previas fallidas
        log_substep "Limpiando cachés de compilaciones anteriores..."
        find . -maxdepth 2 -type d -name "_build" -exec rm -rf {} + 2>/dev/null || true
        find . -maxdepth 2 -type d -name "build" -exec rm -rf {} + 2>/dev/null || true
        find . -maxdepth 3 -path "*/out/linux_amd64" -type d -exec rm -rf {} + 2>/dev/null || true
        find . -maxdepth 2 -name ".build_linux_amd64.txt" -delete 2>/dev/null || true
        log_success "Cachés eliminados"

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
        local total_libs=17
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

        # BoringSSL (REQUERIDO por DataPipes)
        log_substep "Compilando BoringSSL..."
        if library_path="boringssl" ../build-helpers/libraries/build_boringssl.sh >> "$LOG_FILE.libraries" 2>&1; then
            log_success "BoringSSL compilada"
            ((compiled_libs++))
        else
            log_warning "BoringSSL falló"
            failed_libs+=("BoringSSL")
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

        # DataPipes (requiere BoringSSL)
        log_substep "Compilando DataPipes..."
        if library_path="DataPipes" ../build-helpers/libraries/build_datapipes.sh >> "$LOG_FILE.libraries" 2>&1; then
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

        # jsoncpp (compila con C++11 - código TeaSpeak maneja compatibilidad C++17)
        log_substep "Compilando jsoncpp..."
        # Limpiar para forzar recompilación limpia
        rm -rf jsoncpp/build 2>/dev/null || true
        rm -f jsoncpp/.build_linux_amd64.txt 2>/dev/null || true
        # Limpiar archivos instalados anteriormente
        sudo rm -rf /usr/local/include/json 2>/dev/null || true
        sudo rm -f /usr/local/lib/libjsoncpp* 2>/dev/null || true
        sudo rm -rf /usr/local/lib/cmake/jsoncpp 2>/dev/null || true
        if library_path="jsoncpp" ../build-helpers/libraries/build_jsoncpp.sh >> "$LOG_FILE.libraries" 2>&1; then
            # Verificar que la librería se instaló correctamente
            # NOTA: jsoncpp se compila con C++11 (sin string_view) - esto es correcto
            # Los parches en el código fuente (PARCHE 26/26d) manejan la compatibilidad
            if [[ -f jsoncpp/out/linux_amd64/lib/libjsoncpp.a ]] || [[ -f jsoncpp/out/linux_amd64/lib/libjsoncpp.so ]]; then
                log_success "jsoncpp compilada (C++11 - compatibilidad manejada por parches)"
                ((compiled_libs++))
            else
                log_warning "jsoncpp compiló pero archivos no encontrados en jsoncpp/out/linux_amd64/lib/"
                failed_libs+=("jsoncpp (archivos no encontrados)")
            fi
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

        # protobuf (OPCIONAL - el sistema usa protobuf 3.21.12)
        log_substep "Compilando protobuf..."
        if library_path="protobuf" ../build-helpers/libraries/build_protobuf.sh >> "$LOG_FILE.libraries" 2>&1; then
            log_success "protobuf compilada"
            ((compiled_libs++))
        else
            log_warning "protobuf falló (usando protobuf del sistema: $(protoc --version 2>&1))"
            log_info "El servidor usará protobuf del sistema en su lugar"
            # No agregar a failed_libs - protobuf del sistema es suficiente
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

            # Crear marker de compilación exitosa para futuras ejecuciones
            touch "$success_marker"
            log_success "✓ Compilación registrada exitosamente"
            log_info "Futuras ejecuciones saltarán automáticamente la compilación de librerías"
        else
            log_warning "Librerías compiladas: $compiled_libs/$total_libs"
            log_error "Librerías que FALLARON (${#failed_libs[@]}): ${failed_libs[*]}"
            log_error "Ver detalles completos en: $LOG_FILE.libraries"
            echo ""
            log_error "La compilación de librerías no fue completamente exitosa"
            log_info "En la próxima ejecución se volverán a compilar automáticamente"
            exit 1
        fi
        echo "═══════════════════════════════════════════════════════════"

    cd "$SCRIPT_DIR"
}

# ═══════════════════════════════════════════════════════════════════════════
# PASO 9.5: Inicializar submódulos de TeaSpeak
# ═══════════════════════════════════════════════════════════════════════════
initialize_submodules() {
    log_step "PASO 9.5: Inicializando Submódulos de TeaSpeak"

    # El código se compila desde Server/Root/TeaSpeak/, no desde Server/Server/
    cd "$SCRIPT_DIR/Server/Root/TeaSpeak"

    # Limpiar submódulos corruptos o incompletos automáticamente
    log_substep "Verificando integridad de submódulos..."
    for submodule in shared music; do
        # Verificar si es un symlink
        if [[ -L "$submodule" ]]; then
            log_warning "Directorio '$submodule' es un enlace simbólico, eliminando..."
            rm -rf "$submodule"
        # Verificar si es un directorio sin .git
        elif [[ -d "$submodule" && ! -d "$submodule/.git" ]]; then
            log_warning "Directorio '$submodule' corrupto (sin .git), eliminando..."
            rm -rf "$submodule"
        fi
    done

    # Actualizar .gitmodules para usar repositorios de jorgebarreraa
    log_substep "Actualizando .gitmodules a repositorios de ${GITHUB_USER}..."

    if [[ -f ".gitmodules" ]]; then
        # Actualizar URL del submódulo shared
        if grep -q "path = shared" .gitmodules; then
            sed -i "s|url = .*TeaSpeakLibrary.*|url = https://github.com/${GITHUB_USER}/TeaSpeakLibrary.git|g" .gitmodules
            log_info "URL de 'shared' actualizada"
        fi

        # Actualizar URL del submódulo music
        if grep -q "path = music" .gitmodules; then
            sed -i "s|url = .*TeaMusic.*|url = https://github.com/${GITHUB_USER}/TeaMusic-Providers.git|g" .gitmodules
            log_info "URL de 'music' actualizada"
        fi
    fi

    # Verificar y clonar submódulo 'shared' (TeaSpeakLibrary)
    log_substep "Verificando submódulo 'shared'..."
    if [[ -d "shared/.git" ]]; then
        cd shared
        local remote_url=$(git remote get-url origin 2>/dev/null || echo "")
        if [[ "$remote_url" != *"${GITHUB_USER}"* && "$remote_url" != "" ]]; then
            cd ..
            log_warning "Submódulo 'shared' tiene URL antigua, eliminando..."
            rm -rf shared
        else
            cd ..
            log_success "Submódulo 'shared' ya existe con URL correcta"
        fi
    fi

    if [[ ! -d "shared/.git" ]]; then
        log_info "Clonando TeaSpeakLibrary desde ${GITHUB_USER}..."
        if [[ -n "${GITHUB_TOKEN}" ]]; then
            # Usar GIT_TERMINAL_PROMPT=0 para evitar prompts interactivos
            # Usar timeout para evitar bloqueos indefinidos
            # Mostrar progreso al usuario mientras clona
            (
                GIT_TERMINAL_PROMPT=0 timeout 120 git clone --progress \
                    "https://${GITHUB_TOKEN}@github.com/${GITHUB_USER}/TeaSpeakLibrary.git" \
                    shared 2>&1 | tee -a "$LOG_FILE" | grep -E "Cloning|Receiving|Resolving|done" || true
            ) &
            local clone_pid=$!

            # Mostrar indicador de progreso mientras clona
            local dots=0
            while kill -0 $clone_pid 2>/dev/null; do
                echo -n "."
                sleep 2
                dots=$((dots + 1))
                if [ $dots -ge 60 ]; then
                    echo ""
                    log_warning "La clonación está tardando más de lo esperado..."
                    dots=0
                fi
            done
            wait $clone_pid
            local clone_result=$?
            echo "" # Nueva línea después de los puntos

            if [ $clone_result -eq 0 ]; then
                :  # Continuar con la verificación normal
            else
                log_error "Falló la clonación de 'shared' (código de salida: $clone_result)"
                exit 1
            fi
        else
            log_error "Token de GitHub no disponible"
            log_error "El repositorio TeaSpeakLibrary es privado y requiere autenticación"
            exit 1
        fi

        if [[ -d "shared/.git" ]]; then
            log_success "Submódulo 'shared' clonado exitosamente"
        else
            log_error "Falló la clonación de 'shared'"
            log_error "Verifica que:"
            log_error "  • El repositorio ${GITHUB_USER}/TeaSpeakLibrary exista"
            log_error "  • El token tenga acceso a repositorios privados"
            log_error "  • La conexión a internet esté funcionando"
            exit 1
        fi
    fi

    # Verificar y clonar submódulo 'music' (TeaMusic-Providers)
    log_substep "Verificando submódulo 'music'..."
    if [[ -d "music/.git" ]]; then
        cd music
        local remote_url=$(git remote get-url origin 2>/dev/null || echo "")
        if [[ "$remote_url" != *"${GITHUB_USER}"* && "$remote_url" != "" ]]; then
            cd ..
            log_warning "Submódulo 'music' tiene URL antigua, eliminando..."
            rm -rf music
        else
            cd ..
            log_success "Submódulo 'music' ya existe con URL correcta"
        fi
    fi

    if [[ ! -d "music/.git" ]]; then
        log_info "Clonando TeaMusic-Providers desde ${GITHUB_USER}..."
        if [[ -n "${GITHUB_TOKEN}" ]]; then
            # Usar GIT_TERMINAL_PROMPT=0 para evitar prompts interactivos
            # Usar timeout para evitar bloqueos indefinidos
            # Mostrar progreso al usuario mientras clona
            (
                GIT_TERMINAL_PROMPT=0 timeout 120 git clone --progress \
                    "https://${GITHUB_TOKEN}@github.com/${GITHUB_USER}/TeaMusic-Providers.git" \
                    music 2>&1 | tee -a "$LOG_FILE" | grep -E "Cloning|Receiving|Resolving|done" || true
            ) &
            local clone_pid=$!

            # Mostrar indicador de progreso mientras clona
            local dots=0
            while kill -0 $clone_pid 2>/dev/null; do
                echo -n "."
                sleep 2
                dots=$((dots + 1))
                if [ $dots -ge 60 ]; then
                    echo ""
                    log_warning "La clonación está tardando más de lo esperado..."
                    dots=0
                fi
            done
            wait $clone_pid
            local clone_result=$?
            echo "" # Nueva línea después de los puntos

            if [ $clone_result -eq 0 ]; then
                :  # Continuar con la verificación normal
            else
                log_error "Falló la clonación de 'music' (código de salida: $clone_result)"
                exit 1
            fi
        else
            log_error "Token de GitHub no disponible"
            log_error "El repositorio TeaMusic-Providers es privado y requiere autenticación"
            exit 1
        fi

        if [[ -d "music/.git" ]]; then
            log_success "Submódulo 'music' clonado exitosamente"
        else
            log_error "Falló la clonación de 'music'"
            log_error "Verifica que:"
            log_error "  • El repositorio ${GITHUB_USER}/TeaMusic-Providers exista"
            log_error "  • El token tenga acceso a repositorios privados"
            log_error "  • La conexión a internet esté funcionando"
            exit 1
        fi
    fi

    # Aplicar parches de compilación usando el script centralizado
    log_substep "Aplicando parches de compilación..."

    cd "$SCRIPT_DIR"
    if [[ -f "apply_compilation_patches.sh" ]]; then
        bash apply_compilation_patches.sh >> "$LOG_FILE" 2>&1
        if [[ $? -eq 0 ]]; then
            log_success "✓ Parches de compilación aplicados correctamente"
        else
            log_error "✗ Error al aplicar parches de compilación"
            log_error "Ver detalles en: $LOG_FILE"
            exit 1
        fi
    else
        log_error "apply_compilation_patches.sh no encontrado"
        exit 1
    fi

    log_success "Submódulos inicializados correctamente"
    cd "$SCRIPT_DIR"
}

# ═══════════════════════════════════════════════════════════════════════════
# PASO 9.6: Parchar rutas de librerías en CMakeLists.txt principal
# ═══════════════════════════════════════════════════════════════════════════
# NOTA: Este paso ahora se ejecuta automáticamente en apply_compilation_patches.sh
# que se llama desde PASO 9.5. Se mantiene la función vacía para no romper la
# secuencia de pasos.
patch_cmake_library_paths() {
    log_step "PASO 9.6: Parches de CMakeLists.txt (ya aplicados en PASO 9.5)"
    log_success "Los parches se aplicaron automáticamente con apply_compilation_patches.sh"
}

# ═══════════════════════════════════════════════════════════════════════════
# PASO 9.7: Aplicar parches a módulos CMake
# ═══════════════════════════════════════════════════════════════════════════
sync_cmake_modules() {
    log_step "PASO 9.7: Aplicando Parches a Módulos CMake"

    cd "$SCRIPT_DIR"

    if [[ -f "apply_cmake_patches.sh" ]]; then
        log_substep "Ejecutando script de parches CMake..."
        bash apply_cmake_patches.sh >> "$LOG_FILE" 2>&1
        log_success "Parches CMake aplicados"
    else
        log_warning "apply_cmake_patches.sh no encontrado, omitiendo parches"
    fi

    cd "$SCRIPT_DIR"
}

# ═══════════════════════════════════════════════════════════════════════════
# PASO 10: Compilar TeaSpeak
# ═══════════════════════════════════════════════════════════════════════════
compile_teaspeak() {
    log_step "PASO 10: Compilando TeaSpeak ($BUILD_TYPE)"

    cd "$SCRIPT_DIR/Server/Root"

    # Limpiar build del servidor para forzar recompilación con JsonCpp C++17
    log_substep "Limpiando build previo del servidor..."
    rm -rf TeaSpeak/server/build 2>/dev/null || true
    log_info "Build del servidor limpiado - se recompilará con JsonCpp C++17"

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

    # Cargar configuración guardada (si existe)
    load_config

    # Solicitar credenciales de GitHub si no están configuradas
    # Los parámetros --github-user y --github-token tienen prioridad sobre el archivo de config
    if [[ -z "$GITHUB_USER" ]] || [[ -z "$GITHUB_TOKEN" ]]; then
        request_github_credentials
    else
        # Si las credenciales ya están configuradas (por argumentos o archivo), validarlas
        echo ""
        echo "╔═══════════════════════════════════════════════════════════╗"
        echo "║       🔐 VERIFICANDO AUTENTICACIÓN                        ║"
        echo "╚═══════════════════════════════════════════════════════════╝"
        echo -e "${GREEN}✓ Usuario configurado: $GITHUB_USER${NC}"
        echo -e "${GREEN}✓ Token configurado (${GITHUB_TOKEN:0:7}...)${NC}"

        if ! validate_github_token "$GITHUB_TOKEN" "$GITHUB_USER"; then
            echo -e "${RED}✗ La autenticación guardada ya no es válida${NC}"
            echo ""
            # Limpiar configuración inválida
            rm -f "$CONFIG_FILE"
            # Solicitar nuevas credenciales
            request_github_credentials
        fi
    fi

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

    # Verificar que el directorio Server/ existe (parte del repositorio)
    if [[ ! -d "$SCRIPT_DIR/Server" ]]; then
        log_warning "Directorio Server/ no encontrado"
        log_info "Restaurando Server/ desde el repositorio Git..."

        cd "$SCRIPT_DIR"
        if git checkout HEAD -- Server/ >> "$LOG_FILE" 2>&1; then
            log_success "✓ Directorio Server/ restaurado exitosamente"
        else
            log_error "No se pudo restaurar Server/ desde Git"
            log_error "Por favor ejecuta: git checkout HEAD -- Server/"
            exit 1
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
    initialize_submodules       # Clonar submódulos TeaSpeakLibrary y TeaMusic-Providers
    patch_cmake_library_paths   # Parchar rutas de librerías en CMakeLists.txt principal
    sync_cmake_modules          # Sincronizar módulos CMake actualizados desde GitHub
    compile_teaspeak
    verify_build
    show_summary
}

# ═══════════════════════════════════════════════════════════════════════════
# EJECUCIÓN
# ═══════════════════════════════════════════════════════════════════════════
main "$@"
