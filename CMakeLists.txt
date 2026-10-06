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