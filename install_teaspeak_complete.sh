#!/bin/bash
#
# ═══════════════════════════════════════════════════════════════════════
#  Script de Instalación Completa de TeaSpeak - CORREGIDO
# ═══════════════════════════════════════════════════════════════════════
#
#  Este script instala AUTOMÁTICAMENTE todo el entorno necesario para
#  compilar TeaSpeak Server con todas las características habilitadas.
#
#  Lo que hace:
#  1. Verifica el sistema operativo
#  2. Instala TODAS las dependencias (gcc, cmake, meson, autoconf, etc)
#  3. Instala Rust (cargo, rustc) - requerido para compilación
#  4. Deshabilita ld.gold (previene errores de compilación)
#  5. Verifica versiones de herramientas (GCC 13+, OpenSSL 3.0, etc)
#  6. Clona el repositorio con la rama correcta y TODOS los submódulos
#  7. Arregla permisos de scripts
#  8. Compila todas las librerías necesarias con -fPIC
#  9. Configura entorno de compilación
#  10. Compila TeaSpeak en modo STABLE
#  11. Verifica que la compilación fue exitosa
#  12. Muestra resumen final con ubicación de binarios
#
#  Uso:
#    bash install_teaspeak_complete.sh [directorio_instalacion]
#
#  Ejemplo:
#    bash install_teaspeak_complete.sh /opt/TeaSpeak
#    bash install_teaspeak_complete.sh ~/TeaSpeak
#    bash install_teaspeak_complete.sh   # Usa /opt/TeaSpeak por defecto
#
# ═══════════════════════════════════════════════════════════════════════

set -e  # Salir si hay algún error

# ═══════════════════════════════════════════════════════════════════════
# Colores para output
# ═══════════════════════════════════════════════════════════════════════
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# ═══════════════════════════════════════════════════════════════════════
# Funciones de logging
# ═══════════════════════════════════════════════════════════════════════
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
    echo -e "${CYAN}═══════════════════════════════════════════════════════════${NC}"
    echo -e "${CYAN} $1${NC}"
    echo -e "${CYAN}═══════════════════════════════════════════════════════════${NC}"
}

# ═══════════════════════════════════════════════════════════════════════
# Variables globales
# ═══════════════════════════════════════════════════════════════════════
INSTALL_DIR="${1:-/opt/TeaSpeak}"
REPO_URL="https://github.com/jorgebarreraa/TeaSpeak.git"
REPO_BRANCH="main"
REQUIRED_OPENSSL_VERSION="3.0"
MIN_GCC_VERSION="13"

# ═══════════════════════════════════════════════════════════════════════
# PASO 1: Verificar sistema operativo
# ═══════════════════════════════════════════════════════════════════════
check_system() {
    log_step "PASO 1: Verificando Sistema Operativo"

    if [[ ! -f /etc/os-release ]]; then
        log_error "No se pudo detectar el sistema operativo"
        exit 1
    fi

    . /etc/os-release

    log_info "Sistema: $NAME $VERSION"
    log_info "Arquitectura: $(uname -m)"

    # Verificar que sea Linux
    if [[ "$(uname -s)" != "Linux" ]]; then
        log_error "Este script solo funciona en Linux"
        exit 1
    fi

    # Verificar arquitectura
    if [[ "$(uname -m)" != "x86_64" ]]; then
        log_warning "Arquitectura no probada: $(uname -m)"
        log_warning "Este script fue probado en x86_64"
    fi

    # Verificar Ubuntu/Debian
    if [[ "$ID" == "ubuntu" ]] || [[ "$ID" == "debian" ]]; then
        log_success "Sistema operativo compatible detectado"
    else
        log_warning "Sistema $ID no probado oficialmente"
        log_warning "Puede que necesites ajustar los nombres de paquetes"
        read -p "¿Continuar de todos modos? (y/n) " -n 1 -r
        echo
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            exit 1
        fi
    fi
}

