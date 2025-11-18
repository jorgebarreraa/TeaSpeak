//
// Created by WolverinDEV on 21/05/2020.
//

#include "files/Config.h"

using namespace ts::server::file;

std::function<std::shared_ptr<pipes::SSL::Options>()> config::ssl_option_supplier{nullptr};

void config::value_updated(ts::server::file::config::Key) {
    /* we currently do nothing */
}