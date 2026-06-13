cmake_minimum_required(VERSION 3.31)
project(reverse-tunnel CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

include_directories(include)

add_subdirectory(src)
add_subdirectory(thirdparty)
