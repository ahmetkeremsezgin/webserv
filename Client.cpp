#include "Client.hpp"
#include <unistd.h>
#include <sys/socket.h>
#include <iostream>

Client::Client(int fd, const Server& config) 
    : _fd(fd), _serverConfig(config), _isRequestReady(false), _isResponseReady(false) {
}

Client::~Client() {
    close(_fd);
}

int Client::getFd() const {
    return _fd;
}

bool Client::isRequestReady() const {
    return _isRequestReady;
}

bool Client::isResponseReady() const {
    return _isResponseReady;
}

bool Client::readData() {
    char buffer[1024];
    int bytesRead = read(_fd, buffer, sizeof(buffer) - 1);

    if (bytesRead <= 0) {
        return false;
    }

    buffer[bytesRead] = '\0';
    _requestBuffer += buffer;

    if (_requestBuffer.find("\r\n\r\n") != std::string::npos) {
        _isRequestReady = true;
    }

    return true;
}

void Client::processRequest() {
    std::cout << "Gelen Istek:\n" << _requestBuffer << std::endl;

    std::string body = "<h1>Merhaba, yeni Client sinifindan geliyorum!</h1>";
    
    _responseBuffer = "HTTP/1.1 200 OK\r\n";
    _responseBuffer += "Content-Type: text/html\r\n";
    _responseBuffer += "Content-Length: 51\r\n";
    _responseBuffer += "\r\n";
    _responseBuffer += body;

    _isResponseReady = true;
}

bool Client::sendData() {
    if (_responseBuffer.empty()) return true;

    int bytesSent = send(_fd, _responseBuffer.c_str(), _responseBuffer.length(), 0);
    
    if (bytesSent <= 0) {
        return false;
    }

    _responseBuffer.erase(0, bytesSent);

    if (_responseBuffer.empty()) {
        return false;
    }

    return true;
}