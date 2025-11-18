cmake_minimum_required(VERSION 3.6)
project(libtommath C)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

file(GLOB SOURCE_FILES ./bn*.c)
list(APPEND SOURCE_FILES
    tommath.h
    tommath_class.h
    tommath_private.h
    tommath_superclass.h
)

macro(configure_msvc_runtime)
    if(MSVC)
        # Default to statically-linked runtime.
        if("${MSVC_RUNTIME}" STREQUAL "")
            set(MSVC_RUNTIME "static")
        endif()

        # Set compiler options.
        set(variables
                CMAKE_C_FLAGS_DEBUG
                CMAKE_C_FLAGS_MINSIZEREL
                CMAKE_C_FLAGS_RELEASE
                CMAKE_C_FLAGS_RELWITHDEBINFO
                CMAKE_CXX_FLAGS_DEBUG
                CMAKE_CXX_FLAGS_MINSIZEREL
                CMAKE_CXX_FLAGS_RELEASE
                CMAKE_CXX_FLAGS_RELWITHDEBINFO
                )
        if(${MSVC_RUNTIME} STREQUAL "static")
            foreach(variable ${variables})
                if(${variable} MATCHES "/MD")
                    string(REGEX REPLACE "/MD" "/MT" ${variable} "${${variable}}")
                endif()
            endforeach()
        else()
            foreach(variable ${variables})
                if(${variable} MATCHES "/MT")
                    string(REGEX REPLACE "/MT" "/MD" ${variable} "${${variable}}")
                endif()
            endforeach()
        endif()
    endif()
endmacro()
configure_msvc_runtime()


add_library(tommathShared SHARED ${SOURCE_FILES})
add_library(tommathStatic STATIC ${SOURCE_FILES})

# Install library
install(TARGETS tommathStatic tommathShared DESTINATION lib/)
# Install library headers
file(GLOB HEADERS *.h)
install(FILES ${HEADERS} DESTINATION include/)