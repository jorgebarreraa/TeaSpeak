//
// Created by wolverindev on 15.07.19.
//

#include "PermissionManager.h"
#include <iostream>

using namespace std;
using namespace ts::permission::v2;
using PermissionType = ts::permission::PermissionType;

void print_permissions(PermissionManager& manager) {
    {
        auto permissions = manager.permissions();
        cout << "Permissions: " << permissions.size() << endl;
        for(const auto& permission : permissions) {
            cout << " - " << ts::permission::resolvePermissionData(std::get<0>(permission))->name + ": ";
            cout << (std::get<1>(permission).flags.value_set ? to_string(std::get<1>(permission).values.value) : "no value") << " negate: " << std::get<1>(permission).flags.negate << " skip: " << std::get<1>(permission).flags.skip << " ";
            cout << "chan permission: " << std::get<1>(permission).flags.channel_specific << endl;
        }
    }
    cout << "Used memory: " << manager.used_memory() << endl;
}

void print_updates(PermissionManager& manager) {
    const auto updates = manager.flush_db_updates();
    cout << "Permission updates: " << updates.size() << endl;
    for(auto& update : updates) {
        cout << "Permission: " << ts::permission::resolvePermissionData(update.permission)->name << "; Channel: " << update.channel_id << "; DB Ref: " << update.flag_db << endl;
        cout << "  value: " << (update.update_value == PermissionUpdateType::do_nothing ? "do nothing" : update.update_value == PermissionUpdateType::set_value ? "set value to " + to_string(update.values.value) : "delete") << endl;
        cout << "  grant: " << (update.update_grant == PermissionUpdateType::do_nothing ? "do nothing" : update.update_grant == PermissionUpdateType::set_value ? "set value to " + to_string(update.values.grant) : "delete") << endl;
    }
}

int main() {
    ts::permission::setup_permission_resolve();
    /*
     *
Structure size of PermissionManager: 176
Structure size of PermissionContainerBulk<16>: 192
Structure size of PermissionContainer: 12
     */
    cout << "Structure size of PermissionManager: " << sizeof(PermissionManager) << endl;
    cout << "Structure size of PermissionContainerBulk<16>: " << sizeof(PermissionContainerBulk<16>) << endl;
    cout << "Structure size of PermissionContainer: " << sizeof(PermissionContainer) << endl;
    cout << "Permissions/bulk: " << PermissionManager::PERMISSIONS_BULK_ENTRY_COUNT << ". Bulks: " << PermissionManager::BULK_COUNT << " (Max permissions: " << (PermissionManager::PERMISSIONS_BULK_ENTRY_COUNT * PermissionManager::BULK_COUNT) << "; Avl: " << (uint32_t) PermissionType::permission_id_max << ")" << endl;

    PermissionManager manager{};
    print_permissions(manager);
    manager.set_permission(PermissionType::b_client_ban_ip, {1, 0}, PermissionUpdateType::set_value, PermissionUpdateType::do_nothing);
    manager.set_channel_permission(PermissionType::b_client_ban_ip, 2, {1, 0}, PermissionUpdateType::set_value, PermissionUpdateType::do_nothing);
    manager.set_channel_permission(PermissionType::b_client_ban_ip, 2, {1, 0}, PermissionUpdateType::delete_value, PermissionUpdateType::do_nothing);
    print_updates(manager);
    //manager.set_permission(PermissionType::b_client_ban_ip, {1, 0}, PermissionUpdateType::delete_value, PermissionUpdateType::do_nothing);
    //manager.cleanup();
    print_permissions(manager);
    return 0;
}