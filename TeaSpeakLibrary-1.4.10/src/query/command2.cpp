#include "escape.h"
#include <sstream>
#include <algorithm>

using namespace std;
using namespace ts;

/*
std::string command::build(ts::command::format::value type) {
    if(type == format::QUERY || type == format::BRACE_ESCAPED_QUERY) {
        std::stringstream ss;

        if(!this->handle->command.empty())
            ss << this->handle->command << " ";

        if(!this->handle->bulks.empty()) {
            auto max_bulk_index = this->handle->bulks.size() - 1;
            for(size_t bulk_index = 0; bulk_index <= max_bulk_index; bulk_index++) {
                auto& bulk = this->handle->bulks[bulk_index];
                if(bulk->values.empty()) continue; //Do not remove me!

                auto max_pair_index = bulk->values.size() - 1;
                auto pair_it = bulk->values.begin();

                for(size_t pair_index = 0; pair_index <= max_pair_index; pair_index++) {
                    auto pair = *(pair_it++);

                    auto value = pair.second->casted ? pair.second->to_string(pair.second->value) : pair.second->value.has_value() ? any_cast<std::string>(pair.second->value) : "";
                    ss << pair.first << "=";
                    if(type == format::BRACE_ESCAPED_QUERY) {
                        ss << "\"" << value << "\"";
                    } else ss << query::escape(value);

                    if(pair_index != max_pair_index)
                        ss << " ";
                }

                if(bulk_index != max_bulk_index)
                    ss << " | ";
            }
        }

        if(!this->handle->triggers.empty()) {
            auto max_trigger_index = this->handle->triggers.size() - 1;
            for(size_t trigger_index = 0; trigger_index <= max_trigger_index; trigger_index++) {
                ss << " -" << this->handle->triggers[trigger_index];
            }
        }

        return ss.str();
    }
    return "";
}
 */
