#include "server.hpp"

int main(int argc, char* argv[]) {
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
