#pragma once

#include <any>
#include <memory>
#include <vector>
#include <map>
#include <cassert>
#include <string_view>
#include <deque>
#include <tuple>

#include "Error.h"

#include "command_exception.h"
#include "converters/converter.h"
#include "command3.h"

#ifdef WIN32
#define __attribute__used
#else
#define __attribute__used __attribute__((used))
#endif

namespace ts {
    namespace command_handler {
        namespace tliterals {
            template <char... chars>
            using tstring = std::integer_sequence<char, chars...>;

#ifndef WIN32
            template <typename T, T... chars>
            constexpr tstring<chars...> operator""_tstr() { return { }; }
#endif
            template <typename>
            struct tliteral;

            template <char... elements>
            struct tliteral<tstring<elements...>> {
                static constexpr char string[sizeof...(elements) + 1] = { elements..., '\0' };
            };
        }

        /* general description of a function */
        struct parameter_description {
            int type; /* 1 = field | 2 = switch | 3 = command handle */
        };

        struct parameter_description_field : public parameter_description {
            const char* key;
            bool optional;
            const std::type_info& field_type;
        };

        struct parameter_description_switch : public parameter_description {
            const char* key;
        };

        struct parameter_description_command : public parameter_description {  };


        struct invocable_function {
            void operator()(command_parser& command) { this->invoke(command); }
            virtual void invoke(command_parser& command) = 0;
        };

        namespace impl {
            namespace templates {
                template <bool...>
                struct _or_ {
                    constexpr static bool value = false;
                };

                template <bool T, bool... Args>
                struct _or_<T, Args...> {
                    constexpr static bool value = T || _or_<Args...>::value;
                };

                template <typename... >
                struct index;

                template <typename... >
                struct tuple_index;

                template <typename T, typename... R>
                struct index<T, T, R...> : std::integral_constant<size_t, 0>
                { };

                template <typename T, typename F, typename... R>
                struct index<T, F, R...> : std::integral_constant<size_t, 1 + index<T, R...>::value>
                { };

                template <typename T, typename... R>
                struct tuple_index<T, std::tuple<R...>> : std::integral_constant<size_t, index<T, R...>::value>
                { };

                template <typename T>
                struct remove_cr {
                    typedef T type;
                };

                template <typename T>
                struct remove_cr<const T&> {
                    typedef T type;
                };
            }

            template <typename...>
            struct command_invoker {
                constexpr static bool supported = false;
            };

            struct function_parameter {
                virtual ~function_parameter() = default;

                virtual void parse(const command_parser& /* parser */) = 0;
            };

            template <typename key_t, typename value_t, bool optional, bool bulked>
            class value_container;

            /* single value implementation */
            template <typename value_t>
            class single_value_base {
                public:
                    [[nodiscard]] inline const value_t& value() const {
                        if(!this->_value.has_value()) throw command_value_missing_exception{0, "unknown"}; /* this should never happen! */
                        assert(this->_value.has_value());

                        return this->_value.value();
                    }

                    inline operator value_t() const {
                        return this->value();
                    }

                protected:
                    static constexpr bool bulked_value = false;
                    std::optional<value_t> _value{};
            };

            template <typename key_t, typename value_t>
            class value_container<key_t, value_t, false, false> : public single_value_base<value_t> {
                protected:
                    void _parse(const command_parser& parser) {
                        this->_value = std::make_optional(parser.value_as<value_t>(key_t::string)); /* will cause a not found exception if missing */
                    }
            };

            template <typename key_t, typename value_t>
            class value_container<key_t, value_t, true, false> : public single_value_base<value_t> {
                public:
                    [[nodiscard]] inline bool has_value() const { return this->_value.has_value(); }

                    [[nodiscard]] inline const value_t& value_or(value_t&& fallback) const {
                        if(this->_value.has_value())
                            return this->value();
                        return fallback;
                    }

                protected:
                    void _parse(const command_parser& parser) {
                        bool found{};
                        (void) parser.value_raw(key_t::string, found);
                        if(found)
                            this->_value = parser.value_as<value_t>(key_t::string);
                    }
            };

            /* optional bulk implementation */
            template <typename key_t, typename value_t>
            class value_container<key_t, value_t, true, true> {
                public:
                    [[nodiscard]] inline bool has_value(size_t index) const { return this->_values[index].has_value(); }

                    [[nodiscard]] inline const value_t& value(size_t index) const {
                        if(!this->_values[index].has_value()) throw command_value_missing_exception{0, "unknown"}; /* this should never happen! */
                        assert(this->_values[index].has_value());

                        return this->_values[index].value();
                    }


                    [[nodiscard]] inline const value_t& value_or(size_t index, value_t&& fallback) const {
                        if(this->has_value(index))
                            return this->value(index);
                        return fallback;
                    }
                protected:
                    static constexpr bool bulked_value = true;

                    void _parse(const command_parser& parser) {
                        this->_values.reserve(parser.bulk_count());
                        for(const auto& bulk : parser.bulks()) {
                            auto& value = this->_values.emplace_back();

                            bool found{};
                            (void) bulk.value_raw(key_t::string, found);
                            if(found)
                                value = bulk.value_as<value_t>(key_t::string);
                        }
                    }

                private:
                    std::vector<std::optional<value_t>> _values{};
            };

            /* non optional bulk implementation */
            template <typename key_t, typename value_t>
            class value_container<key_t, value_t, false, true> {
                public:
                    [[nodiscard]] inline const value_t& value(size_t index) const {
                        return this->_values[index];
                    }

                protected:
                    static constexpr bool bulked_value = true;

