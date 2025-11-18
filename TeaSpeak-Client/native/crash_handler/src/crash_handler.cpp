#include "crash_handler.h"

#ifdef WIN32
    #include <Windows.h>
    #include <filesystem>
    namespace fs = std::filesystem;
#else
    #include <client/linux/handler/exception_handler.h>
    #include <experimental/filesystem>
    namespace fs = std::experimental::filesystem;
#endif

#include <iostream>
#include <thread>
#include "base64.h"

using namespace std;
using namespace tc;
using namespace tc::signal;

#ifndef WIN32
unique_ptr<google_breakpad::ExceptionHandler> global_crash_handler;
#else
PVOID global_crash_handler = nullptr;
#endif
unique_ptr<CrashContext> crash_context;

bool crash_callback(const fs::path&, CrashContext*, const std::string&, bool);
std::string replace_all(std::string data, const std::string& needle, const std::string& replacement) {
	size_t pos = data.find(needle);
	while(pos != std::string::npos) {
		data.replace(pos, needle.size(), replacement);
		pos = data.find(needle, pos + replacement.size());
	}

	return data;
}

/* taken from the updater */
bool rename_or_move(const fs::path& source, const fs::path& target, std::string& error) {
	try {
		fs::rename(source, target);
		return true;
	} catch(const fs::filesystem_error& ex) {
#ifndef WIN32
		if(ex.code() == errc::cross_device_link) {
			/* try with move command */
			char buffer[2048];
			auto length = snprintf(buffer, 2048, R"(%s "%s" "%s")", "mv", source.c_str(), target.c_str()); /* build the move command */
			if(length < 1 || length >= 2049) {
				error = "failed to prepare move command!";
				return false;
			}
			auto code = system(buffer);
			if(code != 0) {
				error = "move command resulted in " + to_string(code);
				return false;
			}
			return true;
		}
#endif
		error = "rename returned error code " + to_string(ex.code().value()) + " (" + ex.code().message() + ")";
		return false;
	}
}

void crash_execute_detached(const std::string& command_line) {
	cout << "Exec command " << command_line << endl;
	#ifdef WIN32
        STARTUPINFO si;
        PROCESS_INFORMATION pi;

        // set the size of the structures
        ZeroMemory( &si, sizeof(si) );
        si.cb = sizeof(si);
        ZeroMemory( &pi, sizeof(pi) );

        // start the program up
        auto result = CreateProcess(nullptr,   // the path
                                    (LPSTR) command_line.c_str(),        // Command line
                                    nullptr,           // Process handle not inheritable
                                    nullptr,           // Thread handle not inheritable
                                    FALSE,          // Set handle inheritance to FALSE
                                    CREATE_NEW_CONSOLE  ,              // No creation flags
                                    nullptr,           // Use parent's environment block
                                    nullptr,           // Use parent's starting directory
                                    &si,            // Pointer to STARTUPINFO structure
                                    &pi             // Pointer to PROCESS_INFORMATION structure (removed extra parentheses)
        );
        // Close process and thread handles.
        CloseHandle( pi.hProcess );
        CloseHandle( pi.hThread );
	#else
		auto full_command_line = command_line + "&";
		system(full_command_line.c_str());
		std::cout << "Executed crash command " << full_command_line << std::endl;
	#endif
}

#ifndef WIN32
/* we want to prevent allocations while we're within a crash */
const static std::string _message_fail = "failed write crash dump";
const static std::string _message_success;
bool breakpad_crash_callback(const google_breakpad::MinidumpDescriptor& descriptor, void* _context, bool succeeded) {
    return crash_callback(descriptor.path(), &*crash_context, succeeded ? _message_success : _message_fail, succeeded);
}
#else
extern LONG WINAPI unhandled_handler(struct _EXCEPTION_POINTERS* apExceptionInfo);
void win_crash_callback(const fs::path& source_file, const std::string& error, bool success) {
    crash_callback(source_file, &*crash_context, error, success);
}
#endif

bool crash_callback(const fs::path& source_file, CrashContext* context, const std::string& error_message, bool succeeded) {
	if(!succeeded) {
		/* crash dump error handling xD */

		crash_execute_detached(replace_all(
			context->error_command_line,
			"%error_message%",
            error_message
		));
	} else {
		/* "normal" crash handling */
		auto target_directory = fs::u8path(context->crash_dump_folder);
		if(!fs::exists(target_directory)) {
			try {
				fs::create_directories(target_directory);
			} catch(const fs::filesystem_error& error) {
				crash_execute_detached(replace_all(
						context->error_command_line,
						"%error_message%",
						base64::encode("failed write move crash dump (" + source_file.string() + "): Target directory could not be created: " + error.what())
				));
				return succeeded;
			}
		}
		auto target_file = target_directory / ("crash_dump_" + context->component_name + "_" + source_file.filename().string());
		string error;
		if(!rename_or_move(source_file, target_file, error)) {
			crash_execute_detached(replace_all(
					context->error_command_line,
					"%error_message%",
					base64::encode("failed write move crash dump (" + source_file.string() + "): " + error)
			));
			return succeeded;
		}
		crash_execute_detached(replace_all(
				context->success_command_line,
				"%crash_path%",
				base64::encode(target_file.string())
		));
	}
	return succeeded;
}

extern void create_minidump(struct _EXCEPTION_POINTERS* apExceptionInfo);

bool signal::setup(std::unique_ptr<CrashContext>& context) {
#ifndef WIN32
	global_crash_handler = make_unique<google_breakpad::ExceptionHandler>(google_breakpad::MinidumpDescriptor("/tmp"), nullptr, breakpad_crash_callback, nullptr, true, -1);
#else
    global_crash_handler = AddVectoredExceptionHandler(0, unhandled_handler); /* this only works! */
#endif
	crash_context = move(context);
	return true;
}

bool signal::active() {
	return !!crash_context;
}

void signal::finalize() {
#ifndef WIN32
	global_crash_handler.reset();
#else
	if(global_crash_handler)
        RemoveVectoredExceptionHandler(global_crash_handler);

    global_crash_handler = nullptr;
#endif
	crash_context.reset();
}