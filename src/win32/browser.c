#include <windows.h>
#include <objbase.h>
#include <winhttp.h>
#include <stdlib.h>
#include <wchar.h>
#include <WebView2.h>
#include "haedrez/browser.h"

struct hd_browser {
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler environment_done;
    ICoreWebView2CreateCoreWebView2ControllerCompletedHandler controller_done;
    ICoreWebView2NavigationCompletedEventHandler navigation_done;
    ICoreWebView2NavigationStartingEventHandler navigation_start;
    ICoreWebView2NewWindowRequestedEventHandler new_window;
    ICoreWebView2PermissionRequestedEventHandler permission;
    LONG references;
    HWND parent;
    UINT notification;
    BOOL closed;
    LPWSTR uri;
    ICoreWebView2Environment *environment;
    ICoreWebView2Controller *controller;
    ICoreWebView2 *webview;
    EventRegistrationToken navigation_token;
    EventRegistrationToken starting_token;
    EventRegistrationToken window_token;
    EventRegistrationToken permission_token;
    UINT registered;
};

BOOL hd_browser_is_meta_uri(LPCWSTR uri)
{
    WCHAR host[256];
    URL_COMPONENTS parts = { sizeof(URL_COMPONENTS) };
    size_t length;
    if (!uri) return FALSE;
    parts.lpszHostName = host;
    parts.dwHostNameLength = ARRAYSIZE(host);
    parts.dwUserNameLength = (DWORD)-1;
    parts.dwPasswordLength = (DWORD)-1;
    if (!WinHttpCrackUrl(uri, 0, 0, &parts) || parts.nScheme != INTERNET_SCHEME_HTTPS ||
        parts.nPort != 443 || parts.dwUserNameLength || parts.dwPasswordLength) return FALSE;
    length = wcslen(host);
    return _wcsicmp(host, L"facebook.com") == 0 || _wcsicmp(host, L"messenger.com") == 0 ||
        (length > 13 && _wcsicmp(host + length - 13, L".facebook.com") == 0) ||
        (length > 14 && _wcsicmp(host + length - 14, L".messenger.com") == 0);
}

static ULONG browser_addref(hd_browser *browser)
{
    return (ULONG)InterlockedIncrement(&browser->references);
}

static ULONG browser_release(hd_browser *browser)
{
    ULONG remaining = (ULONG)InterlockedDecrement(&browser->references);
    if (!remaining) {
        free(browser->uri);
        free(browser);
    }
    return remaining;
}

static HRESULT browser_query(hd_browser *browser, REFIID requested, void **result)
{
    if (!result) return E_POINTER;
    *result = NULL;
    if (IsEqualIID(requested, &IID_IUnknown) ||
        IsEqualIID(requested, &IID_ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler)) {
        *result = &browser->environment_done;
    } else if (IsEqualIID(requested, &IID_ICoreWebView2CreateCoreWebView2ControllerCompletedHandler)) {
        *result = &browser->controller_done;
    } else if (IsEqualIID(requested, &IID_ICoreWebView2NavigationCompletedEventHandler)) {
        *result = &browser->navigation_done;
    } else if (IsEqualIID(requested, &IID_ICoreWebView2NavigationStartingEventHandler)) {
        *result = &browser->navigation_start;
    } else if (IsEqualIID(requested, &IID_ICoreWebView2NewWindowRequestedEventHandler)) {
        *result = &browser->new_window;
    } else if (IsEqualIID(requested, &IID_ICoreWebView2PermissionRequestedEventHandler)) {
        *result = &browser->permission;
    } else {
        return E_NOINTERFACE;
    }
    browser_addref(browser);
    return S_OK;
}

#define HD_CALLBACK_LIFETIME(type, field, prefix) \
static HRESULT STDMETHODCALLTYPE prefix##_query(type *self, REFIID requested, void **result) \
{ return browser_query(CONTAINING_RECORD(self, hd_browser, field), requested, result); } \
static ULONG STDMETHODCALLTYPE prefix##_addref(type *self) \
{ return browser_addref(CONTAINING_RECORD(self, hd_browser, field)); } \
static ULONG STDMETHODCALLTYPE prefix##_release(type *self) \
{ return browser_release(CONTAINING_RECORD(self, hd_browser, field)); }

