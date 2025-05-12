#include "server.hpp"
#include <iostream>
#include <stdexcept>

int main(int argc, char* argv[]) {
    try {
        if (argc != 3) {
            std::cerr << "Usage: " << argv[0] << " <port> <instances_file>" << std::endl;
            return 1;
        }
        
        int port = std::stoi(argv[1]);
        std::string instances_file = argv[2];
        
        Server server(port, instances_file);
        server.run();
        
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}