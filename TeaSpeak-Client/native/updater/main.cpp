#define _CRT_SECURE_NO_WARNINGS // Disable MSVC localtime warning
#include <string>
#include <chrono>
#include <thread>
#include <filesystem>

#include "./logger.h"
#include "./config.h"
#include "./util.h"
#include "./file.h"

#include "./ui.h"

using namespace std;
using namespace log_helper;

std::string current_time() {
    time_t     now = time(nullptr);
    struct tm  tstruct{};
    char       buf[80];
    tstruct = *localtime(&now);
    strftime(buf, sizeof(buf), "%Y-%m-%d.%X", &tstruct);

    return buf;
}


#ifndef WIN32
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

static bool daemonize() {
    pid_t pid, sid;
    int fd;

    /* already a daemon */
    if (getppid() == 1)
        return true;

    /* Fork off the parent process */
    pid = fork();
    if (pid < 0) {
        return false;
    }

    if (pid > 0) {
        exit(EXIT_SUCCESS); /*Killing the Parent Process*/
    }

    /* At this point we are executing as the child process */

    /* Create a new SID for the child process */
    sid = setsid();
    if (sid < 0) {
        return false;
    }

    /* Change the current working directory. */
    if ((chdir("/")) < 0) {
        return false;
    }


    fd = open("/dev/null",O_RDWR, 0);

    if (fd != -1)
    {
        dup2 (fd, STDIN_FILENO);
        dup2 (fd, STDOUT_FILENO);
        dup2 (fd, STDERR_FILENO);

        if (fd > 2)
        {
            close (fd);
        }
    }

    /*resettign File Creation Mask */
    umask(027);
    return true;
}
#endif

bool requires_permission_elevation() {
    if(!config::permission_test_directory.has_value()) {
        /* Old clients don't provide that. We assume yes. */
        return true;
    }

    return file::directory_writeable(*config::permission_test_directory);
}

std::string log_file_path;
int main(int argc, char** argv) {
    srand((unsigned int) chrono::floor<chrono::nanoseconds>(chrono::system_clock::now().time_since_epoch()).count());

    log_file_path = argc > 2 ? argv[1] : "update_installer.log";
    //logger::info("Starting log at %s", log_file_path.c_str());

    if(!logger::pipe_file(log_file_path)) {
        logger::error("failed to open log file!");
    } else {
        logger::info("----------- log started at %s -----------", current_time().c_str());
        atexit([]{
            logger::info("----------- log ended at %s -----------", current_time().c_str());
	        logger::flush();
        });
    }

    if(argc < 3) {
        logger::fatal("Invalid argument count (%d). Exiting...", argc);
        return 1;
    }


    if(argc >= 4 && std::string_view{argv[3]} != "no-daemon") {
#ifndef WIN32
        logger::info("Deamonize process");
        if(!daemonize()) {
            logger::fatal("Failed to demonize process", argc);
            return 4;
        }
        logger::info("Deamonized process");
#endif
    }

#ifdef WIN32
    if(requires_permission_elevation()) {
        auto admin = is_administrator();
        logger::info("App executed as admin: %s", admin ? "yes" : "no");
        if(!admin) {
            logger::info("Requesting administrator rights");
            if(!request_administrator(argc, argv)) {
                execute_callback_fail_exit("permissions", "failed to get administrator permissions");
            }
            logger::info("Admin right granted. New updater instance executes the update now.");
            return 0;
        }
    }
#endif
    logger::info("loading config from file %s", argv[2]);
    {
        string error;
        if(!config::load(error, argv[2])) {
            logger::fatal("failed to load config: " + error);
            return 2;
        }
    }

    {
        logger::info("Awaiting the unlocking of all files");
        await_unlock:
        auto begin = chrono::system_clock::now();
        while(true) {
            bool locked = false;
            for(shared_ptr<config::LockFile>& file : config::locking_files) {
                if(file::file_locked(file->filename)) {
                    locked = true;
                    if(chrono::system_clock::now() - std::chrono::milliseconds{1000} > begin) { /* we don't use the lock timeout here because we've a new system */
                        auto result = ui::open_file_blocked(file->filename);
                        if(result == ui::FileBlockedResult::PROCESSES_CLOSED || result == ui::FileBlockedResult::NOT_IMPLEMENTED)
                            goto await_unlock;
	                    logger::fatal(
	                    		"Failed to lock file %s. Timeout: %d, Time tried: %d",
	                    		file->filename.c_str(),
	                    		file->timeout,
	                    		chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - begin).count()
                        );
                        execute_callback_fail_exit(file->error_id, "failed to lock file");
                    }
                }
            }
            if(!locked)
                break;
            this_thread::sleep_for(chrono::milliseconds(250));
        }
        auto end = chrono::system_clock::now();
        logger::info("All files have been unlocked (%dms required)", chrono::duration_cast<chrono::milliseconds>(end - begin).count());
    }

    string error;
    for(const std::shared_ptr<config::MovingFile>& move : config::moving_actions) {
        logger::info("Moving file from %s to %s", move->source.c_str(), move->target.c_str());
        if(!file::move(error, move->source, move->target)) {
            logger::fatal("failed to move file %s to %s (%s)", move->source.c_str(), move->target.c_str(), error.c_str());
            execute_callback_fail_exit(move->error_id, error);
        }
    }

    if(config::backup) {
        logger::info("Cleaning up backup directly");
        file::commit();
    }
    logger::info("Update installing successfully!");
    execute_callback_success_exit();
}