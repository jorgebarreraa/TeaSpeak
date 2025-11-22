#pragma once

#include <string>
#include <sstream>
#include <cstring>
#include <sqlite3.h>
#include <functional>
#include <utility>
#include <ThreadPool/ThreadPool.h>
#include <ThreadPool/Future.h>
#include <misc/memtracker.h>
#include <misc/lambda.h>

#include "../Variable.h"

#define ALLOW_STACK_ALLOCATION
#define LOG_SQL_CMD [](const sql::result &res){ if(!res) logCritical(LOG_GENERAL, "Failed to execute sql command: " + std::to_string(res.code()) + "/" + res.msg() + " (" __FILE__ + ":" + std::to_string(__LINE__) + ")"); }
namespace sql {
    class result;
    class SqlManager;
    class AsyncSqlPool;
    class command;
    class model;
    namespace impl {
        template <typename SelfType> class command_base;
    }

    inline std::ostream& operator<<(std::ostream& s,const result& res);
    inline std::ostream& operator<<(std::ostream& s,const result* res);

    using QueryCallback = std::function<int(int, std::string*, std::string*)>;

    class result {
        public:
            static result success;

            result() : result{success} { }
            result(int code, std::string msg)
                    : code_{code}, msg_{std::move(msg)}, sql_{""}, last_insert_rowid_{-1} { }
            result(std::string query, int code, int64_t last_insert_rowid, std::string msg)
                : code_{code}, msg_{std::move(msg)}, sql_{std::move(query)}, last_insert_rowid_{last_insert_rowid} { }

            result(const result&)            = default;
            result(result&&)                 = default;
            result& operator=(const result&) = default;
            result& operator=(result&&)      = default;

            int code() const { return this->code_; }
            std::string msg() const { return this->msg_; }
            std::string sql() const { return this->sql_; }
            int64_t last_insert_rowid() const { return this->last_insert_rowid_; };

            //Returns true on success
            operator bool() const { return code_ == 0; }

            std::string fmtStr() const {
                std::stringstream s;
                operator<<(s, *this);
                return s.str();
            }
        private:
            int code_{0};
            int64_t last_insert_rowid_{0};
            std::string msg_{};
            std::string sql_{};
    };

    enum SqlType {
        TYPE_SQLITE,
        TYPE_MYSQL
    };

    class CommandData {
        public:
            CommandData() = default;
            ~CommandData() = default;

            SqlManager* handle = nullptr;

            template <typename H = SqlManager>
            inline H* sqlHandle() { return dynamic_cast<H*>(handle); }

            std::string sql_command; //variable :<varname>
            std::vector<variable> variables{};
            threads::Mutex lock;
    };

    class SqlManager {
            template <typename SelfType> friend class impl::command_base;
            friend class command;
            friend class model;
        public:
            explicit SqlManager(SqlType);
            virtual ~SqlManager();
            virtual result connect(const std::string&) = 0;
            virtual bool connected() = 0;
            virtual result disconnect() = 0;

            AsyncSqlPool* pool;

            SqlType getType(){ return this->type; }

        protected:
            virtual std::shared_ptr<CommandData> allocateCommandData() = 0;
            virtual std::shared_ptr<CommandData> copyCommandData(std::shared_ptr<CommandData>) = 0;
            virtual result executeCommand(std::shared_ptr<CommandData>) = 0;
            virtual result queryCommand(std::shared_ptr<CommandData>, const QueryCallback& fn) = 0;
        private:
            SqlType type;
    };

    #define SQL_FWD(...) ::std::forward<decltype(__VA_ARGS__)>(__VA_ARGS__)
    namespace impl {
        template <typename ...>
        struct merge;

        template <typename... A, typename... B>
        struct merge<std::tuple<A...>, std::tuple<B...>> {
            using result = std::tuple<A..., B...>;
        };

        /* let me stuff reverse */
        template<typename, typename>
        struct append_reversed { };

        template<typename T, typename... Ts>
        struct append_reversed<T, std::tuple<Ts...>> {
            using type = std::tuple<Ts..., T>;
        };

        template<typename... Ts>
        struct reverse_types {
            using type = std::tuple<>;
        };

        template<typename T, typename... Ts>
        struct reverse_types<T, Ts...> {
            using type = typename append_reversed<T, typename reverse_types<Ts...>::type>::type;
        };

        /* Return type stuff */
        typedef int stardart_return_type;
        template <typename... args>
        using stardart_return = std::function<int(args...)>;

        template <typename type_return, typename enabled = void>
        struct transformer_return {
            using supported = std::false_type;
        };

        /* transforming (no transform) from int(...) to int(...) */
        template <typename return_type>
        struct transformer_return<return_type, typename std::enable_if<std::is_same<return_type, int>::value>::type> {
            using supported = std::true_type;

            template <typename... args>
            static stardart_return<args...> transform(const std::function<return_type(args...)>& function) {
                return function;
            }
        };

        /* transforming from void(...) to int(...) */
        template <typename return_type>
        struct transformer_return<return_type, typename std::enable_if<std::is_void<return_type>::value>::type> {
            using supported = std::true_type;

