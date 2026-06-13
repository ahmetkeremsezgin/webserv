#include "Server.hpp"

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <csignal>
#include <sstream>
#include <stdexcept>

extern volatile sig_atomic_t g_running;

static const int    POLL_TIMEOUT_MS  = 1000;
static const time_t CLIENT_TIMEOUT_S = 30;
static const size_t IO_BUFFER_SIZE   = 65536;

static bool hasChunkedEncoding(const std::string& head) {
    return head.find("Transfer-Encoding: chunked") != std::string::npos ||
           head.find("Transfer-Encoding: Chunked") != std::string::npos;
}

static size_t parseContentLength(const std::string& head) {
    std::string::size_type cl = head.find("Content-Length:");
    if (cl == std::string::npos)
        return 0;

    cl += 15;
    std::string::size_type end = head.find("\r\n", cl);
    std::string value = head.substr(cl, end - cl);
    std::string::size_type start = value.find_first_not_of(" \t");
    if (start != std::string::npos)
        value = value.substr(start);

    std::stringstream ss(value);
    size_t length = 0;
    ss >> length;
    return length;
}

bool Server::isListenFd(int fd) const {
    for (size_t i = 0; i < _listen_fds.size(); ++i)
        if (_listen_fds[i] == fd)
            return true;
    return false;
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

    int listen_port = _servers[default_index].port;
    for (size_t i = 0; i < _servers.size(); ++i) {
        if (_servers[i].port != listen_port)
            continue;
        for (size_t k = 0; k < _servers[i].server_names.size(); ++k) {
            if (_servers[i].server_names[k] == host)
                return _servers[i];
        }
    }
    return _servers[default_index];
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
    _clients[client_fd] = Client(listen_fd);
}

void Server::handleClientData(int client_fd) {
    if (_cgi.find(client_fd) != _cgi.end()) {
        char tmp[IO_BUFFER_SIZE];
        recv(client_fd, tmp, sizeof(tmp), 0);
        return;
    }

    char buffer[IO_BUFFER_SIZE];
    ssize_t n = recv(client_fd, buffer, sizeof(buffer), 0);
    if (n <= 0) {
        closeConnection(client_fd);
        return;
    }

    Client& client = _clients[client_fd];
    client.last_activity = time(NULL);
    client.read_buffer.append(buffer, n);

    if (!client.headers_parsed) {
        std::string::size_type header_end = client.read_buffer.find("\r\n\r\n");
        if (header_end == std::string::npos)
            return;

        client.header_size = header_end + 4;
        client.headers_parsed = true;
        client.chunk_scan_pos = client.header_size;

        std::string head = client.read_buffer.substr(0, header_end);
        if (hasChunkedEncoding(head)) {
            client.is_chunked = true;
        } else {
            client.content_length = parseContentLength(head);
            client.read_buffer.reserve(client.header_size + client.content_length);
        }
    }

    bool complete;
    if (client.is_chunked) {
        complete = isChunkedComplete(client.read_buffer, client.chunk_scan_pos);
    } else {
        size_t body_have = client.read_buffer.size() - client.header_size;
        complete = (body_have >= client.content_length);
    }
    if (!complete)
        return;

    HttpRequest req = parseRequest(client.read_buffer);
    std::string().swap(client.read_buffer);

    const ServerConfig& server = selectServer(client.listen_fd, req);

    std::string response = handleRequest(req, server, client_fd);
    if (!response.empty())
        queueResponse(client_fd, response);
}

void Server::queueResponse(int client_fd, std::string& response) {
    std::map<int, Client>::iterator it = _clients.find(client_fd);
    if (it == _clients.end())
        return;

    it->second.write_buffer.swap(response);
    it->second.write_offset = 0;
    it->second.response_ready = true;
    enablePollOut(client_fd);
}

void Server::handleClientWrite(int client_fd) {
    Client& client = _clients[client_fd];
    client.last_activity = time(NULL);

    if (!client.response_ready)
        return;

    ssize_t sent = send(client_fd, client.write_buffer.c_str() + client.write_offset,
                        client.write_buffer.size() - client.write_offset, 0);

    if (sent <= 0) {
        closeConnection(client_fd);
        return;
    }

    client.write_offset += sent;
    if (client.write_offset >= client.write_buffer.size())
        closeConnection(client_fd);
}

void Server::closeConnection(int fd) {
    if (_cgi.find(fd) != _cgi.end())
        closeCgi(fd);

    close(fd);
    removeFromPoll(fd);
    _clients.erase(fd);
    _closed_fds.insert(fd);
}

void Server::checkTimeouts() {
    time_t now = time(NULL);

    std::vector<int> to_close;
    for (std::map<int, Client>::iterator it = _clients.begin();
         it != _clients.end(); ++it) {
        if (now - it->second.last_activity > CLIENT_TIMEOUT_S)
            to_close.push_back(it->first);
    }

    for (size_t i = 0; i < to_close.size(); ++i)
        closeConnection(to_close[i]);
}

void Server::run() {
    while (g_running) {
        int ready = poll(&_pfds[0], _pfds.size(), POLL_TIMEOUT_MS);
        if (ready == -1) {
            if (!g_running)
                break;
            throw std::runtime_error("poll() failed");
        }

        if (ready > 0) {
            _closed_fds.clear();
            std::vector<struct pollfd> snapshot(_pfds);

            for (size_t i = 0; i < snapshot.size(); ++i) {
                int fd = snapshot[i].fd;
                short revents = snapshot[i].revents;

                if (revents == 0 || _closed_fds.find(fd) != _closed_fds.end())
                    continue;

                if (revents & (POLLIN | POLLHUP | POLLERR)) {
                    if (isListenFd(fd))
                        acceptConnection(fd);
                    else if (isCgiFd(fd))
                        handleCgiRead(fd);
                    else if (_clients.find(fd) != _clients.end())
                        handleClientData(fd);
                }
                else if (revents & POLLOUT) {
                    if (isCgiFd(fd))
                        handleCgiWrite(fd);
                    else if (_clients.find(fd) != _clients.end())
                        handleClientWrite(fd);
                }
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
