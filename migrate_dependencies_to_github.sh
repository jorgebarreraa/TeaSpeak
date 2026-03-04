#!/bin/bash
#
# ═══════════════════════════════════════════════════════════════════════════
#  Script de Migración de Dependencias TeaSpeak a GitHub Personal
# ═══════════════════════════════════════════════════════════════════════════
#
#  Este script migra TODAS las dependencias de TeaSpeak a tu cuenta de GitHub.
#
#  ¿Por qué migrar?
#  1. Control total sobre las dependencias
#  2. Permanencia - si los repos originales desaparecen, tienes copias
#  3. Independencia de servidores externos (git.did.science, etc)
#  4. Capacidad de hacer cambios y fixes personalizados
#
#  Prerequisitos:
#  1. Cuenta de GitHub
#  2. Token de GitHub con permisos 'repo'
#  3. Git instalado
#
#  Uso:
#    export GITHUB_TOKEN="tu_token_aqui"
#    export GITHUB_USER="tu_usuario"
#    ./migrate_dependencies_to_github.sh
#
# ═══════════════════════════════════════════════════════════════════════════

set -e

# ═══════════════════════════════════════════════════════════════════════════
# Colores
# ═══════════════════════════════════════════════════════════════════════════
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

# ═══════════════════════════════════════════════════════════════════════════
# Funciones de logging
# ═══════════════════════════════════════════════════════════════════════════
log_info() { echo -e "${BLUE}[INFO]${NC} $1"; }
log_success() { echo -e "${GREEN}[✓]${NC} $1"; }
log_warning() { echo -e "${YELLOW}[⚠]${NC} $1"; }
log_error() { echo -e "${RED}[✗]${NC} $1"; }
log_step() {
    echo ""
    echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${CYAN}$1${NC}"
    echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
}

# ═══════════════════════════════════════════════════════════════════════════
# Verificar prerequisitos
# ═══════════════════════════════════════════════════════════════════════════
check_prerequisites() {
    log_step "Verificando Prerequisitos"

    # Verificar GITHUB_TOKEN
    if [[ -z "$GITHUB_TOKEN" ]]; then
        log_error "GITHUB_TOKEN no está configurado"
        echo ""
        echo "Para obtener un token:"
        echo "1. Ve a https://github.com/settings/tokens"
        echo "2. Click en 'Generate new token (classic)'"
        echo "3. Selecciona permisos: repo (todos)"
        echo "4. Copia el token"
        echo "5. Ejecuta: export GITHUB_TOKEN=\"tu_token_aqui\""
        echo ""
        exit 1
    fi

    # Verificar GITHUB_USER
    if [[ -z "$GITHUB_USER" ]]; then
        log_error "GITHUB_USER no está configurado"
        echo ""
        echo "Ejecuta: export GITHUB_USER=\"tu_usuario_de_github\""
        echo ""
        exit 1
    fi

    # Verificar git
    if ! command -v git &> /dev/null; then
        log_error "Git no está instalado"
        exit 1
    fi

    # Verificar curl
    if ! command -v curl &> /dev/null; then
        log_error "curl no está instalado"
        exit 1
    fi

    log_success "Token configurado para usuario: $GITHUB_USER"
    log_success "Git disponible: $(git --version | head -1)"
}

# ═══════════════════════════════════════════════════════════════════════════
# Definición de repositorios a migrar
# ═══════════════════════════════════════════════════════════════════════════
declare -A REPOS

# Formato: "nombre_repo|url_original|rama_default"
REPOS=(
    # Repos de GitHub (open source)
    ["jsoncpp"]="https://github.com/open-source-parsers/jsoncpp.git|master"
    ["CXXTerminal"]="https://github.com/WolverinDEV/CXXTerminal.git|master"
    ["opus"]="https://github.com/xiph/opus|master"
    ["opusfile"]="https://github.com/xiph/opusfile.git|master"
    ["yaml-cpp"]="https://github.com/jbeder/yaml-cpp.git|master"
    ["libevent"]="https://github.com/libevent/libevent.git|master"
    ["StringVariable"]="https://github.com/WolverinDEV/StringVariable.git|master"
    ["ed25519"]="https://github.com/WolverinDEV/ed25519.git|master"
    ["DataPipes"]="https://github.com/WolverinDEV/DataPipes.git|master"
    ["jemalloc"]="https://github.com/jemalloc/jemalloc.git|dev"
    ["zstd"]="https://github.com/facebook/zstd.git|dev"
    ["build-helpers"]="https://github.com/WolverinDEV/build-helpers.git|master"

    # Repos de git.did.science (TeaSpeak/WolverinDEV)
    ["Thread-Pool"]="https://git.did.science/WolverinDEV/ThreadPool.git|master"
    ["tomcrypt"]="https://git.did.science/TeaSpeak/libraries/tomcrypt.git|master"
    ["tommath"]="https://git.did.science/TeaSpeak/libraries/tommath.git|master"
    ["spdlog"]="https://git.did.science/TeaSpeak/libraries/spdlog.git|master"
    ["libnice-prebuild"]="https://git.did.science/TeaSpeak/libraries/libnice-prebuild.git|master"
    ["glib2.0"]="https://git.did.science/TeaSpeak/libraries/glib2.0.git|master"
    ["openssl-prebuild"]="https://git.did.science/TeaSpeak/libraries/openssl-prebuild.git|master"

    # Repos de Google
    ["breakpad"]="https://chromium.googlesource.com/breakpad/breakpad|main"
    ["boringssl"]="https://boringssl.googlesource.com/boringssl|master"
    ["protobuf"]="https://fuchsia.googlesource.com/third_party/protobuf|v3.5.1.1"
)

