#include "Server.hpp"
#include "Utils.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <sstream>
#include <stdexcept>

int Server::createListenSocket(const std::string& host, int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);

    if (fd == -1)
        throw std::runtime_error("socket() failed");

    int opt = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        close(fd);
        throw std::runtime_error("setsockopt() failed");
    }

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = parseHost(host);

    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        close(fd);
        std::stringstream ss;
        ss << "bind() failed on port " << port;
        throw std::runtime_error(ss.str());
    }

    if (listen(fd, 128) == -1) {
        close(fd);
        throw std::runtime_error("listen() failed");
    }

    if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1) {
        close(fd);
        throw std::runtime_error("fcntl() failed");
    }

    return fd;
}

void Server::setupSockets() {
    for (size_t i = 0; i < servers.size(); ++i) {
        int fd = createListenSocket(servers[i].host, servers[i].port);

        _listen_fds.push_back(fd);
        _fd_to_server[fd] = static_cast<int>(i);
        addToPoll(fd, POLLIN);

        std::cout << "Listening on port " << servers[i].port << " (fd " << fd << ")" << std::endl;
    }
}

void Server::addToPoll(int fd, short events) {
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = events;
    pfd.revents = 0;
    _pfds.push_back(pfd);
}