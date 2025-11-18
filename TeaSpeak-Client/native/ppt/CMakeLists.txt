set(MODULE_NAME "teaclient_ppt")

set(SOURCE_FILES src/KeyboardHook.cpp)
if (MSVC)
	set(SOURCE_FILES ${SOURCE_FILES} src/Win32KeyboardHook.cpp src/Win32KeyboardRawInput.cpp)
else()
	set(SOURCE_FILES ${SOURCE_FILES} src/X11KeyboardHook.cpp)
endif()

add_nodejs_module(${MODULE_NAME} binding.cc ${SOURCE_FILES})
if (WIN32)
	target_compile_definitions(${MODULE_NAME} PRIVATE -DUSING_UV_SHARED)
else()
	target_compile_definitions(${MODULE_NAME} PRIVATE -DHAVE_X11)
endif()

add_executable(Hook-Test ${SOURCE_FILES} test/HookTest.cpp)

if(NOT MSVC)
	target_link_libraries(${MODULE_NAME} X11)
	target_link_libraries(Hook-Test X11 pthread)
endif()
