#pragma once

#include <spdlog/details/log_msg.h>

#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <string_view>

namespace tc_logger {
	namespace level = spdlog::level;

	namespace category {
		enum value {
			general,
			audio,
			connection,
			voice_connection,
			socket,

			file_transfer,

			memory
		};

		using category = value;
	}

	extern void initialize_raw();
	extern void initialize_node();
	extern void(*_force_log)(category::value /* category */, level::level_enum /* lvl */, const std::string_view& /* message */);
	extern void err_handler(const std::string& /* error */);

	inline bool should_log(level::level_enum lvl) {
		return true;
	}

	template<typename... Args>
	inline void force_log(category::value category, level::level_enum lvl, const char *fmt, const Args &... args) {
		try {
			const auto msg = fmt::format(fmt, args...);
			_force_log(category, lvl, msg);
		}
		catch (const std::exception &ex)
		{
			err_handler(ex.what());
		}
		catch(...)
		{
			err_handler("Unknown exception in logger");
			throw;
		}
	}

	template<typename... Args>
	inline void log(category::value category, level::level_enum lvl, const char *fmt, const Args &... args) {
		if (should_log(lvl)) {
            force_log(category, lvl, fmt, args...);
		}
	}

	template<typename... Args>
	inline void trace(category::value category, const char *fmt, const Args &... args) {
		log(category, level::trace, fmt, args...);
	}

	template<typename... Args>
	inline void debug(category::value category, const char *fmt, const Args &... args) {
		log(category, level::debug, fmt, args...);
	}

	template<typename... Args>
	inline void info(category::value category, const char *fmt, const Args &... args) {
		log(category, level::info, fmt, args...);
	}

	template<typename... Args>
	inline void warn(category::value category, const char *fmt, const Args &... args) {
		log(category, level::warn, fmt, args...);
	}

	template<typename... Args>
	inline void error(category::value category, const char *fmt, const Args &... args) {
		log(category, level::err, fmt, args...);
	}

	template<typename... Args>
	inline void critical(category::value category, const char *fmt, const Args &... args) {
		log(category, level::critical, fmt, args...);
	}
}

namespace logger = tc_logger;
namespace category = logger::category;

#define tr(message) message

#define log_trace(_category, message, ...)   logger::trace(::logger::category::_category, message, ##__VA_ARGS__)
#define log_debug(_category, message, ...)   logger::debug(::logger::category::_category, message, ##__VA_ARGS__)
#define log_info(_category, message, ...)    logger::info(::logger::category::_category, message, ##__VA_ARGS__)
#define log_warn(_category, message, ...)    logger::warn(::logger::category::_category, message, ##__VA_ARGS__)
#define log_error(_category, message, ...)   logger::error(::logger::category::_category, message, ##__VA_ARGS__)
#define log_critical(_category, message, ...)   logger::critical(::logger::category::_category, message, ##__VA_ARGS__)

#define log_allocate(class, this) log_trace(memory, "Allocated new " class ": {}", (void*) this)
#define log_free(class, this) log_trace(memory, "Deallocated " class ": {}", (void*) this)