cmake_minimum_required(VERSION 3.2)
set(CMAKE_C_STANDARD 90)

find_package(FLEX REQUIRED)

flex_target(scanner b.l ${CMAKE_CURRENT_BINARY_DIR}/scanner.c)

add_executable(b b.c)
add_executable(bc bif.c bc.c ${CMAKE_CURRENT_BINARY_DIR}/scanner.c)
add_executable(ba ba.c)

target_include_directories(bc PRIVATE .)

install(TARGETS b bc ba DESTINATION bin)
install(FILES blink DESTINATION share/b)
