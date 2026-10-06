#include <windows.h>
#include <stdio.h>
#include <wchar.h>

typedef struct shell_windows {
    DWORD process_id;
    HWND launcher;
    HWND panel;
} shell_windows;

static BOOL CALLBACK find_windows(HWND window, LPARAM parameter)
{
    shell_windows *windows = (shell_windows *)parameter;
    DWORD process_id;
    WCHAR name[64];
    GetWindowThreadProcessId(window, &process_id);
    if (process_id != windows->process_id) return TRUE;
    GetClassNameW(window, name, ARRAYSIZE(name));
    if (wcscmp(name, L"Haedrez.Launcher") == 0) windows->launcher = window;
    if (wcscmp(name, L"Haedrez.Contacts") == 0) windows->panel = window;
    return TRUE;
}

static BOOL inside_work_area(HWND window)
{
    MONITORINFO monitor = { sizeof(MONITORINFO) };
    RECT bounds;
    if (!GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTOPRIMARY), &monitor)) return FALSE;
    if (!GetWindowRect(window, &bounds)) return FALSE;
    return bounds.left >= monitor.rcWork.left && bounds.top >= monitor.rcWork.top &&
           bounds.right <= monitor.rcWork.right && bounds.bottom <= monitor.rcWork.bottom;
}

static BOOL send_message(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    DWORD_PTR result;
    return SendMessageTimeoutW(window, message, wparam, lparam,
                               SMTO_ABORTIFHUNG | SMTO_BLOCK, 2000, &result) != 0;
}

#define CHECK(condition, label) do { \
    if (!(condition)) { fprintf(stderr, "FAIL: %s (Win32 error %lu)\n", label, GetLastError()); goto cleanup; } \
    puts("PASS: " label); \
} while (0)

int wmain(int argc, wchar_t **argv)
{
    WCHAR command[32768];
    STARTUPINFOW startup = { sizeof(STARTUPINFOW) };
    PROCESS_INFORMATION process = { 0 };
    shell_windows windows = { 0 };
    HWND button;
    HRGN region = NULL;
    DWORD exit_code;
    RECT button_bounds;
    HDC screen = NULL;
    HDC bitmap_dc = NULL;
    HBITMAP bitmap = NULL;
    HGDIOBJ old_bitmap = NULL;
    int result = 1;
    if (argc != 2) return 1;
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    CHECK(swprintf_s(command, ARRAYSIZE(command), L"\"%s\" --smoke-test", argv[1]) > 0, "command line");
    CHECK(CreateProcessW(NULL, command, NULL, NULL, FALSE, 0, NULL, NULL, &startup, &process), "start shell");
    CHECK(WaitForInputIdle(process.hProcess, 5000) == 0, "shell ready");
    windows.process_id = process.dwProcessId;
    CHECK(EnumWindows(find_windows, (LPARAM)&windows), "enumerate windows");
    CHECK(windows.launcher && windows.panel, "launcher and panel created");
    CHECK(IsWindowVisible(windows.launcher), "launcher visible");
    CHECK(!IsWindowVisible(windows.panel), "panel initially hidden");
    CHECK(GetWindowLongPtrW(windows.launcher, GWL_EXSTYLE) & WS_EX_TOPMOST, "launcher always on top");
    CHECK(GetWindowLongPtrW(windows.panel, GWL_EXSTYLE) & WS_EX_TOPMOST, "panel always on top");
    CHECK(inside_work_area(windows.launcher), "launcher inside work area");
    region = CreateRectRgn(0, 0, 0, 0);
    CHECK(region && GetWindowRgn(windows.launcher, region) == COMPLEXREGION, "circular launcher region");
    CHECK(!PtInRegion(region, 0, 0), "launcher corner transparent");
    button = GetDlgItem(windows.launcher, 100);
    CHECK(button != NULL, "accessible launcher button");
    CHECK(send_message(button, WM_PAINT, 0, 0), "launcher painting responds");
    CHECK(GetClientRect(button, &button_bounds), "launcher dimensions");
    screen = GetDC(button);
    CHECK(screen != NULL, "launcher device context");
    bitmap_dc = CreateCompatibleDC(screen);
    bitmap = CreateCompatibleBitmap(screen, button_bounds.right, button_bounds.bottom);
    CHECK(bitmap_dc && bitmap, "render capture allocated");
    old_bitmap = SelectObject(bitmap_dc, bitmap);
    CHECK(PrintWindow(windows.launcher, bitmap_dc, PW_CLIENTONLY), "capture launcher rendering");
    CHECK(GetPixel(bitmap_dc, button_bounds.right / 2, button_bounds.bottom / 2) == RGB(255, 255, 255), "plus rendered");
    ReleaseDC(button, screen);
    screen = NULL;
    CHECK(send_message(button, BM_CLICK, 0, 0), "open panel");
    CHECK(IsWindowVisible(windows.panel), "panel visible after click");
    CHECK(inside_work_area(windows.panel), "panel inside work area");
    CHECK(send_message(windows.panel, WM_CLOSE, 0, 0), "close panel");
    CHECK(!IsWindowVisible(windows.panel), "close collapses panel");
    CHECK(WaitForSingleObject(process.hProcess, 0) == WAIT_TIMEOUT, "shell stays running");
    CHECK(send_message(button, BM_CLICK, 0, 0), "reopen panel");
    CHECK(IsWindowVisible(windows.panel), "panel reopened");
    CHECK(send_message(windows.launcher, WM_SETTINGCHANGE, 0, 0), "work area change responds");
    CHECK(inside_work_area(windows.launcher), "launcher remains in work area");
    CHECK(send_message(windows.launcher, WM_CLOSE, 0, 0), "exit shell");
    CHECK(WaitForSingleObject(process.hProcess, 5000) == WAIT_OBJECT_0, "process exits");
    CHECK(GetExitCodeProcess(process.hProcess, &exit_code) && exit_code == 0, "clean exit");
    result = 0;
cleanup:
    if (screen) ReleaseDC(GetDlgItem(windows.launcher, 100), screen);
    if (old_bitmap) SelectObject(bitmap_dc, old_bitmap);
    if (bitmap) DeleteObject(bitmap);
    if (bitmap_dc) DeleteDC(bitmap_dc);
    if (region) DeleteObject(region);
    if (process.hProcess) {
        if (WaitForSingleObject(process.hProcess, 0) == WAIT_TIMEOUT) {
            if (windows.launcher) PostMessageW(windows.launcher, WM_CLOSE, 0, 0);
            if (WaitForSingleObject(process.hProcess, 2000) == WAIT_TIMEOUT) {
                TerminateProcess(process.hProcess, 1);
                WaitForSingleObject(process.hProcess, 2000);
            }
        }
        CloseHandle(process.hProcess);
        CloseHandle(process.hThread);
    }
    return result;
}