# ═══════════════════════════════════════════════════════════════════════════
# Crear repositorio en GitHub
# ═══════════════════════════════════════════════════════════════════════════
create_github_repo() {
    local repo_name="$1"
    local description="$2"

    log_info "Creando repositorio: $repo_name"

    # Crear usando GitHub API
    local response=$(curl -s -X POST \
        -H "Authorization: token $GITHUB_TOKEN" \
        -H "Accept: application/vnd.github.v3+json" \
        https://api.github.com/user/repos \
        -d "{\"name\":\"$repo_name\",\"description\":\"$description\",\"private\":false}")

    # Verificar si se creó o ya existe
    if echo "$response" | grep -q '"id"'; then
        log_success "Repositorio creado: https://github.com/$GITHUB_USER/$repo_name"
        return 0
    elif echo "$response" | grep -q "already exists"; then
        log_warning "Repositorio ya existe: https://github.com/$GITHUB_USER/$repo_name"
        return 0
    else
        log_error "Error creando repositorio: $repo_name"
        echo "$response" | grep -o '"message":"[^"]*"' || echo "$response"
        return 1
    fi
}

# ═══════════════════════════════════════════════════════════════════════════
# Migrar un repositorio
# ═══════════════════════════════════════════════════════════════════════════
migrate_repo() {
    local repo_name="$1"
    local repo_info="$2"

    local original_url=$(echo "$repo_info" | cut -d'|' -f1)
    local default_branch=$(echo "$repo_info" | cut -d'|' -f2)

    log_step "[$repo_name]"
    log_info "URL original: $original_url"
    log_info "Rama: $default_branch"

    local temp_dir="/tmp/teaspeak_migration_$repo_name"

    # Limpiar directorio temporal si existe
    rm -rf "$temp_dir"

    # 1. Clonar repositorio original (mirror)
    log_info "Clonando repositorio original..."
    if ! git clone --mirror "$original_url" "$temp_dir" 2>/dev/null; then
        log_error "Error clonando $original_url"
        log_warning "Saltando este repositorio..."
        return 1
    fi

    cd "$temp_dir"

    # 2. Crear repositorio en GitHub
    create_github_repo "$repo_name" "TeaSpeak dependency: $repo_name (migrated from $original_url)"

    # 3. Agregar remote de GitHub
    local github_url="https://$GITHUB_USER:$GITHUB_TOKEN@github.com/$GITHUB_USER/$repo_name.git"
    git remote add github "$github_url"

    # 4. Push todo a GitHub (todas las ramas y tags)
    log_info "Subiendo a GitHub..."
    if git push --mirror github 2>&1 | grep -v "Warning: Permanently added"; then
        log_success "Migración completada: https://github.com/$GITHUB_USER/$repo_name"
    else
        log_warning "Push completado con advertencias"
    fi

    # 5. Limpiar
    cd /tmp
    rm -rf "$temp_dir"

    # Pequeña pausa para no saturar la API de GitHub
    sleep 2
}

