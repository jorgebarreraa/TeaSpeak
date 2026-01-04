#!/bin/bash

# Script para limpiar parches viejos de CMakeLists.txt (v1-v10)
# Elimina tanto los parches ANTES del bloque jemalloc (v1-v9)
# como los parches DESPUÉS del bloque jemalloc (v10)

set -e

CMAKE_FILE="Server/Root/TeaSpeak/server/CMakeLists.txt"

if [[ ! -f "$CMAKE_FILE" ]]; then
    echo "ERROR: $CMAKE_FILE no encontrado"
    exit 1
fi

echo "Limpiando parches viejos de CMakeLists.txt..."

# Crear backup
cp "$CMAKE_FILE" "${CMAKE_FILE}.backup_clean"

# Eliminar parches tanto antes como después del bloque jemalloc
awk '
BEGIN { in_old_patch = 0 }

# Detectar inicio de parche viejo (v1-v10)
/^# Fix OpenSSL (and zlib )?linking order for mysql/ {
    in_old_patch = 1
    next
}

# Si estamos dentro de un parche, saltar líneas hasta encontrar el fin
in_old_patch == 1 {
    # Fin del parche: siguiente línea que no es parte del parche
    if (/^set\(DISABLE_JEMALLOC/ || /^add_executable\(Snapshots-Permissions-Test/ || /^$/) {
        # Si es línea vacía después del parche, también saltarla
        if (/^$/) {
            next
        }
        # Encontramos el fin, imprimir esta línea y salir del modo parche
        in_old_patch = 0
        print
        next
    }
    # Seguimos dentro del parche, saltar esta línea
    next
}

# Líneas normales: imprimir
{ print }
' "$CMAKE_FILE" > "${CMAKE_FILE}.cleaned"

# Reemplazar archivo original
mv "${CMAKE_FILE}.cleaned" "$CMAKE_FILE"

echo "✓ Parches viejos eliminados"
echo ""
echo "Para verificar, ejecuta:"
echo "  sed -n '300,330p' $CMAKE_FILE"
