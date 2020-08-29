cmake_minimum_required(VERSION 3.2)
project(cc)
set(CMAKE_C_STANDARD 90)

add_executable(b bparse.c)

install(TARGETS b DESTINATION bin)
