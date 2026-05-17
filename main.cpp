#include <iostream>

int main(int argc, char *argv[]) {
    if (argc != 2)
        return (std::cerr << "./webserv [conf file]" << std::endl, 1);
    
    return 0;
}