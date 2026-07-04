add_executable(client
    main.cpp
    args.cpp
    client.cpp
)

target_link_libraries(client
    argparse
    tunnel_common
)
