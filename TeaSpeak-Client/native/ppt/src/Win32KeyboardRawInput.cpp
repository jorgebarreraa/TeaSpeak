//
// Created by WolverinDEV on 01/05/2020.
//

#include "./Win32KeyboardHook.h"
#include <iostream>
#include <WinUser.h>

namespace hooks {
    Win32RawHook::Win32RawHook() : KeyboardHook{KeyboardHookType::RAW_INPUT} {}
    Win32RawHook::~Win32RawHook() noexcept {
        this->do_detach();
    }

    bool Win32RawHook::attach() {
        if(!KeyboardHook::attach())
            return false;

        this->wactive = true;
        this->set_wstatus(WorkerStatus::INITIALIZING);
        this->window_thread = std::thread([this] { this->window_loop(); });

        std::unique_lock ws_lock{this->wstatus_mutex};
        this->wstatus_changed_cv.wait(ws_lock, [&]{
            return this->wstatus == WorkerStatus::RUNNING || this->wstatus == WorkerStatus::DIED;
        });

        return this->wstatus == WorkerStatus::RUNNING;
    }

    void Win32RawHook::detach() {
        this->do_detach();
    }

    void Win32RawHook::do_detach() {
        this->wactive = false;

        if(this->hwnd) {
            PostMessage(this->hwnd, WM_CLOSE, 0, 0);
        }

        if(this->window_thread.joinable())
            this->window_thread.join();

        this->set_wstatus(WorkerStatus::STOPPED);
        KeyboardHook::detach();
    }

