#!/bin/bash
# Script de limpieza para eliminar líneas duplicadas de include_directories(music/include)
# Este script solo debe ejecutarse UNA VEZ para limpiar aplicaciones múltiples del patch

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CMAKE_FILE="$SCRIPT_DIR/Server/Server/CMakeLists.txt"

if [[ ! -f "$CMAKE_FILE" ]]; then
    echo "ERROR: No se encontró $CMAKE_FILE"
    exit 1
fi

echo "Limpiando líneas duplicadas de include_directories(music/include)..."

# Crear backup
cp "$CMAKE_FILE" "$CMAKE_FILE.before_cleanup"

# Eliminar líneas duplicadas manteniendo solo una instancia
awk '
BEGIN {
    found_music_subdir = 0
    added_include = 0
}
{
    # Si encontramos add_subdirectory(music/)
    if ($0 ~ /^add_subdirectory\(music/) {
        print $0
        found_music_subdir = 1
        next
    }

    # Si es la línea de include_directories después de music subdir
    if (found_music_subdir == 1 && $0 ~ /include_directories\(music\/include\)/) {
        # Solo agregar la primera vez
        if (added_include == 0) {
            print ""
            print "# Add music include directory for server to access MusicPlayer.h"
            print "include_directories(music/include)"
            added_include = 1
        }
        # Saltar esta línea (es duplicada)
        next
    }

    # Si es el comentario antes del include_directories
    if (found_music_subdir == 1 && $0 ~ /# Add music include directory/) {
        # Saltar el comentario duplicado
        next
    }

    # Si encontramos una línea que no es blanca después de music subdir, ya no estamos en ese contexto
    if (found_music_subdir == 1 && $0 !~ /^[[:space:]]*$/ && $0 !~ /include_directories/ && $0 !~ /^#/) {
        found_music_subdir = 0
    }

    # Imprimir todas las demás líneas normalmente
    print $0
}
' "$CMAKE_FILE.before_cleanup" > "$CMAKE_FILE"

# Contar cuántas líneas quedan
count=$(grep -c 'include_directories(music/include)' "$CMAKE_FILE" || echo "0")

if [[ "$count" == "1" ]]; then
    echo "✓ Limpieza exitosa: Solo queda 1 línea include_directories(music/include)"
    echo "✓ Backup guardado en: $CMAKE_FILE.before_cleanup"
elif [[ "$count" == "0" ]]; then
    echo "⚠ ADVERTENCIA: No se encontró ninguna línea include_directories(music/include)"
    echo "  Restaurando desde backup..."
    mv "$CMAKE_FILE.before_cleanup" "$CMAKE_FILE"
    exit 1
else
    echo "⚠ ADVERTENCIA: Aún quedan $count líneas duplicadas"
    echo "  Puedes verificar manualmente con:"
    echo "  grep -n 'include_directories(music/include)' $CMAKE_FILE"
fi

echo ""
echo "Verificación:"
grep -A 3 "add_subdirectory(music" "$CMAKE_FILE"
