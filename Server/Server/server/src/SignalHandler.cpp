
#include "VirtualServer.h"
#include "SignalHandler.h"
#include "VirtualServerManager.h"
#include "InstanceHandler.h"
#include "ShutdownHelper.h"
#include <csignal>
#include <log/LogUtils.h>
#include <experimental/filesystem>
#include <src/terminal/PipedTerminal.h>

#include <iterator>

#define BREAKPAD_EXCEPTION_HANDLER 1
#ifdef BREAKPAD_EXCEPTION_HANDLER
#include <breakpad/client/linux/handler/exception_handler.h>
#endif

using namespace std;
namespace fs = std::experimental::filesystem;

#ifdef BREAKPAD_EXCEPTION_HANDLER
google_breakpad::ExceptionHandler* globalExceptionHandler = nullptr;
#endif
#define SIG(s, c) \
    if(signal(s, c) != nullptr) logError(LOG_GENERAL, "Cant setup signal handler for " #s);


void print_current_exception() {
    if(std::current_exception()) {
        logCritical(LOG_GENERAL, "Exception reached stack root and cause the server to crash!");
        logCritical(LOG_GENERAL, "  Type: {}", std::current_exception().__cxa_exception_type()->name());
        try {
            std::rethrow_exception(std::current_exception());
        } catch(std::exception& ex) {
            logCritical(LOG_GENERAL, "  Message: {}", ex.what());
        } catch(...) {}
    }
}

extern bool mainThreadDone;
#ifdef BREAKPAD_EXCEPTION_HANDLER
static bool dumpCallback(const google_breakpad::MinidumpDescriptor& descriptor, void* context, bool succeeded) {
    if(ts::server::isShuttingDown()) {
        /* We don't care about this crash dump. Remove it. */
        std::error_code error_code{};
        fs::remove(fs::u8path(descriptor.path()), error_code);
        return true;
    }

    logCritical(LOG_GENERAL, "The server crashed!");
    try {
        if(!fs::exists(fs::u8path(ts::config::crash_path))) {
            fs::create_directories(fs::u8path(ts::config::crash_path));
        }

        auto path = fs::u8path(descriptor.path());
        path = fs::u8path(ts::config::crash_path + "crash_dump_" + path.filename().string());
        fs::rename(fs::u8path(descriptor.path()), path);
        logCritical(LOG_GENERAL, "Wrote crash dump to " + path.relative_path().string());
    } catch (...) {
        logCritical(LOG_GENERAL, "Failed to write/move crash dump!");
    }
    print_current_exception();

    logCritical(LOG_GENERAL, "Please report this crash to the TeaSpeak maintainer WolverinDEV");
    logCritical(LOG_GENERAL, "Official issue and bug tracker url: https://github.com/TeaSpeak/TeaSpeak/issues");
    logCritical(LOG_GENERAL, "Any reports of crashes are useless if you not provide the above generated crashlog!");
    logCritical(LOG_GENERAL, "Stopping server");

    terminal::finalize_pipe();
    ts::server::shutdownInstance(ts::config::messages::applicationCrashed);
    while(!mainThreadDone) {
        threads::self::sleep_for(chrono::seconds(1));
    }
    return succeeded;
}
#endif

std::atomic spawn_failed_count = 0;
bool ts::syssignal::setup() {
    logMessage(LOG_GENERAL, "Setting up exception handler");
#ifdef BREAKPAD_EXCEPTION_HANDLER
    globalExceptionHandler = new google_breakpad::ExceptionHandler(google_breakpad::MinidumpDescriptor("."), nullptr, dumpCallback, nullptr, true, -1);
#endif

    SIG(SIGTERM, &ts::syssignal::handleStopSignal);
    if(isatty(fileno(stdin))) {
        //We cant listen for this signal if stdin ist a atty
        SIG(SIGINT, &ts::syssignal::handleStopSignal);
    }
    //SIG(SIGABRT, &ts::syssignal::handleAbortSignal);
    std::set_terminate(ts::syssignal::handleTerminate);

    return true;
}

bool ts::syssignal::setup_threads() {
    threads::set_global_error_handler([](auto error) {
        if(error == threads::ThreadError::HANDLE_DELETE_UNDETACHED) {
            logCritical(LOG_GENERAL, "Missed out thread detachment! This could lead to memory leaks!");
            return threads::ThreadErrorAction::IGNORE;
        } else if(error == threads::ThreadError::SPAWN_FAILED) {
            logCritical(LOG_GENERAL, "Spawning a new thread failed!");
            if(spawn_failed_count++ == 0) {
                logCritical(LOG_GENERAL, "Stopping process!");
                try {
                    std::thread([]{
                        ts::server::shutdownInstance("Failed to spawn new threads! Safety shutdown");
                    }).detach();
                } catch(...) {
                    logCritical(LOG_GENERAL, "Failed to spawn shutdown thread (Of cause...). Stopping application directly!");
                    logCritical(LOG_GENERAL, "If this happens frequently dont forget to checkout std stderr channel for more information");
                    raise(SIGKILL);
                }
            }
            return threads::ThreadErrorAction::RAISE;
        }

        return threads::ThreadErrorAction::RAISE;
    });
    return true;
}

atomic_int signal_count = 0;
void ts::syssignal::handleStopSignal(int signal) {
    logMessageFmt(true, LOG_INSTANCE, "Got stop signal ({}). Stopping instance.", signal == SIGTERM ? "SIGTERM" :
                                                                                  signal == SIGINT ? "SIGINT" :
                                                                                  "UNKNOWN (" + to_string(signal) + ")");
    if(signal_count++ >= 3) {
        logMessageFmt(true, LOG_INSTANCE, "Got stop signal more that tree times. Force exiting instance.");
        raise(SIGKILL);
    }
    ts::server::shutdownInstance();
}

void ts::syssignal::handleAbortSignal(int) {
    logCritical(0, "The server crashed (Abort signal received)!");
    print_current_exception();
}

void ts::syssignal::handleTerminate() {
    logCritical(0, "The server crashed (Received a terminate signal)!");
    print_current_exception();
}