    void Win32RawHook::set_wstatus(WorkerStatus status) {
        std::lock_guard ws_lock{this->wstatus_mutex};
        if(this->wstatus == status) return;
        this->wstatus = status;
        this->wstatus_changed_cv.notify_all();
    }

#define WORKER_CLASS_NAME ("TeaClient - KeyHook worker")
    void Win32RawHook::window_loop() {
        this->set_wstatus(WorkerStatus::INITIALIZING);

        /* setup */
        {
            {
                WNDCLASS wc = {0};
                wc.lpfnWndProc = window_proc;
                wc.cbWndExtra = sizeof(void*);
                wc.hInstance = nullptr;
                wc.lpszClassName = WORKER_CLASS_NAME;
                RegisterClass(&wc);
            }

            this->hwnd = CreateWindow(WORKER_CLASS_NAME, "TeaClient - KeyHook worker window", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, nullptr, this);
            if(!this->hwnd) {
                this->worker_died_reason = "Failed to create window";
                this->set_wstatus(WorkerStatus::DIED);
                goto cleanup;
            }

            RAWINPUTDEVICE devices[2];
            devices[0].usUsagePage  = 0x01; //HID_USAGE_PAGE_GENERIC;
            devices[0].usUsage      = 0x02; //HID_USAGE_GENERIC_MOUSE;
            devices[0].dwFlags      = (uint32_t) RIDEV_NOLEGACY | (uint32_t) RIDEV_INPUTSINK;
            devices[0].hwndTarget   = hwnd;

            devices[1].usUsagePage  = 0x01; //HID_USAGE_PAGE_GENERIC;
            devices[1].usUsage      = 0x06; //HID_USAGE_GENERIC_KEYBOARD;
            devices[1].dwFlags      = (uint32_t) RIDEV_NOLEGACY | (uint32_t) RIDEV_INPUTSINK;
            devices[1].hwndTarget   = hwnd;

            if(!RegisterRawInputDevices(devices, 2, sizeof *devices)) {
                this->worker_died_reason = "failed to register raw input devices";
                this->set_wstatus(WorkerStatus::DIED);
                goto cleanup;
            }
        }

        this->set_wstatus(WorkerStatus::RUNNING);

        BOOL ret;
        MSG msg;
        while (this->wactive) {
            ret = GetMessage(&msg, this->hwnd, 0, 0);
            if(ret == 0)
                break;

            if (ret == -1) {
                this->worker_died_reason = "GetMessage() threw an error";
                this->set_wstatus(WorkerStatus::DIED);
                goto cleanup;
            } else {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
        this->set_wstatus(WorkerStatus::STOPPED);

        cleanup:
        if(auto window{std::exchange(this->hwnd, nullptr)}; window)
            DestroyWindow(window);
    }

    LRESULT Win32RawHook::window_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        auto hook = reinterpret_cast<Win32RawHook *>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

        switch (msg) {
            case WM_CREATE: {
                auto s = reinterpret_cast<CREATESTRUCT *>(lp);
                SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR) s->lpCreateParams);
                return 0;
            }
            case WM_CLOSE:
                /* nothing to do here, the window will be cleaned up by the event dispatchers */
                return 0;

            case WM_INPUT: {
                UINT target_size{0};
                GetRawInputData((HRAWINPUT) lp, RID_INPUT, nullptr, &target_size, sizeof(RAWINPUTHEADER));
                if(target_size > sizeof(RAWINPUT)) {
                    std::cerr << "Failed to retrieve input (Target size is longer than expected)" << std::endl;
                    return 0;
                }
                RAWINPUT input{};
                GetRawInputData((HRAWINPUT) lp, RID_INPUT, &input, &target_size, sizeof(RAWINPUTHEADER));

                hook->handle_raw_input(input);
                return 0;
            }
            default:
                return DefWindowProc(hwnd, msg, wp, lp);
        }
    }

    void Win32RawHook::handle_raw_input(RAWINPUT &input) {
        if(input.header.dwType == RIM_TYPEMOUSE) {
            auto& data = input.data.mouse;
            if(data.ulButtons == 0) return; /* mouse move event */

#define BUTTON_EVENT(number, name) \
            case RI_MOUSE_BUTTON_ ##number ##_DOWN: \
                this->trigger_key_event(KeyEvent::PRESS, name); \
                break; \
            case RI_MOUSE_BUTTON_ ##number ##_UP: \
                this->trigger_key_event(KeyEvent::RELEASE, name); \
                break

            switch (data.ulButtons) {
                BUTTON_EVENT(1, "MOUSE1");
                BUTTON_EVENT(2, "MOUSE2");
                BUTTON_EVENT(3, "MOUSE3");
                BUTTON_EVENT(4, "MOUSEX1");
                BUTTON_EVENT(5, "MOUSEX2");
                default:
                    break;
            }
        } else if(input.header.dwType == RIM_TYPEKEYBOARD) {
            auto& data = input.data.keyboard;

            if(data.Message == WM_KEYDOWN || data.Message == WM_SYSKEYDOWN) {
                auto& state = this->map_key[data.VKey];
                bool typed = state;
                state = true;

                auto type = key_type_from_vk(data.VKey);
                if(type != KeyType::KEY_NORMAL)
                    this->map_special[type] = true;
                else
                    this->map_special[type] = data.VKey;

                if(!typed)
                    this->trigger_key_event(KeyEvent::PRESS, type == KeyType::KEY_NORMAL ? key_string_from_sc(data.MakeCode) : key_string_from_vk(this->map_special[KeyType::KEY_NORMAL], false));
                this->trigger_key_event(KeyEvent::TYPE, type == KeyType::KEY_NORMAL ? key_string_from_sc(data.MakeCode) : key_string_from_vk(this->map_special[KeyType::KEY_NORMAL], false));
            } else if(data.Message == WM_KEYUP || data.Message == WM_SYSKEYUP) {
                auto& state = this->map_key[data.VKey];
                if(!state) return; //Duplicate
                state = false;

                auto type = key_type_from_vk(data.VKey);
                if(type != KeyType::KEY_NORMAL)
                    this->map_special[type] = false;
                else if(this->map_special[KeyType::KEY_NORMAL] == data.VKey)
                    this->map_special[KeyType::KEY_NORMAL] = 0;

                this->trigger_key_event(KeyEvent::RELEASE, type == KeyType::KEY_NORMAL ? key_string_from_sc(data.MakeCode) : key_string_from_vk(this->map_special[KeyType::KEY_NORMAL], false));
            }
        }
    }
}