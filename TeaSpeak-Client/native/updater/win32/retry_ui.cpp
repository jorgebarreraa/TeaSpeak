//
// Created by WolverinDEV on 21/04/2020.
//

#include <Windows.h>
#include <RestartManager.h>
#include <CommCtrl.h>
#include <windowsx.h>

#include <cstdio>
#include <cassert>
#include <array>
#include <vector>
#include <string>
#include <iostream>
#include <thread>
#include <condition_variable>
#include "../ui.h"

struct BlockingProcess {
    RM_APP_TYPE type{};
    std::wstring name{};
    std::wstring exe_path{};
    int pid{};
};

bool blocking_processes(std::vector<BlockingProcess>& result, std::wstring& error, const std::wstring& file) {
    DWORD dwSession{0};
    WCHAR szSessionKey[CCH_RM_SESSION_KEY + 1] = { 0 };
    DWORD dwError = RmStartSession(&dwSession, 0, szSessionKey);
    if(dwError != ERROR_SUCCESS) {
        error = L"Failed to start rm session (" + std::to_wstring(dwError) + L")";
        goto error_exit;
    }

    PCWSTR pszFile = file.data();
    dwError = RmRegisterResources(dwSession, 1, &pszFile, 0, nullptr, 0, nullptr);
    if(dwError != ERROR_SUCCESS) {
        error = L"Failed to register resource (" + std::to_wstring(dwError) + L")";
        goto error_exit;
    }

    DWORD dwReason;
    UINT i;
    UINT nProcInfoNeeded;
    UINT nProcInfo = 10;
    RM_PROCESS_INFO rgpi[10];
    dwError = RmGetList(dwSession, &nProcInfoNeeded, &nProcInfo, rgpi, &dwReason);
    if(dwError != ERROR_SUCCESS) {
        error = L"Failed to get list from rm (" + std::to_wstring(dwError) + L")";
        goto error_exit;
    }

    result.reserve(nProcInfo);
    for (i = 0; i < nProcInfo; i++) {
        auto& info = result.emplace_back();

        info.type = rgpi[i].ApplicationType;
        info.name = rgpi[i].strAppName;
        info.pid = rgpi[i].Process.dwProcessId;
        info.exe_path = L"unknown";

        HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, rgpi[i].Process.dwProcessId);
        if (hProcess) {
            FILETIME ftCreate, ftExit, ftKernel, ftUser;
            if (GetProcessTimes(hProcess, &ftCreate, &ftExit, &ftKernel, &ftUser) && CompareFileTime(&rgpi[i].Process.ProcessStartTime, &ftCreate) == 0) {
                WCHAR sz[MAX_PATH];
                DWORD cch{MAX_PATH};
                if (QueryFullProcessImageNameW(hProcess, 0, sz, &cch) && cch <= MAX_PATH) {
                    info.exe_path = sz;
                }
            }
            CloseHandle(hProcess);
        }
    }

    RmEndSession(dwSession);
    return true;

    error_exit:
    if(dwSession)
        RmEndSession(dwSession);
    return false;
}

#if 0
enum LabelDefault { LABEL_MAX };
enum BrushDefault { BRUSH_MAX };
template <typename Labels = LabelDefault, typename Brush = BrushDefault>
class Win32Window {
    public:
        template <typename Label>
        using WithLabels = Win32Window<Label, Brush>;

        template <typename Brush>
        using WithBrush = Win32Window<Labels, Brush>;

        Win32Window();

    protected:
        std::array<HWND, Labels::LABEL_MAX> hLabel{};
        std::array<HBRUSH, Brush::BRUSH_MAX> hBrush{};
};

static void a() {
    auto window = new Win32Window();
}
#endif

class FileInUseWindow {
    public:
        static constexpr auto ClassName = "FileInUseWindow";
        static bool register_class();

        explicit FileInUseWindow(HWND);
        virtual ~FileInUseWindow();

        bool initialize();
        void finalize();

        void set_file(const std::wstring&);
        void update_blocking_info();

        ui::FileBlockedResult result{ui::FileBlockedResult::UNSET};
        bool deleteOnClose{true};
    private:
        static constexpr auto kBackgroundColor = RGB(240, 240, 240);

