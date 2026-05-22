#ifndef HTTPRESPONSE_HPP
#define HTTPRESPONSE_HPP

#include "HttpRequest.hpp"
#include "Config.hpp"
#include <string>

class HttpResponse {
private:
    HttpRequest _request;
    Server      _serverConfig;
    
    int         _statusCode;
    std::string _statusMessage;
    std::string _contentType;
    std::string _body;
    std::string _rawResponse;

    void buildResponse();
    void handleGet(const Location* loc);
    
    const Location* matchLocation(const std::string& uri);
    std::string getContentType(const std::string& filePath);
    bool readFile(const std::string& filePath, std::string& content);
    void generateErrorResponse(int code);
    void handlePost(const Location* loc);
    void handleDelete(const Location* loc);
    bool isDirectory(const std::string& path);
    void handleAutoindex(const std::string& targetPath, const std::string& uri);
public:
    HttpResponse(const HttpRequest& req, const Server& config);
    ~HttpResponse();

    std::string getRawResponse() const;
};

#endif