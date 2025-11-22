//
// Created by WolverinDEV on 08/04/2020.
//

#include "./Properties.h"
#include <cassert>

using namespace ts;

#ifdef EXTERNALIZE_PROPERTY_DEFINITIONS
using PropertyDescription = property::PropertyDescription;
#include "./PropertyDefinition.h"
const property::PropertyListInfo property::property_list_info = impl::list_info();
#endif

using PropertyType = property::PropertyType;
using PropertyType = property::PropertyType;

/* verifier methods */
constexpr bool validate_property_list_contains_all_types() {
    for(const auto& entry : property::property_list_info.begin_index)
        if(entry >= property::property_list.size())
            return false;
    return true;
}

constexpr bool validate_all_names_valid() {
    for(const auto& entry : property::property_list)
        for(const auto& c : entry.name)
            if(!((c >= 'a' && c <= 'z') || c == '_' || (c >= '0' && c <= '9')))
                return false;
    return true;
}

constexpr bool validate_first_property_undefined() {
    for(const auto& index : property::property_list_info.begin_index)
        if(index < property::property_list.size())
            if(property::property_list[index].name != "undefined")
                return false;
    return true;
}

constexpr bool property_list_ordered() {
    static_assert(!property::property_list.empty());

    /* initial setup */
    std::array<bool, PropertyType::PROP_TYPE_MAX> visited_types{};
    PropertyType current_type{property::property_list[0].type_property};
    visited_types[current_type] = true;
    size_t last_id{(size_t) property::property_list[0].property_index};
    if(last_id != 0) return false;

    /* iterate */
    for(auto index{1}; index < property::property_list.size(); index++) {
        const auto& entry = property::property_list[index];
        if(last_id + 1 != entry.property_index) {
            if(current_type == entry.type_property)
                return false;
            if(visited_types[entry.type_property])
                return false;
            if(entry.property_index != 0) /* new type must start with 0 (undefined) */
                return false;
            visited_types[current_type = entry.type_property] = true;
            last_id = 0;
        } else if(current_type != entry.type_property)
            return false;
        else
            last_id++;
    }
    return true;
}

#ifdef EXTERNALIZE_PROPERTY_DEFINITIONS
__attribute__((constructor)) void validate_properties() {
    assert(property_list_ordered());
    assert(validate_first_property_undefined());
    assert(validate_property_list_contains_all_types());
    assert(validate_all_names_valid());
}
#else
static_assert(property_list_ordered(), "Property list is unordered!");
static_assert(validate_first_property_undefined(), "First property of each type must be the undefined property");
static_assert(validate_property_list_contains_all_types(), "Missing property begin for a property type");
static_assert(validate_all_names_valid(), "Property list contains name which does not match the expected names: [a-z_]+");
#endif