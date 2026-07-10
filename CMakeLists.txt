cmake_minimum_required(VERSION 3.31)
project(reverse-tunnel CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

find_program(CLANG_TIDY_BIN "clang-tidy")
if(CLANG_TIDY_BIN)
    message(STATUS "clang-tidy found: ${CLANG_TIDY_BIN}")
    set(CMAKE_CXX_CLANG_TIDY ${CLANG_TIDY_BIN})
else()
    message(WARNING "clang-tidy not found!")
endif()

include_directories(include)

add_subdirectory(src)
add_subdirectory(thirdparty)
