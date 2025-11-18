#include <map>
#include "file.h"
#include "logger.h"
#include "config.h"
#ifndef WIN32
    #include <stdio.h>
#endif

using namespace std;
#ifndef WIN32
    #define EXPERIMENTAL_FS
#endif

#ifdef EXPERIMENTAL_FS
    #include <experimental/filesystem>
    #include <iostream>
    namespace fs = std::experimental::filesystem;
#else
    #include <filesystem>
    namespace fs = std::filesystem;
#endif
using namespace log_helper;

std::map<std::string, std::string> move_back;
std::map<std::string, std::string> backup_mapping;

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

bool file::move(std::string &error, const std::string &source, const std::string &target) {
    auto source_path = fs::u8path(source);
    if(!fs::exists(source_path)) {
        error = "source file does not exists";
        return false;
    }
    source_path = fs::absolute(source_path);

    if(target.empty() || target == "/dev/null") {
        /* delete the file or directory */

        logger::debug("Deleting file " + argument_t<fs::path::value_type>::value, source_path.c_str());
        if(config::backup) {
            auto backup_target = file::register_backup(source_path.string());
            if(backup_target.empty()) {
                drop_backup(backup_target);
                error = "failed to register backup";
                return false;
            }

            if(!rename_or_move(source_path, fs::u8path(backup_target), error)) {
	            drop_backup(backup_target);
	            error = "failed to move file to backup target (" + error + ")";
	            return false;
            }
        } else {
            if(fs::is_directory(source_path)) {
                try {
                    if(!fs::remove_all(source_path))
                        throw fs::filesystem_error("invalid result", error_code());
                } catch(const fs::filesystem_error& ex) {
                    error = "failed to delete directory (" + string(ex.what()) + ")";
                    return false;
                }
            } else {
                try {
                    if(!fs::remove(source_path))
                        throw fs::filesystem_error("invalid result", error_code());
                } catch(const fs::filesystem_error& ex) {
                    error = "failed to delete file (" + string(ex.what()) + ")";
                    return false;
                }
            }
        }
        return true;
    }

    auto target_path = fs::u8path(target);
    target_path = fs::absolute(target_path);

    if(!fs::is_directory(target_path.parent_path())) {
        if(!fs::exists(target_path.parent_path())) {
            try {
                fs::create_directories(target_path.parent_path());
            } catch(const fs::filesystem_error& ex) {
                error = "failed to create target directories (" + std::string{ex.what()} + ")";
                return false;
            }
        } else {
            error = "target isn't a directory!";
            return false;
        }
    }

    logger::debug("Moving file " + argument_t<fs::path::value_type>::value + " to " + argument_t<fs::path::value_type>::value, source_path.c_str(), target_path.c_str());
    if(fs::exists(target_path)) {
        logger::debug("Target file already exists. Deleting it");
        if(config::backup) {
            auto path = file::register_backup(target_path.string());
            if(path.empty()) {
                drop_backup(path);
                error = "failed to create backup path";
                return false;
            }


	        if(!rename_or_move(target_path, fs::u8path(path), error)) {
		        drop_backup(path);
		        error = "failed to delete old target file (" + error + ")";
		        return false;
	        }
        } else {
            try {
                if(fs::is_directory(target_path))
                    fs::remove_all(target_path);
                else
                    fs::remove(target_path);
            }  catch(const fs::filesystem_error& ex) {
                error = "failed to delete old target file: " + string(ex.what());
                return false;
            }
        }
    }

    move_back[target_path.string()] = source_path.string();

	if(!rename_or_move(source_path, target_path, error)) {
		error = "failed to move file (" + error + ")";
		return false;
	}
    return true;
}


string random_string(size_t length) {
    static const char alphanum[] =
            "0123456789"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz";

    string result;
    result.resize(length, '\0');

    for(char& c : result)
        c = alphanum[rand() % (sizeof(alphanum) - 1)];

    return result;
}

std::string file::register_backup(const std::string &source) {
    {
        auto path = fs::u8path(config::backup_directory);
        if(!fs::exists(path)) {
            try {
                fs::create_directories(path);
            }catch(const fs::filesystem_error& ex) {
                logger::error("Failed to create backup directory \"" + argument_t<fs::path::value_type>::value + "\": %s", path.c_str(), ex.what());
                return "";
            }
        }
    }
    std::string key;
    while(backup_mapping.count((key = random_string(20))) > 0);
    backup_mapping[key] = source;
    return fs::absolute(fs::u8path(config::backup_directory) / key).string();
}

void file::drop_backup(const std::string &source) {
    for(const auto& entry : backup_mapping) {
        if(entry.second == source) {
            backup_mapping.erase(entry.first);
            return;
        }
    }
}