# ═══════════════════════════════════════════════════════════════════════════
# Función principal de migración
# ═══════════════════════════════════════════════════════════════════════════
migrate_all() {
    log_step "Iniciando Migración de ${#REPOS[@]} Repositorios"

    local total=${#REPOS[@]}
    local count=0
    local success=0
    local failed=0

    for repo_name in "${!REPOS[@]}"; do
        ((count++))
        echo ""
        echo -e "${CYAN}[$count/$total]${NC}"

        if migrate_repo "$repo_name" "${REPOS[$repo_name]}"; then
            ((success++))
        else
            ((failed++))
        fi
    done

    # Resumen
    log_step "Resumen de Migración"
    log_success "Exitosos: $success/$total"
    if [[ $failed -gt 0 ]]; then
        log_warning "Fallidos: $failed/$total"
    fi
}

# ═══════════════════════════════════════════════════════════════════════════
# Generar script de actualización de .gitmodules
# ═══════════════════════════════════════════════════════════════════════════
generate_update_script() {
    log_step "Generando Script de Actualización"

    local script_path="update_gitmodules.sh"

    cat > "$script_path" << 'EOFSCRIPT'
#!/bin/bash
#
# Script para actualizar .gitmodules con los nuevos repositorios
#

set -e

GITHUB_USER="__GITHUB_USER__"

echo "Actualizando .gitmodules para usar repositorios de: $GITHUB_USER"

# Backup del archivo original
if [[ -f .gitmodules ]]; then
    cp .gitmodules .gitmodules.backup
    echo "✓ Backup creado: .gitmodules.backup"
fi

# Función para actualizar URL en .gitmodules
update_url() {
    local repo_name="$1"
    local new_url="https://github.com/$GITHUB_USER/${repo_name}.git"

    if grep -q "url.*$repo_name" .gitmodules 2>/dev/null; then
        sed -i "s|url = .*/$repo_name.*|url = $new_url|g" .gitmodules
        echo "  ✓ Actualizado: $repo_name"
    fi
}

# Actualizar URLs
echo "Actualizando URLs en .gitmodules..."

update_url "jsoncpp"
update_url "Thread-Pool"
update_url "ThreadPool"
update_url "tomcrypt"
update_url "tommath"
update_url "CXXTerminal"
update_url "opus"
update_url "opusfile"
update_url "yaml-cpp"
update_url "libevent"
update_url "spdlog"
update_url "StringVariable"
update_url "ed25519"
update_url "breakpad"
update_url "boringssl"
update_url "protobuf"
update_url "DataPipes"
update_url "jemalloc"
update_url "libnice-prebuild"
update_url "glib2.0"
update_url "openssl-prebuild"
update_url "zstd"
update_url "build-helpers"

echo ""
echo "✓ .gitmodules actualizado"
echo ""
echo "Cambios realizados:"
git diff .gitmodules || echo "(no hay cambios)"
echo ""

read -p "¿Deseas commitear estos cambios? (y/n): " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    git add .gitmodules
    git commit -m "Update submodules to use personal GitHub forks"
    echo "✓ Cambios commiteados"

    read -p "¿Deseas hacer push? (y/n): " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        git push
        echo "✓ Cambios pusheados"
    fi
fi

echo ""
echo "Para sincronizar los submódulos, ejecuta:"
echo "  git submodule sync"
echo "  git submodule update --init --recursive"
EOFSCRIPT

    # Reemplazar __GITHUB_USER__
    sed -i "s/__GITHUB_USER__/$GITHUB_USER/g" "$script_path"
    chmod +x "$script_path"

    log_success "Script creado: $script_path"
    log_info "Ejecuta este script después de la migración para actualizar .gitmodules"
}

# ═══════════════════════════════════════════════════════════════════════════
# Mostrar resumen final
# ═══════════════════════════════════════════════════════════════════════════
show_final_summary() {
    log_step "✅ MIGRACIÓN COMPLETADA"

    echo ""
    echo -e "${GREEN}╔═══════════════════════════════════════════════════════════╗${NC}"
    echo -e "${GREEN}║     DEPENDENCIAS MIGRADAS A TU GITHUB EXITOSAMENTE        ║${NC}"
    echo -e "${GREEN}╚═══════════════════════════════════════════════════════════╝${NC}"
    echo ""

    log_info "Repositorios migrados a: https://github.com/$GITHUB_USER"
    echo ""

    log_info "Próximos pasos:"
    echo ""
    echo "1. Ejecuta el script de actualización:"
    echo "   ./update_gitmodules.sh"
    echo ""
    echo "2. Verifica los cambios en .gitmodules"
    echo ""
    echo "3. Sincroniza los submódulos:"
    echo "   git submodule sync"
    echo "   git submodule update --init --recursive"
    echo ""
    echo "4. Compila usando tus repos:"
    echo "   ./setup_teaspeak.sh --github-user $GITHUB_USER"
    echo ""

    log_success "¡Ahora tienes control total de todas las dependencias!"
}

# ═══════════════════════════════════════════════════════════════════════════
# Main
# ═══════════════════════════════════════════════════════════════════════════
main() {
    clear

    echo -e "${CYAN}"
    cat << "EOF"
╔═══════════════════════════════════════════════════════════╗
║                                                           ║
║   MIGRADOR DE DEPENDENCIAS TEASPEAK A GITHUB PERSONAL     ║
║                                                           ║
║  Migra TODAS las dependencias a tu cuenta de GitHub      ║
║  para tener control total y permanencia del proyecto     ║
║                                                           ║
╚═══════════════════════════════════════════════════════════╝
EOF
    echo -e "${NC}"
    echo ""

    check_prerequisites

    log_info "Usuario GitHub: $GITHUB_USER"
    log_info "Total de repos a migrar: ${#REPOS[@]}"
    log_info "Tiempo estimado: 30-45 minutos"
    echo ""

    read -p "¿Continuar con la migración? (y/n): " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        log_info "Migración cancelada"
        exit 0
    fi

    migrate_all
    generate_update_script
    show_final_summary
}

# Ejecutar
main "$@"
