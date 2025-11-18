#pragma once

#include <string>

#ifdef WIN32
	#define __no_return __declspec(noreturn)
#else
	#define __no_return __attribute__((noreturn))
#endif

extern __no_return void execute_callback_fail_exit(const std::string& /* error id */ = "", const std::string& /* error message */ = "");
extern __no_return void execute_callback_success_exit();

#ifdef WIN32
	extern bool is_administrator();
	extern bool request_administrator(int argc, char** argv);
#endif