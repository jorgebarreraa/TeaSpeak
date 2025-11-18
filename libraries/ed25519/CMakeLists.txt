project(ed25519)
cmake_minimum_required(VERSION 3.9)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(USE_OPENSSL "Use openssl for sha 512 (Faster than the default installation" ON)

set(SOURCE
		src/add_scalar.c
		src/fe.c
		src/ge.c
		src/key_exchange.c
		src/keypair.c
		src/sc.c
		src/seed.c
		src/sign.c
		src/verify.c
)

if(USE_OPENSSL)
	add_definitions(-DUSE_OPENSSL)
	set(SOURCE ${SOURCE} src/sha512_openssl.c)
else()
#	message(FATAL_ERROR "Not supported yet!")
	set(SOURCE ${SOURCE} src/sha512.c)
endif()

if (MSVC)
	set(CompilerFlags
			CMAKE_CXX_FLAGS
			CMAKE_CXX_FLAGS_DEBUG
			CMAKE_CXX_FLAGS_RELEASE
			CMAKE_C_FLAGS
			CMAKE_C_FLAGS_DEBUG
			CMAKE_C_FLAGS_RELEASE
			)
	foreach(CompilerFlag ${CompilerFlags})
		string(REPLACE "/MD" "/MT" ${CompilerFlag} "${${CompilerFlag}}")
	endforeach()
endif()
#Some programs need for full support the header files too
set(HEADERS
		include/ed25519.h
		include/fe.h
		include/fixedint.h
		include/ge.h
		include/precomp_data.h
		include/sc.h
		include/sha512.h
)

add_library(ed25519 STATIC ${SOURCE} ${HEADERS})

option(BUILD_TESTS "Build test" ON)
if (BUILD_TESTS)
	add_executable(ed25519_test test.c)
	target_link_libraries(ed25519_test ed25519)

	if(USE_OPENSSL)
		target_link_libraries(ed25519_test crypto)
	endif()
endif ()

INSTALL(DIRECTORY include/ DESTINATION include/ed25519)
INSTALL(TARGETS ed25519
		LIBRARY DESTINATION lib
		ARCHIVE DESTINATION lib
)
