set(MODULE_NAME "teaclient_codec")
set(CMAKE_MODULE_DIR "${CMAKE_SOURCE_DIR}/codec")

set(SOURCE_FILES
		binding.cc
		codec/NativeCodec.cpp
		codec/OpusCodec.cpp
		codec/SpeexCodec.cpp
		codec/CeltCodec.cpp
)

add_nodejs_module(${MODULE_NAME} ${SOURCE_FILES})

if(MSVC)
	set(OPUS_LIBRARY_PATH "${CMAKE_MODULE_DIR}/libraries/generated/opus/lib/opus.lib")
	set(SPEEX_LIBRARY_PATH "${CMAKE_MODULE_DIR}/libraries/generated/speex/lib/libspeex.lib")
else()
	set(OPUS_LIBRARY_PATH "${CMAKE_MODULE_DIR}/libraries/generated/opus/lib/libopus.a")
	set(SPEEX_LIBRARY_PATH "${CMAKE_MODULE_DIR}/libraries/generated/speex/lib/libspeex.a")
	set(CELT_LIBRARY_PATH "${CMAKE_MODULE_DIR}/libraries/generated/celt/lib/libcelt0.a")
endif()

#Detect opus
if(EXISTS "${CMAKE_MODULE_DIR}/libraries/generated/opus/include" AND EXISTS ${OPUS_LIBRARY_PATH})
	set(HAVE_OPUS ON)

	add_definitions(-DHAVE_OPUS)
	include_directories(${CMAKE_MODULE_DIR}/libraries/generated/opus/include)
	target_link_libraries(${MODULE_NAME} ${OPUS_LIBRARY_PATH})
else()
	message(WARNING "Missing opus libraries. Building without opus support!\n" "Build opus with the build script given within the libraries foulder")
endif()

#Detect speex
if(EXISTS "${CMAKE_MODULE_DIR}/libraries/generated/speex/include" AND EXISTS ${SPEEX_LIBRARY_PATH})
	set(HAVE_SPEEX ON)

	add_definitions(-DHAVE_SPEEX)
	include_directories(${CMAKE_MODULE_DIR}/libraries/generated/speex/include)
	target_link_libraries(${MODULE_NAME} ${SPEEX_LIBRARY_PATH})
else()
	message(WARNING "Missing speex libraries. Building without speex support!\n" "Build speex with the build script given within the libraries foulder")
endif()

#Detect celt
set(BUILD_CELT OFF)
if(EXISTS "${CMAKE_MODULE_DIR}/libraries/generated/celt/include" AND EXISTS "${CELT_LIBRARY_PATH}" AND BUILD_CELT)
	set(HAVE_CELT ON)

	add_definitions(-DHAVE_CELT)
	include_directories(${CMAKE_MODULE_DIR}/libraries/generated/celt/include)

	target_link_libraries(${MODULE_NAME} ${CELT_LIBRARY_PATH})
else()
	message(WARNING "Missing celt libraries. Building without celt support!\n" "Build celt with the build script given within the libraries foulder")
endif()

if(HAVE_OPUS AND HAVE_CELT)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,-no-whole-archive,--allow-multiple-definition")
endif()