# ═══════════════════════════════════════════════════════════════════════
# PASO 2: Instalar dependencias del sistema
# ═══════════════════════════════════════════════════════════════════════
install_dependencies() {
    log_step "PASO 2: Instalando Dependencias del Sistema"

    # Verificar si tenemos permisos de root
    if [[ $EUID -ne 0 ]]; then
        log_info "Necesitas permisos de sudo para instalar paquetes"
        SUDO="sudo"
    else
        SUDO=""
    fi

    log_info "Actualizando lista de paquetes..."
    $SUDO apt update

    log_info "Instalando herramientas de compilación..."
    $SUDO apt install -y \
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
        libtool

    log_info "Instalando librerías del sistema..."
    $SUDO apt install -y \
        libssl-dev \
        libmysqlclient-dev \
        zlib1g-dev \
        python3 \
        python3-dev \
        python3-pip

    log_success "Todas las dependencias instaladas"
}

# ═══════════════════════════════════════════════════════════════════════
# PASO 3: Instalar Rust (cargo, rustc)
# ═══════════════════════════════════════════════════════════════════════
install_rust() {
    log_step "PASO 3: Instalando Rust (cargo, rustc)"

    # Verificar si Rust ya está instalado
    if command -v cargo &> /dev/null && command -v rustc &> /dev/null; then
        log_info "Rust ya está instalado:"
        log_info "  cargo: $(cargo --version)"
        log_info "  rustc: $(rustc --version)"
        log_success "Rust ya disponible, saltando instalación"
        return
    fi

    log_info "Descargando e instalando Rust..."
    log_info "Esto puede tomar unos minutos..."

    # Descargar y ejecutar rustup
    curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y

    # Cargar el entorno de Rust
    if [[ -f "$HOME/.cargo/env" ]]; then
        source "$HOME/.cargo/env"
        log_success "Rust instalado correctamente"
        log_info "  cargo: $(cargo --version)"
        log_info "  rustc: $(rustc --version)"
    else
        log_error "Error al instalar Rust"
        exit 1
    fi
}

# ═══════════════════════════════════════════════════════════════════════
# PASO 4: Deshabilitar ld.gold (causa problemas)
# ═══════════════════════════════════════════════════════════════════════
disable_ld_gold() {
    log_step "PASO 4: Verificando y Deshabilitando ld.gold"

    if [[ -f /usr/bin/ld.gold ]]; then
        log_warning "ld.gold detectado - puede causar problemas de compilación"
        log_info "Deshabilitando ld.gold..."

        if [[ $EUID -ne 0 ]]; then
            SUDO="sudo"
        else
            SUDO=""
        fi

        $SUDO mv /usr/bin/ld.gold /usr/bin/NOT_USED_ld.gold 2>/dev/null || true
        log_success "ld.gold deshabilitado"
    else
        log_success "ld.gold no está presente (OK)"
    fi
}

# ═══════════════════════════════════════════════════════════════════════
# PASO 5: Verificar versiones de herramientas
# ═══════════════════════════════════════════════════════════════════════
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
            log_error "GCC versión $gcc_version es muy antigua (se requiere >= $MIN_GCC_VERSION)"
            all_ok=false
        fi
    else
        log_error "GCC no encontrado"
        all_ok=false
    fi

    # Verificar OpenSSL
    if command -v openssl &> /dev/null; then
        openssl_version=$(openssl version)
        log_info "OpenSSL: $openssl_version"

        if [[ $openssl_version == *"$REQUIRED_OPENSSL_VERSION"* ]]; then
            log_success "OpenSSL versión OK ($REQUIRED_OPENSSL_VERSION.x)"
        else
            log_error "OpenSSL versión incorrecta. Se requiere $REQUIRED_OPENSSL_VERSION.x"
            log_error "Tienes: $openssl_version"
            all_ok=false
        fi
    else
        log_error "OpenSSL no encontrado"
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

    # Verificar librerías
    if pkg-config --exists openssl; then
        log_success "libssl-dev instalado"
    else
        log_error "libssl-dev no encontrado"
        all_ok=false
    fi

    if pkg-config --exists mysqlclient; then
        log_success "libmysqlclient-dev instalado"
    else
        log_error "libmysqlclient-dev no encontrado"
        all_ok=false
    fi

    if pkg-config --exists zlib; then
        log_success "zlib1g-dev instalado"
    else
        log_error "zlib1g-dev no encontrado"
        all_ok=false
    fi

    # Verificar Rust (cargo, rustc)
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

    # Verificar autoconf
    if command -v autoconf &> /dev/null; then
        log_info "Autoconf: $(autoconf --version | head -1)"
        log_success "Autoconf instalado"
    else
        log_error "Autoconf no encontrado"
        all_ok=false
    fi

    if [[ "$all_ok" == false ]]; then
        log_error "Algunas verificaciones fallaron. Por favor revisa los errores arriba."
        exit 1
    fi

    log_success "Todas las herramientas verificadas correctamente"
}

