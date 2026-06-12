#ifndef FILEUTILS_HPP
#define FILEUTILS_HPP

#include <string>

#include "Config.hpp"

enum PathType {
    PATH_NOT_FOUND = 0,
    PATH_FILE      = 1,
    PATH_DIRECTORY = 2
};

PathType    pathType(const std::string& path);
bool        readFile(const std::string& path, std::string& content);
bool        isPathSafe(const std::string& path);

const LocationConfig* matchLocation(const ServerConfig& server, const std::string& path);
std::string resolvePath(const LocationConfig& loc, const std::string& reqPath);
std::string generateAutoindex(const std::string& dirPath, const std::string& reqPath);

#endif
