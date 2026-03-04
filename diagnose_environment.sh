#!/bin/bash
#
# Script de Diagnóstico para TeaSpeak
# Recolecta información del entorno para debugging
#

echo "═══════════════════════════════════════════════════════════"
echo "  DIAGNÓSTICO DE ENTORNO - TEASPEAK"
echo "═══════════════════════════════════════════════════════════"
echo ""

# Variables
INSTALL_DIR="${1:-/opt/TeaSpeak}"

echo "Directorio de instalación: $INSTALL_DIR"
echo ""

# 1. SISTEMA OPERATIVO
echo "1. SISTEMA OPERATIVO"
echo "-----------------------------------------------------------"
cat /etc/os-release 2>/dev/null | grep -E "^(NAME|VERSION)=" || echo "No se pudo detectar"
echo "Kernel: $(uname -r)"
echo "Arquitectura: $(uname -m)"
echo ""

# 2. HERRAMIENTAS INSTALADAS
echo "2. HERRAMIENTAS INSTALADAS"
echo "-----------------------------------------------------------"
echo "GCC: $(gcc --version 2>/dev/null | head -1 || echo 'NO INSTALADO')"
echo "CMake: $(cmake --version 2>/dev/null | head -1 || echo 'NO INSTALADO')"
echo "Meson: $(meson --version 2>/dev/null || echo 'NO INSTALADO')"
echo "Ninja: $(ninja --version 2>/dev/null || echo 'NO INSTALADO')"
echo "Autoconf: $(autoconf --version 2>/dev/null | head -1 || echo 'NO INSTALADO')"
echo "Rust cargo: $(cargo --version 2>/dev/null || echo 'NO INSTALADO')"
echo "Rust rustc: $(rustc --version 2>/dev/null || echo 'NO INSTALADO')"
echo "OpenSSL: $(openssl version 2>/dev/null || echo 'NO INSTALADO')"
echo ""

# 3. ESTRUCTURA DE DIRECTORIOS
echo "3. ESTRUCTURA DE DIRECTORIOS"
echo "-----------------------------------------------------------"
if [[ -d "$INSTALL_DIR" ]]; then
    echo "✓ $INSTALL_DIR existe"

    echo ""
    echo "Contenido de $INSTALL_DIR/Server/Root/libraries/:"
    if [[ -d "$INSTALL_DIR/Server/Root/libraries" ]]; then
        ls -la "$INSTALL_DIR/Server/Root/libraries/" | head -30
    else
        echo "✗ Directorio NO existe"
    fi

    echo ""
    echo "Verificando StringVariable:"
    if [[ -d "$INSTALL_DIR/Server/Root/libraries/StringVariable" ]]; then
        echo "✓ StringVariable existe"
        ls -la "$INSTALL_DIR/Server/Root/libraries/StringVariable/"
    else
        echo "✗ StringVariable NO existe"
    fi

    echo ""
    echo "Verificando jsoncpp:"
    if [[ -d "$INSTALL_DIR/Server/Root/libraries/jsoncpp" ]]; then
        echo "✓ jsoncpp existe"
        ls -la "$INSTALL_DIR/Server/Root/libraries/jsoncpp/" | head -10
    else
        echo "✗ jsoncpp NO existe"
    fi

    echo ""
    echo "Verificando event:"
    if [[ -d "$INSTALL_DIR/Server/Root/libraries/event" ]]; then
        echo "✓ event existe"
        ls -la "$INSTALL_DIR/Server/Root/libraries/event/" | head -10
    else
        echo "✗ event NO existe"
    fi

else
    echo "✗ $INSTALL_DIR NO existe"
fi
echo ""

# 4. SCRIPTS DE COMPILACIÓN
echo "4. SCRIPTS DE COMPILACIÓN EN libraries/"
echo "-----------------------------------------------------------"
if [[ -d "$INSTALL_DIR/Server/Root/libraries" ]]; then
    echo "Scripts build_*.sh encontrados:"
    ls -1 "$INSTALL_DIR/Server/Root/libraries/"build_*.sh 2>/dev/null || echo "Ninguno encontrado"

    echo ""
    echo "¿Existe build.sh principal?"
    if [[ -f "$INSTALL_DIR/Server/Root/libraries/build.sh" ]]; then
        echo "✓ SÍ existe build.sh"
        ls -lh "$INSTALL_DIR/Server/Root/libraries/build.sh"
    else
        echo "✗ NO existe build.sh"
    fi
else
    echo "✗ Directorio libraries/ no existe"
fi
echo ""

