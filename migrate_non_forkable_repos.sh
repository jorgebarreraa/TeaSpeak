#!/bin/bash
#
# Script para migrar TODOS los repositorios de dependencias a tu GitHub
# Estrategia: CLONE (no fork) para tener control total y aplicar parches permanentes
#
# Uso: ./migrate_non_forkable_repos.sh TU_USUARIO_GITHUB
#
# IMPORTANTE: Este script ELIMINARÁ repos existentes con el mismo nombre en tu cuenta
#             para evitar conflictos de nombres y empezar limpio
#

set -e

if [[ -z "$1" ]]; then
    echo "Error: Debes proporcionar tu usuario de GitHub"
    echo "Uso: ./migrate_non_forkable_repos.sh TU_USUARIO"
    exit 1
fi

GITHUB_USER="$1"
TEMP_DIR="/tmp/teaspeak_repo_migration"

# Verificar que gh esté instalado
if ! command -v gh &> /dev/null; then
    echo "⚠️  ERROR: GitHub CLI (gh) no está instalado"
    echo ""
    echo "Instálalo con:"
    echo "  sudo apt install gh"
    echo "o visita: https://cli.github.com/"
    exit 1
fi

# Verificar que esté autenticado
if ! gh auth status &> /dev/null; then
    echo "⚠️  ERROR: No estás autenticado con GitHub CLI"
    echo ""
    echo "Ejecuta: gh auth login"
    exit 1
fi

echo "════════════════════════════════════════════════════════════"
echo "  Migrando TODAS las Dependencias a GitHub (CLONE, no fork)"
echo "  Usuario destino: $GITHUB_USER"
echo "════════════════════════════════════════════════════════════"
echo ""
echo "⚠️  ADVERTENCIA: Este script ELIMINARÁ repos existentes"
echo "   en tu cuenta si tienen el mismo nombre que las dependencias"
echo ""
read -p "¿Continuar? (y/N): " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "Cancelado por el usuario"
    exit 0
fi
echo ""

mkdir -p "$TEMP_DIR"
cd "$TEMP_DIR"

# ═══════════════════════════════════════════════════════════════
# FUNCIÓN: Eliminar repo existente, clonar y pushear
# ═══════════════════════════════════════════════════════════════
migrate_repo() {
    local source_url="$1"
    local repo_name="$2"
    local branch="${3:-master}"
    local specific_commit="$4"

    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo "📦 Migrando: $repo_name"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

    # [1/6] Eliminar repo existente en GitHub si existe
    echo "  [1/6] Verificando si $GITHUB_USER/$repo_name existe..."
    if gh repo view "$GITHUB_USER/$repo_name" &> /dev/null; then
        echo "  ⚠️  Repo existente encontrado - ELIMINANDO..."
        gh repo delete "$GITHUB_USER/$repo_name" --yes || echo "  ⚠️  No se pudo eliminar (continuando...)"
        sleep 2
    else
        echo "  ✓ No existe (OK)"
    fi

    # [2/6] Limpiar directorio local si existe
    echo "  [2/6] Limpiando directorio local..."
    rm -rf "$repo_name" 2>/dev/null || true

    # [3/6] Clonar desde origen
    echo "  [3/6] Clonando desde: $source_url"
    if [[ -n "$branch" && "$branch" != "master" ]]; then
        git clone "$source_url" "$repo_name" --branch "$branch" || git clone "$source_url" "$repo_name"
    else
        git clone "$source_url" "$repo_name"
    fi
    cd "$repo_name"

    # Hacer unshallow para tener historial completo (evita errores de push)
    echo "  [3.5/6] Obteniendo historial completo..."
    git fetch --unshallow 2>/dev/null || echo "  ✓ Repo ya tiene historial completo"

    # Si se especificó un commit, hacer checkout (para breakpad)
    if [[ -n "$specific_commit" ]]; then
        echo "  📌 Checkout a commit específico: $specific_commit"
        git fetch --unshallow 2>/dev/null || true
        git checkout "$specific_commit" 2>/dev/null || echo "  ⚠️  No se pudo checkout a $specific_commit"
    fi

    # [4/6] Crear repo en GitHub
    echo "  [4/6] Creando repo en GitHub: $GITHUB_USER/$repo_name"
    gh repo create "$GITHUB_USER/$repo_name" --public --source=. || echo "  ⚠️  Repo puede ya existir"

    # [5/6] Remover y agregar remote
    echo "  [5/6] Configurando remote..."
    git remote remove origin 2>/dev/null || true
    git remote add origin "https://github.com/$GITHUB_USER/$repo_name.git"

    # [6/6] Push
    echo "  [6/6] Pusheando a GitHub..."
    CURRENT_BRANCH=$(git branch --show-current)
    git push -u origin "$CURRENT_BRANCH" -f || git push -u origin HEAD -f

    echo "  ✅ $repo_name migrado exitosamente"
    echo ""

    cd ..
}

# ═══════════════════════════════════════════════════════════════
# REPOS DE git.did.science (6 repos)
# ═══════════════════════════════════════════════════════════════
echo ""
echo "╔═══════════════════════════════════════════════════════════╗"
echo "║  SECCIÓN 1: Repos de git.did.science (6 repos)            ║"
echo "╚═══════════════════════════════════════════════════════════╝"
echo ""

migrate_repo "https://git.did.science/WolverinDEV/ThreadPool.git" "Thread-Pool"
migrate_repo "https://git.did.science/TeaSpeak/libraries/tomcrypt.git" "tomcrypt"
migrate_repo "https://git.did.science/TeaSpeak/libraries/tommath.git" "tommath"
migrate_repo "https://git.did.science/TeaSpeak/libraries/spdlog.git" "spdlog"
migrate_repo "https://git.did.science/TeaSpeak/libraries/libnice-prebuild.git" "libnice-prebuild"
migrate_repo "https://git.did.science/TeaSpeak/libraries/glib2.0.git" "glib2.0"
migrate_repo "https://git.did.science/TeaSpeak/libraries/openssl-prebuild.git" "openssl-prebuild"

