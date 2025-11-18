#!/bin/bash

# Variables de entorno
export build_os_type=linux
export build_os_arch=amd64
export CMAKE_MAKE_OPTIONS="-j$(nproc --all)"
export MAKE_OPTIONS="$CMAKE_MAKE_OPTIONS"
export CMAKE_BUILD_TYPE="RelWithDebInfo"

# Ejecutar el script principal de compilación
echo "Compilando bibliotecas de TeaSpeak..."
echo "Saltando: breakpad, CXXTerminal (tienen errores conocidos)"
echo ""

bash build.sh
