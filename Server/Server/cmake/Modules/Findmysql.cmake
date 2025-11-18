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
    find_path(mysql_ROOT_DIR
            NAMES include/mysql.h include/mysql_version.h
            HINTS ${mysql_ROOT_DIR}
    )

    find_path(mysql_INCLUDE_DIR
            NAMES mysql.h mysql_version.h
            HINTS ${mysql_ROOT_DIR}/include/
    )

    if (NOT TARGET mysql::client::static)
        find_library(MYSQL_CLIENT_STATIC
                NAMES mysql.lib libmysqlclient.a
                HINTS ${mysql_ROOT_DIR} ${mysql_ROOT_DIR}/lib
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