#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <commctrl.h>
#include <wchar.h>
#include <objbase.h>
#include <shlobj.h>
#include <strsafe.h>
#include "haedrez/layout.h"
#include "haedrez/browser.h"

enum { HD_LAUNCH = 100, HD_SEARCH = 101, HD_MESSENGER = 102, HD_OPEN = 200, HD_EXIT = 201,
       HD_TRAY = WM_APP + 1, HD_BROWSER_STATE = WM_APP + 2 };

static HINSTANCE instance;
static HWND launcher;
static HWND launch_button;
static HWND panel;
static HWND search;
static HWND status_label;
static HWND messenger_button;
static HWND messenger_window;
static HWND messenger_status;
static hd_browser *messenger_browser;
static BOOL messenger_failed;
static HWND tooltip;
static HFONT ui_font;
static UINT taskbar_created;
static BOOL tray_added;
static BOOL smoke_mode;

static int scale(int value, UINT dpi);
static RECT work_area(HWND window);

static void browser_status(HRESULT result)
{
    WCHAR text[128];
    messenger_failed = FAILED(result);
    if (messenger_failed) {
        StringCchPrintfW(text, ARRAYSIZE(text), L"Messenger unavailable (0x%08lX)", (unsigned long)result);
        SetWindowTextW(messenger_status, text);
        SetWindowPos(messenger_status, HWND_TOP, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    } else {
        ShowWindow(messenger_status, SW_HIDE);
    }
}

static void start_messenger(void)
{
    LPWSTR folder = NULL;
    WCHAR profile[32768];
    HRESULT result = SHGetKnownFolderPath(&FOLDERID_LocalAppData, 0, NULL, &folder);
    SetWindowTextW(messenger_status, L"Loading Messenger...");
    ShowWindow(messenger_status, SW_SHOW);
    messenger_failed = FALSE;
    if (SUCCEEDED(result)) {
        result = StringCchPrintfW(profile, ARRAYSIZE(profile), L"%ls\\Haedrez\\WebView2", folder);
    }
    CoTaskMemFree(folder);
    if (SUCCEEDED(result)) {
        result = hd_browser_create(messenger_window, profile, L"https://www.facebook.com/messages/",
                                   HD_BROWSER_STATE, &messenger_browser);
    }
    if (FAILED(result)) browser_status(result);
}

static void open_messenger(void)
{
    if (!messenger_window) {
        RECT work = work_area(launcher);
        UINT dpi = GetDpiForWindow(launcher);
        hd_rect bounds = { work.left, work.top, work.right, work.bottom };
        hd_rect target = hd_anchor(bounds, scale(480, dpi), scale(640, dpi), scale(76, dpi));
        messenger_window = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
            L"Haedrez.Messenger", L"Haedrez - Messenger", WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME,
            target.left, target.top, target.right - target.left, target.bottom - target.top,
            launcher, NULL, instance, NULL);
        if (!messenger_window) {
            MessageBoxW(launcher, L"Could not create the Messenger window.", L"Haedrez", MB_OK | MB_ICONERROR);
            return;
        }
        start_messenger();
    } else if (messenger_failed) {
        hd_browser_close(messenger_browser);
        messenger_browser = NULL;
        start_messenger();
    }
    ShowWindow(messenger_window, SW_SHOW);
    SetForegroundWindow(messenger_window);
}

static LRESULT CALLBACK messenger_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message) {
    case WM_CREATE:
        messenger_status = CreateWindowExW(0, L"STATIC", L"Loading Messenger...",
            WS_CHILD | WS_VISIBLE | SS_CENTER, 12, 24, 320, 64, window, NULL, instance, NULL);
        SendMessageW(messenger_status, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
        return 0;
    case HD_BROWSER_STATE:
        browser_status((HRESULT)lparam);
        return 0;
    case WM_SIZE: {
        RECT client;
        GetClientRect(window, &client);
        MoveWindow(messenger_status, 12, 24, client.right > 24 ? client.right - 24 : 1, 64, TRUE);
        if (messenger_browser) hd_browser_resize(messenger_browser);
        return 0;
    }
    case WM_DPICHANGED: {
        const RECT *suggested = (const RECT *)lparam;
        SetWindowPos(window, HWND_TOPMOST, suggested->left, suggested->top,
            suggested->right - suggested->left, suggested->bottom - suggested->top, SWP_NOACTIVATE);
        return 0;
    }
    case WM_CLOSE:
        ShowWindow(window, SW_HIDE);
        return 0;
    case WM_DESTROY:
        hd_browser_close(messenger_browser);
        messenger_browser = NULL;
        messenger_window = NULL;
        return 0;
    default:
        return DefWindowProcW(window, message, wparam, lparam);
    }
}

