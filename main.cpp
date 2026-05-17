#include <iostream>
#include "Config.hpp"
#include "Server.hpp"

int main(int argc, char *argv[]) {
    if (argc != 2)
        return (std::cerr << "./webserv [conf file]" << std::endl, 1);
    (void) argv; //Config config(argv[1]); parser tamamlanınca.
    Config config;
    ServerRunner server(config.servers);
    return 0;
}