# ═══════════════════════════════════════════════════════════════════════
# PASO 6: Clonar repositorio
# ═══════════════════════════════════════════════════════════════════════
clone_repository() {
    log_step "PASO 6: Clonando Repositorio TeaSpeak"

    log_info "Directorio de instalación: $INSTALL_DIR"

    if [[ -d "$INSTALL_DIR" ]]; then
        log_warning "El directorio $INSTALL_DIR ya existe"
        read -p "¿Eliminar y clonar de nuevo? (y/n) " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            log_info "Eliminando directorio existente..."
            rm -rf "$INSTALL_DIR"
        else
            log_info "Usando directorio existente"
            cd "$INSTALL_DIR"

            # Inicializar submódulos si no están
            log_info "Verificando submódulos..."
            git submodule update --init --recursive

            # CRÍTICO: Descargar librerías faltantes
            if [[ -f "Server/Root/libraries/download_libraries_custom.sh" ]]; then
                log_info "Descargando librerías faltantes..."
                cd Server/Root/libraries

                # Limpiar symlinks y directorios vacíos
                rm -f tomcrypt tommath spdlog ed25519 openssl-prebuild libraries 2>/dev/null || true
                for dir in tomcrypt tommath spdlog ed25519 openssl-prebuild; do
                    [[ -d "$dir" ]] && [[ -z "$(ls -A $dir 2>/dev/null)" ]] && rm -rf "$dir"
                done

                bash download_libraries_custom.sh
                cd "$INSTALL_DIR"
            fi

            return
        fi
    fi

    log_info "Clonando desde $REPO_URL (rama: $REPO_BRANCH)..."
    log_info "Esto puede tomar varios minutos..."

    # Clonar con la rama correcta y submódulos
    git clone -b "$REPO_BRANCH" --recurse-submodules "$REPO_URL" "$INSTALL_DIR"

    cd "$INSTALL_DIR"

    log_success "Repositorio clonado exitosamente"

    # Verificar submódulos
    submodule_count=$(git submodule status | wc -l)
    log_info "Total de submódulos git: $submodule_count"

    if [[ $submodule_count -eq 0 ]]; then
        log_warning "No se detectaron submódulos, inicializando..."
        git submodule update --init --recursive
        submodule_count=$(git submodule status | wc -l)
        log_info "Submódulos inicializados: $submodule_count"
    fi

    # CRÍTICO: Descargar TODAS las librerías adicionales
    log_info "Descargando librerías adicionales (StringVariable, event, etc)..."
    if [[ -f "Server/Root/libraries/download_libraries_custom.sh" ]]; then
        cd Server/Root/libraries

        # DEBUG: Mostrar qué hay antes de limpiar
        log_info "DEBUG - Contenido antes de limpiar:"
        ls -la | grep -E "tomcrypt|tommath|spdlog|ed25519|openssl-prebuild|libraries"

        # Limpiar enlaces simbólicos y directorios vacíos que vienen del repositorio
        log_info "Limpiando enlaces simbólicos del repositorio..."

        # Eliminar symlinks específicos que causan conflictos (forzar con -rf)
        rm -rf tomcrypt tommath spdlog ed25519 openssl-prebuild libraries 2>/dev/null || true

        # DEBUG: Mostrar qué hay después de limpiar
        log_info "DEBUG - Contenido después de limpiar:"
        ls -la | grep -E "tomcrypt|tommath|spdlog|ed25519|openssl-prebuild|libraries" || log_info "  (symlinks eliminados correctamente)"

        log_info "Ejecutando download_libraries_custom.sh..."
        bash download_libraries_custom.sh || {
            log_error "Error descargando librerías"
            log_error "DEBUG - Contenido actual:"
            ls -la
            exit 1
        }
        cd "$INSTALL_DIR"
        log_success "Todas las librerías descargadas"
    else
        log_error "download_libraries_custom.sh no encontrado"
        exit 1
    fi
}

