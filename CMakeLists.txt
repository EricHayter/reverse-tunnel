cmake_minimum_required(VERSION 3.31)
project(reverse-tunnel CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

find_program(CLANG_TIDY_BIN "clang-tidy")
if(NOT CLANG_TIDY_BIN)
    message(WARNING "clang-tidy not found!")
elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "clang-tidy enabled (Debug build): ${CLANG_TIDY_BIN}")
    set(CMAKE_CXX_CLANG_TIDY ${CLANG_TIDY_BIN})
else()
    message(STATUS "clang-tidy disabled (build type '${CMAKE_BUILD_TYPE}' is not Debug)")
endif()

include_directories(include)

add_subdirectory(src)
add_subdirectory(thirdparty)
