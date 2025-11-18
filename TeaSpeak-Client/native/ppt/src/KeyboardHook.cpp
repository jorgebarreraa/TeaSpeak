#include "./KeyboardHook.h"
#include <cassert>

using namespace std;

KeyboardHook::KeyboardHook(KeyboardHookType type) : type_{type} {};

KeyboardHook::~KeyboardHook() {
    assert(!this->attached_);
}

bool KeyboardHook::attach() {
    assert(!this->attached_);
    this->attached_ = true;
    return true;
}

void KeyboardHook::detach() {
    assert(this->attached_);
    this->attached_ = false;
}

void KeyboardHook::trigger_key_event(const enum KeyEvent::type& type, const std::string &key) {
	if(!this->callback_event) return;

	auto event = make_shared<KeyboardHook::KeyEvent>();
	event->type = type;
	event->code = key;

	event->key_alt = this->map_special[KeyType::KEY_ALT] > 0;
	event->key_ctrl = this->map_special[KeyType::KEY_CTRL] > 0;
	event->key_windows = this->map_special[KeyType::KEY_WIN] > 0;
	event->key_shift = this->map_special[KeyType::KEY_SHIFT] > 0;

	this->callback_event(event);
}