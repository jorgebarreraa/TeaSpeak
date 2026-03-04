#!/bin/bash
# Script para aplicar parches a módulos CMake de TeaSpeak
# Automatiza correcciones necesarias para compilación en Ubuntu

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "Aplicando parches a módulos CMake..."

# ═══════════════════════════════════════════════════════════════════════════
# Parche 1: Findmysql.cmake - Agregar rutas estándar de Ubuntu
# ═══════════════════════════════════════════════════════════════════════════
echo "  → Parcheando Findmysql.cmake..."
MYSQL_CMAKE="$SCRIPT_DIR/Server/Server/cmake/Modules/Findmysql.cmake"
if [[ -f "$MYSQL_CMAKE" ]]; then
    # Verificar si ya tiene el parche aplicado
    if ! grep -q "/usr/include/mysql" "$MYSQL_CMAKE" 2>/dev/null; then
        echo "    Aplicando parche de rutas estándar de Ubuntu..."
        cp "$MYSQL_CMAKE" "${MYSQL_CMAKE}.bak"

        cat > "$MYSQL_CMAKE" << 'EOFMYSQL'
# - Try to find mysql include dirs and libraries
#
# Usage of this module as follows:
#
#     find_package(mysql)
#
# Variables used by this module, they can change the default behaviour and need
# to be set before calling find_package:
#
#  mysql_ROOT_DIR          Set this variable to the root installation of
#                            mysql if the module has problems finding the
#                            proper installation path.
#
# Variables defined by this module:
#
#  mysql_FOUND             System has mysql, include and library dirs found
#  mysql_INCLUDE_DIR       The mysql include directories.

include(FindPackageHandleStandardArgs)

function(find_mysql)
    # First try to find mysql_ROOT_DIR if provided
    find_path(mysql_ROOT_DIR
            NAMES include/mysql.h include/mysql_version.h include/mysql/mysql.h
            HINTS ${mysql_ROOT_DIR}
            PATHS /usr /usr/local
    )

    # Search for MySQL headers in common Ubuntu/Debian locations
    find_path(mysql_INCLUDE_DIR
            NAMES mysql.h mysql_version.h
            HINTS ${mysql_ROOT_DIR}/include/ ${mysql_ROOT_DIR}/include/mysql/
            PATHS
                /usr/include/mysql
                /usr/local/include/mysql
                /usr/include/mariadb
                /usr/local/include/mariadb
            PATH_SUFFIXES mysql mariadb
    )

    if (NOT TARGET mysql::client::static)
        find_library(MYSQL_CLIENT_STATIC
                NAMES mysql.lib libmysqlclient.a
                HINTS ${mysql_ROOT_DIR} ${mysql_ROOT_DIR}/lib
                PATHS
                    /usr/lib
                    /usr/lib/x86_64-linux-gnu
                    /usr/local/lib
                PATH_SUFFIXES mysql mariadb
        )

        if(MYSQL_CLIENT_STATIC)
            add_library(mysql::client::static STATIC IMPORTED)
            set_target_properties(mysql::client::static PROPERTIES
                    IMPORTED_LOCATION ${MYSQL_CLIENT_STATIC}
                    INTERFACE_INCLUDE_DIRECTORIES ${mysql_INCLUDE_DIR}
            )
        endif()
    endif ()

    find_package_handle_standard_args(mysql DEFAULT_MSG
            mysql_INCLUDE_DIR
    )

    mark_as_advanced(
            mysql_ROOT_DIR
            mysql_INCLUDE_DIR
            MYSQL_CLIENT_STATIC
    )
endfunction()
find_mysql()
EOFMYSQL
        echo "    ✓ Findmysql.cmake parcheado"
    else
        echo "    ✓ Findmysql.cmake ya tiene el parche aplicado"
    fi
fi

