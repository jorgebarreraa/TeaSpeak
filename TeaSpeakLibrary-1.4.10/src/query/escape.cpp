#include "escape.h"
#include <stdexcept>

using namespace ts;
using namespace std;

std::string query::escape(std::string in) {
    size_t index = 0;
    while(index < in.length()) {
        if(in[index] == '\\')
            in.replace(index, 1, "\\\\", 2);
        else if(in[index] == ' ')
            in.replace(index, 1, "\\s", 2);
        else if(in[index] == '/')
            in.replace(index, 1, "\\/", 2);
        else if(in[index] == '|')
            in.replace(index, 1, "\\p", 2);
        else if(in[index] == '\b')
            in.replace(index, 1, "\\b", 2);
        else if(in[index] == '\f')
            in.replace(index, 1, "\\f", 2);
        else if(in[index] == '\n')
            in.replace(index, 1, "\\n", 2);
        else if(in[index] == '\r')
            in.replace(index, 1, "\\r", 2);
        else if(in[index] == '\t')
            in.replace(index, 1, "\\t", 2);
        else if(in[index] == '\x07')
            in.replace(index, 1, "\\a", 2);
        else if(in[index] == '\x0B')
            in.replace(index, 1, "\\v", 2);
        else {
            index += 1;
            continue;
        }
        index += 2;
    }

    return in;
}

/*
if (input[i] = $ED) and   // Prüfe auf High and Low Surrogate
(input[i+3] = $ED) and
(input[i+1] or $F = $AF) and
(input[i+4] or $F = $BF) then
begin
output := output +
        AnsiChar((input[i+1] and $F + 1) shr 2 and 7 or $F0) + // and 7 kann man auch weglassen
        AnsiChar((input[i+2] shr 2 and $F + (input[i+1] and $F + 1) shl 4 and $30) or $80) +
        AnsiChar((input[i+4] and $F + input[i+2] shl 4 and $30) or $80) +
        AnsiChar(input[i+5]);
end
else
output := output + AnsiChar(input);
 */
std::string query::unescape(std::string in, bool throw_error) {
    size_t index = 0;
    while(index < in.length()){
        if(in[index] == '\\'){
            if(in.length() <= index + 1) break;
            char replace = 0;

            switch (in[index + 1]){
                case 's': replace = ' '; break;
                case '/': replace = '/'; break;
                case 'p': replace = '|'; break;
                case 'b': replace = '\b'; break;
                case 'f': replace = '\f'; break;
                case 'n': replace = '\n'; break;
                case 'r': replace = '\r'; break;
                case 't': replace = '\t'; break;
                case 'a': replace = '\x07'; break;
                case 'v': replace = '\x0B'; break;
                case '\\': replace = '\\'; break;
                default:
                    replace = '\x00'; break;
            }
            if(replace != 0x00)
                in.replace(index, 2, string(&replace, 1));
        }

        uint8_t current = (uint8_t) in[index];
        if(in.length() - index >= 6) { //Check for CESU-8
            if((current == 0xED) && ((uint8_t) in[index + 3] == 0xED) && ((((uint8_t) in[index + 1] | 0xF) == 0xAF)) && ((((uint8_t) in[index + 4] | 0xF) == 0xBF))){
                char replaced[4];

                replaced[0] = (((((uint8_t) in[index + 1] & 0xF) + 1) >> 2) & 7) | 0xF0;
                replaced[1] = ((((uint8_t) in[index + 2] >> 2) & 0xF) + (((((uint8_t) in[index + 1] & 0xF)) + (1 << 4)) | 0x80));
                replaced[2] = (((uint8_t) in[index + 4] & 0xF) + (((uint8_t) in[index + 2] << 4) & 0x30)) | 0x80;
                replaced[3] = ((uint8_t) in[index + 5]);

                in.replace(index, 6, string(replaced, 4));
                index -= 2; //Substract the replace difference

                index += 5; //Skip 5 bytes (CESU-8 length for emoty)
            }
        }
        //"\221\210\200\200\200\210\221\221\221\221\204\210\221\221\221\221\221\221\221\221\221\204\210\221\221\221\221\221\204\210\221\221"
        if(current >= 128) { //UTF8 check
            if(current >= 192 && (current <= 193 || current >= 245)) {
                in.replace(index, 1, "", 0); //Cut the character out
                index--;
            } else if(current >= 194 && current <= 223) {
                if(in.length() - index <= 1) {
                    in.replace(index, in.length() - index, "", 0);
                } else if((uint8_t) in[index + 1] >= 128 && (uint8_t) in[index + 1] <= 191) {
                    index += 1; //Valid
                } else {
                    if(throw_error) {
                        throw invalid_argument("Invalid UTF-8 character at index " + to_string(index));
                    }

                    in.replace(index, 2, "", 0); //Cut the two characters out
                    index--;
                }
            } else if(current >= 224 && current <= 239) {
                if(in.length() - index <= 2) {
                    in.replace(index, in.length() - index, "", 0);
                } else if((uint8_t) in[index + 1] >= 128 && (uint8_t) in[index + 1] <= 191 &&
                        (uint8_t) in[index + 2] >= 128 && (uint8_t) in[index + 2] <= 191) {
                    index += 2; //Valid
                } else {
                    if(throw_error) {
                        throw invalid_argument("Invalid UTF-8 character at index " + to_string(index));
                    }

                    in.replace(index, 3, "", 0); //Cut the three characters out
                    index--;
                }
            } else if(current >= 240 && current <= 244) {
                if(in.length() - index <= 3) {
                    in.replace(index, in.length() - index, "", 0);
                } else if((uint8_t) in[index + 1] >= 128 && (uint8_t) in[index + 1] <= 191 &&
                        (uint8_t) in[index + 2] >= 128 && (uint8_t) in[index + 2] <= 191 &&
                        (uint8_t) in[index + 3] >= 128 && (uint8_t) in[index + 3] <= 191) {
                    index += 3; //Valid
                } else {
                    if(throw_error) {
                        throw invalid_argument("Invalid UTF-8 character at index " + to_string(index));
                    }

                    in.replace(index, 4, "", 0); //Cut the three characters out
                    index--;
                }
            } else {
                if(throw_error) {
                    throw invalid_argument("Invalid UTF-8 character at index " + to_string(index));
                }

                in.replace(index, 1, "", 0); //Cut the character out
                index--;
            }
        } else if(current < 0x0A) {
            in.replace(index, 1, " ", 1);
        }
        index++;
    }
    return in;
}