# ═══════════════════════════════════════════════════════════════════════
# PASO 7: Arreglar permisos de scripts
# ═══════════════════════════════════════════════════════════════════════
fix_permissions() {
    log_step "PASO 7: Arreglando Permisos de Scripts"

    cd "$INSTALL_DIR"

    log_info "Dando permisos de ejecución a todos los scripts .sh ..."
    find . -name "*.sh" -exec chmod +x {} \;

    log_success "Permisos arreglados"
}

# ═══════════════════════════════════════════════════════════════════════
# PASO 8: Compilar librerías y parchear rust-webrtc
# ═══════════════════════════════════════════════════════════════════════
compile_libraries() {
    log_step "PASO 8: Compilando Librerías Necesarias"

    cd "$INSTALL_DIR/Server/Root/libraries"

    # Verificar si tenemos permisos de sudo
    if [[ $EUID -ne 0 ]]; then
        SUDO="sudo"
    else
        SUDO=""
    fi

    # Exportar variables de entorno para los scripts de compilación
    export CXX_FLAGS="-fPIC"
    export C_FLAGS="-fPIC"
    export CMAKE_BUILD_TYPE="Release"
    export CMAKE_OPTIONS=""
    export CMAKE_MAKE_OPTIONS="-j$(nproc)"

    # PASO 8.1: Pre-descargar dependencias de Rust y parchear INMEDIATAMENTE
    log_info "Pre-descargando dependencias de Rust para parchear..."
    cd "$INSTALL_DIR/Server/rtc"

    # Forzar descarga de dependencias sin compilar
    timeout 60 cargo fetch 2>/dev/null || true

    # Aplicar parche INMEDIATAMENTE después de descargar
    log_info "Aplicando parche crítico a rust-webrtc Cargo.toml..."

    # Buscar el archivo Cargo.toml de rust-webrtc
    RUST_WEBRTC_CARGO=$(find "$HOME/.cargo/git/checkouts" -type f -path "*/rust-webrtc-*/*/Cargo.toml" 2>/dev/null | head -1)

    if [[ -n "$RUST_WEBRTC_CARGO" ]] && [[ -f "$RUST_WEBRTC_CARGO" ]]; then
        log_info "Encontrado: $RUST_WEBRTC_CARGO"

        # Verificar si necesita el parche
        if grep -q '^\[dev-dependencies\.slog\]$' "$RUST_WEBRTC_CARGO"; then
            if ! grep -A1 '^\[dev-dependencies\.slog\]$' "$RUST_WEBRTC_CARGO" | grep -q 'version'; then
                log_warning "Aplicando parche a $RUST_WEBRTC_CARGO"

                # Hacer backup
                cp "$RUST_WEBRTC_CARGO" "$RUST_WEBRTC_CARGO.bak"

                # Aplicar parche
                sed -i '/^\[dev-dependencies\.slog\]$/a version = "2.5.2"' "$RUST_WEBRTC_CARGO"

                # Verificar que se aplicó
                if grep -A1 '^\[dev-dependencies\.slog\]$' "$RUST_WEBRTC_CARGO" | grep -q 'version'; then
                    log_success "✓ Parche aplicado y verificado correctamente"
                    log_info "Mostrando cambio:"
                    grep -A2 '^\[dev-dependencies\.slog\]$' "$RUST_WEBRTC_CARGO"
                else
                    log_error "✗ Parche falló, restaurando backup"
                    mv "$RUST_WEBRTC_CARGO.bak" "$RUST_WEBRTC_CARGO"
                    exit 1
                fi
            else
                log_success "✓ Parche ya aplicado previamente"
            fi
        fi
    else
        log_warning "rust-webrtc Cargo.toml no encontrado aún (se descargará durante compilación)"
    fi

    cd "$INSTALL_DIR/Server/Root/libraries"

    # PASO 8.2: Build BoringSSL first (required for DataPipes compatibility)
    log_info "Compilando BoringSSL (requerido para DataPipes)..."
    if [[ -f "build_boringssl.sh" ]]; then
        if [[ ! -f "boringssl/lib/libssl.a" ]]; then
            # Install Go if needed (required for BoringSSL code generation)
            if ! command -v go &>/dev/null; then
                log_info "Instalando golang-go (requerido por BoringSSL)..."
                $SUDO apt-get install -y golang-go 2>&1 | tail -5
            fi
            log_info "Construyendo BoringSSL desde código fuente..."
            bash build_boringssl.sh 2>&1 | tee /tmp/build_boringssl.log
            if [[ -f "boringssl/lib/libssl.a" ]]; then
                log_success "BoringSSL compilado exitosamente"
            else
                log_error "BoringSSL falló. Ver /tmp/build_boringssl.log"
                log_warning "Continuando sin BoringSSL (puede causar errores de enlazado)"
            fi
        else
            log_success "BoringSSL ya compilado"
        fi
    else
        log_warning "build_boringssl.sh no encontrado, saltando"
    fi

    # PASO 8.3: Compilar librerías
    log_info "Compilando librerías C/C++..."

    # Compilar usando el script principal
    if [[ -f "build.sh" ]]; then
        log_info "Usando build.sh del proyecto..."
        bash build.sh 2>&1 | tee /tmp/build_libraries.log
        _build_exit="${PIPESTATUS[0]}"
        if [[ $_build_exit -ne 0 ]]; then
            log_error "build.sh falló con código $_build_exit"
            log_error "Últimas 50 líneas del log:"
            tail -50 /tmp/build_libraries.log
            exit $_build_exit
        fi
        log_success "Librerías compiladas con build.sh"
    else
        log_warning "build.sh no encontrado, compilando manualmente..."

        # Librerías individuales
        for lib_script in build_stringvariable.sh build_jsoncpp.sh build_event.sh; do
            if [[ -f "$lib_script" ]]; then
                log_info "Ejecutando $lib_script..."
                bash "$lib_script" 2>&1 | tee "/tmp/$lib_script.log" || true
            fi
        done
    fi

    # Volver al directorio Root
    cd "$INSTALL_DIR/Server/Root"

    log_success "Compilación de librerías completada"
}

