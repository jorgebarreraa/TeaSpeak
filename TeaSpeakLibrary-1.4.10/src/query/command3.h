#pragma once

#include <string>
#include <cstddef>
#include <vector>
#include <deque>
#include <optional>
#include <string_view>

#include "escape.h"
#include "converters/converter.h"
#include "command_exception.h"

namespace ts {
    namespace impl {
        inline bool value_raw_impl(const std::string_view& data, const std::string_view& key, size_t& begin, size_t* end) {
            size_t index{0}, findex, max{data.size()}, key_size{key.size()};
            do {
                findex = data.find(key, index);
                if(findex > max) return false;

                /* not starting with a space so not a match */
                if(findex != 0 && data[findex - 1] != ' ') {
                    index = findex + key_size;
                    continue;
                }

                findex += key_size;
                if(findex < max) {
                    if(data[findex] == '=')
                        begin = findex + 1;
                    else if(data[findex] == ' ')
                        begin = findex; /* empty value */
                    else {
                        index = findex + key_size;
                        continue;
                    }

                    if(end) *end = data.find(' ', findex);
                    return true;
                } else {
                    begin = max;
                    if(end) *end = max;
                    return true;
                }
            } while(true);
        }

        class command_string_parser {
            public:
                [[nodiscard]] inline bool is_empty() const noexcept { return this->data.empty(); }

                [[nodiscard]] inline bool has_key(const std::string_view& key) const {
                    size_t begin{0};
                    return value_raw_impl(this->data, key, begin, nullptr);
                }

                [[nodiscard]] inline std::string_view value_raw(const std::string_view& key) const {
                    bool tmp;
                    auto result = this->value_raw(key, tmp);
                    if(!tmp) throw command_value_missing_exception{this->bulk_index, std::string{key}};
                    return result;
                }

                [[nodiscard]] inline std::string_view value_raw(const std::string_view& key, bool& has_been_found) const noexcept {
                    size_t begin{0}, end;
                    has_been_found = value_raw_impl(this->data, key, begin, &end);
                    if(!has_been_found) return {};

                    return this->data.substr(begin, end - begin);
                }

                [[nodiscard]] inline std::string value(const std::string_view& key) const {
                    const auto value = this->value_raw(key);
                    if(value.empty()) return std::string{};

                    return query::unescape(std::string{value}, false);
                }

                [[nodiscard]] inline std::string value(const std::string_view& key, bool& has_been_found) const noexcept {
                    const auto value = this->value_raw(key, has_been_found);
                    if(value.empty()) return std::string{};

                    return query::unescape(std::string{value}, false);
                }

                template <typename T>
                [[nodiscard]] inline T value_as(const std::string_view& key) const {
                    static_assert(converter<T>::supported, "Target type isn't supported!");
                    static_assert(!converter<T>::supported || converter<T>::from_string_view, "Target type dosn't support parsing");

                    auto value = this->value(key);
                    try {
                        return converter<T>::from_string_view(value);
                    } catch (std::exception& ex) {
                        throw command_value_cast_failed{this->key_command_character_index(key), std::string{key}, value, typeid(T)};
                    }
                }

                template <typename T>
                inline void expect_value_as(const std::string_view& key) const {
                    static_assert(converter<T>::supported, "Target type isn't supported!");
                    static_assert(!converter<T>::supported || converter<T>::from_string_view, "Target type dosn't support parsing");

                    auto value = this->value(key);
                    try {
                        converter<T>::from_string_view(value);
                    } catch (std::exception& ex) {
                        throw command_value_cast_failed{this->key_command_character_index(key), std::string{key}, value, typeid(T)};
                    }
                }

                [[nodiscard]] inline size_t command_character_index() const { return this->abs_index; }
                [[nodiscard]] inline size_t key_command_character_index(const std::string_view& key) const {
                    size_t begin{0};
                    if(!value_raw_impl(this->data, key, begin, nullptr)) return this->abs_index;
                    return this->abs_index + begin;
                }
            protected:
                command_string_parser(size_t bulk_index, size_t abs_index, std::string_view data) : bulk_index{bulk_index}, abs_index{abs_index}, data{data} {}

                size_t abs_index{};
                size_t bulk_index{};
                std::string_view data{};
        };
    }

    struct command_bulk : public impl::command_string_parser {
        command_bulk(size_t index, size_t abs_index, std::string_view data) : command_string_parser{index, abs_index, data} {}

        inline bool next_entry(size_t& index, std::string_view& key, std::string& value) const {
            auto next_key = this->data.find_first_not_of(' ', index);
            if(next_key == std::string::npos) return false;

            auto key_end = this->data.find_first_of(" =", next_key);
            if(key_end == std::string::npos || this->data[key_end] == ' ') {
                key = this->data.substr(next_key, key_end - next_key);
                value.clear();
                index = key_end;
            } else {
                key = this->data.substr(next_key, key_end - next_key);
                auto value_end = this->data.find_first_of(' ', key_end + 1);

                if(value_end == std::string::npos) {
                    value = query::unescape(value = this->data.substr(key_end + 1), false);
                    index = value_end;
                } else {
                    value = query::unescape(value = this->data.substr(key_end + 1, value_end - key_end - 1), false);
                    index = value_end + 1;
                }
            }
            return true;
        }
    };

