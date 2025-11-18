#include <sstream>

#include "util.h"
#include "config.h"
#include "logger.h"
#include "base64.h"
#include "file.h"
#include <cstring>

using namespace std;

#ifdef WIN32
    bool is_executable(const std::string& file) {
        DWORD lpBinaryType[100];
        return GetBinaryTypeA(file.c_str(), lpBinaryType);
    }

    void execute_app(LPCTSTR lpApplicationName,LPSTR arguments)
    {
        STARTUPINFO si;
        memset(&si, 0, sizeof(si));

        PROCESS_INFORMATION pi;
        memset(&pi, 0, sizeof(pi));

        si.cb = sizeof(si);

        // start the program up
        auto result = CreateProcessA(
                lpApplicationName,   // the path
                arguments,        // Command line
                nullptr,           // Process handle not inheritable
                nullptr,           // Thread handle not inheritable
                false,          // Set handle inheritance to FALSE
                0,              // No creation flags
                nullptr,           // Use parent's environment block
                nullptr,           // Use parent's starting directory
                &si,            // Pointer to STARTUPINFO structure
                &pi             // Pointer to PROCESS_INFORMATION structure (removed extra parentheses)
        );
        (void) result;

        // Close process and thread handles.
        CloseHandle( pi.hProcess );
        CloseHandle( pi.hThread );
    }
#else
    #include <unistd.h>
    #include <sys/types.h>
    #include <sys/stat.h>

    bool is_executable(const std::string& file) {
        chmod(file.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
        return access(file.c_str(), F_OK | X_OK) == 0;
    }

    typedef const char* LPSTR;
    typedef const char* LPCTSTR;

    void execute_app(LPCTSTR, LPSTR app_arguments) {
        auto arguments_length = strlen(app_arguments);

        auto buffer_size = arguments_length + 512;
        auto buffer = (char*) malloc(buffer_size);

        memcpy(buffer, app_arguments, arguments_length);
        memcpy(buffer + arguments_length, " &", 2);

        system(app_arguments);
        free(buffer);
    }
#endif

extern std::string log_file_path;
inline std::string build_callback_info(const std::string& error_id, const std::string& error_message) {
    stringstream ss;

    ss << ";log_file:" << base64::encode(log_file_path);
    if(!error_id.empty()) {
        ss << ";error_id:" << base64::encode(error_id);
        ss << ";error_message:" << base64::encode(error_message);
    }
    auto result = ss.str();
    if(result.length() > 0)
        result = result.substr(1);
    return base64::encode(result);
}

void execute_callback_fail_exit(const std::string& error, const std::string& error_message) {
    file::rollback();

    if(!is_executable(config::callback_file)) {
        logger::fatal("callback file (%s) is not executable! Ignoring fail callback", config::callback_file.c_str());
        logger::flush();
        exit(1);
    }

    auto cmd_line = config::callback_file + " " + config::callback_argument_fail + build_callback_info(error, error_message);
    logger::info("executing callback file %s with fail command line %s", config::callback_file.c_str(), cmd_line.c_str());
	logger::flush();
    execute_app((LPCTSTR) config::callback_file.c_str(), (LPSTR) cmd_line.c_str());
    exit(1);
}

void execute_callback_success_exit() {
    if(!is_executable(config::callback_file)) {
        logger::fatal("callback file (%s) is not executable! Ignoring success callback", config::callback_file.c_str());
        logger::flush();
        exit(1);
    }

    auto cmd_line = config::callback_file + " " + config::callback_argument_success + build_callback_info("", "");
    logger::info("executing callback file %s with success with command line %s", config::callback_file.c_str(), cmd_line.c_str());
	logger::flush();
    execute_app((LPCTSTR) config::callback_file.c_str(), (LPSTR) cmd_line.c_str());
    exit(0);
}

#ifdef WIN32
bool is_administrator() {
    bool result{false};
    HANDLE token_handle{nullptr};
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token_handle)) {
        TOKEN_ELEVATION eval{};
        DWORD eval_size = sizeof(TOKEN_ELEVATION);
        if (GetTokenInformation(token_handle, TokenElevation, &eval, eval_size, &eval_size)) {
            result = eval.TokenIsElevated;
        }
    }

    if (token_handle)
        CloseHandle(token_handle);

    return result;
}

extern bool request_administrator(int argc, char** argv) {
    char szPath[MAX_PATH];
    if (GetModuleFileName(nullptr, szPath, MAX_PATH)) {
        SHELLEXECUTEINFO sei = { sizeof(sei) };

        sei.lpVerb = "runas";
        sei.lpFile = szPath;
        sei.hwnd = nullptr;
        sei.nShow = SW_NORMAL;
        
        if(argc > 1) {
            size_t param_size = 0;
            for(int i = 1; i < argc; i++) {
                param_size += strlen(argv[i]) + 1;
            }
            sei.lpParameters = (char*) malloc(param_size);
            if(!sei.lpParameters) {
                return false;
            }

            auto buf = (char*) sei.lpParameters;
            for(int i = 1; i < argc; i++) {
                const auto length = strlen(argv[i]);
                memcpy(buf, argv[i], length);
                buf += length;
                *buf++ = ' ';
            }
            *buf = 0;
        }
        
        bool success = ShellExecuteExA(&sei);
        if(sei.lpParameters) {
            ::free((void*) sei.lpParameters);
        }
        return success;
    }
    return true;
}
#endif