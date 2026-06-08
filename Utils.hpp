#ifndef UTILS_HPP
#define UTILS_HPP

#include "Server.hpp"
#include <string>
#include <vector>
#include <cstddef>
#include <stdint.h>
#include <map>

bool        file_is_rdy(const char *filename);
void        expectSemicolon(const std::vector<std::string>& tokens, size_t& i);
int         parsePort(const std::string& s);
size_t      parseSize(const std::string& s);
int         parseErrorCode(const std::string& s);
bool        parseOnOff(const std::string& s);
uint32_t    parseHost(const std::string& host);
HttpRequest parseRequest(const std::string& raw);
const LocationConfig* matchLocation(const ServerConfig& server, const std::string& path);
std::string resolvePath(const LocationConfig& loc, const std::string& reqPath);
std::string buildResponse(int code, const std::string& status, const std::string& contentType, const std::string& body);
bool        readFile(const std::string& path, std::string& content);
std::string generateAutoindex(const std::string& dirPath, const std::string& reqPath);
int         pathType(const std::string& path);
std::string getMimeType(const std::string& path);
std::string statusText(int code);
std::string buildError(int code, const ServerConfig& server);
std::string buildRedirect(int code, const std::string& location);
bool        isPathSafe(const std::string& path);
std::string buildCgiResponse(const std::string& cgiOutput);
std::string unchunkBody(const std::string& chunked);
bool isChunkedComplete(const std::string& buffer, size_t body_start);

#endif