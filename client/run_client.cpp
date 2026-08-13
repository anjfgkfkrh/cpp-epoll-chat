#include "Client.h"

#include <iostream>

int main() {
    std::cout << "[info] create Client" << std::endl;
    try{
    Client client;
    client.run();
    } catch(const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}