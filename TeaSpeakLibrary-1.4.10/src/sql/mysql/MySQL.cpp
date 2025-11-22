#include <memory>

#include "MySQL.h"
#include "src/log/LogUtils.h"
#include <pipes/misc/http.h>

#include <memory>
#include <utility>

#ifndef CR_CONNECTION_ERROR
    #define CR_CONNECTION_ERROR     (2002)
    #define CR_SERVER_GONE_ERROR    (2006)
    #define CR_SERVER_LOST          (2013)
#endif
using namespace std;
using namespace sql;
using namespace sql::mysql;

//TODO: Cache statements in general any only reapply the values

MySQLManager::MySQLManager() : SqlManager(SqlType::TYPE_MYSQL) {}
MySQLManager::~MySQLManager() {}
//Property info: https://dev.mysql.com/doc/connector-j/5.1/en/connector-j-reference-configuration-properties.html
//mysql://[host][:port]/[database][?propertyName1=propertyValue1[&propertyName2=propertyValue2]...]

#define MYSQL_PREFIX "mysql://"
inline result parse_url(const string& url, std::map<std::string, std::string>& connect_map) {
    string target_url;
    if(url.find(MYSQL_PREFIX) != 0) {
        return {"", ERROR_MYSQL_INVLID_URL, -1, "Missing mysql:// at begin"};
    }

    auto index_parms = url.find('?');
    if(index_parms == string::npos) {
        target_url = "tcp://" + url.substr(strlen(MYSQL_PREFIX));
    } else {
        target_url = "tcp://" + url.substr(strlen(MYSQL_PREFIX), index_parms - strlen(MYSQL_PREFIX));
        auto parms = url.substr(index_parms + 1);
        size_t index = 0;
        do {
            auto idx = parms.find('&', index);
            auto element = parms.substr(index, idx - index);

            auto key_idx = element.find('=');
            auto key = element.substr(0, key_idx);
            auto value = element.substr(key_idx + 1);
            connect_map[key] = http::decode_url(value);
            logTrace(LOG_GENERAL, "Got mysql property {}. Value: {}", key, value);

            index = idx + 1;
        } while(index != 0);
    }
    //TODO: Set CLIENT_MULTI_STATEMENTS
    //if(!connect_map["hostName"].get<const char*>() || strcmp(*connect_map["hostName"].get<const char*>(), ""))
    connect_map["hostName"] = target_url;
    logTrace(LOG_GENERAL, "Got mysql property {}. Value: {}", "hostName", target_url);
    return result::success;
}

//mysql://[host][:port]/[database][?propertyName1=propertyValue1[&propertyName2=propertyValue2]...]
inline bool parse_mysql_data(const string& url, string& error, string& host, uint16_t& port, string& database, map<string, string>& properties) {
    size_t parse_index = 0;
    /* parse the scheme */
    {
        auto index = url.find("://", parse_index);
        if(index == -1 || url.substr(parse_index, index - parse_index) != "mysql") {
            error = "missing/invalid URL scheme";
            return false;
        }

        parse_index = index + 3;
        if(parse_index >= url.length()) {
            error = "unexpected EOL after scheme";
            return false;
        }
    }

    /* parse host[:port]*/
    {
        auto index = url.find('/', parse_index);
        if(index == -1) {
            error = "missing host/port";
            return false;
        }

        auto host_port = url.substr(parse_index, index - parse_index);

        auto port_index = host_port.find(':');
        if(port_index == -1) {
            host = host_port;
        } else {
            host = host_port.substr(0, port_index);
            auto port_str = host_port.substr(port_index + 1);
            try {
                port = stol(port_str);
            } catch(std::exception& ex) {
                error = "failed to parse port";
                return false;
            }
        }
        if(host.empty()) {
            error = "host is empty";
            return false;
        }

        parse_index = index + 1;
        if(parse_index >= url.length()) {
            error = "unexpected EOL after host/port";
            return false;
        }
    }

    /* the database */
    {
        auto index = url.find('?', parse_index);
        if(index == -1) {
            database = url.substr(parse_index);
            parse_index = url.length();
        } else {
            database = url.substr(parse_index, index - parse_index);
            parse_index = index + 1;
        }

        if(database.empty()) {
            error = "database is empty";
            return false;
        }
    }

    /* properties */
    string full_property, property_key, property_value;
    while(parse_index < url.length()){
        /* "read" the next property */
        {
            auto index = url.find('&', parse_index); /* next entry */
            if(index == -1) {
                full_property = url.substr(parse_index);
                parse_index = url.length();
            } else {
                full_property = url.substr(parse_index, index - parse_index);
                parse_index = index + 1;
            }
        }

        /* parse it */
        {
            auto index = full_property.find('=');
            if(index == -1) {
                error = "invalid property format (missing '=')";
                return false;
            }

            property_key = full_property.substr(0, index);
            property_value = full_property.substr(index + 1);
            if(property_key.empty() || property_value.empty()) {
                error = "invalid property key/value (empty)";
                return false;
            }

            properties[property_key] = http::decode_url(property_value);
        }
    }
    return true;
}

