#include "Client.h"

#include <iostream>
#include <cstdlib>

int main(int argc, char** argv) {
    uint16_t port = 8080;
    if (argc > 1)
        port = static_cast<uint16_t>(std::atoi(argv[1]));

    try {
        Client client(port);
        if (!client.is_connected())
            return 1;
        client.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