static int scale(int value, UINT dpi)
{
    return MulDiv(value, (int)dpi, 96);
}

static RECT work_area(HWND window)
{
    MONITORINFO monitor = { sizeof(MONITORINFO) };
    HMONITOR handle = MonitorFromWindow(window, MONITOR_DEFAULTTOPRIMARY);
    GetMonitorInfoW(handle, &monitor);
    return monitor.rcWork;
}

static void position_panel(void)
{
    RECT work = work_area(launcher);
    RECT bubble;
    UINT dpi = GetDpiForWindow(launcher);
    hd_rect bounds = { work.left, work.top, work.right, work.bottom };
    hd_rect target = hd_anchor(bounds, scale(340, dpi), scale(440, dpi), scale(12, dpi));
    GetWindowRect(launcher, &bubble);
    bounds.bottom = bubble.top - scale(12, dpi);
    if (bounds.bottom > bounds.top) {
        target = hd_anchor(bounds, scale(340, dpi), scale(440, dpi), scale(12, dpi));
    }
    SetWindowPos(panel, HWND_TOPMOST, target.left, target.top,
                 target.right - target.left, target.bottom - target.top, SWP_NOACTIVATE);
}

static void position_launcher(void)
{
    RECT work = work_area(launcher);
    UINT dpi = GetDpiForWindow(launcher);
    hd_rect bounds = { work.left, work.top, work.right, work.bottom };
    hd_rect target = hd_anchor(bounds, scale(52, dpi), scale(52, dpi), scale(12, dpi));
    int width = target.right - target.left;
    int height = target.bottom - target.top;
    HRGN region = CreateEllipticRgn(0, 0, width, height);
    SetWindowPos(launcher, HWND_TOPMOST, target.left, target.top, width, height, SWP_NOACTIVATE);
    if (region && !SetWindowRgn(launcher, region, TRUE)) DeleteObject(region);
    if (panel) position_panel();
}

static void add_tray(void)
{
    NOTIFYICONDATAW notification = { sizeof(NOTIFYICONDATAW) };
    if (smoke_mode) return;
    notification.hWnd = launcher;
    notification.uID = 1;
    notification.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    notification.uCallbackMessage = HD_TRAY;
    notification.hIcon = LoadIconW(NULL, IDI_APPLICATION);
    wcscpy_s(notification.szTip, ARRAYSIZE(notification.szTip), L"Haedrez");
    tray_added = Shell_NotifyIconW(NIM_ADD, &notification);
    if (tray_added) {
        notification.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &notification);
    }
}

static void toggle_panel(void)
{
    if (IsWindowVisible(panel)) {
        ShowWindow(panel, SW_HIDE);
    } else {
        position_panel();
        ShowWindow(panel, SW_SHOW);
        SetForegroundWindow(panel);
    }
}

static void show_menu(void)
{
    POINT cursor;
    HMENU menu = CreatePopupMenu();
    UINT command;
    if (!menu) return;
    AppendMenuW(menu, MF_STRING, HD_OPEN, L"Contacts");
    AppendMenuW(menu, MF_STRING, HD_MESSENGER, L"Open Messenger");
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(menu, MF_STRING, HD_EXIT, L"Exit Haedrez");
    GetCursorPos(&cursor);
    SetForegroundWindow(launcher);
    command = (UINT)TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                                  cursor.x, cursor.y, 0, launcher, NULL);
    DestroyMenu(menu);
    if (command == HD_OPEN) toggle_panel();
    if (command == HD_MESSENGER) open_messenger();
    if (command == HD_EXIT) DestroyWindow(launcher);
    else PostMessageW(launcher, WM_NULL, 0, 0);
}

