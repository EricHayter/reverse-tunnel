#include "server/server.h"
#include <iostream>

int main() {
    std::cout << "Starting server!\n";
    Server server{ 4 };
    auto err = server.handle_handshake();
}
