#pragma once

#ifdef byte
    #define byte asdd
    #ifndef WIN32
        #warning The byte macro is already defined! Undefining it!
    #endif
    #undef byte
#endif

#include <stdexcept>
#include <string>
#include <map>
#include <list>
#include <deque>
#include <memory>
#include <pipes/buffer.h>
#include "../Variable.h"

#ifdef HAVE_JSON
    #include <json/json.h>
#endif

namespace ts {
#define PARM_TYPE(type, fromString, toString) \
    BaseCommandParm(std::string key, type value) : BaseCommandParm(std::pair<std::string, std::string>(key, "")) {\
        toString; \
    } \
BaseCommandParm& operator=(type value){ \
    toString; \
    return *this; \
} \
 \
operator type(){ \
    fromString; \
}

    class Command;
    class ValueList;


    //PARM_TYPE(ts::Property, return ts::Property(nullptr, key(), value(), 0), f_value() = value.value());

    class ParameterBulk {
            friend class Command;
            friend class ValueList;
        public:
            ParameterBulk(const ParameterBulk& ref) : parms(ref.parms) {}


            variable operator[](size_t index){
                if(parms.size() > index) return parms[index];
                return variable{"", "", VARTYPE_NULL};
            }

            const variable& operator[](const std::string& key) const {
                for(const auto& elm : parms)
                    if(elm.key() == key){
                        return elm;
                    }

                throw std::invalid_argument("could not find key [" + key + "]");
            }

            variable& operator[](const std::string& key) {
                for(auto& elm : parms)
                    if(elm.key() == key){
                        return elm;
                    }
                this->parms.push_back(variable(key, "", VariableType::VARTYPE_NULL));
                return this->operator[](key);
            }

            bool has(const std::string& key) const {
                for(const auto& elm : parms)
                    if(elm.key() == key && elm.type() != VariableType::VARTYPE_NULL) return true;
                return false;
            }

            std::deque<std::string> keys() const {
                std::deque<std::string> result;
                for(const auto& elm : parms)
                    result.push_back(elm.key());
                return result;
            }

            ParameterBulk& operator=(const ParameterBulk& ref){
                parms = ref.parms;
                return *this;
            }
        private:
            ParameterBulk() {}
            ParameterBulk(std::deque<variable> p) : parms(p) {}
            std::deque<variable> parms;
    };

    class ValueList {
            friend class Command;
        public:
            ValueList() = delete;
            ValueList(const ValueList& ref) : key(ref.key), bulkList(ref.bulkList) {}

            variable operator[](int index){
                while(index >= bulkList.size()) bulkList.push_back(ParameterBulk());
                return bulkList[index][key];
            }

            variable first() const {
                int index = 0;
                while(index < bulkList.size() && !bulkList[index].has(key)) index++;
                if(index < bulkList.size()) return bulkList[index][key];

                return variable{this->key, "", VariableType::VARTYPE_NULL};
                throw std::invalid_argument("could not find key [" + key + "]");
            }

            size_t size(){
                size_t count = 0;
                for(const auto& blk : this->bulkList)
                    if(blk.has(this->key)) count++;
                return count;
            }

            template <typename T>
            ValueList& operator=(T var){ operator[](0) = var; return *this; }
            ValueList& operator=(ts::ValueList& var){ operator[](0) = var.first().value(); return *this; }


            template <typename T>
            T as() const { return first().as<T>(); }

            template <typename T>
            operator T() { return as<T>(); }

            template <typename T>
            bool operator==(T other){ return as<T>() == other; }
            template <typename T>
            bool operator!=(T other){ return as<T>() != other; }

            std::string string() const { return as<std::string>(); }

            friend std::ostream& operator<<(std::ostream&, const ValueList&);
        private:
            ValueList(std::string key, std::deque<ParameterBulk>& bulkList) : key{std::move(key)}, bulkList(bulkList) {}
            std::string key;
        public:
            std::deque<ParameterBulk>& bulkList;
    };

    inline std::ostream& operator<<(std::ostream& stream,const ValueList& list) {
        stream << "{ Key: " << list.key << " = [";
        for(auto it = list.bulkList.begin(); it != list.bulkList.end(); it++)
            if(it->has(list.key)) {
                stream << (*it)[list.key].value();
                if(it + 1 != list.bulkList.end())
                    stream << ", ";
            }
        stream << "]}";
        return stream;
    }

    class Command {
        public:
            static Command parse(const pipes::buffer_view& buffer, bool expect_command = true, bool drop_non_utf8 = false);

            explicit Command(const std::string& command);
            explicit Command(const std::string& command, std::initializer_list<variable>);
            explicit Command(const std::string& command, std::initializer_list<std::initializer_list<variable>>);

            Command(const Command&);
            ~Command();

            inline std::string command() const { return getCommand(); }
            [[nodiscard]] std::string getCommand() const { return _command; }

            [[nodiscard]] std::string build(bool escaped = true) const;

#ifdef HAVE_JSON
            Json::Value buildJson() const;
#endif

            const ParameterBulk& operator[](size_t index) const {
                if(bulks.size() <= index) throw std::invalid_argument("got out of length");
                return bulks[index];
            }

            template <typename T, std::enable_if_t<std::is_integral<T>::value, int> = 0>
            ParameterBulk& operator[](T index){
                while(bulks.size() <= index) bulks.push_back(ParameterBulk{});
                return bulks[index];
            }

            ValueList operator[](const std::string& key){
                return ValueList(key, bulks);
            }

            ValueList operator[](const std::string_view& key){
                return ValueList(std::string{key}, bulks); //FIXME This should not be the normal case!
            }

            ValueList operator[](const char* key){
                return ValueList(std::string{key}, bulks); //FIXME This should not be the normal case!
            }

            size_t bulkCount() const { return bulks.size(); }
            void pop_bulk();
            void push_bulk_front();

            bool hasParm(std::string);
            void clear_parameters() { this->paramethers.clear(); }
            std::deque<std::string> parms();
            void enableParm(const std::string& key){ toggleParm(key, true); }
            void disableParm(const std::string& key){ toggleParm(key, false); }
            void toggleParm(const std::string& key, bool flag);

            void reverseBulks();
        private:
            Command();

            std::string _command;
            std::deque<ParameterBulk> bulks;
            std::deque<std::string> paramethers;
    };
}

DEFINE_VARIABLE_TRANSFORM_TO_STR(ts::ValueList, in.string());
DEFINE_VARIABLE_TRANSFORM_TYPE(ts::ValueList, VARTYPE_TEXT);

#undef PARM_TYPE