# ═══════════════════════════════════════════════════════════════════════
# PASO 9: Configurar entorno
# ═══════════════════════════════════════════════════════════════════════
setup_environment() {
    log_step "PASO 9: Configurando Entorno de Compilación"

    cd "$INSTALL_DIR/Server/Root"

    # Verificar y ejecutar setup_environment.sh si existe
    if [[ -f "setup_environment.sh" ]]; then
        log_info "Ejecutando setup_environment.sh..."
        bash setup_environment.sh
        log_success "Entorno configurado"
    else
        log_info "Configurando manualmente..."

        export build_os_type=linux
        export build_os_arch=amd64

        log_success "Variables de entorno exportadas"
    fi
}

# ═══════════════════════════════════════════════════════════════════════
# PASO 10: Compilar TeaSpeak
# ═══════════════════════════════════════════════════════════════════════
compile_teaspeak() {
    log_step "PASO 10: Compilando TeaSpeak Server (Modo STABLE)"

    cd "$INSTALL_DIR/Server/Root"

    log_info "Esto tomará varios minutos (10-20 min aproximadamente)..."
    log_info "Usando $(nproc) núcleos de CPU"

    # Exportar variables
    export build_os_type=linux
    export build_os_arch=amd64

    # Usar el script de compilación
    if [[ -f "build_teaspeak.sh" ]]; then
        log_info "Usando build_teaspeak.sh..."
        bash build_teaspeak.sh stable 2>&1 | tee /tmp/teaspeak_compile.log
    else
        log_error "No se encontró build_teaspeak.sh"
        exit 1
    fi

    # Verificar si la compilación fue exitosa
    if [[ ${PIPESTATUS[0]} -eq 0 ]]; then
        log_success "TeaSpeak compilado exitosamente!"
    else
        log_error "La compilación falló. Ver /tmp/teaspeak_compile.log para detalles"
        log_error "Últimas 50 líneas del log:"
        tail -50 /tmp/teaspeak_compile.log
        exit 1
    fi
}

