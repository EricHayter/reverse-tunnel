add_executable(server
    server.cpp
    main.cpp
)

target_link_libraries(server
    tunnel_common
    argparse
    spdlog
)

