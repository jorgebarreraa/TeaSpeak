cmake_minimum_required(VERSION 3.0)
project(libtomcrypt)

IF (CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
    ADD_DEFINITIONS(-DLTC_NO_ROLC)
ENDIF ()

ADD_DEFINITIONS(-DLTC_NO_PROTOTYPES)
ADD_DEFINITIONS(-D_CRT_SECURE_NO_WARNINGS)
ADD_DEFINITIONS(-DLTC_SOURCE)

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/src/headers)

SET (ALL_HEADER_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/headers/tomcrypt.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/headers/tomcrypt_cfg.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/headers/tomcrypt_custom.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/headers/tomcrypt_mac.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/headers/tomcrypt_math.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/headers/tomcrypt_pk.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/headers/tomcrypt_prng.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/headers/tomcrypt_argchk.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/headers/tomcrypt_cipher.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/headers/tomcrypt_hash.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/headers/tomcrypt_macros.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/headers/tomcrypt_misc.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/headers/tomcrypt_pkcs.h
        )

file(GLOB_RECURSE ALL_SOURCE_FILES src/*.c)

if (MSVC)
    message(STATUS "Detect MSVC compiler...")
    SET(MSVC_LIKE_COMPILER ON)

    set(CompilerFlags
            CMAKE_CXX_FLAGS
            CMAKE_CXX_FLAGS_DEBUG
            CMAKE_CXX_FLAGS_RELEASE
            CMAKE_CXX_FLAGS_RELWITHDEBINFO
            CMAKE_C_FLAGS
            CMAKE_C_FLAGS_DEBUG
            CMAKE_C_FLAGS_RELEASE
            CMAKE_C_FLAGS_RELWITHDEBINFO
    )
    foreach(CompilerFlag ${CompilerFlags})
        string(REPLACE "/MD" "/MT" ${CompilerFlag} "${${CompilerFlag}}")
    endforeach()
elseif ("x${CMAKE_CXX_COMPILER_ID}" STREQUAL "xIntel")
    if(WIN32)
        message(STATUS "Detect Intel compiler and handle it like MSVC...")
        SET(MSVC_LIKE_COMPILER ON)
    endif ()
endif ()

SET_SOURCE_FILES_PROPERTIES(ALL_HEADER_FILES PROPERTIES HEADER_FILE_ONLY TRUE)
LIST(APPEND ALL_SOURCE_FILES ${ALL_HEADER_FILES})

ADD_LIBRARY(tomcrypt_static STATIC ${ALL_SOURCE_FILES})
set_target_properties(tomcrypt_static PROPERTIES OUTPUT_NAME "tomcrypt")

INSTALL(TARGETS tomcrypt_static
        ARCHIVE DESTINATION  "lib"
        LIBRARY DESTINATION  "lib"
        RUNTIME DESTINATION  "bin"
)

INSTALL(FILES ${ALL_HEADER_FILES} DESTINATION "include")

SET(MSVC_LIKE_COMPILER OFF)

