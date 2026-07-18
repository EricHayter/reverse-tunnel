add_library(tunnel_common
    parsing.cpp
    error.cpp
    sock_helper.cpp
)

target_link_libraries(tunnel_common
    spdlog
)