mysql::Connection::~Connection() {
    {
        lock_guard lock(this->used_lock);
        assert(!this->used);
    }

    if(this->handle) {
        mysql_close(this->handle);
        this->handle = nullptr;
    }
}

result MySQLManager::connect(const std::string &url) {
    this->disconnecting = false;
    string error;

    map<string, string> properties;
    string host, database;
    uint16_t port;

    if(!parse_mysql_data(url, error, host, port, database, properties)) {
        error = "URL parsing failed: " + error;
        return {"", ERROR_MYSQL_INVLID_URL, -1, error};
    }

    size_t connections = 4;
    if(properties.count("connections") > 0) {
        try {
            connections = stol(properties["connections"]);
        } catch(std::exception& ex) {
            return {"", ERROR_MYSQL_INVLID_PROPERTIES, -1, "could not parse connection count"};
        }
    }

    string username, password;
    if(properties.count("userName") > 0) username = properties["userName"];
    if(properties.count("username") > 0) username = properties["username"];
    if(username.empty()) return {"", ERROR_MYSQL_INVLID_PROPERTIES, -1, "missing username property"};

    if(properties.count("password") > 0) password = properties["password"];
    if(password.empty()) return {"", ERROR_MYSQL_INVLID_PROPERTIES, -1, "missing password property"};

    //debugMessage(LOG_GENERAL, R"([MYSQL] Starting {} connections to {}:{} with database "{}" as user "{}")", connections, host, port, database, username);

    for(size_t index = 0; index < connections; index++) {
        auto connection = make_shared<Connection>();
        connection->handle = mysql_init(nullptr);
        if(!connection->handle)
            return {"", -1, -1, "failed to allocate connection " + to_string(index)};

        {
            uint32_t reconnect{true};
            mysql_options(connection->handle, MYSQL_OPT_RECONNECT, &reconnect);
        }
        mysql_options(connection->handle, MYSQL_SET_CHARSET_NAME, "utf8");
        mysql_options(connection->handle, MYSQL_INIT_COMMAND, "SET NAMES utf8");

        auto result = mysql_real_connect(connection->handle, host.c_str(), username.c_str(), password.c_str(), database.c_str(), port, nullptr, 0); //CLIENT_MULTI_RESULTS | CLIENT_MULTI_STATEMENTS
        if(!result)
            return {"", -1, -1, "failed to connect to server with connection " + to_string(index) + ": " + mysql_error(connection->handle)};

        connection->used = false;
        this->connections.push_back(connection);
    }
    return result::success;
}

bool MySQLManager::connected() {
    lock_guard<mutex> lock(this->connections_mutex);
    return !this->connections.empty();
}

result MySQLManager::disconnect() {
    lock_guard<mutex> lock(this->connections_mutex);
    this->disconnecting = true;

    this->connections.clear();
    this->connections_condition.notify_all();

    this->disconnecting = false;
    return result::success;
}

struct StatementGuard {
    MYSQL_STMT* stmt;

    ~StatementGuard() {
        mysql_stmt_close(this->stmt);
    }
};

struct ResultGuard {
    MYSQL_RES* result;

    ~ResultGuard() {
        mysql_free_result(this->result);
    }
};

template <typename T>
struct FreeGuard {
    T* ptr;

