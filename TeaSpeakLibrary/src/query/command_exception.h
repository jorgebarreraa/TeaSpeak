#pragma once

#include <utility>

namespace ts {
    class command_exception : public std::exception {};

    class command_bulk_index_out_of_bounds_exception : public command_exception {
        public:
            explicit command_bulk_index_out_of_bounds_exception(size_t index) : _index(index) { }

            [[nodiscard]] inline size_t index() const { return this->_index; }
        private:
            size_t _index;
    };

    class command_value_missing_exception : public command_exception {
        public:
            command_value_missing_exception(size_t index, std::string key) : _index(index), _key(move(key)) { }

            [[nodiscard]] inline size_t bulk_index() const { return this->_index; }
            [[nodiscard]] inline std::string key() const { return this->_key; }
        private:
            size_t _index;
            std::string _key;
    };

    class command_malformed_exception : public command_exception {
        public:
            explicit command_malformed_exception(size_t index) : _index(index) {}
            [[nodiscard]] inline size_t index() const { return this->_index; }
        private:
            size_t _index;
    };

    class command_value_cast_failed : public command_exception {
        public:
            command_value_cast_failed(size_t index, std::string key, std::string value, const std::type_info& target_type)
                : index_{index}, key_{std::move(key)}, value_{std::move(value)}, target_type_{target_type} {}

            [[nodiscard]] inline size_t index() const { return this->index_; }
            [[nodiscard]] inline const std::string& key() const { return this->key_; }
            [[nodiscard]] inline const std::string& value() const { return this->value_; }
            [[nodiscard]] inline const std::type_info& target_type() const { return this->target_type_; }
        private:
            size_t index_;
            std::string key_;
            std::string value_;
            const std::type_info& target_type_;
    };
}