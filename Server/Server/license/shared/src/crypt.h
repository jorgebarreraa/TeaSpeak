#pragma once

inline void xorBuffer(char *buffer, size_t bufferLength, const char *xOr, size_t xorLength){
	for(int index = 0; index < bufferLength; index++)
		buffer[index] ^= xOr[index % xorLength];
}