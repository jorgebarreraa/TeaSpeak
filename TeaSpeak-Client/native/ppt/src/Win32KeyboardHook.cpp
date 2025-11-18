//
// Created by WolverinDEV on 01/05/2020.
//

#include "./Win32KeyboardHook.h"

namespace hooks {
    std::string key_string_from_vk(DWORD code, bool extended) {
        auto scan_code = MapVirtualKey(code, MAPVK_VK_TO_VSC);
        if(extended)
            scan_code |= KF_EXTENDED;

        char key_buffer[255];
        auto length = GetKeyNameTextA(scan_code << 16, key_buffer, 255);
        if(length == 0)
            return "error";
        else
            return std::string{key_buffer, (size_t) length};
    }

    std::string key_string_from_sc(USHORT code) {
        char key_buffer[255];
        auto length = GetKeyNameTextA(code << 16, key_buffer, 255);
        if(length == 0)
            return "error";
        else
            return std::string{key_buffer, (size_t) length};
    }

    //https://docs.microsoft.com/en-us/windows/desktop/inputdev/virtual-key-codes
    KeyboardHook::KeyType key_type_from_vk(DWORD vk_code) {
        using KeyType = KeyboardHook::KeyType;

        switch(vk_code) {
            case VK_CONTROL:
            case VK_LCONTROL:
            case VK_RCONTROL:
                return KeyType::KEY_CTRL;
            case VK_MENU:
            case VK_RMENU:
            case VK_LMENU:
                return KeyType::KEY_ALT;
            case VK_SHIFT:
            case VK_RSHIFT:
            case VK_LSHIFT:
                return KeyType::KEY_SHIFT;
            case VK_LWIN:
            case VK_RWIN:
                return KeyType::KEY_WIN;
            default:
                return KeyType::KEY_NORMAL;
        }
    }
}