#pragma once

#include <string>
#include <memory>

namespace tc {
	namespace signal {
		struct CrashContext {
			std::string component_name;
			std::string crash_dump_folder;

			std::string success_command_line; /* %crash_path% for crash dumps */
			std::string error_command_line; /* %error_message% for the error message */
		};

		extern bool active();
		extern bool setup(std::unique_ptr<CrashContext>& /* crash context (will be moved) */);
		extern void finalize();
	}
}