#pragma once

#include <random>
#include <cstring>
#include <chrono>

extern const char* rnd_string_chars; //"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
extern std::string rnd_string(int length = 20, const char* avariable = rnd_string_chars);