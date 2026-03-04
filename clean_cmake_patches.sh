#!/bin/bash

# Script para limpiar parches viejos de CMakeLists.txt (v1-v11)
# Elimina TODOS los parches de OpenSSL/zlib linking, incluyendo duplicados

set -e

CMAKE_FILE="Server/Root/TeaSpeak/server/CMakeLists.txt"

if [[ ! -f "$CMAKE_FILE" ]]; then
    echo "ERROR: $CMAKE_FILE no encontrado"
    exit 1
fi

echo "Limpiando parches viejos de CMakeLists.txt (incluyendo duplicados)..."

# Crear backup
cp "$CMAKE_FILE" "${CMAKE_FILE}.backup_clean"

# Eliminar TODOS los bloques de parches (incluyendo duplicados e incompletos)
awk '
BEGIN {
    in_patch = 0
    skip_empty = 0
}

# Detectar inicio de cualquier parche de OpenSSL/zlib
/^# Fix OpenSSL (and zlib )?linking order for mysql/ {
    in_patch = 1
    skip_empty = 1
    next
}

# Si estamos dentro de un parche
in_patch == 1 {
    # Detectar fin del parche: línea que no es comentario ni target_link_options
    if (!/^#/ && !/^target_link_options/ && !/^    "LINKER:/ && !/^    "/ && !/^\)/ && !/^$/) {
        # Salir del modo parche
        in_patch = 0
        skip_empty = 0
        print
        next
    }
    # Estamos dentro del parche, saltarlo
    next
}

# Saltar líneas vacías inmediatamente después del parche
skip_empty == 1 && /^$/ {
    skip_empty = 0
    next
}

# Líneas normales: imprimir
{
    skip_empty = 0
    print
}
' "$CMAKE_FILE" > "${CMAKE_FILE}.cleaned"

# Reemplazar archivo original
mv "${CMAKE_FILE}.cleaned" "$CMAKE_FILE"

echo "✓ Parches viejos eliminados (incluyendo duplicados)"
echo ""
echo "Para verificar, ejecuta:"
echo "  sed -n '300,330p' $CMAKE_FILE"
