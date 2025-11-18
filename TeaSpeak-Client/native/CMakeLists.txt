cmake_minimum_required(VERSION 3.16)

project(TeaClient-Natives VERSION 1.0.0)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreaded)
# set(CMAKE_VERBOSE_MAKEFILE ON)

if (CMAKE_INCLUDE_FILE AND NOT CMAKE_INCLUDE_FILE STREQUAL "")
	message("Include file ${CMAKE_INCLUDE_FILE}")
	include("${CMAKE_INCLUDE_FILE}")
endif ()

if(CMAKE_PLATFORM_INCLUDE AND NOT CMAKE_PLATFORM_INCLUDE STREQUAL "")
	message("Include file ${CMAKE_PLATFORM_INCLUDE}")
	include("${CMAKE_PLATFORM_INCLUDE}")
endif()
message("Library path: ${LIBRARY_PATH}")
message("Module path: ${CMAKE_MODULE_PATH}")

#Setup NodeJS
function(setup_nodejs)
	set(NodeJS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")
	set(NODEJS_URL "https://atom.io/download/atom-shell")
	set(NODEJS_VERSION "v8.5.5")

    #set(NODEJS_URL "https://nodejs.org/download/release/")
	#set(NODEJS_VERSION "v12.13.0")

	find_package(NodeJS REQUIRED)

	set(NODEJS_NAN_DIR "node_modules/nan")
	nodejs_init()

	#Fix nan include headers
	set(NAN_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/../node_modules/nan")
	if(NOT EXISTS "${NAN_INCLUDE_DIR}")
		message(FATAL_ERROR "Failed to find nan headers.")
	endif()
	list(APPEND NODEJS_INCLUDE_DIRS ${NAN_INCLUDE_DIR})

	set(EXE_DIRECTORY "${CMAKE_SOURCE_DIR}/build/exe/" PARENT_SCOPE)
	if (MSVC)
		set(NODE_LIB_DIRECTORY "${CMAKE_SOURCE_DIR}/build/win32_x64/" PARENT_SCOPE)
        set(NODE_PDB_DIRECTORY "${CMAKE_SOURCE_DIR}/build/symbols/" PARENT_SCOPE)
	else()
		set(NODE_LIB_DIRECTORY "${CMAKE_SOURCE_DIR}/build/linux_x64/" PARENT_SCOPE)
	endif()

	#Set some more variables
	set(NODEJS_INCLUDE_DIRS "${NODEJS_INCLUDE_DIRS}" PARENT_SCOPE)
	set(NODEJS_INIT ${NODEJS_INIT} PARENT_SCOPE)

	include_directories("dist/ext_nan")


	function(add_nodejs_module NAME)
		message("Registering module ${NAME}")
		_add_nodejs_module(${NAME} ${ARGN})

		if(MSVC)
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++17")
		else()
			target_compile_features(${NAME} PUBLIC cxx_std_17)
		endif()

		set_target_properties(${NAME}
				PROPERTIES
				LIBRARY_OUTPUT_DIRECTORY "${NODE_LIB_DIRECTORY}/"
		)
		target_include_directories(${NAME} PUBLIC ${NODEJS_INCLUDE_DIRS})
		set_target_properties(${NAME} PROPERTIES CXX_STANDARD 17) #Needs to be overridden after _add_nodejs_module sets it to 11

		if(MSVC)
			add_custom_command(TARGET ${NAME} POST_BUILD
					COMMAND ${CMAKE_COMMAND} -E
					copy "$<TARGET_FILE:${NAME}>" "${NODE_LIB_DIRECTORY}/${NAME}.node"
			)
            add_custom_command(TARGET ${NAME} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E
                    copy "$<TARGET_PDB_FILE:${NAME}>" "${NODE_PDB_DIRECTORY}/${NAME}.pdb"
            )
		else()
			set_target_properties(${NAME}
					PROPERTIES
					LIBRARY_OUTPUT_DIRECTORY "${NODE_LIB_DIRECTORY}/"
			)
		endif()
	endfunction()

	#Forward parameters to global scope
	set(NODEJS_VERSION ${NODEJS_VERSION} PARENT_SCOPE)
	set(NODEJS_SOURCES ${NODEJS_SOURCES} PARENT_SCOPE)
	set(NODEJS_INCLUDE_DIRS ${NODEJS_INCLUDE_DIRS} PARENT_SCOPE)
	set(NODEJS_LIBRARIES ${NODEJS_LIBRARIES} PARENT_SCOPE)
	set(NODEJS_LINK_FLAGS ${NODEJS_LINK_FLAGS} PARENT_SCOPE)
	set(NODEJS_DEFINITIONS ${NODEJS_DEFINITIONS} PARENT_SCOPE)

	# Prevents this function from executing more than once
	set(NODEJS_INIT TRUE PARENT_SCOPE)
endfunction()

#Setup the compiler (Cant be done within a function!)
if (MSVC)
	add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
	add_compile_options(/EHsc) #We require exception handling
	add_compile_options(/wd4275) # needs to have dll-interface to be used by clients of class
	add_compile_options(/wd4251) # needs to have dll-interface to be used by clients of class
	add_compile_options(/std:c++17)
else()
	#This is a bad thing here!
	function(resolve_library VARIABLE FALLBACK PATHS)
		set( _PATHS ${PATHS} ${ARGN} ) # Merge them together

		foreach(PATH IN ITEMS ${_PATHS})
			message(STATUS "Try to use path ${PATH} for ${VARIABLE}")
			if(EXISTS ${PATH})
				message(STATUS "Setting ${VARIABLE} to ${PATH}")
				set(${VARIABLE} ${PATH} PARENT_SCOPE)
				return()
			endif()
		endforeach()

		if(FALLBACK)
			message(WARNING "Failed to resolve library path for ${VARIABLE}. Using default ${VARIABLE}")
		else()
			message(FATAL_ERROR "Failed to find requited library. Variable: ${VARIABLE} Paths: ${_PATHS}")
		endif()
	endfunction()

	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -static-libgcc -static-libstdc++")
	set(CMAKE_SHARED_LIBRARY_CXX_FLAGS "") # Disable -fPIC (We dont want any relonking with build in electron libraries
endif()

setup_nodejs()
if(NOT NODEJS_INCLUDE_DIRS OR NODEJS_INCLUDE_DIRS STREQUAL "")
	message(FATAL_ERROR "Failed to find node headers")
else()
	message("Including NodeJS headers: ${NODEJS_INCLUDE_DIRS}")
endif()
include_directories(${NODEJS_INCLUDE_DIRS})

add_subdirectory(dist/ext_nan/)
function(build_update_installer)
	add_subdirectory(updater)
endfunction()
build_update_installer()


function(build_codec)
	add_subdirectory(codec)
endfunction()
#build_codec()

function(build_ppt)
	add_subdirectory(ppt)
endfunction()
build_ppt()

function(build_connection)
	add_subdirectory(serverconnection)
endfunction()
build_connection()

function(build_crash_handler)
	add_subdirectory(crash_handler)
endfunction()
build_crash_handler()

function(build_dns)
	add_subdirectory(dns)
endfunction()
build_dns()
