#include "Command.h"
#include "command_exception.h"

#include <sstream>
#include <iostream>
#include <algorithm>
#include <pipes/buffer.h>
#include "escape.h"

using namespace std;

namespace ts {
    /*
    Command Command::parse(const pipes::buffer_view& _buffer, bool expect_command) { //FIXME improve!
        string buffer = _buffer.string();
        Command result;

        size_t nextSpace;
        if(expect_command) {
            nextSpace = buffer.find(' ');
            if(nextSpace == -1){
                result._command = buffer;
                return result;
            }

            result._command = buffer.substr(0, nextSpace);
            buffer = buffer.substr(nextSpace + 1);
        }

        int splitIndex = 0;
        do {
            auto nextSplit = buffer.find('|');
            auto splitBuffer = buffer.substr(0, nextSplit);
            if(nextSplit != std::string::npos)
                buffer = buffer.substr(nextSplit + 1);
            else
                buffer = "";
            if(splitBuffer.empty()) continue;


            std::string element;
            ssize_t elementEnd;
            do {
                elementEnd = splitBuffer.find(' ');
                element = splitBuffer.substr(0, elementEnd);
                if(element.empty()) goto nextElement;

                if(element[0] == '-'){
                    result.paramethers.push_back(element.substr(1));
                } else {
                    auto key = element.substr(0, element.find('='));
                    key.erase(std::remove_if(key.begin(), key.end(), [](const char c){ return !((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-'); }), key.end());
                    if(key.length() > 0 && (key.front() == ' ' || key.back() == ' ')) {
                        auto first = key.find_first_not_of(' ');
                        if(first == std::string::npos) continue;
                        auto last = key.find_last_not_of(' ');
                        key = key.substr(first, last - first);
                    }
                    std::string value;
                    if(element.find('=') != std::string::npos)
                        value = element.substr(element.find('=') + 1);
                    if(key.empty() && value.empty()) goto nextElement;
                    if(result[splitIndex].has(key)) goto nextElement; //No double insert. Sometimes malformed input 'banadd ip name=WolverinDEV time=3600 banreason=Test\sban! return_code=1:38 return_code=__1:38_1:38'
                    result[splitIndex][key] = query::unescape(value, true);
                }

                nextElement:
                if(elementEnd != std::string::npos && elementEnd + 1 < splitBuffer.size())
                    splitBuffer = splitBuffer.substr(elementEnd + 1);
                else
                    splitBuffer = "";
            } while(!splitBuffer.empty());
            splitIndex++;
        } while(!buffer.empty());


        return result;
    }
    */