    ~FreeGuard() {
        if(this->ptr) ::free(this->ptr);
    }
};

template <typename T>
struct DeleteAGuard {
    T* ptr;

    ~DeleteAGuard() {
        delete[] this->ptr;
    }
};

std::shared_ptr<CommandData> MySQLManager::allocateCommandData() {
    return make_shared<MySQLCommand>();
}

std::shared_ptr<CommandData> MySQLManager::copyCommandData(std::shared_ptr<CommandData> ptr) {
    auto _new = this->allocateCommandData();
    _new->handle = ptr->handle;
    _new->lock = ptr->lock;
    _new->sql_command = ptr->sql_command;
    _new->variables = ptr->variables;

    auto __new = static_pointer_cast<MySQLCommand>(_new);
    auto __ptr = static_pointer_cast<MySQLCommand>(ptr);
    //__new->stmt = __ptr->stmt;
    return __new;
}

namespace sql::mysql {
        bool evaluate_sql_query(string& sql, const std::vector<variable>& vars, std::vector<variable>& result) {
            char quote = 0;
            for(int index = 0; index < sql.length(); index++) {
                if(sql[index] == '\'' || sql[index] == '"' || sql[index] == '`') {
                    if(quote > 0) {
                        if(quote == sql[index]) quote = 0;
                    } else {
                        quote = sql[index];
                    }
                    continue;
                }
                if(quote > 0) continue;
                if(sql[index] != ':') continue;

                auto index_end = sql.find_first_not_of("0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_", index + 1);
                if(index_end == string::npos) index_end = sql.length();

                string key = sql.substr(index, index_end - index);
                //Now we can replace it with a ?
                sql.replace(index, index_end - index, "?", 1);

                bool insert = false;
                for(const auto& e : vars)
                    if(e.key() == key) {
                        result.push_back(e);
                        insert = true;
                        break;
                    }
                if(!insert)
                    result.emplace_back();
            }
            return true;
        }

        struct BindMemory { };