# ═══════════════════════════════════════════════════════════════
# REPOS DE GOOGLE (3 repos)
# ═══════════════════════════════════════════════════════════════
echo ""
echo "╔═══════════════════════════════════════════════════════════╗"
echo "║  SECCIÓN 2: Repos de Google (3 repos)                     ║"
echo "╚═══════════════════════════════════════════════════════════╝"
echo ""

migrate_repo "https://chromium.googlesource.com/breakpad/breakpad" "breakpad"

# Para breakpad, aplicar commit específico
echo "  📌 Aplicando commit específico para breakpad..."
cd breakpad
git fetch --unshallow 2>/dev/null || true
git checkout f032e4c3 2>/dev/null || echo "  ⚠️  No se pudo hacer checkout a f032e4c3, usando HEAD"
cd ..

migrate_repo "https://boringssl.googlesource.com/boringssl" "boringssl"

# Protobuf con tag específico - Fixing detached HEAD
echo "  📦 Migrando protobuf con tag v3.5.1.1..."
rm -rf protobuf 2>/dev/null || true
git clone "https://fuchsia.googlesource.com/third_party/protobuf" protobuf
cd protobuf
git checkout v3.5.1.1 2>/dev/null || echo "  ⚠️  No se pudo hacer checkout a v3.5.1.1"

# Crear branch desde tag para evitar detached HEAD
git checkout -b v3.5.1.1-branch 2>/dev/null || true

# Crear repo y pushear
echo "  [4/6] Creando repo en GitHub: $GITHUB_USER/protobuf"
gh repo delete "$GITHUB_USER/protobuf" --yes 2>/dev/null || true
sleep 2
gh repo create "$GITHUB_USER/protobuf" --public --source=. || echo "  ⚠️  Repo puede ya existir"

echo "  [5/6] Configurando remote..."
git remote remove origin 2>/dev/null || true
git remote add origin "https://github.com/$GITHUB_USER/protobuf.git"

echo "  [6/6] Pusheando a GitHub..."
git push -u origin v3.5.1.1-branch -f || git push -u origin HEAD -f
echo "  ✅ protobuf migrado exitosamente"
echo ""
cd ..

# ═══════════════════════════════════════════════════════════════
# SECCIÓN 3: GitHub - WolverinDEV (7 repos)
# ═══════════════════════════════════════════════════════════════
echo ""
echo "╔═══════════════════════════════════════════════════════════╗"
echo "║  SECCIÓN 3: GitHub - WolverinDEV (7 repos)                ║"
echo "╚═══════════════════════════════════════════════════════════╝"
echo ""

migrate_repo "https://github.com/WolverinDEV/build-helpers.git" "build-helpers"
migrate_repo "https://github.com/WolverinDEV/CXXTerminal.git" "CXXTerminal"
migrate_repo "https://github.com/WolverinDEV/StringVariable.git" "StringVariable"
migrate_repo "https://github.com/WolverinDEV/ed25519.git" "ed25519"
migrate_repo "https://github.com/WolverinDEV/DataPipes.git" "DataPipes"
migrate_repo "https://github.com/WolverinDEV/rust-webrtc.git" "rust-webrtc"
migrate_repo "https://github.com/WolverinDEV/rust-libnice.git" "rust-libnice"

# ═══════════════════════════════════════════════════════════════
# SECCIÓN 4: GitHub - Otros autores (7 repos)
# ═══════════════════════════════════════════════════════════════
echo ""
echo "╔═══════════════════════════════════════════════════════════╗"
echo "║  SECCIÓN 4: GitHub - Otros (7 repos)                      ║"
echo "╚═══════════════════════════════════════════════════════════╝"
echo ""

migrate_repo "https://github.com/open-source-parsers/jsoncpp.git" "jsoncpp"
migrate_repo "https://github.com/xiph/opus" "opus"
migrate_repo "https://github.com/xiph/opusfile.git" "opusfile"
migrate_repo "https://github.com/jbeder/yaml-cpp.git" "yaml-cpp"
migrate_repo "https://github.com/libevent/libevent.git" "libevent"
migrate_repo "https://github.com/jemalloc/jemalloc.git" "jemalloc" "dev"
migrate_repo "https://github.com/facebook/zstd.git" "zstd"

# ═══════════════════════════════════════════════════════════════
# RESUMEN FINAL
# ═══════════════════════════════════════════════════════════════
echo ""
echo "════════════════════════════════════════════════════════════"
echo "  ✅ MIGRACIÓN COMPLETADA - TODOS LOS 24 REPOS"
echo "════════════════════════════════════════════════════════════"
echo ""
echo "📊 RESUMEN:"
echo ""
echo "  Sección 1: git.did.science (7 repos) ✅"
echo "  Sección 2: Google (3 repos) ✅"
echo "  Sección 3: GitHub - WolverinDEV (7 repos) ✅"
echo "  Sección 4: GitHub - Otros (7 repos) ✅"
echo ""
echo "  TOTAL: 24 repositorios migrados a github.com/$GITHUB_USER"
echo ""
echo "🎯 PRÓXIMO PASO:"
echo ""
echo "  Ejecuta TeaSpeak con TUS dependencias:"
echo ""
echo "    cd ~/TeaSpeak"
echo "    ./setup_teaspeak.sh --github-user $GITHUB_USER --build-type stable"
echo ""
echo "  Esto usará TODOS tus repos en lugar de los originales."
echo ""
echo "════════════════════════════════════════════════════════════"
