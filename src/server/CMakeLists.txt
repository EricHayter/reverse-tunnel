add_executable(server
    server.cpp
    main.cpp
    socket_monitor.cpp
)

target_link_libraries(server
    tunnel_common
    argparse
    spdlog
)

