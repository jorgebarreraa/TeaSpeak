//
// Created by WolverinDEV on 28/01/2021.
//

#include "./RawCommand.h"

using namespace ts::command;

ReassembledCommand *ReassembledCommand::allocate(size_t size) {
    auto instance = (ReassembledCommand*) malloc(sizeof(ReassembledCommand) + size);
    instance->length_ = size;
    instance->capacity_ = size;
    instance->next_command = nullptr;
    return instance;
}

void ReassembledCommand::free(ReassembledCommand *command) {
    ::free(command);
}