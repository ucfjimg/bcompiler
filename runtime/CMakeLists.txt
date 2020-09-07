cmake_minimum_required(VERSION 3.2)
set(CMAKE_C_STANDARD 90)

add_library(brt b0.s blib.s bsys.s bstr.s)
install(TARGETS brt DESTINATION lib)