                    void _parse(const command_parser& parser) {
                        this->_values.reserve(parser.bulk_count());
                        for(const auto& bulk : parser.bulks())
                            this->_values.push_back(bulk.value_as<value_t>(key_t::string)); /* will cause a not found exception if missing */
                    }

                private:
                    std::vector<value_t> _values{};
            };

            template <class key_t, typename value_type_t, class is_optional_t = std::false_type, class bulk_extend = value_container<key_t, value_type_t, is_optional_t::value, false>>
            struct field : public function_parameter, public bulk_extend {
                    friend struct command_invoker<field<key_t, value_type_t, is_optional_t, bulk_extend>>;
                    static_assert(converter<value_type_t>::supported, "Target type isn't supported!");
                    static_assert(!converter<value_type_t>::supported || converter<value_type_t>::from_string_view, "Target type dosn't support parsing");

                public:
                    template <bool flag = true>
                    using as_optional = field<key_t, value_type_t, std::integral_constant<bool, flag>, value_container<key_t, value_type_t, flag, bulk_extend::bulked_value>>;

                    template <bool flag>
                    using as_bulked = field<key_t, value_type_t, is_optional_t, value_container<key_t, value_type_t, is_optional_t::value, flag>>;

                    using optional = as_optional<true>;
                    using bulked = as_bulked<true>;

                    inline static const parameter_description* describe() {
                        static std::unique_ptr<parameter_description> description;
                        if(!description) {
                            description = std::make_unique<parameter_description>(parameter_description_field{
                                1,
                                key_t::string,
                                is_optional_t::value,
                                typeid(value_type_t)
                            });
                        }
                        return &*description;
                    }


                    [[nodiscard]] inline constexpr bool is_optional() { return is_optional_t::value; }
                    [[nodiscard]] inline constexpr bool is_bulked() { return bulk_extend::bulked_value; }


                    ~field() override = default;
                    void parse(const command_parser& parser) override { this->_parse(parser); }
            };

            template <class key_t>
            struct trigger : public function_parameter {
                public:
                    inline static const parameter_description* describe() {
                        static std::unique_ptr<parameter_description_switch> description;
                        if(!description) {
                            description = std::make_unique<parameter_description_switch>(parameter_description_switch{
                                2,
                                key_t::string
                            });
                        }
                        return &*description;
                    }

                    inline bool is_set() const { return this->flag_set; }
                    operator bool() const { return this->flag_set; }


                    void parse(const command_parser& parser) override { this->flag_set = parser.has_switch(key_t::string); }
                private:
                    bool flag_set = false;
            };

            template <typename key_t, typename value_t, class optional_extend, class bulk_extend>
            struct command_invoker<field<key_t, value_t, optional_extend, bulk_extend>> {
                constexpr static bool supported = true;

                typedef field<key_t, value_t, optional_extend, bulk_extend> field_t;

                inline static  const parameter_description* describe() {
                    return field_t::describe();
                }

                inline static field_t apply(command_parser& cmd) {
                    field_t result{};
                    result.parse(cmd);
                    return result;
                }
            };

            template <typename key_t>
            struct command_invoker<trigger<key_t>> {
                constexpr static bool supported = true;

                inline static const parameter_description* describe() {
                    return trigger<key_t>::describe();
                }

                inline static trigger<key_t> apply(command_parser& cmd) {
                    trigger<key_t> result{};
                    result.parse(cmd);
                    return result;
                }
            };

            template <>
            struct command_invoker<command_parser&> {
                constexpr static bool supported = true;

                inline static const parameter_description* describe() {
                    static std::unique_ptr<parameter_description> description;
                    if(!description)
                        description = std::make_unique<parameter_description>(parameter_description_command{3});
                    return &*description;
                }

                inline static command_parser& apply(command_parser& cmd) {
                    return cmd;
                }
            };

            template <typename C>
            struct command_invoker<C> {
                constexpr static bool supported = false;

                using descriptor_t = std::shared_ptr<nullptr_t>;

                inline static descriptor_t describe() {
                    return nullptr;
                }

                inline static command_parser& apply(descriptor_t& descriptor, command_parser& cmd) {
                    return cmd;
                }
            };

            template <typename... args_t>
            struct typed_invocable_function : public invocable_function {
                template <typename arg_t>
                using command_invoker_t = command_invoker<typename impl::templates::remove_cr<arg_t>::type>;

                void(*function)(args_t...);
                void invoke(command_parser& command) override {
                    this->function(command_invoker_t<args_t>::apply(command)...);
                }
            };
        }

        template <class key_t, typename value_type_t>
        using field = impl::field<key_t, value_type_t>;

        template <class key_t>
        using trigger = impl::trigger<key_t>;

        template <typename... args_t>
        inline std::array<const parameter_description*, sizeof...(args_t)> describe_function(void(*)(args_t...)) {
            static_assert(!impl::templates::_or_<(!impl::command_invoker<typename impl::templates::remove_cr<args_t>::type>::supported)...>::value, "Not any function argument type is supported");
            return {impl::command_invoker<typename impl::templates::remove_cr<args_t>::type>::describe()...};
        }

        template <typename... args_t, typename typed_function = impl::typed_invocable_function<args_t...>>
        std::shared_ptr<invocable_function> parse_function(void(*function)(args_t...)) {
            auto result = std::make_shared<typed_function>();
            result->function = function;
            return result;
        }

        /* converts a literal into a template literal */
#define _tlit(literal) ::ts::command_handler::tliterals::tliteral<decltype(literal ##_tstr)>
#define tl(lit) _tlit(lit)
    }

    //using desc = descriptor::base<descriptor::impl::default_options>;
}

#include "command_internal.h"