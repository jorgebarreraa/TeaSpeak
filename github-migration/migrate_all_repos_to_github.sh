#!/bin/bash
set -e

# ==============================================================================
# Script de Migración Total de Repositorios TeaSpeak a GitHub Personal
# ==============================================================================
#
# Este script clona TODOS los repositorios externos usados por TeaSpeak
# y los sube a tu cuenta de GitHub, dándote control total del proyecto.
#
# Uso:
#   export GITHUB_TOKEN="tu_token_aqui"
#   bash migrate_all_repos_to_github.sh
#
# ==============================================================================

# Colores
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${BLUE}════════════════════════════════════════════════════════════${NC}"
echo -e "${BLUE}   Migración Total de Repositorios TeaSpeak a GitHub${NC}"
echo -e "${BLUE}════════════════════════════════════════════════════════════${NC}"
echo ""

# ==============================================================================
# Verificaciones Iniciales
# ==============================================================================

if [ -z "$GITHUB_TOKEN" ]; then
    echo -e "${RED}❌ Error: GITHUB_TOKEN no está configurado${NC}"
    echo ""
    echo "Por favor ejecuta:"
    echo "  export GITHUB_TOKEN=\"tu_token_de_github\""
    echo ""
    echo "Para obtener un token:"
    echo "  1. Ve a https://github.com/settings/tokens"
    echo "  2. Click en 'Generate new token (classic)'"
    echo "  3. Selecciona los permisos: repo (todos)"
    echo "  4. Copia el token y ejecuta export GITHUB_TOKEN=\"....\""
    exit 1
fi

GITHUB_USER="jorgebarreraa"
MIGRATION_DIR="/home/user/TeaSpeak/github-migration/all-repos"

echo -e "${GREEN}✓ Token de GitHub configurado${NC}"
echo -e "${BLUE}Usuario: ${GITHUB_USER}${NC}"
echo -e "${BLUE}Directorio de migración: ${MIGRATION_DIR}${NC}"
echo ""

# Crear directorio de migración
mkdir -p "$MIGRATION_DIR"
cd "$MIGRATION_DIR"

# ==============================================================================
# Lista de Repositorios a Migrar
# ==============================================================================

# Formato: "nombre_repo|url_original|rama_default|descripcion"
REPOS=(
    # Repositorios de GitHub
    "jsoncpp|https://github.com/open-source-parsers/jsoncpp.git|master|JSON parser for C++"
    "CXXTerminal|https://github.com/WolverinDEV/CXXTerminal.git|master|Terminal library for C++"
    "opus|https://github.com/xiph/opus|master|Opus audio codec"
    "opusfile|https://github.com/xiph/opusfile.git|master|Opus file library"
    "yaml-cpp|https://github.com/jbeder/yaml-cpp.git|master|YAML parser for C++"
    "libevent|https://github.com/libevent/libevent.git|master|Event notification library"
    "StringVariable|https://github.com/WolverinDEV/StringVariable.git|master|String variable library"
    "ed25519|https://github.com/WolverinDEV/ed25519.git|master|Ed25519 signature library"
    "protobuf|https://github.com/google/protobuf.git|3.5.1.1|Protocol Buffers"
    "DataPipes|https://github.com/WolverinDEV/DataPipes.git|master|Data pipes library"
    "jemalloc|https://github.com/jemalloc/jemalloc.git|dev|Memory allocator"
    "zstd|https://github.com/facebook/zstd.git|dev|Compression library"

    # Repositorios de git.did.science (TeaSpeak)
    "Thread-Pool|https://git.did.science/WolverinDEV/ThreadPool.git|master|Thread pool library"
    "tomcrypt|https://git.did.science/TeaSpeak/libraries/tomcrypt.git|master|Cryptographic library"
    "tommath|https://git.did.science/TeaSpeak/libraries/tommath.git|develop|Math library"
    "TeaSpeak-Server|https://git.did.science/TeaSpeak/Server/Server|master|TeaSpeak Server core"
    "spdlog|https://git.did.science/TeaSpeak/libraries/spdlog.git|master|Logging library"
    "libnice-prebuild|https://git.did.science/TeaSpeak/libraries/libnice-prebuild.git|master|ICE library prebuilt"
    "glib2.0|https://git.did.science/TeaSpeak/libraries/glib2.0.git|master|GLib library"
    "openssl-prebuild|https://git.did.science/TeaSpeak/libraries/openssl-prebuild.git|master|OpenSSL prebuilt"
    "TeaDNS|https://git.did.science/TeaSpeak/WebDNS.git|master|TeaSpeak DNS resolver"

    # Repositorios de Google (chromium/boringssl)
    "breakpad|https://chromium.googlesource.com/breakpad/breakpad|main|Crash reporting"
    "boringssl|https://boringssl.googlesource.com/boringssl|master|Google's SSL/TLS"
)

# Repos ya preparados en /home/user/TeaSpeak/github-migration/
PREPARED_REPOS=(
    "build-helpers"
    "TeaSpeak-shared"
)

