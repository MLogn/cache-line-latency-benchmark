cmake_minimum_required(VERSION 3.16)

project(cache_line_latency LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

add_executable(cache_line_latency main.cpp)
target_link_libraries(cache_line_latency PRIVATE Threads::Threads)

target_compile_options(cache_line_latency PRIVATE -O3 -Wall -Wextra -Wpedantic)
