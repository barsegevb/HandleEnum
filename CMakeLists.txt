cmake_minimum_required(VERSION 3.20)
project(HandleEnum LANGUAGES CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(HandleEnum
    src/main.cpp
    src/cli_parser.cpp
)

add_executable(cli_parser_tests
  tests/cli_parser_tests.cpp
  src/cli_parser.cpp
)

target_include_directories(HandleEnum PRIVATE include)
target_include_directories(cli_parser_tests PRIVATE include)

# Windows libs (MinGW)
if (WIN32)
  target_link_libraries(HandleEnum PRIVATE advapi32)
endif()

# Warnings
if (MINGW)
  target_compile_options(HandleEnum PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(cli_parser_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME cli_parser_tests COMMAND cli_parser_tests)
