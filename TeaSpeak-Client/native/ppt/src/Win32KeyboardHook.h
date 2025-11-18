#pragma once

#include "./KeyboardHook.h"
#include <condition_variable>
#include <Windows.h>

namespace hooks {
    extern KeyboardHook::KeyType key_type_from_vk(DWORD vk_code);
    extern std::string key_string_from_vk(DWORD code, bool extended);
    extern std::string key_string_from_sc(USHORT code);

    class Win32RawHook : public KeyboardHook {
        public:
            Win32RawHook();
            ~Win32RawHook() override;

            bool attach() override;
            void detach() override;

            [[nodiscard]] bool keytype_supported() const override { return true; }
        private:
            static LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

            void do_detach();

            std::thread window_thread;
            void window_loop();

            enum struct WorkerStatus {
                STOPPED,
                DIED,

                INITIALIZING,
                RUNNING
            };

            bool wactive{false};
            WorkerStatus wstatus{WorkerStatus::STOPPED};
            std::mutex wstatus_mutex{};
            std::condition_variable wstatus_changed_cv{};
            std::string worker_died_reason{};

            void set_wstatus(WorkerStatus);
            void handle_raw_input(RAWINPUT&);

            HWND hwnd{nullptr};
    };
}