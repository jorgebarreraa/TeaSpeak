#include <algorithm>
#include <mutex>
#include <array>
#include <utility>
#include "log/LogUtils.h"
#include "misc/memtracker.h"
#include "Properties.h"

using namespace ts;
using namespace ts::property;
using namespace std;

Properties::Properties()    {
    memtrack::allocated<Properties>(this);
}
Properties::~Properties()   {
    memtrack::freed<Properties>(this);
}

bool Properties::has(property::PropertyType type, int index) {
    for(auto it = this->properties.begin(); it != this->properties.end(); it++) {
        if(!*it) continue;
        if(it->get()->type != type) continue;

        return index < it->get()->length;
    }
    return false;
}

PropertyWrapper Properties::find(property::PropertyType type, int index) {
    for (auto &bulk : this->properties) {
        if(!bulk) continue;
        if(bulk->type != type)
            continue;

        if(index >= bulk->length)
            break;

        return PropertyWrapper{this, &bulk->properties[index], bulk};
    }

    throw std::invalid_argument("missing property type");
}

std::vector<PropertyWrapper> Properties::list_properties(ts::property::flag_type flagMask, ts::property::flag_type negatedFlagMask) {
    vector<PropertyWrapper> result;
    result.reserve(this->properties_count);

    for (auto &bulk : this->properties) {
        for(int index = 0; index < bulk->length; index++) {
            auto& property = bulk->properties[index];
            if((property.description->flags & flagMask) > 0 && (property.description->flags & negatedFlagMask) == 0)
                result.emplace_back(this, &property, bulk);
        }
    }
    return result;
}

std::vector<PropertyWrapper> Properties::all_properties() {
    vector<PropertyWrapper> result;
    result.reserve(this->properties_count);

    for (auto &bulk : this->properties) {
        for(int index = 0; index < bulk->length; index++)
            result.emplace_back(this, &bulk->properties[index], bulk);
    }
    return result;
}

PropertyWrapper::PropertyWrapper(ts::Properties* handle, ts::PropertyData *data, std::shared_ptr<ts::PropertyBundle> bundle_lock) : handle{handle}, data_ptr{data}, bundle_lock{std::move(bundle_lock)} {
}

void PropertyWrapper::trigger_update() {
    this->data_ptr->flag_modified = true;

    if(this->handle) {
        for(const auto& elm : this->handle->notifyFunctions)
            elm(*this);
    }
}

bool Properties::register_property_type(ts::property::PropertyType type, size_t length) {
    for(auto& bulk : this->properties)
        if(bulk->type == type)
            return false;

    const auto alloc_length = sizeof(PropertyBundle) + sizeof(PropertyData) * length;
    auto ptr = shared_ptr<PropertyBundle>((PropertyBundle*) malloc(alloc_length), [](PropertyBundle* bundle) {
        if(!bundle) return;

        for(int index = 0; index < bundle->length; index++) {
            auto& property = bundle->properties[index];
            property.value.~string();
            property.value_lock.~spin_lock();
            property.casted_value.~any();
        }
        ::free(bundle);
    });

    ptr->type = type;
    ptr->length = length;

    for(int index = 0; index < length; index++) {
        auto& property = ptr->properties[index];

        new (&property.casted_value) any();
        new (&property.value_lock) spin_lock();
        new (&property.value) string();
        property.description = &property::describe(type, index);
        property.flag_modified = false;
        property.flag_db_reference = false;

        property.value = property.description->default_value;
        this->properties_count++;
    }

    this->properties.push_back(ptr);
    return false;
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