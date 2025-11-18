set(MODULE_NAME "teaclient_crash_handler")

set(SOURCE_FILES ${SOURCE_FILES} src/crash_handler.cpp)
if (MSVC)
	set(SOURCE_FILES ${SOURCE_FILES} src/win_crash_generator.cpp)
else()
    find_package(Breakpad REQUIRED)

	set(REQUIRED_LIBRARIES
		breakpad::static
		libstdc++fs.a
	)
endif()

add_nodejs_module(${MODULE_NAME} binding.cc ${SOURCE_FILES})
target_link_libraries(${MODULE_NAME} ${REQUIRED_LIBRARIES})
