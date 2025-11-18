#include "logger.h"
#include <mutex>
#include <stdio.h>
#include <cstring>
#include <map>
#include <iostream>
#include <fstream>
#include <memory>
#include <iomanip>
#include <sstream>

#ifndef WIN32
    #include <cstdarg>
#endif

#define LOG_BUFFER_SIZE 4096
thread_local std::unique_ptr<char, decltype(free)*> log_buffer{nullptr, nullptr};

std::mutex target_file_lock;
std::unique_ptr<std::ofstream> file_stream;

std::string logging_session;
void logger::log_raw(logger::level::value level, const char* format, ...) {
    if(!log_buffer) {
        log_buffer = std::unique_ptr<char, decltype(free)*>((char*) malloc(LOG_BUFFER_SIZE), ::free);
    }

    if(logging_session.empty()) {
        std::ostringstream os;
        os << std::uppercase << std::setfill('0') << std::setw(4) << std::hex << (uint32_t) rand();
        logging_session = "[" + os.str() + "]";
    }


    va_list arg_lst;
    va_start(arg_lst, format);
    auto result = vsnprintf(log_buffer.get(), LOG_BUFFER_SIZE, format, arg_lst);
    va_end(arg_lst);


    {
        std::lock_guard lock(target_file_lock);
        if(result < 0) {
            fprintf(stdout, "failed to format log message (%d)\n", result);
            if(file_stream)
                *file_stream << logging_session << "f ailed to format log message (" << result << ")\n";
        } else {
            fprintf(stdout, "[%d] ", level);
            fwrite(log_buffer.get(), result, 1, stdout);
            fprintf(stdout, "\n");

            if(file_stream) {
                *file_stream << logging_session << "[" << level << "] ";
                file_stream->write(log_buffer.get(), result);
                *file_stream << "\n";
            }
        }
    }
}

void logger::flush() {
    std::lock_guard lock(target_file_lock);
   if(file_stream) {
       file_stream->flush();
   }
}


bool logger::pipe_file(const std::string_view &target) {
    auto handle = std::make_unique<std::ofstream>(std::string(target.data(), target.size()), std::ofstream::app | std::ofstream::out);
    if(!handle->good()) {
        return false;
    }
    file_stream = std::move(handle);
    return true;
}