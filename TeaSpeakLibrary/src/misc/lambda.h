#pragma once

#include <tuple>
#include <type_traits>
#include <regex>

//https://qiita.com/angeart/items/94734d68999eca575881
namespace stx {
    namespace lambda_detail {
        template <typename...>
        struct member_type;

        template <typename ret, typename klass, typename... args>
        struct member_type<std::true_type, ret, klass, args...> {
            using member = std::true_type;
            using invoker_function = std::function<ret(klass*, args...)>;
        };
        template <typename ret, typename klass, typename... args>
        struct member_type<std::false_type, ret, klass, args...> {
            using member = std::false_type;
            using invoker_function = std::function<ret(args...)>;
        };

        template<typename t_member, class t_return_type, class t_klass, class flag_mutable, class... t_args>
        struct types : member_type<t_member, t_return_type, t_klass, t_args...> {
            public:
                static constexpr bool has_klass = std::is_class<t_klass>::value;
                static constexpr int argc = sizeof...(t_args);

                using flag_member = t_member;
                using klass = t_klass;
                using return_type = t_return_type;
                using is_mutable = flag_mutable;

                using args = std::tuple<t_args...>;

                template<size_t i>
                struct arg {
                    typedef typename std::tuple_element<i, std::tuple<t_args...>>::type type;
                };
        };

        template<class lambda>
        struct lambda_type_impl;

        template<class ret, class klass, class... args>
        struct lambda_type_impl<ret(klass::*)(args...) const> : lambda_detail::types<std::false_type, ret, klass, std::true_type, args...> {};
    }

    template<class lambda>
    struct lambda_type : lambda_detail::lambda_type_impl<decltype(&lambda::operator())> { };

    template<class ret, class klass, class... args>
    struct lambda_type<ret(klass::*)(args...)> : lambda_detail::types<typename std::is_member_function_pointer<ret(klass::*)(args...)>::type,ret,klass,std::true_type,args...> { };

    template<class ret, class klass, class... args>
    struct lambda_type<ret(klass::*)(args...) const> : lambda_detail::types<typename std::is_member_function_pointer<ret(klass::*)(args...) const>::type, ret, klass, std::false_type, args...> {  };

};