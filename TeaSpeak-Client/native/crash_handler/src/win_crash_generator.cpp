#include <Windows.h>
#include <DbgHelp.h>
#include <tchar.h>
#include <string>
#include <algorithm>
#include <strsafe.h>

#ifdef WIN32
    #include <filesystem>
    namespace fs = std::filesystem;
#else
    #include <experimental/filesystem>
    namespace fs = std::experimental::filesystem;
#endif

using namespace std;
extern void win_crash_callback(const fs::path& source_file, const std::string& error, bool success);

typedef BOOL (WINAPI *MINIDUMPWRITEDUMP)(
        HANDLE hProcess,
        DWORD dwPid,
        HANDLE hFile,
        MINIDUMP_TYPE DumpType,
        CONST PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam,
        CONST PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam,
        CONST PMINIDUMP_CALLBACK_INFORMATION CallbackParam
);

fs::path generate_temp_file(std::string& error) {
    WCHAR szPath[MAX_PATH];
    WCHAR szFileName[MAX_PATH];
    DWORD dwBufferSize = MAX_PATH;
    SYSTEMTIME stLocalTime;

    GetLocalTime( &stLocalTime );
    GetTempPathW( dwBufferSize, szPath );

    CreateDirectoryW( szFileName, nullptr );
    StringCchPrintfW(szFileName, MAX_PATH, L"%s\\%04d%02d%02d-%02d%02d%02d-%ld-%ld.dmp",
                     szPath,
                     stLocalTime.wYear, stLocalTime.wMonth, stLocalTime.wDay,
                     stLocalTime.wHour, stLocalTime.wMinute, stLocalTime.wSecond,
                     GetCurrentProcessId(), GetCurrentThreadId());
    return fs::path(szFileName);
}

void create_minidump(struct _EXCEPTION_POINTERS* apExceptionInfo)
{
    string error;
    HANDLE hDumpFile = nullptr;
    fs::path file_path;
    HMODULE mhLib = ::LoadLibrary(_T("dbghelp.dll"));
    if(!mhLib) {
        error = "failed to file dbghelp.dll";
        goto error_handling;
    }

    auto pDump = (MINIDUMPWRITEDUMP)::GetProcAddress(mhLib, "MiniDumpWriteDump");
    if(!pDump) {
        error = "failed to file find MiniDumpWriteDump handle";
        goto error_handling;
    }

    file_path = generate_temp_file(error);
    hDumpFile = CreateFileW(file_path.wstring().c_str(), GENERIC_READ|GENERIC_WRITE, FILE_SHARE_WRITE|FILE_SHARE_READ, nullptr, CREATE_ALWAYS, 0, nullptr);
    if(!hDumpFile) {
        error = "failed to open file";
        goto error_handling;
    }

    {
        _MINIDUMP_EXCEPTION_INFORMATION ExInfo{};
        ExInfo.ThreadId = ::GetCurrentThreadId();
        ExInfo.ExceptionPointers = apExceptionInfo;
        ExInfo.ClientPointers = FALSE;

        if(!pDump(GetCurrentProcess(), GetCurrentProcessId(), hDumpFile, MiniDumpNormal, &ExInfo, nullptr, nullptr)) {
            error = "failed to generate dump file";
            goto error_handling;
        }
        ::CloseHandle(hDumpFile);
        win_crash_callback(file_path, error, true);
        return;
    }

    error_handling:
    if(hDumpFile) {
        ::CloseHandle(hDumpFile);
    }
    win_crash_callback(file_path, error, false);
}

LONG WINAPI unhandled_handler(struct _EXCEPTION_POINTERS* apExceptionInfo) {
    auto code = apExceptionInfo->ExceptionRecord->ExceptionCode;
    auto crash = false;
    switch(code) {
        case EXCEPTION_ACCESS_VIOLATION:
        case EXCEPTION_ILLEGAL_INSTRUCTION:
        case EXCEPTION_STACK_OVERFLOW:
        case EXCEPTION_FLT_DIVIDE_BY_ZERO:
        case EXCEPTION_INT_DIVIDE_BY_ZERO:
        case EXCEPTION_IN_PAGE_ERROR:
        case EXCEPTION_NONCONTINUABLE_EXCEPTION:
            crash = true;
            break;
        default:
            crash = false;
    }

    if(!crash)
        return EXCEPTION_CONTINUE_SEARCH;

    create_minidump(apExceptionInfo);
    return EXCEPTION_EXECUTE_HANDLER;
}
