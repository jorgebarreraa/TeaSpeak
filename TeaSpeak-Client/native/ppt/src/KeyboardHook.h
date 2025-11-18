#pragma once

#include <string>
#include <functional>
#include <thread>
#include <map>

enum struct KeyboardHookType {
	X11,
	RAW_INPUT
};

class KeyboardHook {
	public:
		typedef unsigned int KeyID;

		enum struct KeyType {
			KEY_UNKNOWN,

			KEY_NORMAL,
			KEY_SHIFT,
			KEY_ALT,
			KEY_WIN,
			KEY_CTRL
		};

		struct KeyEvent {
			enum type {
				PRESS,
				RELEASE,
				TYPE
			};

			type type;
			std::string key;
			std::string code;

			bool key_ctrl;
			bool key_windows;
			bool key_shift;
			bool key_alt;
		};
		typedef std::function<void(const std::shared_ptr<KeyEvent>& /* event */)> callback_event_t;

		explicit KeyboardHook(KeyboardHookType);
		virtual ~KeyboardHook();

		[[nodiscard]] inline KeyboardHookType type() const { return this->type_; }
		[[nodiscard]] virtual bool keytype_supported() const = 0;

		[[nodiscard]] virtual bool attach();
		[[nodiscard]] inline bool attached() const { return this->attached_; }
		virtual void detach();

		void trigger_key_event(const enum KeyEvent::type&, const std::string& /* key */);
		callback_event_t callback_event;
	protected:
		const KeyboardHookType type_;

		std::map<KeyID, bool> map_key;
		std::map<KeyType, KeyID> map_special;

		bool attached_ = false;
};
