//
// Created by WolverinDEV on 22/05/2020.
//

#include <pipes/misc/http.h>
#include "HTTPUtils.h"

bool http::parse_url_parameters(const std::string_view &query, std::map<std::string, std::string>& result) {
    const auto query_offset = query.find('?');
    if(query_offset == std::string::npos) return false;

    const auto query_end_offset = query.find('#', query_offset); /* fragment (if there is any) */

    auto offset = query_offset + 1;
    size_t next_param;
    while(offset > 0) {
        next_param = query.find('&', offset);
        if(next_param >= query_end_offset)
            next_param = query_end_offset;

        if(offset >= next_param)
            break;

        /* parameter: [offset;next_param) */
        const auto param_view = query.substr(offset, next_param - offset);
        const auto assignment_index = param_view.find('=');
        if(assignment_index == std::string::npos)
            result[std::string{param_view}] = "";
        else
            result[std::string{param_view.substr(0, assignment_index)}] = http::decode_url(std::string{param_view.substr(assignment_index + 1)});

        offset = next_param + 1;
    }
    return true;
}