        /* memory must be freed via ::free! */
        bool create_bind(BindMemory*& memory, const std::vector<variable>& variables) {
            size_t required_bytes = sizeof(MYSQL_BIND) * variables.size();

            /* first lets calculate the required memory */
            {
                for(auto& variable : variables) {
                    switch (variable.type()) {
                        case VARTYPE_NULL:
                            break;
                        case VARTYPE_BOOLEAN:
                            required_bytes += sizeof(bool);
                            break;
                        case VARTYPE_INT:
                            required_bytes += sizeof(int32_t);
                            break;
                        case VARTYPE_LONG:
                            required_bytes += sizeof(int64_t);
                            break;
                        case VARTYPE_DOUBLE:
                            required_bytes += sizeof(double);
                            break;
                        case VARTYPE_FLOAT:
                            required_bytes += sizeof(float);
                            break;
                        case VARTYPE_TEXT:
                            //TODO: Use a direct pointer to the variable's value instead of copying it
                            required_bytes += sizeof(unsigned long*) + variable.value().length();
                            break;
                        default:
                            return false; /* unknown variable type */
                    }
                }
            }
            if(!required_bytes) {
                memory = nullptr;
                return true;
            }

            //logTrace(LOG_GENERAL, "[MYSQL] Allocated {} bytes for parameters", required_bytes);
            memory = (BindMemory*) malloc(required_bytes);
            if(!memory)
                return false;

            memset(memory, 0, required_bytes);
            /* lets fill the values */
            {
                size_t memory_index = variables.size() * sizeof(MYSQL_BIND);
                auto bind_ptr = (MYSQL_BIND*) memory;
                auto payload_ptr = (char*) memory + sizeof(MYSQL_BIND) * variables.size();

                for(size_t index = 0; index < variables.size(); index++) {
                    bind_ptr->buffer = payload_ptr;

                    auto& variable = variables[index];
                    switch (variable.type()) {
                        case VARTYPE_NULL:
                            bind_ptr->buffer_type = enum_field_types::MYSQL_TYPE_NULL;
                            break;
                        case VARTYPE_BOOLEAN:
                            bind_ptr->buffer_type = enum_field_types::MYSQL_TYPE_TINY;
                            bind_ptr->buffer_length = sizeof(bool);
                            *(bool*) payload_ptr = variable.as<bool>();
                            break;
                        case VARTYPE_INT:
                            bind_ptr->buffer_type = enum_field_types::MYSQL_TYPE_LONG;
                            bind_ptr->buffer_length = sizeof(int32_t);
                            *(int32_t*) payload_ptr = variable.as<int32_t>();
                            break;
                        case VARTYPE_LONG:
                            bind_ptr->buffer_type = enum_field_types::MYSQL_TYPE_LONGLONG;
                            bind_ptr->buffer_length = sizeof(int64_t);
                            *(int64_t*) payload_ptr = variable.as<int64_t>();
                            break;
                        case VARTYPE_DOUBLE:
                            bind_ptr->buffer_type = enum_field_types::MYSQL_TYPE_DOUBLE;
                            bind_ptr->buffer_length = sizeof(double);
                            *(double*) payload_ptr = variable.as<double>();
                            break;
                        case VARTYPE_FLOAT:
                            bind_ptr->buffer_type = enum_field_types::MYSQL_TYPE_FLOAT;
                            bind_ptr->buffer_length = sizeof(float);
                            *(float*) payload_ptr = variable.as<float>();
                            break;
                        case VARTYPE_TEXT: {
                            auto value = variable.value();

                            //TODO: Use a direct pointer to the variable's value instead of copying it
                            //May use a string object allocated on the memory_ptr? (Special deinit needed then!)
                            bind_ptr->buffer_type = enum_field_types::MYSQL_TYPE_STRING;
                            bind_ptr->buffer_length = value.length();

                            bind_ptr->length = (unsigned long*) payload_ptr;
                            *bind_ptr->length = bind_ptr->buffer_length;

                            payload_ptr += sizeof(unsigned long*);
                            memory_index += sizeof(unsigned long*);

                            memcpy(payload_ptr, value.data(), value.length());
                            bind_ptr->buffer = payload_ptr;
                            break;
                        }
                        default:
                            return false; /* unknown variable type */
                    }

                    payload_ptr += bind_ptr->buffer_length;
                    bind_ptr++;
                    assert(memory_index <= required_bytes);
                }
            }

            return true;
        }

        struct ResultBindDescriptor {
            size_t primitive_size = 0;

            void(*destroy)(char*& /* primitive ptr */) = nullptr;
            bool(*create)(const MYSQL_FIELD& /* field */, MYSQL_BIND& /* bind */, char*& /* primitive ptr */) = nullptr;

            bool(*get_as_string)(MYSQL_BIND& /* bind */, std::string& /* result */) = nullptr;
        };

        /* memory to primitive string */
        template <typename T>
        bool m2ps(MYSQL_BIND& bind, string& str) {
            if(bind.error_value || (bind.error && *bind.error)) return false;
            str = std::to_string(*(T*) bind.buffer);
            return true;
        }

        template <size_t size>
        void _do_destroy_primitive(char*& primitive_ptr) {
            primitive_ptr += size;
        }

        template <uint8_t type, size_t size>
        bool _do_bind_primitive(const MYSQL_FIELD&, MYSQL_BIND& bind, char*& primitive_ptr) {
            bind.buffer = (void*) primitive_ptr;
            bind.buffer_length = size;
            bind.buffer_type = (enum_field_types) type;
            primitive_ptr += size;
            return true;
        }

