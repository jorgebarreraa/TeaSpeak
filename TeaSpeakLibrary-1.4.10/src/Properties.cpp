#include <algorithm>
#include <array>
#include <utility>
#include "misc/memtracker.h"
#include "Properties.h"

using namespace ts;
using namespace ts::property;
using namespace std;

PropertyManager::PropertyManager()    {
    memtrack::allocated<PropertyManager>(this);
}
PropertyManager::~PropertyManager()   {
    memtrack::freed<PropertyManager>(this);
}

bool PropertyManager::has(property::PropertyType type, int index) {
    for(auto it = this->properties.begin(); it != this->properties.end(); it++) {
        auto& bundle = *it;
        if(bundle->type != type) {
            continue;
        }

        return index < bundle->property_count;
    }
    return false;
}

Property PropertyManager::get(property::PropertyType type, int index) {
    for (auto &bulk : this->properties) {
        if(bulk->type != type) {
            continue;
        }

        if(index >= bulk->property_count) {
            break;
        }

        return Property{this->weak_from_this().lock(), &bulk->properties[index], bulk};
    }

    throw std::invalid_argument("missing property type");
}

std::vector<Property> PropertyManager::list_properties(ts::property::flag_type flagMask, ts::property::flag_type negatedFlagMask) {
    std::vector<Property> result{};
    result.reserve(this->properties_count);

    auto self_ref = this->weak_from_this().lock();
    for (auto &bulk : this->properties) {
        for(int index = 0; index < bulk->property_count; index++) {
            auto& property = bulk->properties[index];
            if((property.description->flags & flagMask) > 0 && (property.description->flags & negatedFlagMask) == 0) {
                result.emplace_back(self_ref, &property, bulk);
            }
        }
    }
    return result;
}

std::vector<Property> PropertyManager::all_properties() {
    std::vector<Property> result{};
    result.reserve(this->properties_count);

    auto self_ref = this->weak_from_this().lock();
    for (auto &bulk : this->properties) {
        for(int index = 0; index < bulk->property_count; index++) {
            result.emplace_back(self_ref, &bulk->properties[index], bulk);
        }
    }
    return result;
}

Property::Property(std::shared_ptr<PropertyManager> handle, ts::PropertyData *data, std::shared_ptr<ts::PropertyBundle> bundle_lock)
    : handle{std::move(handle)}, property_data{data}, bundle_lock{std::move(bundle_lock)} {
}

void Property::trigger_update() {
    this->property_data->flag_modified = true;

    if(this->handle) {
        for(const auto& elm : this->handle->notifyFunctions)
            elm(*this);
    }
}

void PropertyManager::do_register_property_type(ts::property::PropertyType type, size_t length) {
    for(auto& bulk : this->properties) {
        if(bulk->type == type) {
            return;
        }
    }

    const auto alloc_length = sizeof(PropertyBundle) + sizeof(PropertyData) * length;
    auto ptr = shared_ptr<PropertyBundle>((PropertyBundle*) malloc(alloc_length), [](PropertyBundle* bundle) {
        if(!bundle) {
            return;
        }

        for(int index = 0; index < bundle->property_count; index++) {
            auto& property = bundle->properties[index];
            property.value.~string();
            property.casted_value.~any();
        }

        bundle->value_mutex.~mutex();
        ::free(bundle);
    });

    new (&ptr->value_mutex) std::mutex{};
    ptr->type = type;
    ptr->property_count = length;

    for(int index = 0; index < length; index++) {
        auto& property = ptr->properties[index];

        new (&property.casted_value) any{};
        new (&property.value) string{};
        property.description = &property::describe(type, index);
        property.flag_modified = false;
        property.flag_database_reference = false;

        property.value = property.description->default_value;
        this->properties_count++;
    }

    this->properties.push_back(ptr);
    return;
}

namespace ts {
    namespace property {
        namespace impl {
            bool validateInput(const std::string& input, ValueType type) {
                if(type == ValueType::TYPE_UNKNOWN) return true;
                else if(type == ValueType::TYPE_UNSIGNED_NUMBER) {
                    if(input.empty() || input.find_first_not_of("0123456789") != string::npos) return false;
                    try {
                        stoull(input);
                        return true;
                    } catch (std::exception& /* ex */) { return false; }
                }
                else if(type == ValueType::TYPE_SIGNED_NUMBER) {
                    if(input.empty() || input.find_first_not_of("-0123456789") != string::npos) return false;
                    auto minus = input.find_first_of('-');
                    auto flag_result = minus == string::npos || (minus == 0 && input.find_first_of('-', minus + 1) == string::npos);
                    if(!flag_result) return false;

                    try {
                        stoll(input);
                        return true;
                    } catch (std::exception& /* ex */) { return false; }
                } else if(type == ValueType::TYPE_BOOL) return input.length() == 1 && input.find_first_not_of("01") == string::npos;
                else if(type == ValueType::TYPE_STRING) return true;
                else if(type == ValueType::TYPE_FLOAT) {
                    if(input.empty() || input.find_first_not_of("-.0123456789") != string::npos) return false;
                    auto minus = input.find_first_of('-');
                    if(minus != string::npos && (input.find_first_of('-', minus + 1) != string::npos || minus != 0)) return false;
                    auto point = input.find_first_of('.');
                    auto flag_result = point == string::npos || input.find_first_of('.', point + 1) == string::npos;
                    if(!flag_result) return false;

                    try {
                        stof(input);
                        return true;
                    } catch (std::exception& /* ex */) { return false; }
                }
                return false;
            }
        }
    }
}