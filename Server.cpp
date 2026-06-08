#include "Server.hpp"
#include "Utils.hpp"

#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <csignal>
#include <sstream>
#include <stdexcept>

extern volatile sig_atomic_t g_running;

bool Server::isListenFd(int fd) const {
    for (size_t i = 0; i < _listen_fds.size(); ++i)
        if (_listen_fds[i] == fd)
            return true;
    return false;
}

bool Server::isCgiFd(int fd) const {
    return _cgi_fd_map.find(fd) != _cgi_fd_map.end();
}

void Server::enablePollOut(int fd) {
    for (size_t i = 0; i < _pfds.size(); ++i) {
        if (_pfds[i].fd == fd) {
            _pfds[i].events |= POLLOUT;
            break;
        }
    }
}

const ServerConfig& Server::selectServer(int listen_fd, const HttpRequest& req) {
    int default_index = 0;
    std::map<int, int>::iterator it = _fd_to_server.find(listen_fd);
    if (it != _fd_to_server.end())
        default_index = it->second;

    std::string host;
    std::map<std::string, std::string>::const_iterator h = req.headers.find("Host");
    if (h != req.headers.end()) {
        host = h->second;
        std::string::size_type colon = host.find(':');
        if (colon != std::string::npos)
            host = host.substr(0, colon);
    }

    int listen_port = servers[default_index].port;
    for (size_t i = 0; i < servers.size(); ++i) {
        if (servers[i].port != listen_port)
            continue;
        for (size_t k = 0; k < servers[i].server_names.size(); ++k) {
            if (servers[i].server_names[k] == host)
                return servers[i];
        }
    }
    return servers[default_index];
}

void Server::acceptConnection(int listen_fd) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    int client_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &client_len);
    if (client_fd == -1)
        return;

    if (fcntl(client_fd, F_SETFL, O_NONBLOCK) == -1) {
        close(client_fd);
        return;
    }

    addToPoll(client_fd, POLLIN);
    Client c(client_fd, listen_fd);
    c.last_activity = time(NULL);
    _clients[client_fd] = c;
}

bool Server::isRequestComplete(const std::string& buffer) {
    std::string::size_type header_end = buffer.find("\r\n\r\n");
    if (header_end == std::string::npos)
        return false;

    if (buffer.find("Transfer-Encoding: chunked") != std::string::npos ||
        buffer.find("Transfer-Encoding: Chunked") != std::string::npos) {
        std::string::size_type body_start = header_end + 4;
        std::string body = buffer.substr(body_start);
        return body.find("0\r\n\r\n") != std::string::npos;
    }

    std::string::size_type cl_pos = buffer.find("Content-Length:");
    if (cl_pos == std::string::npos)
        return true;

    cl_pos += 15;
    std::string::size_type cl_end = buffer.find("\r\n", cl_pos);
    std::string cl_str = buffer.substr(cl_pos, cl_end - cl_pos);
    std::string::size_type start = cl_str.find_first_not_of(" \t");
    if (start != std::string::npos)
        cl_str = cl_str.substr(start);

    std::stringstream ss(cl_str);
    size_t content_length;
    ss >> content_length;

    std::string::size_type body_start = header_end + 4;
    size_t body_received = buffer.size() - body_start;
    return body_received >= content_length;
}