    class command_parser : public impl::command_string_parser {
        public:
            enum struct parse_status {
                succeeded,
                failed
            };

            explicit command_parser(std::string_view command) : impl::command_string_parser{std::string::npos, 0, command} { }
            explicit command_parser(std::string command) : impl::command_string_parser{std::string::npos, 0, command}, _command_memory{std::move(command)} { }

            bool parse(bool /* contains identifier */);

            [[nodiscard]] inline const std::string_view& identifier() const noexcept { return this->command_type; }
            [[nodiscard]] inline size_t bulk_count() const noexcept { return this->_bulks.size(); }
            [[nodiscard]] inline const command_bulk& bulk(size_t index) const {
                if(this->_bulks.size() <= index)
                    throw command_bulk_index_out_of_bounds_exception{index};

                return this->_bulks[index];
            }

            const command_bulk& operator[](size_t index) const { return this->bulk(index); }

            [[nodiscard]] inline bool has_switch(const std::string_view& key) const noexcept {
                size_t index{0};
                do {
                    index = this->data.find(key, index);
                    index--;

                    if(index > this->data.length())
                        return false; /* index was zero or std::string::npos*/

                    if(this->data[index] != '-') {
                        index += 2;
                        continue;
                    }

                    if(index != 0 && this->data[index - 1] != ' ') {
                        index += 2;
                        continue;
                    }

                    if(index + 1 + key.length() >= this->data.length())
                        return true;

                    if(this->data[index + 1 + key.length()] == ' ')
                        return true;

                    index += 2;
                } while(true);
            }

            [[nodiscard]] const std::deque<command_bulk>& bulks() const { return this->_bulks; }

            [[nodiscard]]  std::string_view payload_view(size_t bulk_index) const noexcept;
            [[nodiscard]] std::optional<size_t> next_bulk_containing(const std::string_view& /* key */, size_t /* bulk offset */) const;
        private:
            const std::string _command_memory{};
            std::string_view command_type{};
            std::deque<command_bulk> _bulks{};
    };

    class command_builder_bulk {
            template <typename vector_t>
            friend class command_builder_impl;
        public:
            inline void reserve(size_t length, bool accumulative = true) {
                this->bulk->reserve(length + (accumulative ? this->bulk->size() : 0));
            }

            inline void reset() {
                this->bulk->clear();
            }

            inline void put(const std::string_view& key, const std::string_view& value) {
                size_t begin, end;
                if(impl::value_raw_impl(*this->bulk, key, begin, &end)) {
                    std::string result{};
                    result.reserve(this->bulk->size());

                    result.append(*this->bulk, 0, begin - key.size() - 1); /* key incl = */
                    result.append(*this->bulk, end + 1); /* get rid of the space */
                    *this->bulk = result;
                }
                this->impl_put_unchecked(key, value);
            }

            template <typename T, std::enable_if_t<!(std::is_same<T, std::string_view>::value || std::is_same<T, std::string>::value), int> = 1>
            inline void put(const std::string_view& key, const T& value) {
                static_assert(converter<T>::supported, "Target type isn't supported!");
                static_assert(!converter<T>::supported || converter<T>::to_string, "Target type dosn't support building");
                auto data = converter<T>::to_string(value);
                this->put(key, std::string_view{data});
            }

#ifdef PROPERTIES_DEFINED
            template <typename PropertyType, typename T, std::enable_if_t<std::is_enum<PropertyType>::value, int> = 0>
            inline void put(PropertyType key, const T& value) {
                this->put(property::name(key), value);
            }
#endif

            /* directly puts data without checking for duplicates */
            inline void put_unchecked(const std::string_view& key, const std::string_view& value) {
                this->impl_put_unchecked(key, value);
            }

            inline void put_unchecked(const std::string_view& key, const std::string& value) {
                this->put_unchecked(key, std::string_view{value});
            }

#ifdef PROPERTIES_DEFINED
            template <typename PropertyType, typename T, std::enable_if_t<std::is_enum<PropertyType>::value, int> = 0>
            inline void put_unchecked(PropertyType key, const T& value) {
                this->put_unchecked(property::name(key), value);
            }
#endif

            template <typename T, std::enable_if_t<!(std::is_same<T, std::string_view>::value || std::is_same<T, std::string>::value), int> = 1>
            inline void put_unchecked(const std::string_view& key, const T& value) {
                static_assert(converter<T>::supported, "Target type isn't supported!");
                static_assert(!converter<T>::supported || converter<T>::to_string, "Target type dosn't support building");
                auto data = converter<T>::to_string(value);
                this->put_unchecked(key, std::string_view{data});
            }