static void layout_panel(HWND window)
{
    RECT client;
    UINT dpi = GetDpiForWindow(window);
    int margin = scale(16, dpi);
    int width;
    HFONT old_font = ui_font;
    GetClientRect(window, &client);
    width = client.right - margin * 2;
    if (width < 1) width = 1;
    ui_font = CreateFontW(-scale(15, dpi), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                         DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                         CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    SendMessageW(search, WM_SETFONT, (WPARAM)ui_font, TRUE);
    SendMessageW(status_label, WM_SETFONT, (WPARAM)ui_font, TRUE);
    SendMessageW(messenger_button, WM_SETFONT, (WPARAM)ui_font, TRUE);
    if (old_font) DeleteObject(old_font);
    MoveWindow(search, margin, margin, width, scale(32, dpi), TRUE);
    MoveWindow(status_label, margin, scale(68, dpi), width, scale(72, dpi), TRUE);
    MoveWindow(messenger_button, margin, scale(148, dpi), width, scale(36, dpi), TRUE);
}

static LRESULT CALLBACK panel_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message) {
    case WM_CREATE:
        search = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", NULL,
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
            0, 0, 1, 1, window, (HMENU)(INT_PTR)HD_SEARCH, instance, NULL);
        SendMessageW(search, EM_SETCUEBANNER, TRUE, (LPARAM)L"Search contacts");
        EnableWindow(search, FALSE);
        status_label = CreateWindowExW(0, L"STATIC", L"No connected provider",
            WS_CHILD | WS_VISIBLE | SS_LEFT, 0, 0, 1, 1, window, NULL, instance, NULL);
        messenger_button = CreateWindowExW(0, L"BUTTON", L"Open Messenger",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
            0, 0, 1, 1, window, (HMENU)(INT_PTR)HD_MESSENGER, instance, NULL);
        layout_panel(window);
        return 0;
    case WM_SIZE:
        layout_panel(window);
        return 0;
    case WM_DPICHANGED:
        position_panel();
        layout_panel(window);
        return 0;
    case WM_CLOSE:
        ShowWindow(window, SW_HIDE);
        return 0;
    case WM_COMMAND:
        if (LOWORD(wparam) == HD_MESSENGER && HIWORD(wparam) == BN_CLICKED) open_messenger();
        return 0;
    case WM_KEYDOWN:
        if (wparam == VK_ESCAPE) ShowWindow(window, SW_HIDE);
        return 0;
    default:
        return DefWindowProcW(window, message, wparam, lparam);
    }
}

static LRESULT CALLBACK launcher_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (taskbar_created && message == taskbar_created) {
        add_tray();
        position_launcher();
        return 0;
    }
    switch (message) {
    case WM_CREATE: {
        TOOLINFOW tool = { sizeof(TOOLINFOW) };
        launcher = window;
        launch_button = CreateWindowExW(0, L"BUTTON", L"Open contacts",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
            0, 0, 52, 52, window, (HMENU)(INT_PTR)HD_LAUNCH, instance, NULL);
        tooltip = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, NULL,
            WS_POPUP | TTS_ALWAYSTIP, CW_USEDEFAULT, CW_USEDEFAULT,
            CW_USEDEFAULT, CW_USEDEFAULT, window, NULL, instance, NULL);
        tool.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
        tool.hwnd = window;
        tool.uId = (UINT_PTR)launch_button;
        tool.lpszText = L"Contacts";
        SendMessageW(tooltip, TTM_ADDTOOLW, 0, (LPARAM)&tool);
        add_tray();
        return 0;
    }
    case WM_SIZE:
        MoveWindow(launch_button, 0, 0, LOWORD(lparam), HIWORD(lparam), TRUE);
        return 0;
    case WM_PRINTCLIENT: {
        DRAWITEMSTRUCT draw = { 0 };
        draw.hDC = (HDC)wparam;
        GetClientRect(window, &draw.rcItem);
        SendMessageW(window, WM_DRAWITEM, HD_LAUNCH, (LPARAM)&draw);
        return 0;
    }
    case WM_DRAWITEM: {
        const DRAWITEMSTRUCT *draw = (const DRAWITEMSTRUCT *)lparam;
        HBRUSH brush = CreateSolidBrush((draw->itemState & ODS_SELECTED) ? RGB(15, 103, 93) : RGB(20, 133, 119));
        HPEN pen = CreatePen(PS_SOLID, scale(3, GetDpiForWindow(window)), RGB(255, 255, 255));
        HGDIOBJ old_brush = SelectObject(draw->hDC, brush);
        HGDIOBJ old_pen = SelectObject(draw->hDC, GetStockObject(NULL_PEN));
        int center_x = draw->rcItem.right / 2;
        int center_y = draw->rcItem.bottom / 2;
        int radius = scale(9, GetDpiForWindow(window));
        Ellipse(draw->hDC, 0, 0, draw->rcItem.right, draw->rcItem.bottom);
        SelectObject(draw->hDC, pen);
        MoveToEx(draw->hDC, center_x - radius, center_y, NULL);
        LineTo(draw->hDC, center_x + radius, center_y);
        MoveToEx(draw->hDC, center_x, center_y - radius, NULL);
        LineTo(draw->hDC, center_x, center_y + radius);
        if (draw->itemState & ODS_FOCUS) {
            RECT focus = { center_x - radius - 4, center_y - radius - 4,
                           center_x + radius + 4, center_y + radius + 4 };
            DrawFocusRect(draw->hDC, &focus);
        }
        SelectObject(draw->hDC, old_pen);
        SelectObject(draw->hDC, old_brush);
        DeleteObject(pen);
        DeleteObject(brush);
        return TRUE;
    }
    case WM_COMMAND:
        if (LOWORD(wparam) == HD_LAUNCH && HIWORD(wparam) == BN_CLICKED) toggle_panel();
        return 0;
    case WM_CONTEXTMENU:
        show_menu();
        return 0;
    case HD_TRAY:
        if (LOWORD(lparam) == WM_CONTEXTMENU) show_menu();
        if (LOWORD(lparam) == NIN_SELECT || LOWORD(lparam) == NIN_KEYSELECT) toggle_panel();
        return 0;
    case WM_DPICHANGED:
    case WM_DISPLAYCHANGE:
    case WM_SETTINGCHANGE:
        position_launcher();
        return 0;
    case WM_DESTROY: {
        NOTIFYICONDATAW notification = { sizeof(NOTIFYICONDATAW) };
        notification.hWnd = window;
        notification.uID = 1;
        if (tray_added) Shell_NotifyIconW(NIM_DELETE, &notification);
        if (messenger_window) DestroyWindow(messenger_window);
        if (panel) DestroyWindow(panel);
        if (ui_font) DeleteObject(ui_font);
        PostQuitMessage(0);
        return 0;
    }
    default:
        return DefWindowProcW(window, message, wparam, lparam);
    }
}

