cmake_minimum_required(VERSION 3.2)
project(b)
set(CMAKE_C_STANDARD 90)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
enable_language(AS)

add_subdirectory(compiler)
add_subdirectory(runtime)

