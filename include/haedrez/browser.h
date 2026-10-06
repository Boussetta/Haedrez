#ifndef HAEDREZ_BROWSER_H
#define HAEDREZ_BROWSER_H

#include <windows.h>

typedef struct hd_browser hd_browser;

BOOL hd_browser_is_meta_uri(LPCWSTR uri);
HRESULT hd_browser_create(HWND parent, LPCWSTR profile, LPCWSTR uri, UINT notification, hd_browser **result);
HRESULT hd_browser_resize(hd_browser *browser);
HRESULT hd_browser_title(hd_browser *browser, LPWSTR *title);
void hd_browser_close(hd_browser *browser);

#endif