# ═══════════════════════════════════════════════════════════════════════
# PASO 11: Verificar binarios
# ═══════════════════════════════════════════════════════════════════════
verify_build() {
    log_step "PASO 11: Verificando Binarios Compilados"

    cd "$INSTALL_DIR/Server/Root"

    local binaries=(
        "TeaSpeak/Server/server/out/linux_amd64/TeaSpeakServer"
        "TeaSpeak/MusicBot/provider/ffmpeg/out/linux_amd64/libProviderFFMpeg.so"
        "TeaSpeak/MusicBot/provider/yt/out/linux_amd64/libProviderYT.so"
        "TeaSpeak/MusicBot/out/linux_amd64/libTeaMusic.so"
    )

    local all_found=true

    for binary in "${binaries[@]}"; do
        if [[ -f "$binary" ]]; then
            size=$(du -h "$binary" | cut -f1)
            log_success "Encontrado: $binary ($size)"
        else
            log_warning "No encontrado: $binary"
            all_found=false
        fi
    done

    if [[ "$all_found" == true ]]; then
        log_success "Todos los binarios principales encontrados"
    else
        log_warning "Algunos binarios no fueron encontrados"
        log_warning "La compilación puede haber sido parcial"
    fi
}

# ═══════════════════════════════════════════════════════════════════════
# PASO 12: Resumen final
# ═══════════════════════════════════════════════════════════════════════
show_summary() {
    log_step "✅ INSTALACIÓN COMPLETA"

    echo ""
    echo -e "${GREEN}╔═══════════════════════════════════════════════════════════╗${NC}"
    echo -e "${GREEN}║                 TEASPEAK INSTALADO EXITOSAMENTE           ║${NC}"
    echo -e "${GREEN}╚═══════════════════════════════════════════════════════════╝${NC}"
    echo ""

    log_info "Directorio de instalación:"
    echo "  $INSTALL_DIR"
    echo ""

    log_info "Binario principal:"
    echo "  $INSTALL_DIR/Server/Root/TeaSpeak/Server/server/out/linux_amd64/TeaSpeakServer"
    echo ""

    log_info "Para ejecutar TeaSpeak:"
    echo "  cd $INSTALL_DIR/Server/Root/TeaSpeak/Server/server/out/linux_amd64"
    echo "  ./TeaSpeakServer"
    echo ""

    log_info "Para recompilar en el futuro:"
    echo "  cd $INSTALL_DIR/Server/Root"
    echo "  export build_os_type=linux"
    echo "  export build_os_arch=amd64"
    echo "  bash build_teaspeak.sh stable"
    echo ""

    log_info "Logs de compilación guardados en:"
    echo "  /tmp/teaspeak_compile.log"
    echo ""

    log_success "¡Todo listo para usar TeaSpeak!"
}

# ═══════════════════════════════════════════════════════════════════════
# FUNCIÓN PRINCIPAL
# ═══════════════════════════════════════════════════════════════════════
main() {
    clear

    echo -e "${CYAN}"
    echo "╔═══════════════════════════════════════════════════════════╗"
    echo "║     INSTALADOR AUTOMÁTICO DE TEASPEAK SERVER             ║"
    echo "║                    VERSIÓN CORREGIDA                      ║"
    echo "║                                                           ║"
    echo "║  Este script instalará TODAS las dependencias y          ║"
    echo "║  compilará TeaSpeak completamente de forma automática    ║"
    echo "╚═══════════════════════════════════════════════════════════╝"
    echo -e "${NC}"
    echo ""

    log_info "Directorio de instalación: $INSTALL_DIR"
    log_info "Rama de GitHub: $REPO_BRANCH"
    log_info "Tiempo estimado: 15-30 minutos"
    echo ""

    read -p "¿Continuar con la instalación? (y/n) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        log_info "Instalación cancelada"
        exit 0
    fi

    # Ejecutar todos los pasos
    check_system
    install_dependencies
    install_rust
    disable_ld_gold
    verify_tools
    clone_repository
    fix_permissions
    compile_libraries
    setup_environment
    compile_teaspeak
    verify_build
    show_summary
}

# ═══════════════════════════════════════════════════════════════════════
# EJECUCIÓN
# ═══════════════════════════════════════════════════════════════════════
main "$@"
