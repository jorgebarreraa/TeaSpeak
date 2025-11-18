#pragma once

#include <X11/Xlib.h>
#include "./KeyboardHook.h"

namespace hooks {
    class X11KeyboardHook : public KeyboardHook {
        public:
            X11KeyboardHook();
            ~X11KeyboardHook() override;

            bool attach() override;
            void detach() override;

            bool keytype_supported() const override { return true; }
        private:
            Display* display{nullptr};
            Window window_root{0};
            Window window_focused{0};
            int focus_revert{};

            long end_id{0};

            bool active{false};
            std::thread poll_thread;
            void poll_events();
    };
}