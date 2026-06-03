#include <iostream>
#include "Config.hpp"
#include "Server.hpp"

int main(int argc, char *argv[]) {
    if (argc != 2)
        return (std::cerr << "./webserv [conf file]" << std::endl, 1);
    try {
        Config config(argv[1]);
        ServerRunner server(config.servers);
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
    return 0;
}