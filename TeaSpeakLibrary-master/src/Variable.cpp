//
// Created by wolverindev on 22.11.17.
//

#include "Variable.h"

variable_data::variable_data(const std::pair<std::string, std::string> &pair, VariableType _type) : pair(pair), _type(_type) {}

variable& variable::operator=(const variable &ref) {
    this->data = ref.data;
    return *this;
}

variable& variable::operator=(variable &&ref) {
    this->data = ref.data;
    return *this;
}