#pragma once

namespace ts {
    class command_exception : public std::exception {};

    class command_casted_exception : public command_exception { };
    class command_cannot_uncast_exception : public command_exception { };
    class command_bulk_exceed_index_exception : public command_exception { };
    class command_value_missing_exception : public command_exception {
        public:
            command_value_missing_exception(size_t index, std::string key) : _index(index), _key(move(key)) { }

            inline size_t index() const { return this->_index; }
            inline std::string key() const { return this->_key; }
        private:
            size_t _index;
            std::string _key;
    };
    class command_malformed_exception : public command_exception {
        public:
            command_malformed_exception(size_t index) : _index(index) {}
            inline size_t index() const { return this->_index; }
        private:
            size_t _index;
    };
}