#include <iostream>
#include "Config.hpp"
#include "Server.hpp"

int main(int argc, char *argv[]) {
    if (argc != 2)
        return (std::cerr << "./webserv [conf file]" << std::endl, 1);
    Config config(argv[1]);
    ServerRunner server(config.servers);
    return 0;
}