void file::rollback() {
    if(!config::backup)
        return;

    logger::info("Rollbacking %d moved files", move_back.size());
    for(const auto& [backup, original] : move_back) {
        logger::debug("Attempting to moveback %s to %s", backup.c_str(), original.c_str());
        try {
            fs::rename(fs::u8path(backup), fs::u8path(original));
        } catch(const fs::filesystem_error& ex) {
            logger::warn("Failed to moveback file from %s to %s (%s)", backup.c_str(), original.c_str(), ex.what());
            continue;
        }
        logger::debug("=> success");
    }

    logger::info("Rollbacking %d deleted files", backup_mapping.size());

    for(const auto& [backup, original] : backup_mapping) {
        logger::debug("Attempting to restore %s to %s", backup.c_str(), original.c_str());
        try {
            auto source = fs::absolute(fs::u8path(config::backup_directory) / backup);
            if(!fs::exists(source)) {
                logger::warn("Failed to restore file %s (Source file missing)", original.c_str());
                continue;
            }

            auto target = fs::u8path(original);
            fs::rename(source, target);
        } catch(const fs::filesystem_error& ex) {
            logger::warn("Failed to restore file %s (%s)", original.c_str(), ex.what());
            continue;
        }
        logger::debug("=> success");
    }
    logger::info("Rollback done");
}

void file::commit() {
    if(!config::backup)
        return;

    try {
        if(!fs::remove_all(config::backup_directory))
            throw fs::filesystem_error("invalid result", error_code());
    } catch(const fs::filesystem_error& ex) {
        logger::warn("Failed to cleanup backup directory (" + string{ex.what()} + ")");
    }
    
    backup_mapping.clear();
}

#ifdef WIN32
    bool file::file_locked(const std::string &file) {
        auto file_handle = CreateFile(
                (LPCSTR) file.c_str(), /* file name*/
                (DWORD) GENERIC_WRITE,
                (DWORD) 0, /* we dont want to share */
                (LPSECURITY_ATTRIBUTES) nullptr,
                (DWORD) OPEN_EXISTING, /* file should be available */
                FILE_ATTRIBUTE_NORMAL,
                nullptr
        );
        if(file_handle == INVALID_HANDLE_VALUE) {
            if(GetLastError() == ERROR_SHARING_VIOLATION)
                return true; /* file is beeing used */

            wchar_t buf[256];
            FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf, (sizeof(buf) / sizeof(wchar_t)), nullptr);
            buf[255] = L'\0'; /* even if FormatMessageW fails we've a 0 terminator */
            auto r = wcschr(buf, L'\r');
            if(r) *r = L'\0';
            
            logger::info("Failed to open file! (%S) (%d)", buf, GetLastError());
            return true;
        }
        CloseHandle(file_handle);
        return false;
    }
#else
    bool file::file_locked(const std::string &file) {
        auto handle = fopen(file.c_str(), "a+");
        if(handle) {
            fclose(handle);
            return false;
        }
        return true;
    }
#endif

#ifdef WIN32
bool CanAccessFolder(LPCTSTR folderName, DWORD genericAccessRights)
{
    bool bRet = false;
    DWORD length = 0;
    if (!::GetFileSecurity(folderName, OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, nullptr, 0, &length) && ERROR_INSUFFICIENT_BUFFER == ::GetLastError()) {
        auto security = static_cast<PSECURITY_DESCRIPTOR>(::malloc(length));
        if (security && ::GetFileSecurity(folderName, OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, security, length, &length )) {
            HANDLE hToken = NULL;
            if (::OpenProcessToken( ::GetCurrentProcess(), TOKEN_IMPERSONATE | TOKEN_QUERY |
                                                           TOKEN_DUPLICATE | STANDARD_RIGHTS_READ, &hToken )) {
                HANDLE hImpersonatedToken = NULL;
                if (::DuplicateToken( hToken, SecurityImpersonation, &hImpersonatedToken )) {
                    GENERIC_MAPPING mapping = { 0xFFFFFFFF };
                    PRIVILEGE_SET privileges = { 0 };
                    DWORD grantedAccess = 0, privilegesLength = sizeof( privileges );
                    BOOL result = FALSE;

                    mapping.GenericRead = FILE_GENERIC_READ;
                    mapping.GenericWrite = FILE_GENERIC_WRITE;
                    mapping.GenericExecute = FILE_GENERIC_EXECUTE;
                    mapping.GenericAll = FILE_ALL_ACCESS;

                    ::MapGenericMask( &genericAccessRights, &mapping );
                    if (::AccessCheck( security, hImpersonatedToken, genericAccessRights,
                                       &mapping, &privileges, &privilegesLength, &grantedAccess, &result )) {
                        bRet = (result == TRUE);
                    }
                    ::CloseHandle( hImpersonatedToken );
                }
                ::CloseHandle( hToken );
            }
            ::free( security );
        }
    }

    return bRet;
}
#endif

bool file::directory_writeable(const std::string &path) {
#ifdef WIN32
    return CanAccessFolder(path.c_str(), GENERIC_WRITE);
#else
    /* TODO: Check for file permissions? Is this method even needed? */
    return false;
#endif
}