HD_CALLBACK_LIFETIME(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler, environment_done, environment)
HD_CALLBACK_LIFETIME(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler, controller_done, controller)
HD_CALLBACK_LIFETIME(ICoreWebView2NavigationCompletedEventHandler, navigation_done, navigation)
HD_CALLBACK_LIFETIME(ICoreWebView2NavigationStartingEventHandler, navigation_start, starting)
HD_CALLBACK_LIFETIME(ICoreWebView2NewWindowRequestedEventHandler, new_window, popup)
HD_CALLBACK_LIFETIME(ICoreWebView2PermissionRequestedEventHandler, permission, permission)

static void notify(hd_browser *browser, HRESULT result)
{
    if (!browser->closed && browser->parent) {
        PostMessageW(browser->parent, browser->notification, 0, (LPARAM)result);
    }
}

HRESULT hd_browser_resize(hd_browser *browser)
{
    RECT bounds;
    if (!browser || browser->closed) return E_ABORT;
    if (!browser->controller) return S_FALSE;
    if (!GetClientRect(browser->parent, &bounds)) return HRESULT_FROM_WIN32(GetLastError());
    return ICoreWebView2Controller_put_Bounds(browser->controller, bounds);
}

HRESULT hd_browser_title(hd_browser *browser, LPWSTR *title)
{
    if (!title) return E_POINTER;
    *title = NULL;
    if (!browser || !browser->webview || browser->closed) return E_ABORT;
    return ICoreWebView2_get_DocumentTitle(browser->webview, title);
}

