#ifndef SERVER_HPP
#define SERVER_HPP

#include "Config.hpp"
#include <map>

class ServerRunner {
    private:
        std::map<int, Server> fd_to_server; 
        int    OpenSocket(std::vector<Server> servers);
    public:
        ServerRunner(std::vector<Server> servers);
};

#endif