            template <typename... args>
            static stardart_return<args...> transform(const std::function<return_type(args...)>& function) {
                return [function](args... parms) -> int {
                    function(parms...);
                    return 0;
                };
            }
        };

        /* transforming from ~integral~(...) to int(...) */
        template <typename return_type>
        struct transformer_return<return_type, typename std::enable_if<std::is_integral<return_type>::value && !std::is_same<return_type, int>::value>::type> {
            using supported = std::true_type;

            template <typename... args>
            static stardart_return<args...> transform(const std::function<return_type(args...)>& function) {
                return [function](args... parms) -> int {
                    return (int) function(parms...);
                };
            }
        };

        /* method stuff */
        using stardart_function = std::function<stardart_return_type(int, std::string*, std::string*)>;

        template <typename, typename>
        struct transformer_arguments {
            using supported = std::false_type;
            using arguments_reversed = std::tuple<>;
            static constexpr int argument_count = 0;
        };


        /* we don't need to proxy a standard function */
        template <>
        struct transformer_arguments<std::tuple<std::string*, std::string*, int>, std::tuple<>> {
            using supported = std::true_type;
            using arguments_reversed = std::tuple<>;
            static constexpr int argument_count = 0;

            typedef std::function<stardart_return_type(int, std::string*, std::string*)> typed_function;

            static stardart_function transform(const typed_function& function) {
                return function;
            }
        };

        /* proxy a standard function with left sided arguments */
        template <typename... additional_reversed, typename... additional>
        struct transformer_arguments<std::tuple<std::string*, std::string*, int, additional_reversed...>, std::tuple<additional...>> {
            using supported = std::true_type;
            using arguments_reversed = std::tuple<typename std::remove_const<additional_reversed>::type...>; /* note: these are still reversed */
            static constexpr int argument_count = sizeof...(additional_reversed);

            typedef std::function<stardart_return_type(additional..., int, std::string*, std::string*)> typed_function;
            static stardart_function transform(const typed_function& function, additional&&... args) {
                return [function, &args...](int length, std::string* values, std::string* names) {
                    return function(args..., length, values, names);
                };
            }
        };

        /* proxy int(..., int, char**, char**) function with left sided arguments */
        template <typename... additional_reversed, typename... additional>
        struct transformer_arguments<std::tuple<char**, char**, int, additional_reversed...>, std::tuple<additional...>> {
            using supported = std::true_type;
            using arguments_reversed = std::tuple<typename std::remove_const<additional_reversed>::type...>;
            static constexpr int argument_count = sizeof...(additional);

            typedef std::function<stardart_return_type(additional..., int, char**, char**)> typed_function;
            static stardart_function transform(const typed_function& function, additional&&... args) {
                return [function, &args...](int length, std::string* values, std::string* names) {
#ifdef ALLOW_STACK_ALLOCATION
                    char* array_values[length];
                    char* array_names[length];
#else
                    char** array_values = (char**) malloc(length * sizeof(char*));
                    char** array_names = (char**) malloc(length * sizeof(char*));
#endif
                    for(int i = 0; i < length; i++) {
                        array_values[i] = (char*) values[i].c_str();
                        array_names[i] = (char*) names[i].c_str();
                    }
                    auto result = function(args..., length, array_values, array_names);

#ifndef ALLOW_STACK_ALLOCATION
                    free(array_values);
                    free(array_names);
#endif
                    return result;
                };
            }
        };

        template <typename SelfType>
        class command_base {
                friend class ::sql::command;
                friend class ::sql::model;
            public:
                explicit command_base(std::nullptr_t) : _data{nullptr} {}

                command_base(SqlManager* handle, const std::string &sql, std::initializer_list<variable> values) {
                    assert(handle);
                    assert(!sql.empty());
                    this->_data = handle->allocateCommandData();
                    this->_data->handle = handle;
                    this->_data->sql_command = sql;
                    for(const auto& val : values)
                        this->value(val);
                 }

                template<typename... Ts>
                command_base(SqlManager* handle, const std::string& sql, Ts&&... vars) : command_base(handle, sql, {}) { values(vars...); }

                command_base(const command_base<SelfType>& ref) : _data(ref._data)  {}
                command_base(command_base<SelfType>&& ref) noexcept : _data(ref._data) { }

                virtual ~command_base() = default;

                SelfType& value(const variable& val) {
                    this->_data->variables.push_back(val);
                    return *(SelfType*) this;
                }

                template <typename T>
                SelfType& value(const std::string& key, T&& value) {
                    this->_data->variables.push_back(variable{key, value});

                    return *(SelfType*) this;
                }

                SelfType& values(){ return *(SelfType*) this; }

                template<typename value_t, typename... values_t>
                SelfType& values(const value_t& firstValue, values_t&&... values){
                    this->value(firstValue);
                    this->values(values...);
                    return *(SelfType*) this;
                }

                std::string sqlCommand(){ return _data->sql_command; }
                SqlManager* handle(){ return _data->handle; }

                command_base& operator=(const command_base& other) {
                    this->_data = other._data;
                    return *this;
                }

