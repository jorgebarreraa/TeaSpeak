#include "../src/KeyboardHook.h"
#include "../src/Win32KeyboardHook.h"
#include <iostream>
#include <thread>
#include <Windows.h>

using namespace std::chrono;
using namespace std;

#if 0
LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
	// Retrieve "this" pointer
	// Guaranteed to be garbage until WM_CREATE finishes, but
	// we don't actually use this value until WM_CREATE writes a valid one
	//vrpn_DirectXRumblePad *me = reinterpret_cast<vrpn_DirectXRumblePad *>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

	switch (msg) {
		// Window is being created; store "this" pointer for future retrieval
		case WM_CREATE: {
			CREATESTRUCT *s = reinterpret_cast<CREATESTRUCT *>(lp);
			SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR) s->lpCreateParams);
			return 0;
		}
			// Something (most likely ~vrpn_DirectXRumblePad) wants to close the window
			// Go ahead and signal shutdown
		case WM_CLOSE:
			DestroyWindow(hwnd);
			PostQuitMessage(0);
			break;

		case WM_INPUT: {
			UINT dwSize;
			UINT buffer_size = sizeof(RAWINPUT);
			GetRawInputData((HRAWINPUT) lp, RID_INPUT, NULL, &dwSize,sizeof(RAWINPUTHEADER));
			if(dwSize > buffer_size) {
				std::cerr << "Failed to retreive input" << std::endl;
				return 0;
			}
			RAWINPUT input{};
			GetRawInputData((HRAWINPUT) lp, RID_INPUT, &input, &buffer_size, sizeof(RAWINPUTHEADER));

			if(input.header.dwType != RIM_TYPEMOUSE)
				return 0;

			auto& mouse_data = input.data.mouse;
			std::cout << "Input" << std::endl;
			std::cout << "Buttons: " << (int) mouse_data.ulButtons << std::endl;
		}

			// Everything not explicitly handled goes to DefWindowProc as per usual
		default:
			return DefWindowProc(hwnd, msg, wp, lp);
	}

	return 0;
}
#endif

std::string GetLastErrorAsString() {
	//Get the error message, if any.
	DWORD errorMessageID = ::GetLastError();
	if(errorMessageID == 0)
		return std::string(); //No error message has been recorded

	LPSTR messageBuffer = nullptr;
	size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
								 NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

	std::string message(messageBuffer, size);

	//Free the buffer.
	LocalFree(messageBuffer);

	return message;
}

int main() {
#if 1
	KeyboardHook* hook = new hooks::Win32RawHook{};
	hook->callback_event = [](const shared_ptr<KeyboardHook::KeyEvent>& event) {
		if(event->type == KeyboardHook::KeyEvent::PRESS)
			cout << "press   " << event->code.c_str() << ": shift: " << event->key_shift << ", alt: " << event->key_alt << ", ctrl: " << event->key_ctrl << ", win: " << event->key_windows << endl;
		else if(event->type == KeyboardHook::KeyEvent::TYPE)
			cout << "type    " << event->code.c_str() << ": shift: " << event->key_shift << ", alt: " << event->key_alt << ", ctrl: " << event->key_ctrl << ", win: " << event->key_windows << endl;
		else
			cout << "release " << event->code.c_str() << ": shift: " << event->key_shift << ", alt: " << event->key_alt << ", ctrl: " << event->key_ctrl << ", win: " << event->key_windows << endl;
	};

	if(!hook->attach()) {
		cerr << "failed to attach!" << endl;
		return 0;
	}
#else
#define CLASS_NAME "TeaClient - Hook"
	WNDCLASS wc = {0};
	wc.lpfnWndProc = window_proc;
	wc.cbWndExtra = sizeof(void*);
	wc.hInstance = 0;
	wc.lpszClassName = CLASS_NAME;
	RegisterClass(&wc);

	auto hwnd = CreateWindow(CLASS_NAME, "TeaClient - PPT hook", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, NULL, 0);

	RAWINPUTDEVICE device;
	device.usUsagePage  = 0x01; //HID_USAGE_PAGE_GENERIC;
	device.usUsage      = 0x02; //HID_USAGE_GENERIC_MOUSE;
	//device.usUsage      = 0x06; //HID_USAGE_GENERIC_KEYBOARD;
	device.dwFlags = RIDEV_NOLEGACY | RIDEV_INPUTSINK;
	device.hwndTarget = hwnd;
	if(!RegisterRawInputDevices(&device, 1, sizeof device)) {
		std::cerr << "Invalid: " << GetLastErrorAsString() << std::endl;
	}

	BOOL ret;
	MSG msg;
	while ((ret = GetMessage(&msg, hwnd, 0, 0)) != 0) {
		if (ret == -1) {
			std::cerr << "GetMessage() threw an error." << std::endl;
			return 1;
		}
		else {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
#endif

	this_thread::sleep_for(seconds(10));
	return 0;
}