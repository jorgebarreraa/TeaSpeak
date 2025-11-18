//
// Created by WolverinDEV on 31/07/2020.
//

#include "PipedTerminal.h"
#include "CommandHandler.h"

#include <event.h>
#include <sys/stat.h>
#include <thread>
#include <log/LogUtils.h>
#include <StringVariable.h>

std::string pipe_path_in{}, pipe_path_out{};

int file_descriptor_in{0}, file_descriptor_out{0};
std::thread event_loop_dispatcher{};
::event_base* event_base{nullptr};

event* event_read{nullptr};
event* event_write{nullptr};

void event_loop_executor(void*);
void event_read_callback(int, short, void*);
void event_write_callback(int, short, void*);

void terminal::initialize_pipe(const std::string& pipe_path) {
    {
        std::string path;
        if(pipe_path.empty()) {
            path = "/tmp/teaspeak_${pid}_${direction}.term";
        } else {
            path = pipe_path;
        }

        pipe_path_in = strvar::transform(path, strvar::StringValue{"pid", std::to_string(getpid())}, strvar::StringValue{"direction", "in"});
        pipe_path_out = strvar::transform(path, strvar::StringValue{"pid", std::to_string(getpid())}, strvar::StringValue{"direction", "out"});
    }

    auto result = mkfifo(pipe_path_in.c_str(), 0666);
    if(result != 0){
        logWarning(LOG_INSTANCE, "Failed to create incoming terminal pipe ({}/{})", errno, strerror(errno));
        finalize_pipe();
        return;
    }

    file_descriptor_in = open(pipe_path_in.c_str(), (unsigned) O_NONBLOCK | (unsigned) O_RDONLY);
    if(file_descriptor_in <= 0) {
        logWarning(LOG_INSTANCE, "Failed to open incoming terminal pipe ({}/{})", errno, strerror(errno));
        finalize_pipe();
        return;
    }

    result = mkfifo(pipe_path_out.c_str(), 0666);
    if(result != 0){
        logWarning(LOG_INSTANCE, "Failed to create outgoing terminal pipe ({}/{})", errno, strerror(errno));
        finalize_pipe();
        return;
    }

    /* we can't do a write only open, else along with the O_NONBLOCK we'll get No such device or address */
    file_descriptor_out = open(pipe_path_out.c_str(), (unsigned) O_NONBLOCK | (unsigned) O_RDWR);
    if(file_descriptor_out <= 0) {
        logWarning(LOG_INSTANCE, "Failed to open outgoing terminal pipe ({}/{})", errno, strerror(errno));
        finalize_pipe();
        return;
    }

    event_base = event_base_new();
    if(!event_base) {
        logWarning(LOG_INSTANCE, "Failed to open terminal pipe ({}/{})", errno, strerror(errno));
        finalize_pipe();
        return;
    }

    event_loop_dispatcher = std::thread{event_loop_executor, event_base};
    event_read = event_new(event_base, file_descriptor_in, (unsigned) EV_READ | (unsigned) EV_PERSIST, event_read_callback, nullptr);
    event_write = event_new(event_base, file_descriptor_out, EV_WRITE, event_write_callback, nullptr);
    event_add(event_read, nullptr);

    logMessage(LOG_INSTANCE, "Terminal pipe started (Incoming: {}, Outgoing: {}).", pipe_path_in, pipe_path_out);
}

void terminal::finalize_pipe() {
    if(auto ev_base{std::exchange(event_base, nullptr)}; ev_base) {
        event_base_loopexit(ev_base, nullptr);
        if(event_loop_dispatcher.joinable() && std::this_thread::get_id() != event_loop_dispatcher.get_id())
            event_loop_dispatcher.join();

        /* events get deleted when the base gets freed */
        event_read = nullptr;
        event_write = nullptr;
    }

    if(file_descriptor_in > 0) {
        close(file_descriptor_in);
        file_descriptor_in = 0;

        remove(pipe_path_in.c_str());
    }

    if(file_descriptor_out > 0) {
        close(file_descriptor_out);
        file_descriptor_out = 0;

        remove(pipe_path_out.c_str());
    }
}

void event_loop_executor(void* ptr_event_base) {
    auto base = (struct event_base*) ptr_event_base;

    while(!event_base_got_exit(base))
        event_base_loop(base, EVLOOP_NO_EXIT_ON_EMPTY);

    event_base_free(base);
}

