# Haedrez

Personal Windows chat-head client. Application source is C11, using
Win32 APIs and CMake. No C++, C#, WPF, or Electron application code.

The repository is temporarily public with protected `main`. Account sessions
remain local to each device and must never be committed.

## Status

Milestone 1 implements the native topmost launcher and tray shell.
Milestone 2 adds a C COM WebView2 host in a resizable, always-on-top Messenger
window. Local-page startup, cancellation, navigation, resize and shutdown tests
pass; the Facebook Messages entry page has loaded without account sign-in.
Authenticated messaging, 2FA, attachments, session persistence across restarts,
and voice/video calls still require manual compatibility testing.

The owner approved an embedded browser trial. All application source is C, but
the Microsoft browser runtime is a third-party dependency, not a C-only engine.
The conversation UI is Meta's website, not a native message renderer.
Official Meta APIs still cannot access personal Messenger conversations or
friends' online presence. No scraping, credential collection, cookie export,
private API use, authentication bypass, or automatic contact import is implemented.

## Build

Install Visual Studio 2022 Build Tools with Desktop development with C++
(the component supplies the MSVC C compiler and Windows SDK), and CMake 3.21+.
Use the x64 preset. Configuration downloads official WebView2 SDK `1.0.4258.31`
from NuGet and verifies its SHA-256 checksum. Microsoft Edge WebView2 Evergreen
Runtime must be installed to open Messenger. A missing runtime skips only the
browser test; compilation and shell/core tests are still required.

```powershell
cmake --preset msvc
cmake --build --preset msvc-debug
ctest --preset msvc-debug
```

## Try Messenger

```powershell
.\build\msvc\Debug\haedrez.exe --messenger
```

Without the argument, only the circular `+` appears. Click it and select
**Open Messenger**, or use the launcher/tray context menu. Sign in directly
on Meta's page; never provide passwords or verification codes through chat.
Contact search in the native panel remains disabled. Select contacts within
Messenger's own interface for this trial.

Closing the Messenger window hides it; opening it again reuses the same session.
Hiding is not logout and does not end an active call. Use the Messenger UI to
end calls or log out; use **Exit Haedrez** in the tray menu to stop the host.
If loading fails, opening Messenger again retries browser initialization.

The profile is `%LOCALAPPDATA%\Haedrez\WebView2`, outside the repository.
Do not sync this folder between devices or upload it to GitHub. Keep
`WebView2Loader.dll` beside `haedrez.exe` when moving the build; the Visual C++
x64 runtime is also required. Runtime and installer packaging is milestone 4.

Only HTTPS Facebook/Messenger domains are allowed for top-level navigation.
Popups to these domains navigate in the existing window; other popup domains
are blocked. This may limit login or call flows and must be tested. Camera and
microphone access require explicit approval in a default-deny native prompt;
other requested permissions are denied. Calls are not advertised as supported yet.

Manual acceptance: sign in and complete 2FA, select a contact, send/receive a
message, test an attachment, restart and check session persistence, then test
voice/video with permission denial and approval. Do not post private chat or
account screenshots publicly. Milestone 2 remains a feasibility draft until these
account-dependent checks pass.

Launch-at-sign-in is disabled by default. Always-on-top windows cannot cover
Windows secure desktop or all exclusive-fullscreen applications.

## Development

Work on `milestone/mN-description`, use small commits, and merge through pull
requests only after Windows CI passes. `main` must have required CI, pull
requests, no force pushes or deletion, and protection applied to administrators.
For a sole maintainer, reviews are optional; required self-approval is impossible.
Returning the repository to private requires a GitHub plan that preserves branch
protection. Do not change visibility or weaken protection without owner approval.