        protected:
            explicit command_builder_bulk(bool& change_flag, std::string& bulk) : flag_changed{&change_flag}, bulk{&bulk} {}

        private:
            bool* flag_changed;
            std::string* bulk;

            void impl_put_unchecked(const std::string_view& key, const std::string_view& value) {
                auto escaped_value = ts::query::escape(std::string{value});

                this->bulk->reserve(this->bulk->length() + key.size() + escaped_value.size() + 2);
                this->bulk->append(key);
                if(!escaped_value.empty()) {
                    this->bulk->append("=");
                    this->bulk->append(escaped_value);
                }
                this->bulk->append(" ");
                *this->flag_changed = true;
            }
    };

    class standalone_command_builder_bulk : public command_builder_bulk {
        public:
            explicit standalone_command_builder_bulk(size_t expected_length = 0) : command_builder_bulk{this->flag_changed_, this->buffer_} {
                if(expected_length > 0)
                    this->buffer_.reserve(expected_length);
            }

            [[nodiscard]] inline const std::string& buffer() const { return this->buffer_; }
            [[nodiscard]] inline std::string& buffer() { return this->buffer_; }
        private:
            bool flag_changed_{};
            std::string buffer_{};
    };

    template <typename vector_t = std::vector<std::string>>
    class command_builder_impl {
        public:
            explicit command_builder_impl(std::string command, size_t expected_bulk_size = 128, size_t expected_bulks = 1) : identifier_{std::move(command)}, expected_bulk_size{expected_bulk_size} {
                for(size_t index = 0; index < expected_bulks; index++)
                    this->bulks.emplace_back("").reserve(expected_bulk_size);
            }

            inline command_builder_impl<std::vector<std::string>> as_normalized() {
                return command_builder_impl<std::vector<std::string>>{this->expected_bulk_size, this->identifier_, this->bulks.begin(), this->bulks.end()};
            }

            inline std::string build(bool with_empty = false) const {
                if(this->builded.has_value() && !this->flag_changed) {
                    return this->builded.value();
                }

                std::string result{};
                size_t required_size{this->identifier_.size()};
                for(auto& entry : this->bulks) {
                    required_size += entry.size() + 1;
                }

                if(!this->identifier_.empty()) {
                    result.append(this->identifier_);
                    result.push_back(' ');
                }

                for(auto it = this->bulks.begin(); it != this->bulks.end(); it++) {
                    if(it->empty() && !with_empty) {
                        continue;
                    }

                    result.append(*it, 0, it->length() - 1);
                    if(it + 1 != this->bulks.end()) {
                        result.append("|");
                    }
                }

                if(!with_empty && !result.empty() && result.back() == '|') {
                    this->builded = result.substr(0, result.length() - 1);
                } else {
                    this->builded = result;
                }

                this->flag_changed = false;
                return this->builded.value();
            }

            inline void reserve_bulks(size_t count) { this->bulks.reserve(count); }

            [[nodiscard]] inline command_builder_bulk bulk(size_t index) {
                while(this->bulks.size() <= index) {
                    this->bulks.emplace_back("").reserve(expected_bulk_size);
                }

                return command_builder_bulk{this->flag_changed, this->bulks[index]};
            }

            template <typename KeyT, typename ValueT>
            inline void put(size_t index, const KeyT& key, const ValueT& value) {
                this->bulk(index).put(key, value);
            }

            /* directly puts data without checking for duplicates */
            template <typename KeyT, typename ValueT>
            inline void put_unchecked(size_t index,  const KeyT& key, const ValueT& value) {
                this->bulk(index).put_unchecked(key, value);
            }

            [[nodiscard]] inline size_t current_size() const {
                if(this->bulks.empty()) return 0;

                size_t result{0};
                for(const auto& entry : this->bulks)
                    result += entry.length() + 2;
                return result;
            }

            inline void push_bulk(standalone_command_builder_bulk&& bulk) {
                this->bulks.push_back(std::move(bulk.buffer()));
                this->flag_changed = true;
            }

            inline void set_bulk(size_t index, standalone_command_builder_bulk&& bulk) {
                while(this->bulks.size() <= index) {
                    this->bulks.emplace_back("").reserve(expected_bulk_size);
                }
                this->bulks[index] = std::move(bulk.buffer());
                this->flag_changed = true;
            }

            inline void reset() {
                for(auto& bulk : this->bulks)
                    bulk = " ";
            }
        private:
            command_builder_impl(size_t expected, std::string identifier, typename vector_t::iterator begin, typename vector_t::iterator end) : expected_bulk_size{expected}, identifier_{std::move(identifier)}, bulks{begin, end} {}

            const size_t expected_bulk_size;
            const std::string identifier_;
            mutable bool flag_changed{false};
            mutable std::optional<std::string> builded{};
            vector_t bulks{};
    };

    using command_builder = command_builder_impl<>;
}

#define COMMAND_BUILDER_DEFINED