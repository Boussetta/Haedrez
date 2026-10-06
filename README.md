# Haedrez

Private, personal Windows chat-head client. Application source is C11, using
Win32 APIs and CMake. No C++, C#, WPF, or Electron application code.

## Status

Milestone 1: native always-on-top shell and tested work-area positioning.
Messenger integration is not implemented. Official Meta APIs cannot access
personal Messenger conversations or friends' online presence. Being the sole
user does not remove those restrictions.

An embedded Messenger website is a possible future provider, subject to user
approval of a browser runtime dependency and a successful login compatibility
test. C application source can call COM interfaces; this does not mean the
third-party browser engine is itself written in C. Do not scrape private APIs,
collect Facebook passwords, export cookies, or bypass authentication controls.

## Build

Install Visual Studio 2022 Build Tools with Desktop development with C++
(the component supplies the MSVC C compiler and Windows SDK), and CMake 3.21+.

```powershell
cmake --preset msvc
cmake --build --preset msvc-debug
ctest --preset msvc-debug
```

Launch-at-sign-in is disabled by default. Always-on-top windows cannot cover
Windows secure desktop or all exclusive-fullscreen applications.

## Development

Work on `milestone/mN-description`, use small commits, and merge through pull
requests only after Windows CI passes. `main` must have required CI, pull
requests, no force pushes or deletion, and protection applied to administrators.
For a sole maintainer, reviews are optional; required self-approval is impossible.
Private-repository protection may require GitHub Pro or an appropriate organization
plan. Never make the repository public or weaken protection as a workaround.