#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <functional>
#include <cassert>
#include <cstring>
#include <iostream>
#include "X11KeyboardHook.h"

#define ClientMessageMask		(1L << 24)
#define SelectMask              (KeyPressMask | KeyReleaseMask | FocusChangeMask | ClientMessageMask)

using namespace hooks;
using KeyType = KeyboardHook::KeyType;

inline KeyType key_type(XID key_id) {
	switch (key_id) {
		case XK_Shift_L:
		case XK_Shift_R:
			return KeyType::KEY_SHIFT;
		case XK_Alt_L:
		case XK_Alt_R:
			return KeyType::KEY_ALT;
		case XK_Control_L:
		case XK_Control_R:
			return KeyType::KEY_CTRL;
		case XK_Menu:
			return KeyType::KEY_WIN;

		default:
			break;
	}

	/*
	if(key_id >= XK_space && key_id <= XK_asciitilde) return KeyType::KEY_NORMAL; //Normal ASCI
	if(key_id >= XK_KP_Space && key_id <= XK_KP_Divide) return KeyType::KEY_NORMAL; // Keypad functions, keypad numbers cleverly chosen to map to ASCII
	if(key_id >= XK_Kanji && key_id <= XK_Mae_Koho) return KeyType::KEY_NORMAL; // Japanese keyboard support
	*/
	return KeyType::KEY_NORMAL;
}

X11KeyboardHook::X11KeyboardHook() : KeyboardHook{KeyboardHookType::X11} {
    this->map_special[KeyType::KEY_NORMAL] = XK_VoidSymbol;
}

X11KeyboardHook::~X11KeyboardHook() {
    if(this->active) {
        (void) this->detach();
    }
}

bool X11KeyboardHook::attach() {
	assert(!this->active);
	this->active = true;

	this->display = XOpenDisplay(nullptr);
	if(!this->display) return false;

	{
		this->window_root = XDefaultRootWindow(this->display);

		XGetInputFocus(display, &this->window_focused, &this->focus_revert);
		if(this->window_focused == PointerRoot)
			this->window_focused = this->window_root;
		XSelectInput(this->display, this->window_focused, SelectMask);
		//XGrabKeyboard(this->display, this->window_focused, false, GrabModeAsync, GrabModeAsync, CurrentTime);
	}

	XSetErrorHandler([](Display* dp, XErrorEvent* event) {
		if(event->type == BadWindow)
			return 0;
		/*
	      X Error of failed request:  BadWindow (invalid Window parameter)
		  Major opcode of failed request:  2 (X_ChangeWindowAttributes)
		  Resource id in failed request:  0x0
		  Serial number of failed request:  32
		  Current serial number in output stream:  32
		 */
		std::cerr << "Caught X11 error: " << event->type << std::endl;
		return 0;
	});

	this->poll_thread = std::thread(std::bind(&X11KeyboardHook::poll_events, this));
	return true;
}

void X11KeyboardHook::detach() {
	assert(this->active);
	this->active = false;

	{
		// push a dummy event into the queue so that the event loop has a chance to stop
		XClientMessageEvent dummy_event;
		memset((void*) &dummy_event, 0, sizeof(XClientMessageEvent));
		dummy_event.type = ClientMessage;
		dummy_event.window = this->window_focused;
		dummy_event.format = 32;
		dummy_event.data.l[0] = this->end_id = rand();

		XSendEvent(this->display, this->window_focused, 0, ClientMessageMask, (XEvent*) &dummy_event);
		XFlush(this->display);
	}

	if(this->poll_thread.joinable())
		this->poll_thread.join();

	XCloseDisplay(this->display);
	this->display = nullptr;
}

void X11KeyboardHook::poll_events() {
	XEvent event;
	bool has_event = false;

	while(this->active) {
		if(!has_event) {
			auto result = XNextEvent(this->display, &event);
			if(result != 0) {
				//TODO throw error
				std::cout << "XNextEvent returned invalid code: " << result << std::endl;
				break;
			}
		}
		has_event = false;

		if(event.type == FocusOut) {
			std::cout << "Old windows lost focus. Attaching to new one" << std::endl;
			int result = 0;
			if(this->window_root != this->window_focused) {
				result = XSelectInput(display, this->window_focused, 0); //Release events
			}
			result = XGetInputFocus(display, &this->window_focused, &this->focus_revert);
			if(this->window_focused == PointerRoot)
				this->window_focused = this->window_root;
			result = XSelectInput(display, this->window_focused, SelectMask);
			continue;
		} else if(event.type == FocusIn) {

		} else if(event.type == KeyPress) {
			auto& old_flag = this->map_key[event.xkey.keycode];
			bool typed = old_flag;
			old_flag = true;

			auto key_sym = XLookupKeysym(&event.xkey, 0);
			auto type = key_type(key_sym);

			if(type != KeyType::KEY_NORMAL)
				this->map_special[type] = true;
			else
				this->map_special[type] = key_sym;

			if(!typed)
				this->trigger_key_event(KeyEvent::PRESS, type == KeyType::KEY_NORMAL ? XKeysymToString(key_sym) : XKeysymToString(this->map_special[KeyType::KEY_NORMAL]));
			this->trigger_key_event(KeyEvent::TYPE, type == KeyType::KEY_NORMAL ? XKeysymToString(key_sym) : XKeysymToString(this->map_special[KeyType::KEY_NORMAL]));
		} else if(event.type == KeyRelease) {
			auto key_sym = XLookupKeysym(&event.xkey, 0);
			if(XEventsQueued(this->display, 0) > 0) {
				auto result = XNextEvent(this->display, &event);
				if(result != 0) {
					std::cerr << "XNextEvent(...) => " << result << std::endl;
					break;
				}
				has_event = true;
				if(event.type == KeyPress && XLookupKeysym(&event.xkey, 0) == key_sym) //Key retriggered
					continue; //Handle repress
			}

			auto& old_flag = this->map_key[event.xkey.keycode];
			if(!old_flag) continue; //Nothing to update
			old_flag = false;

			auto type = key_type(key_sym);
			if(type != KeyType::KEY_NORMAL)
				this->map_special[type] = false;
			else if(this->map_special[KeyType::KEY_NORMAL] == key_sym)
				this->map_special[KeyType::KEY_NORMAL] = XK_VoidSymbol;

			this->trigger_key_event(KeyEvent::RELEASE, type == KeyType::KEY_NORMAL ? XKeysymToString(key_sym) : XKeysymToString(this->map_special[KeyType::KEY_NORMAL]));
		} else if(event.type == ClientMessage) {
			std::cout << "Got client message. End ID: " << event.xclient.data.l[0] << " <=> " << this->end_id << std::endl;
			if(event.xclient.data.l[0] == this->end_id) break;
		} else if(event.type == ButtonPress) {
			std::cout << "Got button" << std::endl;
		} else if(event.type == ButtonRelease) {
			std::cout << "Got button" << std::endl;
		} else {
			std::cerr << "Got unknown event of type " << event.type << std::endl;
		}
	}
}