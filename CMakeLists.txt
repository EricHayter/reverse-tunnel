cmake_minimum_required(VERSION 3.31)
project(reverse-tunnel CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

find_program(CLANG_TIDY_BIN "clang-tidy")
if(NOT CLANG_TIDY_BIN)
    message(WARNING "clang-tidy not found!")
elseif(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "clang-tidy enabled (non-debug build): ${CLANG_TIDY_BIN}")
    set(CMAKE_CXX_CLANG_TIDY ${CLANG_TIDY_BIN})
else()
    message(STATUS "clang-tidy disabled (build type '${CMAKE_BUILD_TYPE}' is not Debug)")
endif()

include_directories(include)

find_program(CLANG_FORMAT_BIN "clang-format")
if(NOT CLANG_FORMAT_BIN)
    message(WARNING "clang-format not found! 'format' target unavailable")
else()
    file(GLOB_RECURSE FORMAT_SOURCES
        "${CMAKE_SOURCE_DIR}/src/*.cpp"
        "${CMAKE_SOURCE_DIR}/src/*.h"
        "${CMAKE_SOURCE_DIR}/include/*.h"
    )
    add_custom_target(format
        COMMAND ${CLANG_FORMAT_BIN} -i --style=file ${FORMAT_SOURCES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Formatting sources with clang-format"
        VERBATIM
    )
endif()

add_subdirectory(src)
add_subdirectory(thirdparty)