void Server::handleClientData(int client_fd) {
    if (_cgi.find(client_fd) != _cgi.end()) {
        char tmp[4096];
        recv(client_fd, tmp, sizeof(tmp), 0);
        return;
    }

    char buffer[4096];
    ssize_t n = recv(client_fd, buffer, sizeof(buffer), 0);

    if (n <= 0) {
        closeConnection(client_fd);
        return;
    }

    Client& client = _clients[client_fd];
    client.last_activity = time(NULL);
    client.read_buffer.append(buffer, n);

    if (!client.headers_parsed) {
        std::string::size_type he = client.read_buffer.find("\r\n\r\n");
        if (he == std::string::npos)
            return;

        client.header_size = he + 4;
        client.headers_parsed = true;

        std::string head = client.read_buffer.substr(0, he);
        if (head.find("Transfer-Encoding: chunked") != std::string::npos ||
            head.find("Transfer-Encoding: Chunked") != std::string::npos) {
            client.is_chunked = true;
        } else {
            std::string::size_type cl = head.find("Content-Length:");
            if (cl != std::string::npos) {
                cl += 15;
                std::string::size_type ce = head.find("\r\n", cl);
                std::string cs = head.substr(cl, ce - cl);
                std::string::size_type s = cs.find_first_not_of(" \t");
                if (s != std::string::npos) cs = cs.substr(s);
                std::stringstream ss(cs);
                ss >> client.content_length;
            }
        }
    }

    bool complete = false;
    if (client.is_chunked) {
        complete = isChunkedComplete(client.read_buffer, client.header_size);
    } else {
        size_t body_have = client.read_buffer.size() - client.header_size;
        if (body_have >= client.content_length)
            complete = true;
    }

    if (!complete)
        return;

    HttpRequest req = parseRequest(client.read_buffer);
    const ServerConfig& server = selectServer(client.listen_fd, req);

    std::string response = handleRequest(req, server, client_fd);
    if (response.empty())
        return;

    client.write_buffer = response;
    client.response_ready = true;
    enablePollOut(client_fd);
}

void Server::closeConnection(int fd) {
    if (_cgi.find(fd) != _cgi.end())
        closeCgi(fd);

    close(fd);

    for (size_t i = 0; i < _pfds.size(); ++i) {
        if (_pfds[i].fd == fd) {
            _pfds.erase(_pfds.begin() + i);
            break;
        }
    }
    _clients.erase(fd);
}

void Server::handleClientWrite(int client_fd) {
    Client& client = _clients[client_fd];
    client.last_activity = time(NULL);

    if (!client.response_ready)
        return;

    ssize_t sent = send(client_fd, client.write_buffer.c_str(),
                        client.write_buffer.size(), 0);

    if (sent > 0)
        client.write_buffer.erase(0, sent);

    if (sent == 0) {
        closeConnection(client_fd);
        return;
    }

    if (client.write_buffer.empty())
        closeConnection(client_fd);
}

void Server::handleCgiWrite(int fd) {
    int client_fd = _cgi_fd_map[fd];
    CgiProcess& cgi = _cgi[client_fd];

    size_t remaining = cgi.write_buffer.size() - cgi.write_offset;
    ssize_t w = write(fd, cgi.write_buffer.c_str() + cgi.write_offset, remaining);

    if (w > 0)
        cgi.write_offset += w;

    if (cgi.write_offset >= cgi.write_buffer.size()) {
        _cgi_fd_map.erase(fd);
        for (size_t i = 0; i < _pfds.size(); ++i) {
            if (_pfds[i].fd == fd) { _pfds.erase(_pfds.begin() + i); break; }
        }
        close(fd);
        cgi.in_fd = -1;
        cgi.in_done = true;
    }
}


void Server::handleCgiRead(int fd) {
    std::map<int, int>::iterator mit = _cgi_fd_map.find(fd);
    if (mit == _cgi_fd_map.end()) {
        for (size_t i = 0; i < _pfds.size(); ++i)
            if (_pfds[i].fd == fd) { _pfds.erase(_pfds.begin() + i); break; }
        close(fd);
        return;
    }
    int client_fd = mit->second;

    std::map<int, CgiProcess>::iterator cit = _cgi.find(client_fd);
    if (cit == _cgi.end()) {
        _cgi_fd_map.erase(fd);
        for (size_t i = 0; i < _pfds.size(); ++i)
            if (_pfds[i].fd == fd) { _pfds.erase(_pfds.begin() + i); break; }
        close(fd);
        return;
    }
    CgiProcess& cgi = cit->second;

    char buffer[4096];
    ssize_t n = read(fd, buffer, sizeof(buffer));

    if (n > 0) {
        cgi.output.append(buffer, n);
        return;
    }
    if (n < 0)
        return;

    _cgi_fd_map.erase(fd);
    for (size_t i = 0; i < _pfds.size(); ++i)
        if (_pfds[i].fd == fd) { _pfds.erase(_pfds.begin() + i); break; }
    close(fd);
    cgi.out_fd = -1;
    cgi.out_done = true;

    finishCgi(client_fd);
}