                command_base& operator=(command_base&& other) {
                    this->_data = std::move(other._data);
                    return *this;
                }

            protected:
                explicit command_base(std::shared_ptr<CommandData> data) : _data(std::move(data)) {}
                std::shared_ptr<CommandData> _data;
        };
    }

    class model : public impl::command_base<model> {
        public:
            model(SqlManager* db, const std::string &sql, std::initializer_list<variable> values) : command_base(db, sql, values){};

            template<typename... Ts>
            model(SqlManager* handle, const std::string &sql, Ts... vars) : model(handle, sql, {}) { values(vars...); }

            explicit model(std::nullptr_t) : command_base{nullptr} {};
            //model(const model& v) : command_base{v} {};
            //model(model&& v) noexcept  : command_base{std::forward<model>(v)} {};
            ~model() override = default;

            sql::command command();
            sql::model copy();
        private:
            explicit model(const std::shared_ptr<CommandData>&);
    };

    class command : public impl::command_base<command> {
        public:
            command(SqlManager* db, const std::string &sql, std::initializer_list<variable> values) : command_base(db, sql, values) {};

            template<typename... Ts>
            command(SqlManager* handle, const std::string &sql, Ts... vars) : command_base(handle, sql, {}) { values(SQL_FWD(vars)...); }
            
             /*
            template<typename arg_0_t, typename arg_1_t>
            command(SqlManager* handle, const std::string &sql, std::initializer_list<typename V> arg_0, std::initializer_list<arg_1_t> arg_1) : command_base(handle, sql, {}) {
                //static_assert(false, "testing");
            }

            command(SqlManager* handle, const std::string &sql) : command_base(handle, sql, {}) {}
            */

            explicit command(model& c) : command_base(c.handle(), c.sqlCommand()) {
                this->_data = c._data->handle->copyCommandData(c._data);
            }

            command(const command& v): command_base(v) {};
            command(command&& v) noexcept : command_base(v){};
            ~command() override = default;;

            result execute() {
                return this->_data->handle->executeCommand(this->_data);
            }

            threads::Future<result> executeLater();

            //Convert lambdas to std::function
            template <typename lambda, typename... arguments>
            result query(const lambda& lam, arguments&&... args) {
                typedef stx::lambda_type<lambda> info;
                return this->query((typename info::invoker_function) lam, SQL_FWD(args)...);
            }

            template <typename call_ret, typename... call_args, typename... args>
            result query(const std::function<call_ret(call_args...)>& callback, args&&... parms) { //Query without data
                typedef impl::transformer_return<call_ret> ret_transformer;
                typedef impl::transformer_arguments<typename impl::reverse_types<call_args...>::type, std::tuple<args...>> args_transformer;

                constexpr bool valid_return_type = ret_transformer::supported::value;
                constexpr bool valid_arguments = args_transformer::supported::value;
                constexpr bool valid_argument_count =
                        !valid_arguments || //Don't throw when function is invalid
                        !valid_return_type ||  //Don't throw when function is invalid
                        args_transformer::argument_count == sizeof...(args);
                constexpr bool valid_argument_types =
                        !valid_arguments || //Don't throw when function is invalid
                        !valid_return_type ||  //Don't throw when function is invalid
                        !valid_argument_count || //Don't throw when arg count is invalid
                        std::is_same<typename args_transformer::arguments_reversed, typename impl::reverse_types<args...>::type>::value;

                static_assert(valid_return_type, "Return type isn't supported!");
                static_assert(valid_arguments, "Arguments not supported!");
                static_assert(valid_argument_count, "Invalid argument count!");
                static_assert(valid_argument_types, "Invalid argument types!");

                return this->query_invoke<ret_transformer, args_transformer>(callback, SQL_FWD(parms)...);
            };
        private:
            template <typename ret_transformer, typename args_transformer, typename type_call, typename... args>
            inline result query_invoke(const type_call& callback, args&&... parms) {
                auto standard_return = ret_transformer::transform(callback);
                auto standard_function = args_transformer::transform(standard_return, SQL_FWD(parms)...);

                return this->_data->handle->queryCommand(this->_data, standard_function);
            }
    };

    class AsyncSqlPool {
        public:
            explicit AsyncSqlPool(size_t threads);
            ~AsyncSqlPool();
            AsyncSqlPool(AsyncSqlPool&) = delete;
            AsyncSqlPool(const AsyncSqlPool&) = delete;
            AsyncSqlPool(AsyncSqlPool&&) = delete;

            threads::Future<result> executeLater(const command& cmd);
            threads::ThreadPool* threads(){ return _threads; }
        private:
            threads::ThreadPool* _threads = nullptr;
    };
}


inline std::ostream& sql::operator<<(std::ostream& s,const result* res){
    if(!res) s << "nullptr";
    else {
        if(!res->sql().empty())
            s << " sql: " << res->sql() << " returned -> ";
        s << res->code() << "/" << res->msg();
    }
    return s;
}

inline std::ostream& sql::operator<<(std::ostream& s,const result& res){
    return sql::operator<<(s, &res);
}