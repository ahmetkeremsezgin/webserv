#include "Client.hpp"
#include <unistd.h>
#include <sys/socket.h>
#include <iostream>
#include <sstream>
#include "HttpResponse.hpp"
#include <cstdlib>

Client::Client(int fd, const Server& config) 
    : _fd(fd), _serverConfig(config), _isRequestReady(false), _isResponseReady(false),
      _state(READING_HEADERS), _headerLength(0), _contentLength(0) {
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
    char buffer[4096];
    int bytesRead = read(_fd, buffer, sizeof(buffer) - 1);

    if (bytesRead <= 0) {
        return false;
    }

    buffer[bytesRead] = '\0';
    _requestBuffer.append(buffer, bytesRead);

    if (_state == READING_HEADERS) {
        size_t headerEnd = _requestBuffer.find("\r\n\r\n");
        if (headerEnd != std::string::npos) {
            _headerLength = headerEnd + 4;
            
            std::string headerPart = _requestBuffer.substr(0, headerEnd);
            
            if (headerPart.find("Transfer-Encoding: chunked") != std::string::npos) {
                _state = READING_CHUNKED;
            } 
            else {
                size_t clPos = headerPart.find("Content-Length: ");
                if (clPos != std::string::npos) {
                    size_t clEnd = headerPart.find("\r\n", clPos);
                    std::string clStr = headerPart.substr(clPos + 16, clEnd - (clPos + 16));
                    _contentLength = std::atoi(clStr.c_str());
                    _state = READING_BODY;
                } else {
                    _state = REQUEST_READY;
                }
            }
        }
    }

    if (_state == READING_BODY) {
        if (_requestBuffer.length() >= _headerLength + _contentLength) {
            _state = REQUEST_READY;
        }
    }

    if (_state == READING_CHUNKED) {
        if (_requestBuffer.find("0\r\n\r\n") != std::string::npos) {
            _state = REQUEST_READY;
        }
    }

    if (_state == REQUEST_READY) {
        _isRequestReady = true;
    }

    return true;
}

void Client::processRequest() {
    _request.parse(_requestBuffer);

    if (!_request.isParsed()) {
        _responseBuffer = "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n";
        _isResponseReady = true;
        return;
    }

    HttpResponse response(_request, _serverConfig);
    _responseBuffer = response.getRawResponse();

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