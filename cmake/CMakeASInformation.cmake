if(NOT CMAKE_AS_COMPILE_OBJECT)
    set(CMAKE_AS_COMPILE_OBJECT "<CMAKE_AS_COMPILER> -o <OBJECT> -march=i386 --32 <SOURCE>")
endif()
if(NOT CMAKE_AS_CREATE_STATIC_LIBRARY)
    set(CMAKE_AS_CREATE_STATIC_LIBRARY "<CMAKE_AR> rcs <TARGET> <OBJECTS>")
endif()
set(CMAKE_AS_INFORMATION_LOADED 1)