# ═══════════════════════════════════════════════════════════════════════════
# Parche 2: FindEd25519.cmake - Agregar targets IMPORTED
# ═══════════════════════════════════════════════════════════════════════════
echo "  → Parcheando FindEd25519.cmake..."
ED25519_CMAKE="$SCRIPT_DIR/Server/Root/build-helpers/cmake/FindEd25519.cmake"
if [[ -f "$ED25519_CMAKE" ]]; then
    # Verificar si ya tiene el parche aplicado
    if ! grep -q "ed25519::static" "$ED25519_CMAKE" 2>/dev/null; then
        echo "    Aplicando parche de targets IMPORTED..."
        cp "$ED25519_CMAKE" "${ED25519_CMAKE}.bak"

        cat > "$ED25519_CMAKE" << 'EOFED25519'
# - Try to find ed25519 include dirs and libraries
#
# Usage of this module as follows:
#
#     find_package(ed25519)
#
# Variables used by this module, they can change the default behaviour and need
# to be set before calling find_package:
#
#  ed25519_ROOT_DIR          Set this variable to the root installation of
#                            ed25519 if the module has problems finding the
#                            proper installation path.
#
# Variables defined by this module:
#
#  ed25519_FOUND             System has ed25519, include and library dirs found
#  ed25519_INCLUDE_DIR       The ed25519 include directories.
#  ed25519_LIBRARIES_STATIC  The ed25519 libraries.
#  ed25519_LIBRARIES_SHARED  The ed25519 libraries.

include(tearoot-helper)
include(FindPackageHandleStandardArgs)

find_path(ed25519_ROOT_DIR
        NAMES include/ed25519/ed25519
		HINTS ${ed25519_ROOT_DIR} ${ed25519_ROOT_DIR}/${BUILD_OUTPUT}
)

find_path(ed25519_INCLUDE_DIR
        NAMES ed25519/ed25519.h
		HINTS ${ed25519_ROOT_DIR} ${ed25519_ROOT_DIR}/include/
)

find_library(ed25519_LIBRARIES_STATIC
        NAMES ed25519.lib ed25519.a libed25519.a
		HINTS ${ed25519_ROOT_DIR} ${ed25519_ROOT_DIR}/lib
)

find_library(ed25519_LIBRARIES_SHARED
        NAMES ed25519.dll ed25519.so
		HINTS ${ed25519_ROOT_DIR} ${ed25519_ROOT_DIR}/lib
)

# Create IMPORTED target for static library
if (NOT TARGET ed25519::static)
    if(ed25519_LIBRARIES_STATIC)
        add_library(ed25519::static STATIC IMPORTED)
        set_target_properties(ed25519::static PROPERTIES
                IMPORTED_LOCATION ${ed25519_LIBRARIES_STATIC}
                INTERFACE_INCLUDE_DIRECTORIES ${ed25519_INCLUDE_DIR}
        )
    endif()
endif()

# Create IMPORTED target for shared library
if (NOT TARGET ed25519::shared)
    if(ed25519_LIBRARIES_SHARED)
        add_library(ed25519::shared SHARED IMPORTED)
        set_target_properties(ed25519::shared PROPERTIES
                IMPORTED_LOCATION ${ed25519_LIBRARIES_SHARED}
                INTERFACE_INCLUDE_DIRECTORIES ${ed25519_INCLUDE_DIR}
        )
    endif()
endif()

find_package_handle_standard_args(ed25519 DEFAULT_MSG
        ed25519_INCLUDE_DIR
)

mark_as_advanced(
        ed25519_ROOT_DIR
        ed25519_INCLUDE_DIR
        ed25519_LIBRARIES_STATIC
        ed25519_LIBRARIES_SHARED
)
EOFED25519
        echo "    ✓ FindEd25519.cmake parcheado"
    else
        echo "    ✓ FindEd25519.cmake ya tiene el parche aplicado"
    fi
fi

echo "✓ Todos los parches CMake aplicados exitosamente!"