        static LRESULT CALLBACK window_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
        INT_PTR window_proc_color_static(HWND hElement, HDC hDC);
        INT_PTR window_proc_command(HWND hwnd, DWORD cID);
        void set_blocking_info(const std::wstring_view&, const std::vector<BlockingProcess>&);

        enum Label {
            LABEL_FIRST_LINE,
            LABEL_FILE_NAME,
            LABEL_SECOND_LINE,
            LABEL_MAX
        };

        enum Brush {
            BRUSH_BACKGROUND,
            BRUSH_MAX
        };

        enum Font {
            FONT_TEXT,
            FONT_FILE_NAME,
            FONT_PROCESS_INFO,
            FONT_MAX
        };

        enum Tooltip {
            TOOLTIP_FILE,
            TOOLTIP_MAX
        };

        enum Button {
            BUTTON_CANCEL,
            BUTTON_CONTINUE,
            BUTTON_REFRESH,
            BUTTON_MAX
        };

        HWND hWindow;

        std::array<HWND, Label::LABEL_MAX> hLabels{};
        std::array<HWND, Tooltip::TOOLTIP_MAX> hTooltips{};
        std::array<HWND, Button::BUTTON_MAX> hButton{};
        HWND hListBox{};

        std::array<HBRUSH, Brush::BRUSH_MAX> hBrush{};
        std::array<HFONT, Font::FONT_MAX> hFont{};

        std::wstring blocking_file{};
        bool window_active{false};

        bool update_exit{false};
        bool force_update{false};
        std::thread update_thread{};
        std::condition_variable update_cv{};
        std::mutex update_mutex{};
};

bool FileInUseWindow::register_class() {
    WNDCLASS wc = {};
    wc.lpfnWndProc   = FileInUseWindow::window_proc;
    wc.hInstance     = GetModuleHandle(nullptr);
    wc.lpszClassName = "FileInUseWindow";
    return RegisterClass(&wc);
}

LRESULT FileInUseWindow::window_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    auto window = (FileInUseWindow*) GetWindowLongPtr(hwnd, GWLP_USERDATA);
    switch(uMsg) {
        case WM_CREATE:
            window = new FileInUseWindow(hwnd);
            if(!window->initialize())
                assert(false);
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR) window);
            /* Certain window data is cached, so changes you make using SetWindowLongPtr will not take effect until you call the SetWindowPos function */
            SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);
            return 0;
        case WM_DESTROY:
            window->finalize();
            if(window->deleteOnClose)
                delete window;
            PostQuitMessage(0);
            return 0;
        case WM_CLOSE:
            if(window->result == ui::FileBlockedResult::UNSET) {
                if (MessageBoxW(hwnd, L"Do you really want to cancel the update?", L"Are you sure?", MB_OKCANCEL) == IDOK) {
                    window->result = ui::FileBlockedResult::CANCELED;
                    DestroyWindow(hwnd);
                }
            } else {
                DestroyWindow(hwnd);
            }
            return 0;
        case WM_CTLCOLORSTATIC:
            return window->window_proc_color_static((HWND) lParam, (HDC) wParam);
        case WM_COMMAND:
            return window->window_proc_command((HWND) lParam, LOWORD(wParam));
        case WM_ACTIVATE:
            if(!window) return 0;
            if(wParam == WA_INACTIVE) {
                window->window_active = false;
            } else if(wParam == WA_ACTIVE) {
                window->window_active = true;
                window->force_update = true;
                window->update_cv.notify_all(); /* update the list */
            }
            return 0;
        default:
            return DefWindowProcA(hwnd, uMsg, wParam, lParam);
    }
}

HWND CreateToolTip(HWND window, HWND hwnd, PTSTR pszText) {
    HWND hwndTip = CreateWindowEx(NULL, TOOLTIPS_CLASS, NULL,
                                  WS_POPUP | TTS_ALWAYSTIP | TTS_BALLOON,
                                  CW_USEDEFAULT, CW_USEDEFAULT,
                                  CW_USEDEFAULT, CW_USEDEFAULT,
                                  hwnd, NULL,
                                  (HINSTANCE) GetWindowLongPtr(hwnd, GWLP_WNDPROC), NULL);

    if(!hwndTip) return nullptr;

    // Associate the tooltip with the tool.
    TOOLINFO toolInfo = { 0 };
    toolInfo.cbSize = sizeof(toolInfo);
    toolInfo.uId = 22;
    toolInfo.hwnd = hwnd;
    toolInfo.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
    toolInfo.lpszText = pszText;
    SendMessage(hwndTip, TTM_ADDTOOL, 0, (LPARAM)&toolInfo);
    SendMessage(hwndTip, TTM_ACTIVATE, TRUE, 0);

    return hwndTip;
}

