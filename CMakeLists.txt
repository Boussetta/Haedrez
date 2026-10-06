cmake_minimum_required(VERSION 3.21)
project(haedrez VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_library(hd_warnings INTERFACE)
if(MSVC)
  target_compile_options(hd_warnings INTERFACE /W4 /WX)
else()
  target_compile_options(hd_warnings INTERFACE -Wall -Wextra -Wpedantic -Werror)
endif()

add_library(hd_core STATIC src/core/layout.c)
target_include_directories(hd_core PUBLIC include)
target_link_libraries(hd_core PRIVATE hd_warnings)

enable_testing()
add_executable(hd_core_tests tests/core_tests.c)
target_link_libraries(hd_core_tests PRIVATE hd_core hd_warnings)
add_test(NAME core COMMAND hd_core_tests)

if(WIN32)
  include(FetchContent)
  if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
  endif()
  FetchContent_Declare(webview2_sdk
    URL https://api.nuget.org/v3-flatcontainer/microsoft.web.webview2/1.0.4258.31/microsoft.web.webview2.1.0.4258.31.nupkg
    URL_HASH SHA256=56F7F4B8BF9AEE4B8EFEFBBDD4F67D5F74EBD1B100ED0806DA71BF76AF481AA9)
  FetchContent_MakeAvailable(webview2_sdk)
  add_library(hd_webview_sdk INTERFACE)
  target_include_directories(hd_webview_sdk SYSTEM INTERFACE "${webview2_sdk_SOURCE_DIR}/build/native/include")
  target_link_libraries(hd_webview_sdk INTERFACE "${webview2_sdk_SOURCE_DIR}/build/native/x64/WebView2Loader.dll.lib" ole32 uuid)
  add_library(hd_browser STATIC src/win32/browser.c)
  target_include_directories(hd_browser PUBLIC include)
  target_compile_definitions(hd_browser PRIVATE UNICODE _UNICODE COBJMACROS WIN32_LEAN_AND_MEAN NOMINMAX)
  target_link_libraries(hd_browser PRIVATE hd_webview_sdk hd_warnings winhttp user32)
  add_executable(hd_webview_tests tests/webview_tests.c)
  target_compile_definitions(hd_webview_tests PRIVATE UNICODE _UNICODE COBJMACROS WIN32_LEAN_AND_MEAN NOMINMAX)
  target_link_libraries(hd_webview_tests PRIVATE hd_browser hd_webview_sdk hd_warnings)
  add_custom_command(TARGET hd_webview_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${webview2_sdk_SOURCE_DIR}/build/native/x64/WebView2Loader.dll"
      "$<TARGET_FILE_DIR:hd_webview_tests>/WebView2Loader.dll")
  add_test(NAME webview COMMAND hd_webview_tests)
  set_tests_properties(webview PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 40)
  add_executable(haedrez WIN32 src/win32/main.c)
  target_compile_definitions(haedrez PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX _WIN32_WINNT=0x0A00)
  target_link_libraries(haedrez PRIVATE hd_core hd_browser hd_warnings user32 gdi32 shell32 comctl32)
  add_custom_command(TARGET haedrez POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${webview2_sdk_SOURCE_DIR}/build/native/x64/WebView2Loader.dll"
      "$<TARGET_FILE_DIR:haedrez>/WebView2Loader.dll")
  add_executable(hd_shell_tests tests/shell_tests.c)
  target_compile_definitions(hd_shell_tests PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX _WIN32_WINNT=0x0A00)
  target_link_libraries(hd_shell_tests PRIVATE hd_warnings user32 gdi32)
  add_test(NAME shell COMMAND hd_shell_tests $<TARGET_FILE:haedrez>)
  set_tests_properties(shell PROPERTIES TIMEOUT 20)
endif()