find_program(
    CMAKE_AS_COMPILER
        NAMES "as"
        HINTS "/usr/bin"
        DOC "Assembler"
)
mark_as_advanced(CMAKE_AS_COMPILER)

set (CMAKE_AS_SOURCE_FILE_EXTENSIONS s)
set (CMAKE_AS_OUTPUT_EXTENSION o)
set (CMAKE_AS_COMPILER_ENV_VAR "AS")

configure_file(${CMAKE_CURRENT_LIST_DIR}/CMakeASCompiler.cmake.in
               ${CMAKE_PLATFORM_INFO_DIR}/CMakeASCompiler.cmake)


