cmake_minimum_required(VERSION 3.20)
project(HandleEnum LANGUAGES CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(HandleEnum
  src/app.cpp
    src/main.cpp
    src/cli_parser.cpp
  src/nt.cpp
)

add_executable(cli_parser_tests
  tests/cli_parser_tests.cpp
  src/cli_parser.cpp
)

add_executable(app_tests
  tests/app_tests.cpp
  src/app.cpp
  src/cli_parser.cpp
)

add_executable(nt_tests
  tests/nt_tests.cpp
  src/nt.cpp
)

target_include_directories(HandleEnum PRIVATE include)
target_include_directories(cli_parser_tests PRIVATE include)
target_include_directories(app_tests PRIVATE include)
target_include_directories(nt_tests PRIVATE include)

# Windows libs (MinGW)
target_link_libraries(HandleEnum PRIVATE advapi32)
target_link_libraries(nt_tests PRIVATE advapi32)

# Warnings
if (MINGW)
  target_compile_options(HandleEnum PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(cli_parser_tests PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(app_tests PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(nt_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME cli_parser_tests COMMAND cli_parser_tests)
add_test(NAME app_tests COMMAND app_tests)
add_test(NAME nt_tests COMMAND nt_tests)