static HRESULT STDMETHODCALLTYPE navigation_invoke(ICoreWebView2NavigationCompletedEventHandler *self,
    ICoreWebView2 *sender, ICoreWebView2NavigationCompletedEventArgs *args)
{
    hd_browser *browser = CONTAINING_RECORD(self, hd_browser, navigation_done);
    BOOL success = FALSE;
    HRESULT result;
    (void)sender;
    if (browser->closed) return S_OK;
    result = ICoreWebView2NavigationCompletedEventArgs_get_IsSuccess(args, &success);
    notify(browser, FAILED(result) ? result : (success ? S_OK : HRESULT_FROM_WIN32(ERROR_CONNECTION_ABORTED)));
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE starting_invoke(ICoreWebView2NavigationStartingEventHandler *self,
    ICoreWebView2 *sender, ICoreWebView2NavigationStartingEventArgs *args)
{
    hd_browser *browser = CONTAINING_RECORD(self, hd_browser, navigation_start);
    LPWSTR uri = NULL;
    HRESULT result;
    (void)sender;
    if (browser->closed || !browser->uri) return S_OK;
    result = ICoreWebView2NavigationStartingEventArgs_get_Uri(args, &uri);
    if (FAILED(result) || !uri || (wcscmp(uri, L"about:blank") != 0 && !hd_browser_is_meta_uri(uri))) {
        ICoreWebView2NavigationStartingEventArgs_put_Cancel(args, TRUE);
    }
    CoTaskMemFree(uri);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE popup_invoke(ICoreWebView2NewWindowRequestedEventHandler *self,
    ICoreWebView2 *sender, ICoreWebView2NewWindowRequestedEventArgs *args)
{
    hd_browser *browser = CONTAINING_RECORD(self, hd_browser, new_window);
    LPWSTR uri = NULL;
    ICoreWebView2NewWindowRequestedEventArgs_put_Handled(args, TRUE);
    if (!browser->closed && SUCCEEDED(ICoreWebView2NewWindowRequestedEventArgs_get_Uri(args, &uri)) &&
        hd_browser_is_meta_uri(uri)) {
        ICoreWebView2_Navigate(sender, uri);
    }
    CoTaskMemFree(uri);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE permission_invoke(ICoreWebView2PermissionRequestedEventHandler *self,
    ICoreWebView2 *sender, ICoreWebView2PermissionRequestedEventArgs *args)
{
    hd_browser *browser = CONTAINING_RECORD(self, hd_browser, permission);
    COREWEBVIEW2_PERMISSION_KIND kind;
    LPWSTR uri = NULL;
    COREWEBVIEW2_PERMISSION_STATE state = COREWEBVIEW2_PERMISSION_STATE_DENY;
    HRESULT result;
    (void)sender;
    browser_addref(browser);
    if (!browser->closed && SUCCEEDED(ICoreWebView2PermissionRequestedEventArgs_get_PermissionKind(args, &kind)) &&
        (kind == COREWEBVIEW2_PERMISSION_KIND_CAMERA || kind == COREWEBVIEW2_PERMISSION_KIND_MICROPHONE) &&
        SUCCEEDED(ICoreWebView2PermissionRequestedEventArgs_get_Uri(args, &uri)) && hd_browser_is_meta_uri(uri)) {
        LPCWSTR prompt = kind == COREWEBVIEW2_PERMISSION_KIND_CAMERA ?
            L"Allow this Facebook/Messenger page to use your camera?" :
            L"Allow this Facebook/Messenger page to use your microphone?";
        if (MessageBoxW(browser->parent, prompt, L"Haedrez - Device permission",
                        MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) == IDYES && !browser->closed) {
            state = COREWEBVIEW2_PERMISSION_STATE_ALLOW;
        }
    }
    CoTaskMemFree(uri);
    result = ICoreWebView2PermissionRequestedEventArgs_put_State(args, state);
    browser_release(browser);
    return result;
}

static HRESULT STDMETHODCALLTYPE controller_invoke(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *self,
    HRESULT error, ICoreWebView2Controller *controller)
{
    hd_browser *browser = CONTAINING_RECORD(self, hd_browser, controller_done);
    HRESULT result = error;
    if (browser->closed) {
        if (controller) ICoreWebView2Controller_Close(controller);
        return S_OK;
    }
    if (FAILED(error) || !controller) { notify(browser, FAILED(error) ? error : E_FAIL); return S_OK; }
    browser->controller = controller;
    ICoreWebView2Controller_AddRef(controller);
    result = ICoreWebView2Controller_get_CoreWebView2(controller, &browser->webview);
    if (FAILED(result)) goto finished;
    result = ICoreWebView2_add_NavigationCompleted(browser->webview, &browser->navigation_done, &browser->navigation_token);
    if (FAILED(result)) goto finished;
    browser->registered |= 1;
    result = ICoreWebView2_add_NavigationStarting(browser->webview, &browser->navigation_start, &browser->starting_token);
    if (FAILED(result)) goto finished;
    browser->registered |= 2;
    result = ICoreWebView2_add_NewWindowRequested(browser->webview, &browser->new_window, &browser->window_token);
    if (FAILED(result)) goto finished;
    browser->registered |= 4;
    result = ICoreWebView2_add_PermissionRequested(browser->webview, &browser->permission, &browser->permission_token);
    if (FAILED(result)) goto finished;
    browser->registered |= 8;
    result = hd_browser_resize(browser);
    if (FAILED(result)) goto finished;
    result = ICoreWebView2Controller_put_IsVisible(controller, TRUE);
    if (FAILED(result)) goto finished;
    if (browser->uri) {
        result = ICoreWebView2_Navigate(browser->webview, browser->uri);
    } else {
        result = ICoreWebView2_NavigateToString(browser->webview,
            L"<!doctype html><html><head><title>Haedrez WebView2 probe</title></head>"
            L"<body style='background:#148577;color:white;font:24px Segoe UI'>"
            L"<h1>Haedrez</h1><p>Native C browser host</p></body></html>");
    }
finished:
    if (FAILED(result)) notify(browser, result);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE environment_invoke(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *self,
    HRESULT error, ICoreWebView2Environment *environment)
{
    hd_browser *browser = CONTAINING_RECORD(self, hd_browser, environment_done);
    HRESULT result;
    if (browser->closed) return S_OK;
    if (FAILED(error) || !environment) { notify(browser, FAILED(error) ? error : E_FAIL); return S_OK; }
    browser->environment = environment;
    ICoreWebView2Environment_AddRef(environment);
    result = ICoreWebView2Environment_CreateCoreWebView2Controller(environment, browser->parent, &browser->controller_done);
    if (FAILED(result)) notify(browser, result);
    return S_OK;
}

static ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandlerVtbl environment_vtable = {
    environment_query, environment_addref, environment_release, environment_invoke };
static ICoreWebView2CreateCoreWebView2ControllerCompletedHandlerVtbl controller_vtable = {
    controller_query, controller_addref, controller_release, controller_invoke };
static ICoreWebView2NavigationCompletedEventHandlerVtbl navigation_vtable = {
    navigation_query, navigation_addref, navigation_release, navigation_invoke };
static ICoreWebView2NavigationStartingEventHandlerVtbl starting_vtable = {
    starting_query, starting_addref, starting_release, starting_invoke };
static ICoreWebView2NewWindowRequestedEventHandlerVtbl popup_vtable = {
    popup_query, popup_addref, popup_release, popup_invoke };
static ICoreWebView2PermissionRequestedEventHandlerVtbl permission_vtable = {
    permission_query, permission_addref, permission_release, permission_invoke };

HRESULT hd_browser_create(HWND parent, LPCWSTR profile, LPCWSTR uri, UINT notification, hd_browser **result)
{
    hd_browser *browser;
    HRESULT error;
    if (!result) return E_POINTER;
    *result = NULL;
    if (!IsWindow(parent) || !profile || (uri && !hd_browser_is_meta_uri(uri))) return E_INVALIDARG;
    browser = calloc(1, sizeof(*browser));
    if (!browser) return E_OUTOFMEMORY;
    browser->references = 1;
    browser->parent = parent;
    browser->notification = notification;
    browser->environment_done.lpVtbl = &environment_vtable;
    browser->controller_done.lpVtbl = &controller_vtable;
    browser->navigation_done.lpVtbl = &navigation_vtable;
    browser->navigation_start.lpVtbl = &starting_vtable;
    browser->new_window.lpVtbl = &popup_vtable;
    browser->permission.lpVtbl = &permission_vtable;
    if (uri) {
        browser->uri = _wcsdup(uri);
        if (!browser->uri) { browser_release(browser); return E_OUTOFMEMORY; }
    }
    error = CreateCoreWebView2EnvironmentWithOptions(NULL, profile, NULL, &browser->environment_done);
    if (FAILED(error)) { hd_browser_close(browser); return error; }
    *result = browser;
    return S_OK;
}

void hd_browser_close(hd_browser *browser)
{
    if (!browser) return;
    browser->closed = TRUE;
    browser->parent = NULL;
    if (browser->webview) {
        if (browser->registered & 1) ICoreWebView2_remove_NavigationCompleted(browser->webview, browser->navigation_token);
        if (browser->registered & 2) ICoreWebView2_remove_NavigationStarting(browser->webview, browser->starting_token);
        if (browser->registered & 4) ICoreWebView2_remove_NewWindowRequested(browser->webview, browser->window_token);
        if (browser->registered & 8) ICoreWebView2_remove_PermissionRequested(browser->webview, browser->permission_token);
        ICoreWebView2_Release(browser->webview);
        browser->webview = NULL;
    }
    if (browser->controller) {
        ICoreWebView2Controller_Close(browser->controller);
        ICoreWebView2Controller_Release(browser->controller);
        browser->controller = NULL;
    }
    if (browser->environment) {
        ICoreWebView2Environment_Release(browser->environment);
        browser->environment = NULL;
    }
    browser_release(browser);
}