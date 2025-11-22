#include <string>
#include <array>
#include <utility>
#include <vector>
#include <tuple>
#include <iostream>
#include <cassert>
#include <sql/SqlQuery.h>

#if 0
template <typename T>
struct Column {
    constexpr explicit Column(const std::string_view& name) : name{name} { }

    const std::string name;
};

struct BindingNameGenerator {
    public:
        [[nodiscard]] static std::string generate(size_t index) {
            assert(index > 0);
            std::string result{};
            result.resize(std::max(index >> 4U, 1UL));
            for(auto it = result.begin(); index > 0; index >>= 4U)
                *(it++) = number_map[index & 0xFU];
            return result;
        }

    private:
        constexpr static std::array<char, 16> number_map{
                'a', 'b', 'c', 'd', 'e', 'f',
                'g', 'h', 'i', 'j', 'k', 'l',
                'm', 'n', 'o', 'p'
        };
};

template <typename... ColTypes>
struct InsertQueryGenerator {
    struct GenerateOptions {
        bool enable_ignore{false};
        sql::SqlType target{sql::SqlType::TYPE_SQLITE};
    };

    explicit InsertQueryGenerator(std::string target_table, Column<ColTypes>... columns) : table_name{std::move(target_table)}, columns{std::move(columns.name)...} { }

    [[nodiscard]] constexpr inline auto column_count() const { return sizeof...(ColTypes); };

    [[nodiscard]] inline std::string generate_query(size_t entry_count, sql::SqlType target = sql::SqlType::TYPE_SQLITE, bool enable_ignore = false) const {
        if(entry_count == 0) return "-- No entries given";

        std::string result{"INSERT "};
        if(enable_ignore) {
            if(target == sql::TYPE_MYSQL)
                result += "IGNORE ";
            else
                result += "OR IGNORE ";
        }

        result += this->table_name + " (";
        //"INSERT INTO " + this->table_name + " ("
        for(auto it = this->columns.begin();;) {
            result += "`" + *it + "`";
            if(++it != this->columns.end()) {
                result += ", ";
            } else {
                break;
            }
        }
        result += ") VALUES (";
        for(size_t index{1}; index <= this->column_count() * entry_count; index++)
            result += ":" + BindingNameGenerator::generate(index) + ", ";
        result = result.substr(0, result.length() - 2) + ");";
        return result;
    }

    const std::string table_name{};
    const std::array<std::string, sizeof...(ColTypes)> columns{};
};

template <typename... ColTypes>
struct InsertQuery : public InsertQueryGenerator<ColTypes...> {
    struct ExecuteResult {
        std::vector<std::tuple<size_t, sql::result>> failed_entries{};

        [[nodiscard]] inline auto has_succeeded() const { return this->failed_entries.empty(); }
    };

    explicit InsertQuery(const std::string& target_table, Column<ColTypes>... columns) : InsertQueryGenerator<ColTypes...>{target_table, std::move(columns)...} { }

    inline void reserve_entries(size_t amount) {
        this->entries.reserve(amount);
    }

    inline void add_entry(const ColTypes&... values) {
        this->entries.push_back(std::array<variable, sizeof...(ColTypes)>{variable{"", values}...});
    }

    inline ExecuteResult execute(sql::SqlManager* sql, bool ignore_fails = false) {
        if(this->entries.empty()) return ExecuteResult{};

        ExecuteResult result{};

        const auto chunk_size = std::min(2UL, this->entries.size());
        sql::model chunk_base{sql, this->generate_query(chunk_size, sql->getType(), ignore_fails)};
        sql::model entry_model{sql, this->generate_query(1, sql->getType(), ignore_fails)};

        for(size_t chunk{0}; chunk < this->entries.size() / chunk_size; chunk++) {
            auto command = chunk_base.command();
            size_t parameter_index{1};
            for(size_t index{chunk * chunk_size}; index < (chunk + 1) * chunk_size; index++) {
                for(auto& var : this->entries[index]) {
                    var.set_key(":" + BindingNameGenerator::generate(parameter_index++));
                    command.value(var);
                }
            }

            auto exec_result = command.execute();
            if(!exec_result) {
                /* try every entry 'till we've found the error one */
                for(size_t index{chunk * chunk_size}; index < (chunk + 1) * chunk_size; index++) {
                    parameter_index = 1;

                    auto entry_command = entry_model.command();
                    for(auto& var : this->entries[index]) {
                        var.set_key(":" + BindingNameGenerator::generate(parameter_index++));
                        entry_command.value(var);
                    }
                    exec_result = entry_command.execute();
                    if(!exec_result) {
                        result.failed_entries.emplace_back(index, std::move(exec_result));
                        if(!ignore_fails)
                            return result;
                    }
                }
            }
        }

        for(size_t index{(this->entries.size() / chunk_size) * chunk_size}; index < this->entries.size(); index++) {
            size_t parameter_index{1};
            auto entry_command = entry_model.command();
            for(auto& var : this->entries[index]) {
                var.set_key(":" + BindingNameGenerator::generate(parameter_index++));
                entry_command.value(var);
            }
            auto exec_result = entry_command.execute();
            if(!exec_result) {
                result.failed_entries.emplace_back(index, std::move(exec_result));
                if(!ignore_fails)
                    return result;
            }
        }

        return result;
    }

    std::vector<std::array<variable, sizeof...(ColTypes)>> entries{};
};

int main() {
    InsertQuery insert{"hello", Column<int>{"column_a"}, Column<std::string>{"column_ab"}};

    for(int i{0}; i < 100; i++)
        insert.add_entry(i, "X");

    std::cout << "Result: " << insert.generate_query(2) << "\n";

    std::string value{};
    insert.add_entry(2, value);
    insert.execute(nullptr);
}
#endif

struct A {
    A() {}
    ~A() = default;
};

#include <netinet/in.h>
#include <src/misc/net.h>

int main() {
#if 0
    std::set<int> elements{};
    if(!elements.empty()) {
        auto it = elements.begin();
        const int* last_element{&*(it++)}; /* if elements would be empty this would be undefined behaviour */
        while(it != elements.end()) {
            const auto now = *it++;
            const auto diff = last_element - now;
        }
    }
#endif
    sockaddr_in addr{};
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(9987);

    std::cout << "Result: " << (int) net::address_available(*(sockaddr_storage*) &addr, net::binding_type::TCP) << "\n";
    std::cout << strerror(errno) << "\n";
}