set(MODULE_NAME "updater")

set(SOURCE_FILES
    main.cpp
    config.cpp
    logger.cpp
    json.hpp
    util.cpp
    file.cpp
)

if (WIN32)
	list(APPEND SOURCE_FILES win32/retry_ui.cpp win32/Resource.rc)
else()
	list(APPEND SOURCE_FILES unix/block_info.cpp)
endif()

add_executable(update_installer ${SOURCE_FILES})

if(WIN32)
	target_link_libraries(update_installer kernel32.lib;user32.lib;gdi32.lib;winspool.lib;comdlg32.lib;advapi32.lib;shell32.lib;ole32.lib;oleaut32.lib;uuid.lib;odbc32.lib;Shlwapi.lib;Rstrtmgr.lib)

	add_custom_command(TARGET update_installer POST_BUILD
	    COMMAND ${CMAKE_COMMAND} -E
	    copy "$<TARGET_FILE:update_installer>" "${EXE_DIRECTORY}/update-installer.exe"
	)
else()
	target_link_libraries(update_installer libstdc++fs.a)

	add_custom_command(TARGET update_installer POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E
		copy "$<TARGET_FILE:update_installer>" "${EXE_DIRECTORY}/update-installer"
	)
endif()