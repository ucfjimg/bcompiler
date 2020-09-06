cmake_minimum_required(VERSION 3.2)
set(CMAKE_C_STANDARD 90)

add_library(b0 b0.s)
add_library(brt bn.s blib.s bsys.s bstr.s)
install(TARGETS b0 brt DESTINATION lib)