        #define CREATE_PRIMATIVE_BIND_DESCRIPTOR(mysql_type, c_type, size) \
            case mysql_type:\
                static ResultBindDescriptor _ ##mysql_type = {\
                        size,\
                        _do_destroy_primitive<size>,\
                        _do_bind_primitive<mysql_type, size>,\
                        m2ps<c_type>\
                };\
                return &_ ##mysql_type;

        const ResultBindDescriptor* get_bind_descriptor(enum_field_types type) {
            switch (type) {
                case MYSQL_TYPE_NULL:
                    static ResultBindDescriptor _null = {
                            /* primitive_size */    0,
                            /* destroy */           _do_destroy_primitive<0>,
                            /* create */            _do_bind_primitive<MYSQL_TYPE_NULL, 0>,
                            /* get_as_string */     [](MYSQL_BIND&, string& str) { str.clear(); return true; }
                    };
                    return &_null;
                CREATE_PRIMATIVE_BIND_DESCRIPTOR(MYSQL_TYPE_TINY, int8_t, 1);
                CREATE_PRIMATIVE_BIND_DESCRIPTOR(MYSQL_TYPE_SHORT, int16_t, 2);
                CREATE_PRIMATIVE_BIND_DESCRIPTOR(MYSQL_TYPE_INT24, int32_t, 4);
                CREATE_PRIMATIVE_BIND_DESCRIPTOR(MYSQL_TYPE_LONG, int32_t, 4);
                CREATE_PRIMATIVE_BIND_DESCRIPTOR(MYSQL_TYPE_LONGLONG, int64_t, 8);
                CREATE_PRIMATIVE_BIND_DESCRIPTOR(MYSQL_TYPE_DOUBLE, double, sizeof(double));
                CREATE_PRIMATIVE_BIND_DESCRIPTOR(MYSQL_TYPE_FLOAT, float, sizeof(float));
                case MYSQL_TYPE_VAR_STRING:
                case MYSQL_TYPE_STRING:
                case MYSQL_TYPE_BLOB:
                    static ResultBindDescriptor _string = {
                            /* we store the allocated buffer in the primitive types buffer and the length */
                            /* primitive_size */ sizeof(void*) + sizeof(unsigned long*),

                            /* destroy */ [](char*& primitive) {
                                    ::free(*(void**) primitive);
                                    primitive += sizeof(void*);
                                    primitive += sizeof(unsigned long*);
                            },
                            /* create */ [](const MYSQL_FIELD& field, MYSQL_BIND& bind, char*& primitive) {
                                bind.buffer_length = field.max_length > 0 ? field.max_length : min(field.length, 5UL * 1024UL * 1024UL);
                                bind.buffer = malloc(bind.buffer_length);
                                bind.buffer_type = MYSQL_TYPE_BLOB;

                                *(void**) primitive = bind.buffer;
                                primitive += sizeof(void*);

                                bind.length = (unsigned long*) primitive;
                                primitive += sizeof(unsigned long*);

                                return bind.buffer != nullptr;
                            },
                            /* get_as_string */ [](MYSQL_BIND& bind, std::string& result) {
                                auto length = bind.length ? *bind.length : bind.length_value;
                                result.reserve(length);
                                result.assign((const char*) bind.buffer, length);
                                return true;
                            }
                    };
                    return &_string;
                default:
                    return nullptr;
            }
        }

        #undef CREATE_PRIMATIVE_BIND_DESCRIPTOR

        struct ResultBind {
            size_t field_count = 0;
            BindMemory* memory = nullptr;
            const ResultBindDescriptor** descriptors = nullptr;

            ~ResultBind() {
                if(memory) {
                    auto memory_ptr = (char*) this->memory + (sizeof(MYSQL_BIND) * field_count);

                    for(size_t index = 0; index < this->field_count; index++)
                        this->descriptors[index]->destroy(memory_ptr);

                    ::free(memory);
                }
                delete[] descriptors;
            }

            ResultBind(size_t a, BindMemory* b, const ResultBindDescriptor** c) : field_count{a}, memory{b}, descriptors{c} {}
            ResultBind(const ResultBind&) = delete;
            ResultBind(ResultBind&&) = default;

            inline bool get_as_string(size_t column, string& result) {
                if(!descriptors) return false;

                auto& bind_ptr = *(MYSQL_BIND*) ((char*) this->memory + sizeof(MYSQL_BIND) * column);
                return this->descriptors[column]->get_as_string(bind_ptr, result);
            }

            inline bool get_as_string(string* results) {
                if(!descriptors) return false;

                auto bind_ptr = (MYSQL_BIND*) this->memory;
                for(int index = 0; index <  this->field_count; index++)
                    if(!this->descriptors[index]->get_as_string(*bind_ptr, results[index]))
                        return false;
                    else
                        bind_ptr++;
                return true;
            }
        };

        bool create_result_bind(size_t field_count, MYSQL_FIELD* fields, ResultBind& result) {
            size_t required_bytes = sizeof(MYSQL_BIND) * field_count;

            assert(!result.field_count);
            assert(!result.descriptors);
            assert(!result.memory);
            result.descriptors = new const ResultBindDescriptor*[field_count];
            result.field_count = field_count;

            for(size_t index = 0; index < field_count; index++) {
                result.descriptors[index] = get_bind_descriptor(fields[index].type);
                if(!result.descriptors[index]) return false;

                required_bytes += result.descriptors[index]->primitive_size;
            }

            if(!required_bytes) {
                result.memory = nullptr;
                return true;
            }

            //logTrace(LOG_GENERAL, "[MYSQL] Allocated {} bytes for response", required_bytes);
            result.memory = (BindMemory*) malloc(required_bytes);
            if(!result.memory)
                return false;

            memset(result.memory, 0, required_bytes);
            auto memory_ptr = (char*) result.memory + (sizeof(MYSQL_BIND) * field_count);
            auto bind_ptr = (MYSQL_BIND*) result.memory;
            for(size_t index = 0; index < field_count; index++) {
                if(!result.descriptors[index]->create(fields[index], *bind_ptr, memory_ptr)) return false;
                bind_ptr->buffer_type = fields[index].type;
                bind_ptr++;
            }
            assert(memory_ptr == ((char*) result.memory + required_bytes)); /* Overflow check */
            return true;
        }
    }

