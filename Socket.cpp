#include "Server.hpp"

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <cctype>
#include <iostream>
#include <utility>
#include <sstream>
#include <stdexcept>

static uint32_t parseHost(const std::string& host) {
    if (host.empty() || host == "0.0.0.0")
        return htonl(INADDR_ANY);

    unsigned int parts[4];
    int count = 0;
    std::stringstream ss(host);
    std::string segment;

    while (std::getline(ss, segment, '.')) {
        if (count >= 4)
            throw std::runtime_error("Invalid host (too many parts): " + host);
        if (segment.empty())
            throw std::runtime_error("Invalid host (empty segment): " + host);
        for (size_t k = 0; k < segment.size(); ++k)
            if (!std::isdigit(static_cast<unsigned char>(segment[k])))
                throw std::runtime_error("Invalid host (non-digit): " + host);

        std::stringstream conv(segment);
        unsigned int val;
        conv >> val;
        if (val > 255)
            throw std::runtime_error("Invalid host (octet > 255): " + host);

        parts[count] = val;
        ++count;
    }

    if (count != 4)
        throw std::runtime_error("Invalid host (need 4 parts): " + host);

    uint32_t addr = (parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8) | parts[3];
    return htonl(addr);
}

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
    std::map<std::pair<std::string, int>, bool> bound;

    for (size_t i = 0; i < _servers.size(); ++i) {
        std::pair<std::string, int> key(_servers[i].host, _servers[i].port);
        if (bound.find(key) != bound.end())
            continue;
        bound[key] = true;

        int fd = createListenSocket(_servers[i].host, _servers[i].port);

        _listen_fds.push_back(fd);
        _fd_to_server[fd] = static_cast<int>(i);
        addToPoll(fd, POLLIN);

        std::cout << "Listening on port " << _servers[i].port << " (fd " << fd << ")" << std::endl;
    }
}

void Server::addToPoll(int fd, short events) {
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = events;
    pfd.revents = 0;
    _pfds.push_back(pfd);
}

void Server::removeFromPoll(int fd) {
    for (size_t i = 0; i < _pfds.size(); ++i) {
        if (_pfds[i].fd == fd) {
            _pfds.erase(_pfds.begin() + i);
            break;
        }
    }
}

void Server::enablePollOut(int fd) {
    for (size_t i = 0; i < _pfds.size(); ++i) {
        if (_pfds[i].fd == fd) {
            _pfds[i].events |= POLLOUT;
            break;
        }
    }
}
