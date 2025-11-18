#pragma once

#include <memory>
#include <pipes/ssl.h>

namespace ts::server::file::config {
    enum struct Key {
        SSL_OPTION_SUPPLIER
    };

    extern void value_updated(Key /* value */);
    extern std::function<std::shared_ptr<pipes::SSL::Options>()> ssl_option_supplier;
}