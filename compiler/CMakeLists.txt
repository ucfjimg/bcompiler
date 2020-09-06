cmake_minimum_required(VERSION 3.2)
set(CMAKE_C_STANDARD 90)

add_executable(b b.c)
add_executable(bc bif.c bc.c lex.c)
add_executable(ba ba.c)

install(TARGETS b bc ba DESTINATION bin)
install(FILES blink DESTINATION share/b)
