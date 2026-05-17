#include <iostream>
#include "Config.hpp"


int main(int argc, char *argv[]) {
    if (argc != 2)
        return (std::cerr << "./webserv [conf file]" << std::endl, 1);
    (void) argv; //Config config(argv[1]); parser tamamlanınca.
    Config config;
    
    //std::cout << config.servers[0].interface << std::endl;
    return 0;
}