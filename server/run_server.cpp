#include <iostream>

#include "Server.h"

int main() {
    try {
        Server server(8080, 1024);
        server.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}