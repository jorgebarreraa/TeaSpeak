#pragma once

#include <string>
#include <memory>
#include <iostream>
#include <utility>

enum VariableType {
    VARTYPE_NULL,
    VARTYPE_TEXT,
    VARTYPE_INT,
    VARTYPE_LONG,
    VARTYPE_DOUBLE,
    VARTYPE_FLOAT,
    VARTYPE_BOOLEAN
};

class variable;

namespace typecast {
    template<typename T>
    struct FalseType : std::false_type {};

    template <typename T>
    inline T variable_cast(const variable& in) { static_assert( FalseType<T>::value , "this function has to be implemented for desired type"); return nullptr; }; //str to type

    template <typename T>
    inline std::string variable_cast(T& in) { static_assert( FalseType<T>::value , "this function has to be implemented for desired type"); return "undefined"; }; //type to str

    template <typename T>
    inline VariableType variable_type() {  static_assert( FalseType<T>::value , "this function has to be implemented for desired type"); return VARTYPE_NULL; }; //type to str
}

struct variable_data {
    variable_data(const std::pair<std::string, std::string> &pair, VariableType _type);
    ~variable_data() = default;

    std::pair<std::string, std::string> pair;
    VariableType _type;
};

class variable {
    public:
        variable() noexcept : data(std::make_shared<variable_data>(std::pair<std::string, std::string>{"", ""}, VariableType::VARTYPE_NULL)) {}
        variable(const std::string& key) noexcept : variable() { r_key() = key; }
        variable(const std::string& key, const variable& value) noexcept : variable(key) {
            this->r_value() = value.string();
            this->data->pair.second = value.string();
        }
        variable(const std::string& key, const std::string& value, VariableType type) noexcept : data(std::make_shared<variable_data>(std::pair<std::string, std::string>{key, value}, VariableType::VARTYPE_TEXT)) {}
        variable(const variable& ref) : data(ref.data) {}
        variable(variable&& ref) : data(ref.data) { }
        virtual ~variable()  = default;

        template <typename T>
        variable(const std::string& key, T value) : variable(key) { operator=(value); }

        variable& operator=(const variable& ref);
        variable& operator=(variable&& ref);

        [[nodiscard]] std::string key() const { return this->r_key(); }
        void set_key(const std::string_view& key) const { this->r_key() = key; }

        std::string value() const { return this->r_value(); }
        VariableType type() const { return data->_type; }
        variable clone(){ return variable(key(), value(), type()); }

        variable(std::string key, std::nullptr_t) : variable() { r_key() = std::move(key); data->_type = VariableType ::VARTYPE_NULL; }
        explicit variable(std::nullptr_t) noexcept : variable() { data->_type = VariableType ::VARTYPE_NULL; }
        variable&operator=(std::nullptr_t) { r_value() = ""; data->_type = VARTYPE_NULL; return *this;}

        template <typename T>
        T as() const {
            try {
                return typecast::variable_cast<T>(*this);
            } catch(std::exception& ex) {
                throw std::invalid_argument{"failed to parse " + this->r_key() + " as " + typeid(T).name() + " (" + ex.what() + ")"};
            }
        }
        std::string string() const { return as<std::string>(); } //fast

        template <typename T> //TODO more secure and not just try and fail
        bool castable() {
            try {
                as<T>();
                return true;
            } catch (...) { return false; }
        }


        template <typename T>
        variable& operator=(T obj){
            r_value() = typecast::variable_cast<T>(obj);
            data->_type = typecast::variable_type<T>();
            return *this;
        }

        template <typename T>
        operator T() const { return as<T>(); }
    private:
        std::string& r_key() const { return data->pair.first; }
        std::string& r_value() const { return data->pair.second; }

        std::shared_ptr<variable_data> data;
};

#define DEFINE_VARIABLE_TRANSFORM_TO_STR(type, to_str)                              \
namespace typecast {                                                                \
    template <>                                                                     \
    inline std::string variable_cast<type>(type& in) { return to_str; }             \
}

#define DEFINE_VARIABLE_TRANSFORM_TO_VAR(type, from_str)                            \
namespace typecast {                                                                \
    template <>                                                                     \
    inline type variable_cast<type>(const variable& in) { return from_str; }        \
}

#define DEFINE_VARIABLE_TRANSFORM_TYPE(type, vartype)                               \
namespace typecast {                                                                \
    template <>                                                                     \
    inline VariableType variable_type<type>() { return VariableType::vartype; }     \
}

#define DEFINE_VARIABLE_TRANSFORM(type, ntype, to_str, from_str)                    \
DEFINE_VARIABLE_TRANSFORM_TO_STR(type, to_str);                                     \
DEFINE_VARIABLE_TRANSFORM_TO_VAR(type, from_str)                                    \
DEFINE_VARIABLE_TRANSFORM_TYPE(type, ntype)

#define DEFINE_VARIABLE_TRANSFORM_ENUM(class, size_type)                            \
DEFINE_VARIABLE_TRANSFORM(class, VARTYPE_INT, std::to_string((size_type) in), static_cast<class>(in.as<size_type>()));

DEFINE_VARIABLE_TRANSFORM(std::string, VARTYPE_TEXT, in, in.value());
DEFINE_VARIABLE_TRANSFORM(std::string_view, VARTYPE_TEXT, std::string{in}, std::string_view{in.value()});
DEFINE_VARIABLE_TRANSFORM(char*, VARTYPE_TEXT, std::string((const char*) in), (char*) in.value().c_str());
DEFINE_VARIABLE_TRANSFORM(const char*, VARTYPE_TEXT, std::string((const char*) in), in.value().c_str());

DEFINE_VARIABLE_TRANSFORM(int8_t, VARTYPE_INT, std::to_string(in), (int8_t) std::stoi(in.value()));
DEFINE_VARIABLE_TRANSFORM(uint8_t, VARTYPE_INT, std::to_string(in), (uint8_t) std::stoul(in.value()));

DEFINE_VARIABLE_TRANSFORM(int16_t, VARTYPE_INT, std::to_string(in), (int16_t) std::stoi(in.value()));
DEFINE_VARIABLE_TRANSFORM(uint16_t, VARTYPE_INT, std::to_string(in), (uint16_t) std::stoul(in.value()));

DEFINE_VARIABLE_TRANSFORM(int32_t, VARTYPE_INT, std::to_string(in), std::stoi(in.value()));
DEFINE_VARIABLE_TRANSFORM(uint32_t, VARTYPE_INT, std::to_string(in), (uint32_t) std::stoul(in.value()));

DEFINE_VARIABLE_TRANSFORM(int64_t, VARTYPE_LONG, std::to_string(in), std::stoll(in.value()));
DEFINE_VARIABLE_TRANSFORM(uint64_t, VARTYPE_LONG, std::to_string(in), std::stoull(in.value()));

DEFINE_VARIABLE_TRANSFORM(bool, VARTYPE_INT, in ? "1" : "0", in.value() == "1");
DEFINE_VARIABLE_TRANSFORM(double, VARTYPE_DOUBLE, std::to_string(in), std::stod(in.value()));
DEFINE_VARIABLE_TRANSFORM(float, VARTYPE_FLOAT, std::to_string(in), std::stof(in.value()));