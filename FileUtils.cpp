#include "FileUtils.hpp"

#include <dirent.h>
#include <sys/stat.h>
#include <fstream>
#include <sstream>

PathType pathType(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0)
        return PATH_NOT_FOUND;
    if (S_ISDIR(st.st_mode))
        return PATH_DIRECTORY;
    if (S_ISREG(st.st_mode))
        return PATH_FILE;
    return PATH_NOT_FOUND;
}

bool readFile(const std::string& path, std::string& content) {
    std::ifstream file(path.c_str(), std::ios::binary);
    if (!file.is_open())
        return false;

    std::stringstream ss;
    ss << file.rdbuf();
    content = ss.str();
    return true;
}

bool isPathSafe(const std::string& path) {
    int depth = 0;
    std::stringstream ss(path);
    std::string segment;

    while (std::getline(ss, segment, '/')) {
        if (segment.empty() || segment == ".")
            continue;
        if (segment == "..") {
            if (--depth < 0)
                return false;
        } else {
            ++depth;
        }
    }
    return true;
}

const LocationConfig* matchLocation(const ServerConfig& server, const std::string& path) {
    const LocationConfig* best = NULL;
    size_t best_len = 0;

    for (size_t i = 0; i < server.locations.size(); ++i) {
        const std::string& loc_path = server.locations[i].path;

        if (path.compare(0, loc_path.size(), loc_path) == 0 &&
            loc_path.size() > best_len) {
            best = &server.locations[i];
            best_len = loc_path.size();
        }
    }
    return best;
}

std::string resolvePath(const LocationConfig& loc, const std::string& reqPath) {
    std::string rest = reqPath.substr(loc.path.size());
    std::string full = loc.root;

    if (!full.empty() && full[full.size() - 1] == '/' && !rest.empty() && rest[0] == '/')
        rest = rest.substr(1);
    else if (!full.empty() && full[full.size() - 1] != '/' && !rest.empty() && rest[0] != '/')
        full += "/";

    return full + rest;
}

std::string generateAutoindex(const std::string& dirPath, const std::string& reqPath) {
    DIR* dir = opendir(dirPath.c_str());
    if (!dir)
        return "";

    std::stringstream html;
    html << "<html><head><title>Index of " << reqPath << "</title></head><body>";
    html << "<h1>Index of " << reqPath << "</h1><hr><ul>";

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        std::string name = entry->d_name;
        if (name == ".")
            continue;

        std::string href = reqPath;
        if (href.empty() || href[href.size() - 1] != '/')
            href += "/";
        href += name;

        html << "<li><a href=\"" << href << "\">" << name << "</a></li>";
    }

    html << "</ul><hr></body></html>";
    closedir(dir);
    return html.str();
}
