#ifndef HTTP_HPP
#define HTTP_HPP

#include <string>
#include <map>

#include "Config.hpp"

struct HttpRequest {
    std::string                        method;
    std::string                        path;
    std::string                        version;
    std::map<std::string, std::string> headers;
    std::string                        body;
    bool                               valid;

    HttpRequest() : valid(false) {}
};

HttpRequest parseRequest(const std::string& raw);
std::string stripQuery(const std::string& path);
std::string unchunkBody(const std::string& chunked);
bool        isChunkedComplete(const std::string& buffer, size_t& scan_pos);

std::string buildResponse(int code, const std::string& status,
                          const std::string& contentType, const std::string& body);
std::string buildError(int code, const ServerConfig& server);
std::string buildRedirect(int code, const std::string& location);
std::string buildCgiResponse(const std::string& cgiOutput);
std::string statusText(int code);
std::string getMimeType(const std::string& path);

#endif
