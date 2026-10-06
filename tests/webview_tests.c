#include <windows.h>
#include <objbase.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <WebView2.h>
#include "haedrez/browser.h"

static hd_browser *browser;
static BOOL messenger_probe;
enum { PROBE_RESULT = WM_APP + 9 };

static LRESULT CALLBACK probe_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message) {
    case WM_SIZE:
        if (browser) hd_browser_resize(browser);
        return 0;
    case PROBE_RESULT: {
        LPWSTR title = NULL;
        HRESULT result = (HRESULT)lparam;
        BOOL success = SUCCEEDED(result) && SUCCEEDED(hd_browser_title(browser, &title)) && title &&
            (messenger_probe ? title[0] != L'\0' : wcscmp(title, L"Haedrez WebView2 probe") == 0);
        CoTaskMemFree(title);
        if (success) puts("PASS: asynchronous C host navigated and received document title");
        else fprintf(stderr, "FAIL: browser navigation or title: 0x%08lX\n", (unsigned long)result);
        PostQuitMessage(success ? 0 : 1);
        return 0;
    }
    case WM_TIMER:
        fputs("FAIL: WebView2 startup timed out\n", stderr);
        PostQuitMessage(1);
        return 0;
    default:
        return DefWindowProcW(window, message, wparam, lparam);
    }
}

int main(int argc, char **argv)
{
    LPWSTR version = NULL;
    WCHAR profile[MAX_PATH];
    WCHAR temporary[MAX_PATH];
    WNDCLASSEXW window_class = { sizeof(WNDCLASSEXW) };
    HWND window = NULL;
    hd_browser *cancelled = NULL;
    MSG message;
    int exit_code = 1;
    HRESULT result;
    BOOL initialized = FALSE;
    messenger_probe = argc == 2 && strcmp(argv[1], "--messenger") == 0;
    if (!hd_browser_is_meta_uri(L"https://www.facebook.com/messages/") ||
        !hd_browser_is_meta_uri(L"https://messenger.com/") ||
        hd_browser_is_meta_uri(L"https://facebook.com.evil.example/") ||
        hd_browser_is_meta_uri(L"https://evilfacebook.com/") ||
        hd_browser_is_meta_uri(L"https://facebook.com@evil.example/") ||
        hd_browser_is_meta_uri(L"http://facebook.com/") ||
        hd_browser_is_meta_uri(L"https://facebook.com:444/") ||
        hd_browser_is_meta_uri(L"file:///C:/Windows/") ||
        hd_browser_is_meta_uri(L"https://user:password@facebook.com/")) {
        fputs("FAIL: trusted origin policy\n", stderr);
        return 1;
    }
    puts("PASS: trusted Meta HTTPS origin policy");
    result = GetAvailableCoreWebView2BrowserVersionString(NULL, &version);
    if (FAILED(result)) {
        fprintf(stderr, "WebView2 runtime unavailable: 0x%08lX\n", (unsigned long)result);
        return result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) ? 77 : 1;
    }
    wprintf(L"PASS: C SDK and loader; WebView2 runtime %ls\n", version);
    CoTaskMemFree(version);
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    result = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(result)) goto cleanup;
    initialized = TRUE;
    if (!GetTempPathW(ARRAYSIZE(temporary), temporary)) goto cleanup;
    if (swprintf_s(profile, ARRAYSIZE(profile), L"%lsHaedrez-WebView2-Probe", temporary) < 0) goto cleanup;
    window_class.hInstance = GetModuleHandleW(NULL);
    window_class.lpfnWndProc = probe_proc;
    window_class.lpszClassName = L"Haedrez.WebViewProbe";
    if (!RegisterClassExW(&window_class)) goto cleanup;
    window = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, window_class.lpszClassName,
        L"Haedrez WebView2 Probe", WS_OVERLAPPEDWINDOW, 80, 80, 640, 560,
        NULL, NULL, window_class.hInstance, NULL);
    if (!window) goto cleanup;
    result = hd_browser_create(window, profile, NULL, PROBE_RESULT, &cancelled);
    if (FAILED(result)) goto cleanup;
    hd_browser_close(cancelled);
    cancelled = NULL;
    puts("PASS: cancellation during asynchronous startup");
    result = hd_browser_create(window, profile, messenger_probe ? L"https://www.facebook.com/messages/" : NULL,
                               PROBE_RESULT, &browser);
    if (FAILED(result)) goto cleanup;
    if (FAILED(hd_browser_resize(browser))) goto cleanup;
    ShowWindow(window, SW_SHOWNOACTIVATE);
    if (!SetTimer(window, 1, 25000, NULL)) goto cleanup;
    while (GetMessageW(&message, NULL, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    exit_code = (int)message.wParam;
cleanup:
    if (cancelled) hd_browser_close(cancelled);
    if (browser) { hd_browser_close(browser); browser = NULL; }
    if (window) DestroyWindow(window);
    if (initialized) CoUninitialize();
    if (exit_code) fprintf(stderr, "FAIL: C browser host (HRESULT 0x%08lX)\n", (unsigned long)result);
    return exit_code;
}