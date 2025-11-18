//
// Created by WolverinDEV on 20/04/2021.
//

#pragma once

#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

/* Attention: Do not call any libteaspeak_file_* functions within being in the callback! */
struct TeaFileNativeCallbacks {
    uint32_t version;

    void(*log)(uint8_t /* level */, const void* /* callback data */, const char* /* message */, uint32_t /* length */);
};

struct TeaFilePath {
    uint32_t type;
    uint64_t channel_id;
    const char* path;
};

struct TeaFilePathInfo {
    int32_t query_result;
    uint32_t file_type;
    const char* name;
    uint64_t modify_timestamp;
    uint64_t file_size;
    bool directory_empty;
};

extern const char* libteaspeak_file_version();
extern void libteaspeak_free_str(const char* /* ptr */);

extern const char* libteaspeak_file_initialize(const TeaFileNativeCallbacks* /* */, size_t /* size of the callback struct */);
extern void libteaspeak_file_finalize();

extern void libteaspeak_file_system_register_server(const char* /* server unique id */);
extern void libteaspeak_file_system_unregister_server(const char* /* server unique id */, bool /* delete files */);

extern void libteaspeak_file_free_file_info(const TeaFilePathInfo*);
extern const char* libteaspeak_file_system_query_file_info(const char* /* server unique id */, const TeaFilePath* /* file path */, size_t /* path count */, const TeaFilePathInfo ** /* result */);
extern const char* libteaspeak_file_system_query_directory(const char* /* server unique id */, const TeaFilePath* /* file path */, const TeaFilePathInfo ** /* result */);
extern const char* libteaspeak_file_system_delete_files(const char* /* server unique id */, const TeaFilePath* /* file path */, size_t /* path count */, const TeaFilePathInfo ** /* result */);
extern uint32_t libteaspeak_file_system_create_channel_directory(const char* /* server unique id */, uint32_t /* channel id */, const char* /* path */);
extern uint32_t libteaspeak_file_system_rename_channel_file(const char* /* server unique id */, uint32_t /* old channel id */, const char* /* old path */, uint32_t /* new channel id */, const char* /* new path */);

#ifdef __cplusplus
}
#endif