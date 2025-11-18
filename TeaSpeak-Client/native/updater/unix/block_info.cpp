//
// Created by WolverinDEV on 13/06/2020.
//

#include "../ui.h"

#if 0
FILE *fp = fopen("results.txt", "w");
if (fp == NULL) {
    if (errno == EACCES)
        cerr << "Permission denied" << endl;
    else
        cerr << "Something went wrong: " << strerror(errno) << endl;
}
#endif

ui::FileBlockedResult ui::open_file_blocked(const std::string &path) {
    return ui::FileBlockedResult::NOT_IMPLEMENTED;
}