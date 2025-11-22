#include "./threads.h"
#include <system_error>
#include <cstring>

#ifdef WIN32
#include <Windows.h>
#else
#include <pthread.h>
#endif

std::string threads::name(std::thread &thread) {
#ifdef WIN32
    static std::string _empty{};
    return _empty;
#else
    char buffer[255]; /* min 16 characters */
	pthread_getname_np(thread.native_handle(), buffer, 255);
	return std::string{buffer};
#endif
}

bool threads::name(std::thread &thread, const std::string_view &name) {
#ifdef WIN32
    return false;
#else
    char buffer[255]; /* min 16 characters */

	memcpy(buffer, name.data(), name.length());
	buffer[name.length()] = '\0';
	buffer[16] = '\0'; /* cut of the name after 16 characters */

	auto error = pthread_setname_np(thread.native_handle(), buffer);
	return error == 0;
#endif
}

bool threads::save_join(std::thread &thread, bool rd) {
    try {
        if(thread.joinable())
            thread.join();
    } catch(const std::system_error& ex) {
        if(ex.code() == std::errc::resource_deadlock_would_occur) {
            if(rd)
                return false;
            throw;
        } else if(ex.code() == std::errc::no_such_process) {
            return false;
        } else if(ex.code() == std::errc::invalid_argument) {
            return false;
        } else {
            throw;
        }
    }
    return true;
}

bool threads::timed_join(std::thread &thread, const std::chrono::nanoseconds &timeout) {
#ifdef WIN32
    auto result = WaitForSingleObject(thread.native_handle(), (DWORD) std::chrono::floor<std::chrono::milliseconds>(timeout).count());
    if(result != 0)
        return false;
    if(thread.joinable())
        thread.join();
    return result;
#else
    struct timespec ts{};
	if (clock_gettime(CLOCK_REALTIME, &ts) == -1)
		return false; /* failed to get clock time */

	auto seconds = std::chrono::floor<std::chrono::seconds>(timeout);
	auto nanoseconds = std::chrono::ceil<std::chrono::nanoseconds>(timeout) - seconds;
	ts.tv_sec += seconds.count();
	ts.tv_nsec += nanoseconds.count();
	if(ts.tv_nsec >= 1e9) {
		ts.tv_sec += 1;
		ts.tv_nsec -= 1e9;
	}
	auto result = pthread_timedjoin_np(thread.native_handle(), nullptr, &ts);
	if(result > 0 && result != ESRCH) return false;

	/* let the std lib set their flags */
	std::thread _empty{}; /* could be destroyed even with an "active" thread handle because we overwrote the std::terminate() function with a macro */
	_empty = std::move(thread);

	/*
	 * Undefined behaviour:
	 *   We destroy everything in a non trivial class,
	 *   But when we take a closer look its just a wrapper around the native_handle type which could be an DWORD or a pthread_t which are both trivial destructible!
	 */
	memset(&_empty, 0, sizeof(_empty)); // NOLINT(bugprone-undefined-memory-manipulation)

	return true;
#endif
}
