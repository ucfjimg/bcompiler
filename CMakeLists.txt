cmake_minimum_required(VERSION 3.2)
project(cc)
set(CMAKE_C_STANDARD 90)

add_executable(bc bif.c bc.c lex.c)

install(TARGETS bc DESTINATION bin)
