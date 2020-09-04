cmake_minimum_required(VERSION 3.2)
set(CMAKE_C_STANDARD 90)

add_library(b0 b0.s)

set_target_properties(b0 PROPERTIES LINKER_LANGUAGE C)

install(TARGETS b0 DESTINATION lib)