TOTAL_REPOS=$((${#REPOS[@]} + ${#PREPARED_REPOS[@]}))
CURRENT=0

echo -e "${BLUE}Total de repositorios a migrar: ${TOTAL_REPOS}${NC}"
echo ""

# ==============================================================================
# Función para clonar y subir un repositorio
# ==============================================================================

migrate_repo() {
    local repo_name=$1
    local repo_url=$2
    local default_branch=$3
    local description=$4

    CURRENT=$((CURRENT + 1))

    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${BLUE}[$CURRENT/$TOTAL_REPOS] Migrando: ${repo_name}${NC}"
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo "  URL original: $repo_url"
    echo "  Rama: $default_branch"
    echo ""

    # Limpiar si ya existe
    if [ -d "$repo_name" ]; then
        echo "  Limpiando directorio existente..."
        rm -rf "$repo_name"
    fi

    # Clonar repositorio
    echo "  Clonando repositorio..."
    if ! git clone --mirror "$repo_url" "$repo_name.git" 2>/dev/null; then
        echo -e "${YELLOW}  ⚠️  Clone directo falló, intentando con bare clone...${NC}"
        git clone --bare "$repo_url" "$repo_name.git" || {
            echo -e "${RED}  ❌ Error clonando $repo_name${NC}"
            return 1
        }
    fi

    cd "$repo_name.git"

    # Crear repo en GitHub usando la API
    echo "  Creando repositorio en GitHub..."
    response=$(curl -s -H "Authorization: token $GITHUB_TOKEN" \
         -H "Accept: application/vnd.github.v3+json" \
         https://api.github.com/user/repos \
         -d "{\"name\":\"$repo_name\",\"description\":\"$description (Fork for TeaSpeak)\",\"private\":false}")

    # Verificar si ya existe
    if echo "$response" | grep -q "name already exists"; then
        echo -e "${YELLOW}  ⚠️  Repositorio ya existe, continuando...${NC}"
    elif echo "$response" | grep -q "\"id\":"; then
        echo -e "${GREEN}  ✓ Repositorio creado exitosamente${NC}"
    else
        echo -e "${RED}  ❌ Error creando repositorio${NC}"
        echo "  Respuesta: $response"
        cd ..
        return 1
    fi

    # Push all branches and tags
    echo "  Subiendo código a GitHub..."
    git push --mirror "https://${GITHUB_TOKEN}@github.com/${GITHUB_USER}/${repo_name}.git" 2>&1 | grep -v "token" || {
        echo -e "${YELLOW}  ⚠️  Push falló, repositorio puede estar vacío o ya sincronizado${NC}"
    }

    cd ..

    echo -e "${GREEN}  ✓ Migración completada${NC}"
    echo -e "${GREEN}  → https://github.com/${GITHUB_USER}/${repo_name}${NC}"
    echo ""

    return 0
}

# ==============================================================================
# Migrar todos los repositorios
# ==============================================================================

SUCCESS_COUNT=0
FAILED_REPOS=()

for repo_info in "${REPOS[@]}"; do
    IFS='|' read -r name url branch desc <<< "$repo_info"

    if migrate_repo "$name" "$url" "$branch" "$desc"; then
        SUCCESS_COUNT=$((SUCCESS_COUNT + 1))
    else
        FAILED_REPOS+=("$name")
    fi

    # Pequeña pausa para no saturar la API
    sleep 2
done

# ==============================================================================
# Migrar repos ya preparados
# ==============================================================================

echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${BLUE}Migrando repositorios ya preparados...${NC}"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo ""

cd /home/user/TeaSpeak/github-migration

for repo_name in "${PREPARED_REPOS[@]}"; do
    CURRENT=$((CURRENT + 1))

    echo -e "${BLUE}[$CURRENT/$TOTAL_REPOS] Migrando: ${repo_name}${NC}"

    if [ ! -d "$repo_name" ]; then
        echo -e "${RED}  ❌ Directorio $repo_name no encontrado${NC}"
        FAILED_REPOS+=("$repo_name")
        continue
    fi

    cd "$repo_name"

    # Crear repo en GitHub
    echo "  Creando repositorio en GitHub..."
    desc="TeaSpeak dependency (prepared with fixes)"
    response=$(curl -s -H "Authorization: token $GITHUB_TOKEN" \
         -H "Accept: application/vnd.github.v3+json" \
         https://api.github.com/user/repos \
         -d "{\"name\":\"$repo_name\",\"description\":\"$desc\",\"private\":false}")

    if echo "$response" | grep -q "name already exists"; then
        echo -e "${YELLOW}  ⚠️  Repositorio ya existe${NC}"
    elif echo "$response" | grep -q "\"id\":"; then
        echo -e "${GREEN}  ✓ Repositorio creado${NC}"
    fi

    # Cambiar remote y push
    echo "  Subiendo código..."
    git remote set-url origin "https://${GITHUB_TOKEN}@github.com/${GITHUB_USER}/${repo_name}.git"
    git push -u origin --all 2>&1 | grep -v "token"

    echo -e "${GREEN}  ✓ Migración completada${NC}"
    echo -e "${GREEN}  → https://github.com/${GITHUB_USER}/${repo_name}${NC}"
    echo ""

    SUCCESS_COUNT=$((SUCCESS_COUNT + 1))
    cd ..
done

# ==============================================================================
# Resumen Final
# ==============================================================================

echo -e "${GREEN}════════════════════════════════════════════════════════════${NC}"
echo -e "${GREEN}   Migración Completada${NC}"
echo -e "${GREEN}════════════════════════════════════════════════════════════${NC}"
echo ""
echo -e "${GREEN}Repositorios migrados exitosamente: ${SUCCESS_COUNT}/${TOTAL_REPOS}${NC}"
echo ""

if [ ${#FAILED_REPOS[@]} -gt 0 ]; then
    echo -e "${YELLOW}Repositorios con errores:${NC}"
    for repo in "${FAILED_REPOS[@]}"; do
        echo -e "${YELLOW}  - $repo${NC}"
    done
    echo ""
fi

echo -e "${BLUE}Próximo paso:${NC}"
echo "  Ejecuta el script de actualización de referencias:"
echo -e "  ${GREEN}bash update_all_submodule_references.sh${NC}"
echo ""