AcquiredConnection::AcquiredConnection(MySQLManager* owner, std::shared_ptr<sql::mysql::Connection> connection) : owner(owner), connection(std::move(connection)) { }
AcquiredConnection::~AcquiredConnection() {
    {
        lock_guard lock{this->connection->used_lock};
        this->connection->used = false;
    }

    {
        lock_guard lock(this->owner->connections_mutex);
        this->owner->connections_condition.notify_one();
    }
}
std::unique_ptr<AcquiredConnection> MySQLManager::next_connection() {
    unique_ptr<AcquiredConnection> result;
    {
        unique_lock connections_lock(this->connections_mutex);

        while(!result) {
            size_t available_connections = 0;
            for(const auto& connection : this->connections) {
                available_connections++;

                {
                    lock_guard use_lock(connection->used_lock);
                    if(connection->used) continue;
                        connection->used = true;
                }

                result = std::make_unique<AcquiredConnection>(this, connection);
                break;
            }

            if(!result) {
                if(available_connections == 0) {
                    if(this->listener_disconnected)
                        this->listener_disconnected(false);
                    this->disconnect();
                    return nullptr;
                }

                this->connections_condition.wait(connections_lock); /* wait for the next connection */
            }
        }
    }
    //TODO: Test if the connection hasn't been used for a longer while if so use mysql_ping() to verify the connection

    return result;
}

void MySQLManager::connection_closed(const std::shared_ptr<sql::mysql::Connection> &connection) {
    bool call_disconnect;
    {
        unique_lock connections_lock{this->connections_mutex};
        auto index = std::find(this->connections.begin(), this->connections.end(), connection);
        if(index == this->connections.end()) {
            return;
        }

        this->connections.erase(index);
        call_disconnect = this->connections.empty();
    }

    auto dl = this->listener_disconnected;
    if(call_disconnect && dl) {
        dl(this->disconnecting);
    }
}

