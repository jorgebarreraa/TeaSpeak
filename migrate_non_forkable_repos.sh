#!/bin/bash
#
# Script para migrar repos que NO se pueden forkear directamente
# (git.did.science y Google repos)
#
# Uso: ./migrate_non_forkable_repos.sh TU_USUARIO_GITHUB
#

set -e

if [[ -z "$1" ]]; then
    echo "Error: Debes proporcionar tu usuario de GitHub"
    echo "Uso: ./migrate_non_forkable_repos.sh TU_USUARIO"
    exit 1
fi

GITHUB_USER="$1"
TEMP_DIR="/tmp/teaspeak_repo_migration"

echo "════════════════════════════════════════════════════════════"
echo "  Migrando Repositorios No-Forkeables a GitHub"
echo "  Usuario destino: $GITHUB_USER"
echo "════════════════════════════════════════════════════════════"
echo ""

mkdir -p "$TEMP_DIR"
cd "$TEMP_DIR"

# ═══════════════════════════════════════════════════════════════
# FUNCIÓN: Clonar y preparar para push
# ═══════════════════════════════════════════════════════════════
migrate_repo() {
    local source_url="$1"
    local repo_name="$2"
    local branch="${3:-master}"

    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo "📦 Migrando: $repo_name"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

    # Limpiar si existe
    rm -rf "$repo_name" 2>/dev/null || true

    # Clonar
    echo "  [1/4] Clonando desde: $source_url"
    git clone "$source_url" "$repo_name" --branch "$branch" || git clone "$source_url" "$repo_name"
    cd "$repo_name"

    # Remover remote origin
    echo "  [2/4] Removiendo remote original..."
    git remote remove origin

    # Agregar nuevo remote
    echo "  [3/4] Agregando remote a: https://github.com/$GITHUB_USER/$repo_name"
    git remote add origin "https://github.com/$GITHUB_USER/$repo_name.git"

    # Mostrar instrucciones
    echo "  [4/4] Listo para push"
    echo ""
    echo "  ✅ SIGUIENTE PASO:"
    echo "     1. Ve a: https://github.com/new"
    echo "     2. Crea un repo llamado: $repo_name"
    echo "     3. Deja el repo VACÍO (no agregues README, .gitignore, etc.)"
    echo "     4. Luego ejecuta:"
    echo ""
    echo "        cd $TEMP_DIR/$repo_name"
    echo "        git push -u origin \$(git branch --show-current)"
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

# Protobuf con tag específico
echo "  📦 Clonando protobuf con tag v3.5.1.1..."
rm -rf protobuf 2>/dev/null || true
git clone "https://fuchsia.googlesource.com/third_party/protobuf" protobuf
cd protobuf
git checkout v3.5.1.1 2>/dev/null || echo "  ⚠️  No se pudo hacer checkout a v3.5.1.1"
git remote remove origin
git remote add origin "https://github.com/$GITHUB_USER/protobuf.git"
echo ""
echo "  ✅ protobuf listo para push"
echo "     Crear repo: https://github.com/new"
echo "     Nombre: protobuf"
echo "     Push: cd $TEMP_DIR/protobuf && git push -u origin \$(git branch --show-current)"
echo ""
cd ..

# ═══════════════════════════════════════════════════════════════
# RESUMEN FINAL
# ═══════════════════════════════════════════════════════════════
echo ""
echo "════════════════════════════════════════════════════════════"
echo "  ✅ MIGRACIÓN PREPARADA"
echo "════════════════════════════════════════════════════════════"
echo ""
echo "📋 SIGUIENTES PASOS:"
echo ""
echo "1. Crea los siguientes 10 repos en GitHub (vacíos, sin README):"
echo "   https://github.com/new"
echo ""
echo "   Nombres de repos a crear:"
for repo in Thread-Pool tomcrypt tommath spdlog libnice-prebuild glib2.0 openssl-prebuild breakpad boringssl protobuf; do
    echo "   - $repo"
done
echo ""
echo "2. Luego ejecuta este comando para pushear TODOS:"
echo ""
echo "   cd $TEMP_DIR"
echo "   for repo in Thread-Pool tomcrypt tommath spdlog libnice-prebuild glib2.0 openssl-prebuild breakpad boringssl protobuf; do"
echo "       echo \"Pushing \$repo...\""
echo "       cd \$repo && git push -u origin \$(git branch --show-current) && cd .."
echo "   done"
echo ""
echo "3. Para los repos de GitHub que SÍ se pueden forkear (14 repos):"
echo "   Usa la interfaz web de GitHub para hacer fork:"
echo "   Ver lista completa en: FORK_ALL_DEPENDENCIES.md"
echo ""
echo "4. Cuando termines, ejecuta:"
echo "   ./setup_teaspeak.sh --github-user $GITHUB_USER --build-type stable"
echo ""
echo "════════════════════════════════════════════════════════════"
