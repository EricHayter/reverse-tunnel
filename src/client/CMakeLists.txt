add_executable(client
    main.cpp
    file_parsing.cpp
    list_parsing.cpp
)

target_link_libraries(client
    argparse
    tunnel
)