void Server::finishCgi(int client_fd) {
    std::map<int, CgiProcess>::iterator cit = _cgi.find(client_fd);
    if (cit == _cgi.end())
        return;
    CgiProcess& cgi = cit->second;

    if (cgi.in_fd != -1) {
        _cgi_fd_map.erase(cgi.in_fd);
        for (size_t i = 0; i < _pfds.size(); ++i) {
            if (_pfds[i].fd == cgi.in_fd) { _pfds.erase(_pfds.begin() + i); break; }
        }
        close(cgi.in_fd);
        cgi.in_fd = -1;
    }

    int status;
    waitpid(cgi.pid, &status, 0);

    std::string response = buildCgiResponse(cgi.output);

    int target = cgi.client_fd;
    _cgi.erase(client_fd);

    std::map<int, Client>::iterator it = _clients.find(target);
    if (it != _clients.end()) {
        it->second.write_buffer = response;
        it->second.response_ready = true;
        enablePollOut(target);
    }
}

void Server::closeCgi(int client_fd) {
    std::map<int, CgiProcess>::iterator it = _cgi.find(client_fd);
    if (it == _cgi.end())
        return;

    CgiProcess& cgi = it->second;

    if (cgi.in_fd != -1) {
        _cgi_fd_map.erase(cgi.in_fd);
        for (size_t i = 0; i < _pfds.size(); ++i)
            if (_pfds[i].fd == cgi.in_fd) { _pfds.erase(_pfds.begin() + i); break; }
        close(cgi.in_fd);
    }
    if (cgi.out_fd != -1) {
        _cgi_fd_map.erase(cgi.out_fd);
        for (size_t i = 0; i < _pfds.size(); ++i)
            if (_pfds[i].fd == cgi.out_fd) { _pfds.erase(_pfds.begin() + i); break; }
        close(cgi.out_fd);
    }

    if (cgi.pid > 0) {
        kill(cgi.pid, SIGKILL);
        waitpid(cgi.pid, NULL, 0);
    }

    _cgi.erase(client_fd);
}

void Server::checkCgiTimeouts() {
    time_t now = time(NULL);
    const time_t CGI_TIMEOUT = 10;

    std::vector<int> timed_out;
    for (std::map<int, CgiProcess>::iterator it = _cgi.begin();
         it != _cgi.end(); ++it) {
        if (now - it->second.start_time > CGI_TIMEOUT)
            timed_out.push_back(it->first);
    }

    for (size_t i = 0; i < timed_out.size(); ++i) {
        int client_fd = timed_out[i];
        closeCgi(client_fd);
        sendErrorToClient(client_fd, 504);
    }
}

void Server::checkTimeouts() {
    time_t now = time(NULL);
    const time_t TIMEOUT = 30;

    std::vector<int> to_close;
    for (std::map<int, Client>::iterator it = _clients.begin();
         it != _clients.end(); ++it) {
        if (now - it->second.last_activity > TIMEOUT)
            to_close.push_back(it->first);
    }

    for (size_t i = 0; i < to_close.size(); ++i)
        closeConnection(to_close[i]);
}

void Server::run() {
    while (g_running) {
        int ready = poll(&_pfds[0], _pfds.size(), 1000);
        if (ready == -1) {
            if (!g_running) break;
            throw std::runtime_error("poll() failed");
        }

        if (ready > 0) {
            for (size_t i = 0; i < _pfds.size(); ++i) {
                int fd = _pfds[i].fd;
                short revents = _pfds[i].revents;

                if (revents & (POLLIN | POLLHUP)) {
                    if (isListenFd(fd))
                        acceptConnection(fd);
                    else if (isCgiFd(fd))
                        handleCgiRead(fd);
                    else
                        handleClientData(fd);
                }
                else if (revents & POLLOUT) {
                    if (isCgiFd(fd))
                        handleCgiWrite(fd);
                    else
                        handleClientWrite(fd);
                }

                if (i < _pfds.size() && _pfds[i].fd != fd)
                    break;
            }
        }

        checkTimeouts();
        checkCgiTimeouts();
    }
}

Server::~Server() {
    for (size_t i = 0; i < _pfds.size(); ++i) {
        if (_pfds[i].fd != -1)
            close(_pfds[i].fd);
    }
}