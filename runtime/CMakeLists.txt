cmake_minimum_required(VERSION 3.2)
set(CMAKE_C_STANDARD 90)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/rt.i 
    COMMAND bc ARGS ${CMAKE_CURRENT_SOURCE_DIR}/rt.b
    MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/rt.b
)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/rt.s 
    COMMAND ba ARGS ${CMAKE_CURRENT_BINARY_DIR}/rt.i
    MAIN_DEPENDENCY ${CMAKE_CURRENT_BINARY_DIR}/rt.i
)

add_library(brt b0.s blib.s bsys.s bstr.s ${CMAKE_CURRENT_BINARY_DIR}/rt.s)
install(TARGETS brt DESTINATION lib)
