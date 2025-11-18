#include "rnd.h"

const char* rnd_string_chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
std::string rnd_string(int length, const char* source) {
    char* buffer = new char[length];
    auto source_length = strlen(source);

    std::default_random_engine generator{0};
    generator.seed(static_cast<unsigned long>(std::chrono::system_clock::now().time_since_epoch().count()));
    std::uniform_int_distribution<int> gen(0, static_cast<int>(source_length - 1));
    for(int i = 0; i < length; i++){
        buffer[i] = source[gen(generator)];
    }

    auto result = std::string(buffer, static_cast<unsigned long>(length));
    delete[] buffer;
    return result;
}