result MySQLManager::executeCommand(std::shared_ptr<CommandData> command_data) {
    auto mysql_data = static_pointer_cast<MySQLCommand>(command_data);
    if(!mysql_data) {
        return {"", -1, -1, "invalid command handle"};
    }

    std::lock_guard<threads::Mutex> lock(mysql_data->lock);
    auto command = mysql_data->sql_command;

    auto variables = mysql_data->variables;
    vector<variable> mapped_variables;
    if(!sql::mysql::evaluate_sql_query(command, variables, mapped_variables)) {
        return {mysql_data->sql_command, -1, -1, "Could not map sqlite vars to mysql!"};
    }

    FreeGuard<BindMemory> bind_parameter_memory{nullptr};
    if(!sql::mysql::create_bind(bind_parameter_memory.ptr, mapped_variables)) {
        return {mysql_data->sql_command, -1, -1, "Failed to allocate bind memory!"};
    }

    ResultBind bind_result_data{0, nullptr, nullptr};

    auto connection = this->next_connection();
    if(!connection) {
        return {mysql_data->sql_command, -1, -1, "Could not get a valid connection!"};
    }

    StatementGuard stmt_guard{mysql_stmt_init(connection->connection->handle)};
    if(!stmt_guard.stmt) {
        return {mysql_data->sql_command, -1, -1, "failed to allocate statement"};
    }

    if(mysql_stmt_prepare(stmt_guard.stmt, command.c_str(), command.length())) {
        auto errc = mysql_stmt_errno(stmt_guard.stmt);
        if(errc == CR_SERVER_GONE_ERROR || errc == CR_SERVER_LOST || errc == CR_CONNECTION_ERROR) {
            this->connection_closed(connection->connection);
        }

        return {mysql_data->sql_command, -1, -1, "failed to prepare statement: " + string(mysql_stmt_error(stmt_guard.stmt))};
    }

    /* validate all parameters */
    auto parameter_count = mysql_stmt_param_count(stmt_guard.stmt);
    if(parameter_count != mapped_variables.size()) {
        return {mysql_data->sql_command, -1, -1, "invalid parameter count. Statement contains " + to_string(parameter_count) + " parameters but only " + to_string(mapped_variables.size()) + " are given."};
    }

    if(bind_parameter_memory.ptr) {
        if(mysql_stmt_bind_param(stmt_guard.stmt, (MYSQL_BIND*) bind_parameter_memory.ptr)) {
            return {mysql_data->sql_command, -1, -1, "failed to bind parameters to statement: " + string(mysql_stmt_error(stmt_guard.stmt))};
        }
    } else if(parameter_count > 0) {
        return {mysql_data->sql_command, -1, -1, "invalid parameter count. Statement contains " + to_string(parameter_count) + " parameters but only " + to_string(mapped_variables.size()) + " are given (bind nullptr)."};
    }


    if(mysql_stmt_execute(stmt_guard.stmt)) {
        auto errc = mysql_stmt_errno(stmt_guard.stmt);
        if(errc == CR_SERVER_GONE_ERROR || errc == CR_SERVER_LOST || errc == CR_CONNECTION_ERROR) {
            this->connection_closed(connection->connection);
        }

        return {mysql_data->sql_command, -1, -1, "failed to execute query statement: " + string(mysql_stmt_error(stmt_guard.stmt))};
    }

    auto insert_row_id = mysql_stmt_insert_id(stmt_guard.stmt);
    return {mysql_data->sql_command, 0, (int64_t) insert_row_id, "success"};
}

