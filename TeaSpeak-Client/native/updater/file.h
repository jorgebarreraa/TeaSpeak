#pragma once


#include <string>

namespace file {
    /**
     * @param source
     * @param target If empty then the file will be deleted
     * @return
     */
    extern bool move(std::string& error, const std::string& source, const std::string& target);

    extern void drop_backup(const std::string& source);
    extern std::string register_backup(const std::string& source);
    extern void rollback();
    extern void commit();

    /**
     * @param path The target path to test
     * @returns true if the target path is writeable or if it does not exists is createable.
     */
    extern bool directory_writeable(const std::string &path /* file */);
    extern bool file_locked(const std::string& file);
}