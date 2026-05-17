#ifndef SERVER_HPP
#define SERVER_HPP

#include "Config.hpp"

class ServerRunner {
    private:
        int    OpenSocket(Server servers);
    public:
        ServerRunner(std::vector<Server> servers);
};

#endif