int WINAPI wWinMain(HINSTANCE app_instance, HINSTANCE previous, PWSTR command_line, int show)
{
    WNDCLASSEXW window_class = { sizeof(WNDCLASSEXW) };
    INITCOMMONCONTROLSEX controls = { sizeof(INITCOMMONCONTROLSEX), ICC_STANDARD_CLASSES };
    MSG message;
    HANDLE singleton = NULL;
    BOOL result;
    BOOL com_initialized = FALSE;
    (void)previous;
    (void)show;
    smoke_mode = wcscmp(command_line, L"--smoke-test") == 0;
    if (!smoke_mode) {
        singleton = CreateMutexW(NULL, FALSE, L"Local\\Haedrez.NativeShell");
        if (!singleton) return 1;
        if (GetLastError() == ERROR_ALREADY_EXISTS) {
            CloseHandle(singleton);
            return 0;
        }
    }
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    if (FAILED(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED))) goto failed;
    com_initialized = TRUE;
    InitCommonControlsEx(&controls);
    instance = app_instance;
    taskbar_created = RegisterWindowMessageW(L"TaskbarCreated");
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursorW(NULL, IDC_ARROW);
    window_class.lpfnWndProc = launcher_proc;
    window_class.lpszClassName = L"Haedrez.Launcher";
    if (!RegisterClassExW(&window_class)) goto failed;
    window_class.lpfnWndProc = messenger_proc;
    window_class.lpszClassName = L"Haedrez.Messenger";
    if (!RegisterClassExW(&window_class)) goto failed;
    window_class.lpfnWndProc = panel_proc;
    window_class.lpszClassName = L"Haedrez.Contacts";
    window_class.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    if (!RegisterClassExW(&window_class)) goto failed;
    launcher = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        L"Haedrez.Launcher", L"Haedrez", WS_POPUP, 0, 0, 52, 52,
        NULL, NULL, instance, NULL);
    if (!launcher) goto failed;
    panel = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        L"Haedrez.Contacts", L"Haedrez - Contacts", WS_POPUP | WS_CAPTION | WS_SYSMENU,
        0, 0, 340, 440, launcher, NULL, instance, NULL);
    if (!panel || !launch_button || !search || !status_label || !messenger_button) goto failed;
    position_launcher();
    ShowWindow(launcher, SW_SHOWNOACTIVATE);
    if (wcscmp(command_line, L"--messenger") == 0) open_messenger();
    while ((result = GetMessageW(&message, NULL, 0, 0)) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    if (singleton) CloseHandle(singleton);
    if (com_initialized) CoUninitialize();
    return result == -1 ? 1 : (int)message.wParam;
failed:
    MessageBoxW(NULL, L"Haedrez could not create its desktop windows.", L"Haedrez", MB_OK | MB_ICONERROR);
    if (launcher) DestroyWindow(launcher);
    if (singleton) CloseHandle(singleton);
    if (com_initialized) CoUninitialize();
    return 1;
}