    Command Command::parse(const pipes::buffer_view &buffer, bool expect_type, bool drop_non_utf8) {
        string_view data{buffer.data_ptr<const char>(), buffer.length()};

        Command result;

        size_t current_index = std::string::npos, end_index;
        if(expect_type) {
            current_index = data.find(' ', 0);
            if(current_index == std::string::npos){
                result._command = std::string(data);
                return result;
            } else {
                result._command = std::string(data.substr(0, current_index));
            }
        }

        size_t bulk_index = 0;
        while(++current_index > 0 || (current_index == 0 && !expect_type && (expect_type = true))) {
            end_index = data.find_first_of(" |", current_index);

            if(end_index != current_index && current_index < data.length()) { /* else we've found another space or a pipe */
                if(data[current_index] == '-') {
                    string trigger(data.substr(current_index + 1, end_index - current_index - 1));
                    result.paramethers.push_back(trigger);
                } else {
                    auto index_assign = data.find_first_of('=', current_index);
                    string key, value;
                    if(index_assign == string::npos || index_assign > end_index) {
                        key = data.substr(current_index, end_index - current_index);
                    } else {
                        key = data.substr(current_index, index_assign - current_index);
                        try {
                            value = query::unescape(string(data.substr(index_assign + 1, end_index - index_assign - 1)), true);
                        } catch(const std::invalid_argument& ex) {
                            (void) ex;

                            /* invalid character at index X */
                            if(!drop_non_utf8)
                                throw;
                            goto skip_assign;
                        }
                    }

                    {
                        const static auto key_validator =  [](char c){ return !((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-'); };
                        auto invalid_index = find_if(key.begin(), key.end(), key_validator);
                        if(invalid_index != key.end())
                            throw command_malformed_exception(current_index + distance(key.begin(), invalid_index));

                        if(!key.empty() && !result[bulk_index].has(key))  //No double insert. Sometimes malformed input 'banadd ip name=WolverinDEV time=3600 banreason=Test\sban! return_code=1:38 return_code=__1:38_1:38'
                            result[bulk_index][key] = value;
                    }
                    skip_assign:;
                }
            }

            if(end_index < data.length() && data[end_index] == '|')
                bulk_index++;
            current_index = end_index;
        }

        return result;
    }

    Command::Command(const std::string& command) {
        this->_command = command;
    }

    Command::Command(const std::string& command, std::initializer_list<variable> parms) : Command(command, {parms}) { }


    Command::Command(const std::string& command, std::initializer_list<std::initializer_list<variable>> bulks) : Command(command) {
        int blkIndex = 0;
        for(auto blk = bulks.begin(); blk != bulks.end(); blk++, blkIndex++)
            for(auto it = blk->begin(); it != blk->end(); it++)
                operator[](blkIndex).parms.push_back(*it);
    }

    Command::Command(const Command& other) {
        this->_command = other._command;
        this->bulks = other.bulks;
        this->paramethers = other.paramethers;
    }

    Command::Command() = default;

    Command::~Command() = default;

    std::string Command::build(bool escaped) const {
        std::stringstream out;
        out << this->_command;

        bool bulkBegin = false;
        for(auto it = this->bulks.begin(); it != this->bulks.end(); it++){
            for(const auto& elm : it->parms) {
                if(!bulkBegin) out << " ";
                else bulkBegin = false;
                if(elm.key().empty()) {
                    out << elm.value(); /* special case used for permission list */
                } else {
                    out << elm.key();
                    if(!elm.value().empty()){
                        out << "=" << (escaped ? query::escape(elm.value()) : ("'" + elm.value() + "'"));
                    }
                }
            }
            if(it + 1 != this->bulks.end()){
                out << "|";
                bulkBegin = true;
            }
        }
        for(const auto& parm : this->paramethers) out << " -" << parm;

        auto str = out.str();
        if(str.length() > 1) if(str[0] == ' ') str = str.substr(1);
        return str;
    }

#ifdef HAVE_JSON
    Json::Value Command::buildJson() const {
        Json::Value result;
        result["command"] = this->_command;

        int index = 0;
        for(auto it = this->bulks.begin(); it != this->bulks.end(); it++){
            Json::Value& node = result["data"][index++];
            for(const auto& elm : it->parms)
                node[elm.key()] = elm.value();
        }

        Json::Value& triggers = result["triggers"];
        index = 0;
        for(const auto& parm : this->paramethers)
            triggers[index++] = parm;

        return result;
    }
#endif

    std::deque<std::string> Command::parms() { return this->paramethers; }
    bool Command::hasParm(std::string parm) { return std::find(this->paramethers.begin(), this->paramethers.end(), parm) != this->paramethers.end(); }

    void Command::toggleParm(const std::string& key, bool flag) {
        if(flag){
            if(!hasParm(key)) this->paramethers.push_back(key);
        } else {
            auto found = std::find(this->paramethers.begin(), this->paramethers.end(), key);
            if(found != this->paramethers.end())
                this->paramethers.erase(found);
        };
    }

    void Command::reverseBulks() {
        std::reverse(this->bulks.begin(), this->bulks.end());
    }

    void Command::pop_bulk() {
        if(!this->bulks.empty())
            this->bulks.pop_front();
    }

    void Command::push_bulk_front() {
        this->bulks.push_front(ParameterBulk{});
    }
}
