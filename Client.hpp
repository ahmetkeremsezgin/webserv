#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>
#include "Config.hpp"
#include "HttpRequest.hpp"

enum RequestState {
    READING_HEADERS,
    READING_BODY,
    READING_CHUNKED,
    REQUEST_READY
};

class Client {
private:
    int             _fd;
    Server          _serverConfig;
    
    std::string     _requestBuffer;
    std::string     _responseBuffer;
    
    bool            _isRequestReady;
    bool            _isResponseReady;

    HttpRequest     _request;

    RequestState    _state;
    size_t          _headerLength;
    size_t          _contentLength;

public:
    Client(int fd, const Server& config);
    ~Client();

    int getFd() const;
    bool isRequestReady() const;
    bool isResponseReady() const;

    bool readData();
    bool sendData();
    
    void processRequest();
};

#endif