result MySQLManager::queryCommand(shared_ptr<CommandData> command_data, const QueryCallback &fn) {
    auto mysql_data = static_pointer_cast<MySQLCommand>(command_data);
    if(!mysql_data) {
        return {"", -1, -1, "invalid command handle"};
    }

    std::lock_guard<threads::Mutex> lock(mysql_data->lock);
    auto command = mysql_data->sql_command;

    auto variables = mysql_data->variables;
    vector<variable> mapped_variables;
    if(!sql::mysql::evaluate_sql_query(command, variables, mapped_variables)) return {mysql_data->sql_command, -1, -1, "Could not map sqlite vars to mysql!"};

    FreeGuard<BindMemory> bind_parameter_memory{nullptr};
    if(!sql::mysql::create_bind(bind_parameter_memory.ptr, mapped_variables)) return {mysql_data->sql_command, -1, -1, "Failed to allocate bind memory!"};

    ResultBind bind_result_data{0, nullptr, nullptr};

    auto connection = this->next_connection();
    if(!connection) return {mysql_data->sql_command, -1, -1, "Could not get a valid connection!"};

    StatementGuard stmt_guard{mysql_stmt_init(connection->connection->handle)};
    if(!stmt_guard.stmt)
        return {mysql_data->sql_command, -1, -1, "failed to allocate statement"};

    if(mysql_stmt_prepare(stmt_guard.stmt, command.c_str(), command.length())) {
        auto errc = mysql_stmt_errno(stmt_guard.stmt);
        if(errc == CR_SERVER_GONE_ERROR || errc == CR_SERVER_LOST || errc == CR_CONNECTION_ERROR)
            this->connection_closed(connection->connection);

        return {mysql_data->sql_command, -1, -1, "failed to prepare statement: " + string(mysql_stmt_error(stmt_guard.stmt))};
    }

    /* validate all parameters */
    {
        auto parameter_count = mysql_stmt_param_count(stmt_guard.stmt);
        if(parameter_count != mapped_variables.size())
            return {mysql_data->sql_command, -1, -1, "invalid parameter count. Statement contains " + to_string(parameter_count) + " parameters but only " + to_string(mapped_variables.size()) + " are given."};
    }

    if(bind_parameter_memory.ptr) {
        if(mysql_stmt_bind_param(stmt_guard.stmt, (MYSQL_BIND*) bind_parameter_memory.ptr))
            return {mysql_data->sql_command, -1, -1, "failed to bind parameters to statement: " + string(mysql_stmt_error(stmt_guard.stmt))};
    }

    if(mysql_stmt_execute(stmt_guard.stmt)) {
        auto errc = mysql_stmt_errno(stmt_guard.stmt);
        if(errc == CR_SERVER_GONE_ERROR || errc == CR_SERVER_LOST || errc == CR_CONNECTION_ERROR)
            this->connection_closed(connection->connection);

        return {mysql_data->sql_command, -1, -1, "failed to execute query statement: " + string(mysql_stmt_error(stmt_guard.stmt))};
    }

    //if(mysql_stmt_store_result(stmt_guard.stmt))
    //    return {ptr->sql_command, -1, "failed to store query result: " + string(mysql_stmt_error(stmt_guard.stmt))};

    ResultGuard result_guard{mysql_stmt_result_metadata(stmt_guard.stmt)};
    if(!result_guard.result)
        return {mysql_data->sql_command, -1, -1, "failed to query result metadata: " + string(mysql_stmt_error(stmt_guard.stmt))};


    auto field_count = mysql_num_fields(result_guard.result);
    DeleteAGuard<string> field_names{new string[field_count]};
    DeleteAGuard<string> field_values{new string[field_count]};

    {
        auto field_meta = mysql_fetch_fields(result_guard.result);
        if(!field_meta && field_count > 0)
            return {mysql_data->sql_command, -1, -1, "failed to fetch field meta"};

        if(!sql::mysql::create_result_bind(field_count, field_meta, bind_result_data))
            return {mysql_data->sql_command, -1, -1, "failed to allocate result buffer"};

        if(mysql_stmt_bind_result(stmt_guard.stmt, (MYSQL_BIND*) bind_result_data.memory))
            return {mysql_data->sql_command, -1, -1, "failed to bind response buffer to statement: " + string(mysql_stmt_error(stmt_guard.stmt))};

        for(size_t index = 0; index < field_count; index++) {
            field_names.ptr[index] = field_meta[index].name; // field_meta cant be null because it has been checked above
            //cout << field_names.ptr[index] << " - " << field_meta[index].max_length << endl;
        }
    }

    bool user_quit = false;
    int stmt_code, row_id = 0;
    while(!(stmt_code = mysql_stmt_fetch(stmt_guard.stmt))) {
        bind_result_data.get_as_string(field_values.ptr);

        if(fn(field_count, field_values.ptr, field_names.ptr) != 0) {
            user_quit = true;
            break;
        }

        row_id++;
    }

    if(!user_quit) {
        if(stmt_code == 1) {
            auto errc = mysql_stmt_errno(stmt_guard.stmt);
            if(errc == CR_SERVER_GONE_ERROR || errc == CR_SERVER_LOST || errc == CR_CONNECTION_ERROR)
                this->connection_closed(connection->connection);

            return {mysql_data->sql_command, -1, -1, "failed to fetch response row " + to_string(row_id) + ": " + string(mysql_stmt_error(stmt_guard.stmt))};
        } else if(stmt_code == MYSQL_NO_DATA)
            ;
        else if(stmt_code == MYSQL_DATA_TRUNCATED)
            return {mysql_data->sql_command, -1, -1, "response data has been truncated"};
    }
    return result::success;
}