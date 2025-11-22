#include <deque>
#include <string>
#include <algorithm>
#include "bbcodes.h"

using namespace std;
using namespace bbcode;

bool bbcode::sloppy::has_tag(std::string message, std::deque<std::string> tag) {
    std::transform(message.begin(), message.end(), message.begin(), ::tolower);
    for(auto& entry : tag) {
        std::transform(entry.begin(), entry.end(), entry.begin(), ::tolower);
    }

    std::deque<std::string> begins;
    size_t index = 0, found, length = 0;
    do {
        found = string::npos;
        for(auto it = tag.begin(); it != tag.end() && found == string::npos; it++) {
            found = message.find(*it, index);
            length = it->length();
        }

        if(found > 0 && found + length < message.length()) {
            if(message[found + length] == ']' || (message[found + length] == '=' && message.find(']', found + length) != string::npos)) {
                if(message[found - 1] == '/') {
                    auto found_tag = message.substr(found, length);
                    for(const auto& entry : begins) {
                        if(entry == found_tag) {
                            return true;
                        }
                    }
                } else if(message[found - 1] == '[' && (found < 2 || message[found - 2] != '\\')) {
                    begins.push_back(message.substr(found, length));
                }
                if(message[found + length] !=  ']') {
                    found = message.find(']', found + length);
                }
            }
        }
        index = found + 1;
    } while(index != 0);

    return false;
}