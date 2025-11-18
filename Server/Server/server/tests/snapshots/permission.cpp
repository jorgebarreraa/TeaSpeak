//
// Created by WolverinDEV on 11/04/2020.
//

#include <snapshots/permission.h>

using namespace ts::server::snapshots;

void test_write() {
    std::string error{};
    size_t offset{0};
    ts::command_builder result{""};

    permission_writer writer{type::TEAMSPEAK, 0, result, ts::permission::teamspeak::GroupType::GENERAL};
    std::deque<permission_entry> entries{};
    {
        {
            auto& entry = entries.emplace_back();
            entry.type = ts::permission::resolvePermissionData("b_virtualserver_modify_host");
            entry.granted = {2, true};
            entry.value = {4, true};
        }
        {
            auto& entry = entries.emplace_back();
            entry.type = ts::permission::resolvePermissionData("i_icon_id");
            entry.granted = {0, false};
            entry.value = {4, true};
            entry.flag_skip = true;
        }
    }

    if(!writer.write(error, offset, entries)) {
        std::cerr << error << "\n";
        assert(false);
        return;
    }

    std::cout << "Offset: " << offset << ". Command: " << result.build() << "\n";
}

int main() {
    test_write();
}