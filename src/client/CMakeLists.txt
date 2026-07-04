add_executable(client
    main.cpp
    args.cpp
    connection.cpp
)

target_link_libraries(client
    argparse
    tunnel_common
)
