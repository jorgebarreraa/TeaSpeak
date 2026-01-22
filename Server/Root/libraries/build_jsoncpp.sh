#!/usr/bin/env bash

# Build JsonCpp with C++17 support for TeaSpeak
# This script MUST compile JsonCpp with C++17 to match TeaSpeak server requirements

set -e  # Exit on error

# Get the script directory to handle relative paths
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# If called from build-helpers context, library_path is set as environment variable
# Otherwise, we're in the libraries directory directly
if [[ -n "${library_path}" ]]; then
    # Called from build-helpers - change to the library directory
    cd "${library_path}" || {
        echo "ERROR: Cannot cd to library_path: ${library_path}"
        exit 1
    }
fi

echo "=== Building JsonCpp with C++17 support ==="

# Verify we're in the jsoncpp directory or can access it
if [[ -d "jsoncpp" ]]; then
    JSONCPP_DIR="jsoncpp"
elif [[ -f "CMakeLists.txt" ]] && grep -q "jsoncpp" CMakeLists.txt 2>/dev/null; then
    # We're already in jsoncpp directory
    JSONCPP_DIR="."
else
    echo "ERROR: Cannot find jsoncpp directory"
    echo "Current directory: $(pwd)"
    echo "Contents: $(ls -la)"
    exit 1
fi

echo "Using JsonCpp directory: $JSONCPP_DIR"

# Desinstalar versión del sistema para evitar conflictos
echo "Desinstalando JsonCpp del sistema..."
sudo apt-get remove -y libjsoncpp-dev libjsoncpp25 2>/dev/null || true

# Reinstalar cmake si se eliminó como dependencia
if ! command -v cmake &> /dev/null; then
    echo "Reinstalando cmake..."
    sudo apt-get install -y cmake
fi

# Limpiar headers y librerías previas
echo "Limpiando instalación previa de JsonCpp..."
sudo rm -rf /usr/local/include/json
sudo rm -f /usr/local/lib/libjsoncpp*
sudo rm -rf /usr/local/lib/cmake/jsoncpp
sudo rm -rf /usr/local/lib/objects-*/jsoncpp_object
sudo rm -f /usr/local/lib/pkgconfig/jsoncpp.pc

# Limpiar source y rebuild
echo "Restaurando código fuente limpio de JsonCpp..."
cd "$JSONCPP_DIR"
git checkout . 2>/dev/null || true
git clean -fdx 2>/dev/null || true

# CRÍTICO: Forzar C++17 en CMakeLists.txt
echo "Forzando C++17 en CMakeLists.txt de JsonCpp..."
if [[ -f "CMakeLists.txt" ]]; then
    sed -i 's/set(CMAKE_CXX_STANDARD [0-9]\+)/set(CMAKE_CXX_STANDARD 17)/' CMakeLists.txt
    echo "✓ CMakeLists.txt modificado para usar C++17"
    grep "CMAKE_CXX_STANDARD" CMakeLists.txt || echo "ADVERTENCIA: No se encontró CMAKE_CXX_STANDARD"
else
    echo "ERROR: CMakeLists.txt no encontrado en $(pwd)"
    exit 1
fi

# Volver al directorio padre si estábamos en jsoncpp
[[ "$JSONCPP_DIR" != "." ]] && cd ..

# Crear directorio de build limpio
echo "Creando directorio de build..."
rm -rf "$JSONCPP_DIR/build"
mkdir -p "$JSONCPP_DIR/build"

cd "$JSONCPP_DIR/build"
echo "Compilando JsonCpp con C++17..."
cmake .. \
    -DCMAKE_CXX_STANDARD=17 \
    -DCMAKE_CXX_STANDARD_REQUIRED=ON \
    -DCMAKE_CXX_FLAGS="-fPIC ${CXX_FLAGS}" \
    -DCMAKE_C_FLAGS="${C_FLAGS}" \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    ${CMAKE_OPTIONS}

echo "Ejecutando make..."
make ${CMAKE_MAKE_OPTIONS}

echo "Instalando JsonCpp..."
sudo make install

# Actualizar cache del linker
sudo ldconfig

# Verificación crítica
echo "=== Verificación de instalación ==="
if [[ -f "/usr/local/lib/libjsoncpp.so" ]]; then
    echo "✓ JsonCpp instalado en /usr/local/lib"
    STRING_VIEW_COUNT=$(nm -D /usr/local/lib/libjsoncpp.so 2>/dev/null | grep -c "string_view" || echo "0")
    if [[ "$STRING_VIEW_COUNT" -gt 0 ]]; then
        echo "✓ JsonCpp contiene $STRING_VIEW_COUNT símbolos string_view (C++17)"
        exit 0
    else
        echo "✗ ADVERTENCIA: JsonCpp NO contiene símbolos string_view"
        echo "  Esto causará errores de linker. La compilación probablemente falló en usar C++17."
        exit 1
    fi
else
    echo "✗ ERROR: JsonCpp no se instaló correctamente"
    exit 1
fi
