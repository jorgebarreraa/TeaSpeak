#include "./base64.h"
#include <tomcrypt.h>


std::string base64::decode(const char* input, size_t size) {
    auto out = new unsigned char[size];
    if(base64_strict_decode((unsigned char*) input, (unsigned long) size, out, (unsigned long*) &size) != CRYPT_OK){
        //std::cerr << "Invalid base 64 string '" << input << "'" << std::endl;
        return "";
    }
    std::string ret((char*) out, size);
    delete[] out;
    return ret;
}

std::string base64::encode(const char* input, size_t inputSize) {
    auto outlen = static_cast<unsigned long>(inputSize + (inputSize / 3.0) + 16);
    auto outbuf = new unsigned char[outlen]; //Reserve output memory
    if(base64_encode((unsigned char*) input, inputSize, outbuf, &outlen) != CRYPT_OK){
        //std::cerr << "Invalid input '" << input << "'" << std::endl;
        return "";
    }
    std::string ret((char*) outbuf, outlen);
    delete[] outbuf;
    return ret;
}