/* no buffer lock needed since we're only accessing them via one thread */
constexpr static auto kReadBufferSize{8 * 1024};
char read_buffer[kReadBufferSize];
size_t read_buffer_index{0};

constexpr static auto kWriteBufferSize{64 * 1024};
char write_buffer[kWriteBufferSize];
size_t write_buffer_index{0};

void append_write_buffer(std::string_view message) {
    if(message.length() > kWriteBufferSize) {
        logWarning(LOG_INSTANCE, "Trying to write a too long message to the terminal pipe. Truncating {} bytes from the beginning.", message.length() - kWriteBufferSize);
        message = message.substr(message.length() - kWriteBufferSize);
    }

    if(write_buffer_index + message.length() > kWriteBufferSize) {
        logWarning(LOG_INSTANCE, "Encountering a write buffer overflow. Truncating bytes form the beginning.");

        auto offset = message.length() + write_buffer_index - kWriteBufferSize;
        if(write_buffer_index > offset) {
            memcpy(write_buffer, write_buffer + offset, write_buffer_index - offset);
            write_buffer_index = write_buffer_index - offset;
        } else {
            write_buffer_index = 0;
        }
    }

    memcpy(write_buffer + write_buffer_index, message.data(), message.length());
    write_buffer_index += message.length();

    event_add(event_write, nullptr);
}

bool process_next_command() {
    auto new_line_index = (char*) memchr(read_buffer, '\n', read_buffer_index);
    if(!new_line_index) {
        return false;
    }

    std::string command{read_buffer, (size_t) (new_line_index - read_buffer)};
    if(new_line_index == read_buffer + read_buffer_index) {
        read_buffer_index = 0;
    } else {
        auto length_left = read_buffer_index - (new_line_index - read_buffer + 1);
        memcpy(read_buffer, new_line_index + 1, length_left);
        read_buffer_index = length_left;
    }

    if(command.find_first_not_of(' ') == std::string::npos) {
        process_next_command();
        return false;
    }

    logMessage(LOG_INSTANCE, "Dispatching command received via pipe \"{}\".", command);

    terminal::chandler::CommandHandle handle{};
    handle.command = command;
    if(!terminal::chandler::handleCommand(handle)) {
        append_write_buffer("error\n");
    } else {
        append_write_buffer("ok\n");
    }

    for(const auto& line : handle.response) {
        append_write_buffer(line + "\n");
    }

    append_write_buffer("\r\n");
    append_write_buffer("\r\n");
    return true;
}

void event_read_callback(int fd, short events, void*) {
    if((unsigned) events & (unsigned) EV_READ) {
        while(true) {
            if(kReadBufferSize == read_buffer_index) {
                logWarning(LOG_INSTANCE, "Terminal pipe line buffer overflow. Flushing buffer.");
                read_buffer_index = 0;
            }

            auto read = ::read(fd, read_buffer + read_buffer_index, kReadBufferSize - read_buffer_index);
            if(read < 0) {
                if(errno == EAGAIN) {
                    event_add(event_read, nullptr);
                    return;
                }

                logError(LOG_INSTANCE, "Terminal pipe encountered a read error: {}/{}. Closing terminal.", errno, strerror(errno));
                terminal::finalize_pipe();
                return;
            } else if(read == 0) {
                return;
            }

            read_buffer_index += read;
            if(process_next_command())
                return;
        }
    }
}

void event_write_callback(int fd, short events, void*) {
    if((unsigned) events & (unsigned) EV_WRITE) {
        while(true) {
            auto written = ::write(fd, write_buffer, write_buffer_index);
            if(written < 0) {
                if(errno == EAGAIN) {
                    event_add(event_write, nullptr);
                    return;
                }

                logError(LOG_INSTANCE, "Terminal pipe encountered a write error: {}/{}. Closing terminal.", errno, strerror(errno));
                terminal::finalize_pipe();
                return;
            } else if(written == 0) {
                return;
            } else if(written == write_buffer_index) {
                write_buffer_index = 0;
                return;
            } else {
                memcpy(write_buffer, write_buffer + written, write_buffer_index - written);
                write_buffer_index -= written;
            }
        }
    }
}
