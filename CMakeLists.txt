cmake_minimum_required(VERSION 3.20)
project(HandleEnum LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(HandleEnum
    src/main.cpp
    src/cli_parser.cpp
    src/nt.cpp
    src/handle_enum.cpp
    src/handle_query.cpp
    src/formatter.cpp
)

target_include_directories(HandleEnum PRIVATE include)

# Windows libs (MinGW)
target_link_libraries(HandleEnum PRIVATE advapi32)

# Warnings
if (MINGW)
  target_compile_options(HandleEnum PRIVATE -Wall -Wextra -Wpedantic)
endif()