HWND CreateAHorizontalScrollBar(HWND hwndParent, int sbHeight)
{
    RECT rect;

    // Get the dimensions of the parent window's client area;
    if (!GetClientRect(hwndParent, &rect))
        return NULL;

    // Create the scroll bar.
    return (CreateWindowExW(
            0,                      // no extended styles
            L"SCROLLBAR",           // scroll bar control class
            nullptr,                // no window text
            WS_CHILD | WS_VISIBLE   // window styles
            | SBS_HORZ,             // horizontal scroll bar style
            rect.left,              // horizontal position
            rect.bottom - sbHeight - 12, // vertical position
            rect.right,             // width of the scroll bar
            sbHeight,               // height of the scroll bar
            hwndParent,             // handle to main window
            (HMENU) NULL,           // no menu
            (HINSTANCE) GetWindowLongPtr(hwndParent, GWLP_WNDPROC),                // instance owning this window
            (PVOID) NULL            // pointer not needed
    ));
}

FileInUseWindow::FileInUseWindow(HWND hwnd) : hWindow{hwnd} {}
FileInUseWindow::~FileInUseWindow() = default;

bool FileInUseWindow::initialize() {
    ::SetWindowLong(this->hWindow, GWL_STYLE, GetWindowLong(this->hWindow, GWL_STYLE) & ~ (WS_SIZEBOX | WS_MINIMIZEBOX | WS_MAXIMIZEBOX));

    this->hBrush[Brush::BRUSH_BACKGROUND] = CreateSolidBrush(kBackgroundColor);
    this->hFont[Font::FONT_TEXT] = CreateFontW(16, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Arial");
    this->hFont[Font::FONT_FILE_NAME] = CreateFontW(16, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Consolas");
    this->hFont[Font::FONT_PROCESS_INFO] = CreateFontW(16, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Consolas");

    SetClassLongPtr(this->hWindow, GCLP_HBRBACKGROUND, (LONG_PTR) this->hBrush[Brush::BRUSH_BACKGROUND]);

    {
        this->hLabels[Label::LABEL_FIRST_LINE] = CreateWindow(WC_STATIC, "Failed to unlock the following file:",
                                                              WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                                                              10, 10, 400, 20,
                                                              this->hWindow, nullptr,
                                                              (HINSTANCE) GetWindowLongPtr(this->hWindow, GWLP_WNDPROC), nullptr);

        this->hLabels[Label::LABEL_FILE_NAME] = CreateWindow(WC_STATIC, "<file name here>",
                                                             WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                                                             10, 30, 400, 20,
                                                             this->hWindow, nullptr,
                                                             (HINSTANCE) GetWindowLongPtr(this->hWindow, GWLP_WNDPROC), nullptr);
        this->hTooltips[Tooltip::TOOLTIP_FILE] = CreateToolTip(this->hWindow, this->hLabels[Label::LABEL_FILE_NAME], "Hello World"); //FIXME: Tooltip not working...

        this->hLabels[Label::LABEL_SECOND_LINE] = CreateWindow(WC_STATIC, "Please close these processes:",
                                                              WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                                                              10, 60, 400, 20,
                                                              this->hWindow, nullptr,
                                                              (HINSTANCE) GetWindowLongPtr(this->hWindow, GWLP_WNDPROC), nullptr);

        for(auto& hLabel : this->hLabels)
            SendMessage(hLabel, WM_SETFONT, (WPARAM) this->hFont[Font::FONT_TEXT], TRUE);
        SendMessage(this->hLabels[Label::LABEL_FILE_NAME], WM_SETFONT, (WPARAM) this->hFont[Font::FONT_FILE_NAME], TRUE);
    }

    {

        this->hListBox = CreateWindow("ListBox", nullptr, WS_VISIBLE | WS_CHILD | LBS_STANDARD | LBS_NOTIFY | WS_HSCROLL , 10, 80, 565, 200, this->hWindow, nullptr,
                                        (HINSTANCE) GetWindowLongPtr(this->hWindow, GWLP_WNDPROC), nullptr);
        SendMessage(this->hListBox, WM_SETFONT, (WPARAM) this->hFont[Font::FONT_PROCESS_INFO], TRUE);
    }

    {
#if 0
        this->hButton[Button::BUTTON_CANCEL] = CreateWindowW(
                WC_BUTTONW,  // Predefined class; Unicode assumed
                L"Cancel",      // Button text
                WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,  // Styles
                10,         // x position
                280,         // y position
                100,        // Button width
                30,        // Button height
                this->hWindow,     // Parent window
                nullptr,       // No menu.
                (HINSTANCE)GetWindowLongPtr(this->hWindow, GWLP_HINSTANCE),
                nullptr);      // Pointer not needed.
#endif

        this->hButton[Button::BUTTON_REFRESH] = CreateWindowW(
                WC_BUTTONW,  // Predefined class; Unicode assumed
                L"Refresh",      // Button text
                WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,  // Styles
                10,         // x position
                280,         // y position
                100,        // Button width
                30,        // Button height
                this->hWindow,     // Parent window
                nullptr,       // No menu.
                (HINSTANCE)GetWindowLongPtr(this->hWindow, GWLP_HINSTANCE),
                nullptr);      // Pointer not needed.

        this->hButton[Button::BUTTON_CONTINUE] = CreateWindowW(
                WC_BUTTONW,  // Predefined class; Unicode assumed
                L"Continue",      // Button text
                WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,  // Styles
                475,         // x position
                280,         // y position
                100,        // Button width
                30,        // Button height
                this->hWindow,     // Parent window
                nullptr,       // No menu.
                (HINSTANCE)GetWindowLongPtr(this->hWindow, GWLP_HINSTANCE),
                nullptr);      // Pointer not needed.
    }

    ShowWindow(this->hWindow, SW_SHOWNORMAL);
    this->window_active = true;

    this->update_thread = std::thread([&]{
        while(!this->update_exit) {
            {
                std::unique_lock update_lock{this->update_mutex};
                this->update_cv.wait_for(update_lock, std::chrono::milliseconds{1000});
                if(this->update_exit) return;
            }
            if(this->window_active && !this->force_update) continue; /* only auto update in the background */
            this->force_update = false;

            this->update_blocking_info();
        }
    });
    return true;
}

INT_PTR FileInUseWindow::window_proc_color_static(HWND hElement, HDC hDC) {
    SetBkColor(hDC, kBackgroundColor);
    return (INT_PTR) this->hBrush[Brush::BRUSH_BACKGROUND];
}

INT_PTR FileInUseWindow::window_proc_command(HWND hwnd, DWORD cID) {
    if(hwnd == this->hButton[Button::BUTTON_CANCEL]) {
        SendMessageA(this->hWindow, WM_CLOSE, (WPARAM) nullptr, (LPARAM) nullptr);
    } else if(hwnd == this->hButton[Button::BUTTON_REFRESH]) {
        this->force_update = true;
        this->update_cv.notify_all();
    } else if(hwnd == this->hButton[Button::BUTTON_CONTINUE]) {
        this->result = ui::FileBlockedResult::PROCESSES_CLOSED;
        SendMessageA(this->hWindow, WM_CLOSE, (WPARAM) nullptr, (LPARAM) nullptr);
    }

    return 0;
}

void FileInUseWindow::finalize() {
    this->update_exit = true;
    this->update_cv.notify_all();
    if(this->update_thread.joinable())
        this->update_thread.join();

    //TODO: Free resources?
}

static inline std::wstring_view app_type_prefix(RM_APP_TYPE type) {
    switch (type) {
        case RM_APP_TYPE::RmOtherWindow:
            return L"Child App  : ";
        case RM_APP_TYPE::RmMainWindow:
            return L"Application: ";
        case RM_APP_TYPE::RmConsole:
            return L"Console App: ";
        case RM_APP_TYPE::RmService:
            return L"NT Service : ";
        case RM_APP_TYPE::RmExplorer:
            return L"Explorer   : ";
        case RM_APP_TYPE::RmCritical:
            return L"System     : ";
        default:
            return L"             ";
    }
}

void FileInUseWindow::set_file(const std::wstring &file) {
    this->blocking_file = file;
    this->update_blocking_info();
}

void FileInUseWindow::update_blocking_info() {
    std::wstring error{};
    std::vector<BlockingProcess> result_{};
    if(!blocking_processes(result_, error, this->blocking_file)) {
        MessageBoxW(this->hWindow, (L"Failed to get file info:\n" + error).c_str(), L"Failed to query file info", MB_OK | MB_ICONERROR);
        SendMessageA(this->hWindow, WM_CLOSE, (WPARAM) nullptr, (LPARAM) nullptr);
        return;
    }

    this->set_blocking_info(this->blocking_file, result_);
}

void FileInUseWindow::set_blocking_info(const std::wstring_view &file, const std::vector<BlockingProcess> &processes) {
    SetWindowTextW(this->hLabels[Label::LABEL_FILE_NAME], file.data());

    std::vector<BlockingProcess> output_processes{};
    for(const auto& type : {
            RM_APP_TYPE::RmMainWindow,
            RM_APP_TYPE::RmConsole,
            RM_APP_TYPE::RmService,
            RM_APP_TYPE::RmCritical,
            RM_APP_TYPE::RmOtherWindow
    }) {
        for(const auto& proc : processes) {
            if(proc.type != type) continue;
            auto it = std::find_if(output_processes.begin(), output_processes.end(), [&](const BlockingProcess& entry) {
                return entry.name == proc.name && proc.exe_path == entry.exe_path;
            });
            if(it != output_processes.end())
                continue;
            output_processes.emplace_back(proc);
        }
    }

    HDC hDC = GetDC(NULL);
    SelectFont(hDC, this->hFont[Font::FONT_PROCESS_INFO]);

    size_t width{0};
    SendMessage(this->hListBox, LB_RESETCONTENT, 0, 0);
    for(auto& process : output_processes) {
        auto message = L" " + std::wstring{app_type_prefix(process.type)} + process.name + L" " + std::to_wstring(process.pid) + L" (" + process.exe_path + L") ";
        SendMessageW(this->hListBox, LB_ADDSTRING, 0, (LPARAM) message.c_str());

        RECT r = { 0, 0, 0, 0 };
        DrawTextW(hDC, message.data(), message.length(), &r, DT_CALCRECT);
        if(r.right - r.left > width)
            width = r.right - r.left;
    }

    SendMessage(this->hListBox, LB_SETHORIZONTALEXTENT, (WPARAM) width, 0);
    ReleaseDC(NULL, hDC);

    EnableWindow(this->hButton[Button::BUTTON_CONTINUE], output_processes.empty());
}

inline std::wstring mbtow(const std::string_view& str) {
    int length = MultiByteToWideChar(CP_UTF8, 0, str.data(), -1, nullptr, 0);
    std::wstring result{};
    result.resize(length + 1);
    MultiByteToWideChar(CP_UTF8, 0, str.data(), -1, result.data() , length);
    return result;
}

#define IDR_APP_ICON                       101
ui::FileBlockedResult ui::open_file_blocked(const std::string& file) {
    FileInUseWindow::register_class();
    auto hInstance = GetModuleHandle(nullptr);

    auto hWindow = CreateWindowEx(
            0,                              // Optional window styles.
            FileInUseWindow::ClassName,                     // Window class
            "One or more files are still in use",    // Window text
            WS_OVERLAPPED | WS_CAPTION | WS_THICKFRAME | WS_SYSMENU,            // Window style

            // Size and position
            CW_USEDEFAULT, CW_USEDEFAULT, 600, 360,

            nullptr,       // Parent window
            nullptr,       // Menu
            hInstance,  // Instance handle
            nullptr        // Additional application data
    );
    if (hWindow == nullptr)
        return ui::FileBlockedResult::INTERNAL_ERROR;

    auto hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDR_APP_ICON));
    SendMessage(hWindow, WM_SETICON, ICON_BIG, (LPARAM) hIcon);
    SendMessage(hWindow, WM_SETICON, ICON_SMALL, (LPARAM) hIcon);
    DestroyIcon(hIcon);

    auto window = (FileInUseWindow*) GetWindowLongPtr(hWindow, GWLP_USERDATA);
    window->deleteOnClose = false;

    window->set_file(mbtow(file));

    MSG msg = { };
    while (GetMessage(&msg, hWindow, 0, 0))
    {
        if (msg.message == WM_NULL)
            break;

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    auto result = window->result;
    delete window;
    return result;
}