# 5. SUBMÓDULOS GIT
echo "5. SUBMÓDULOS GIT"
echo "-----------------------------------------------------------"
if [[ -d "$INSTALL_DIR/.git" ]]; then
    cd "$INSTALL_DIR"
    echo "Estado de submódulos:"
    git submodule status 2>/dev/null | head -20 || echo "Error al leer submódulos"

    echo ""
    echo "Total de submódulos:"
    git submodule status 2>/dev/null | wc -l
else
    echo "✗ No es un repositorio git"
fi
echo ""

# 6. CARGO Y RUST-WEBRTC
echo "6. CARGO Y DEPENDENCIAS RUST"
echo "-----------------------------------------------------------"
if [[ -d "$HOME/.cargo" ]]; then
    echo "✓ Directorio .cargo existe"

    echo ""
    echo "Checkouts de Cargo:"
    if [[ -d "$HOME/.cargo/git/checkouts" ]]; then
        ls -1 "$HOME/.cargo/git/checkouts" 2>/dev/null || echo "Vacío"

        echo ""
        echo "¿Existe rust-webrtc?"
        if ls "$HOME/.cargo/git/checkouts"/rust-webrtc-* 2>/dev/null; then
            echo "✓ rust-webrtc encontrado"

            echo ""
            echo "Versiones/commits de rust-webrtc:"
            ls -1 "$HOME/.cargo/git/checkouts"/rust-webrtc-*/

            echo ""
            echo "Contenido de Cargo.toml de rust-webrtc:"
            find "$HOME/.cargo/git/checkouts" -path "*/rust-webrtc-*/*/Cargo.toml" -type f 2>/dev/null | while read f; do
                echo "Archivo: $f"
                echo "Sección dev-dependencies.slog:"
                grep -A3 "^\[dev-dependencies.slog\]" "$f" 2>/dev/null || echo "No encontrado"
                echo ""
            done
        else
            echo "✗ rust-webrtc NO encontrado en checkouts"
        fi
    else
        echo "✗ $HOME/.cargo/git/checkouts no existe"
    fi
else
    echo "✗ Cargo no está instalado o no se ha usado"
fi
echo ""

# 7. VERIFICAR SI PARCHE SE APLICÓ
echo "7. VERIFICACIÓN DE PARCHE rust-webrtc"
echo "-----------------------------------------------------------"
if [[ -d "$HOME/.cargo/git/checkouts" ]]; then
    find "$HOME/.cargo/git/checkouts" -path "*/rust-webrtc-*/*/Cargo.toml" -type f 2>/dev/null | while read cargo_file; do
        echo "Verificando: $cargo_file"

        if grep -q '^\[dev-dependencies\.slog\]$' "$cargo_file"; then
            echo "  - Tiene sección [dev-dependencies.slog]"

            if grep -A1 '^\[dev-dependencies\.slog\]$' "$cargo_file" | grep -q 'version'; then
                echo "  ✓ TIENE version (parche aplicado)"
                grep -A2 '^\[dev-dependencies\.slog\]$' "$cargo_file"
            else
                echo "  ✗ NO TIENE version (parche NO aplicado)"
                grep -A2 '^\[dev-dependencies\.slog\]$' "$cargo_file"
            fi
        else
            echo "  - NO tiene sección [dev-dependencies.slog]"
        fi
        echo ""
    done
else
    echo "No hay checkouts de Cargo"
fi
echo ""

# 8. LOGS DE COMPILACIÓN
echo "8. LOGS DE COMPILACIÓN RECIENTES"
echo "-----------------------------------------------------------"
echo "Logs en /tmp:"
ls -lht /tmp/*teaspeak*.log /tmp/build*.log 2>/dev/null | head -10 || echo "No se encontraron logs"
echo ""

# 9. PERMISOS Y PROPIETARIO
echo "9. PERMISOS Y PROPIETARIO"
echo "-----------------------------------------------------------"
echo "Usuario actual: $(whoami)"
echo "UID: $(id -u)"
if [[ -d "$INSTALL_DIR" ]]; then
    echo "Propietario de $INSTALL_DIR:"
    ls -ld "$INSTALL_DIR"
else
    echo "$INSTALL_DIR no existe"
fi
echo ""

# 10. VARIABLES DE ENTORNO
echo "10. VARIABLES DE ENTORNO RELEVANTES"
echo "-----------------------------------------------------------"
echo "PATH: $PATH"
echo "HOME: $HOME"
echo "build_os_type: ${build_os_type:-no definido}"
echo "build_os_arch: ${build_os_arch:-no definido}"
echo ""

echo "═══════════════════════════════════════════════════════════"
echo "  FIN DEL DIAGNÓSTICO"
echo "═══════════════════════════════════════════════════════════"
echo ""
echo "Para guardar este diagnóstico en un archivo:"
echo "  bash diagnose_environment.sh > diagnostico.txt 2>&1"
echo ""
