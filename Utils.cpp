#include "Server.hpp"
#include "Utils.hpp"

#include <dirent.h>
#include <sys/stat.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sstream>
#include <cctype>
#include <stdexcept>
#include <stdint.h>
#include <netinet/in.h>
#include <fstream>

bool file_is_rdy(const char *filename) {
    struct stat st;

    if (stat(filename, &st) != 0)
        return false;

    if (!S_ISREG(st.st_mode))
        return false;

    if (access(filename, R_OK) != 0)
        return false;

    return true;
}

void expectSemicolon(const std::vector<std::string>& tokens, size_t& i) {
    if (i >= tokens.size() || tokens[i] != ";")
        throw std::runtime_error("Expected ';'");
    ++i; 
}

int parsePort(const std::string& s) {
    for (size_t k = 0; k < s.size(); ++k)
        if (!std::isdigit(s[k]))
            throw std::runtime_error("Invalid port: " + s);

    std::stringstream ss(s);
    long port;
    ss >> port;
    if (port < 1 || port > 65535)
        throw std::runtime_error("Port out of range: " + s);
    return static_cast<int>(port);
}

size_t parseSize(const std::string& s) {
    for (size_t k = 0; k < s.size(); ++k)
        if (!std::isdigit(s[k]))
            throw std::runtime_error("Invalid size: " + s);

    std::stringstream ss(s);
    size_t size;
    ss >> size;
    return size;
}

int parseErrorCode(const std::string& s) {
    for (size_t k = 0; k < s.size(); ++k)
        if (!std::isdigit(s[k]))
            throw std::runtime_error("Invalid error code: " + s);

    std::stringstream ss(s);
    int code;
    ss >> code;
    if (code < 100 || code > 599)
        throw std::runtime_error("Error code out of range: " + s);
    return code;
}

bool parseOnOff(const std::string& s) {
    if (s == "on")
        return true;
    if (s == "off")
        return false;
    throw std::runtime_error("Expected 'on' or 'off', got: " + s);
}

uint32_t parseHost(const std::string& host) {
    if (host.empty() || host == "0.0.0.0")
        return htonl(INADDR_ANY);

    unsigned int parts[4];
    int count = 0;
    std::stringstream ss(host);
    std::string segment;

    while (std::getline(ss, segment, '.')) {
        if (count >= 4)
            throw std::runtime_error("Invalid host (too many parts): " + host);

        if (segment.empty())
            throw std::runtime_error("Invalid host (empty segment): " + host);
        for (size_t k = 0; k < segment.size(); ++k)
            if (!std::isdigit(static_cast<unsigned char>(segment[k])))
                throw std::runtime_error("Invalid host (non-digit): " + host);

        std::stringstream conv(segment);
        unsigned int val;
        conv >> val;
        if (val > 255)
            throw std::runtime_error("Invalid host (octet > 255): " + host);

        parts[count] = val;
        ++count;
    }

    if (count != 4)
        throw std::runtime_error("Invalid host (need 4 parts): " + host);
    uint32_t addr = (parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8) | parts[3];

    return htonl(addr);
}

const LocationConfig* matchLocation(const ServerConfig& server, const std::string& path) {
    const LocationConfig* best = NULL;
    size_t best_len = 0;

    for (size_t i = 0; i < server.locations.size(); ++i) {
        const std::string& loc_path = server.locations[i].path;

        if (path.compare(0, loc_path.size(), loc_path) == 0) {
            if (loc_path.size() > best_len) {
                best = &server.locations[i];
                best_len = loc_path.size();
            }
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

bool readFile(const std::string& path, std::string& content) {
    std::ifstream file(path.c_str(), std::ios::binary);
    if (!file.is_open())
        return false;

    std::stringstream ss;
    ss << file.rdbuf();
    content = ss.str();
    file.close();
    return true;
}

std::string buildResponse(int code, const std::string& status,
                          const std::string& contentType, const std::string& body) {
    std::stringstream res;
    res << "HTTP/1.1 " << code << " " << status << "\r\n";
    res << "Content-Type: " << contentType << "\r\n";
    res << "Content-Length: " << body.size() << "\r\n";
    res << "Connection: close\r\n";
    res << "\r\n";
    res << body;
    return res.str();
}

int pathType(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0)
        return 0;
    if (S_ISDIR(st.st_mode))
        return 2;
    if (S_ISREG(st.st_mode))
        return 1;
    return 0;
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

std::string getMimeType(const std::string& path) {
    std::string::size_type dot = path.rfind('.');
    if (dot == std::string::npos)
        return "application/octet-stream";

    std::string ext = path.substr(dot + 1);

    if (ext == "html" || ext == "htm")  return "text/html";
    if (ext == "css")                   return "text/css";
    if (ext == "js")                    return "application/javascript";
    if (ext == "json")                  return "application/json";
    if (ext == "txt")                   return "text/plain";
    if (ext == "jpg" || ext == "jpeg")  return "image/jpeg";
    if (ext == "png")                   return "image/png";
    if (ext == "gif")                   return "image/gif";
    if (ext == "svg")                   return "image/svg+xml";
    if (ext == "ico")                   return "image/x-icon";
    if (ext == "pdf")                   return "application/pdf";

    return "application/octet-stream";
}

std::string statusText(int code) {
    switch (code) {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 400: return "Bad Request";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 413: return "Payload Too Large";
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        case 504: return "Gateway Timeout";
        default:  return "Unknown";
    }
}

std::string buildError(int code, const ServerConfig& server) {
    std::map<int, std::string>::const_iterator it = server.error_pages.find(code);
    if (it != server.error_pages.end()) {
        std::string content;
        if (readFile(it->second, content))
            return buildResponse(code, statusText(code), "text/html", content);
    }

    std::string text = statusText(code);
    std::stringstream body;
    body << "<html><head><title>" << code << " " << text << "</title></head>";
    body << "<body><center><h1>" << code << " " << text << "</h1></center>";
    body << "<hr><center>webserv</center></body></html>";
    return buildResponse(code, text, "text/html", body.str());
}

std::string buildRedirect(int code, const std::string& location) {
    std::string text = statusText(code);

    std::stringstream body;
    body << "<html><head><title>" << code << " " << text << "</title></head>";
    body << "<body><center><h1>" << code << " " << text << "</h1></center>";
    body << "<hr><center>webserv</center></body></html>";
    std::string bodyStr = body.str();

    std::stringstream res;
    res << "HTTP/1.1 " << code << " " << text << "\r\n";
    res << "Location: " << location << "\r\n";
    res << "Content-Type: text/html\r\n";
    res << "Content-Length: " << bodyStr.size() << "\r\n";
    res << "Connection: close\r\n";
    res << "\r\n";
    res << bodyStr;
    return res.str();
}

bool isPathSafe(const std::string& path) {
    int depth = 0;
    std::stringstream ss(path);
    std::string segment;

    while (std::getline(ss, segment, '/')) {
        if (segment.empty() || segment == ".")
            continue;
        if (segment == "..") {
            depth--;
            if (depth < 0)
                return false;
        }
        else {
            depth++;
        }
    }
    return true;
}

std::string unchunkBody(const std::string& chunked) {
    std::string result;
    std::string::size_type pos = 0;

    while (pos < chunked.size()) {
        std::string::size_type line_end = chunked.find("\r\n", pos);
        if (line_end == std::string::npos)
            break;

        std::string size_str = chunked.substr(pos, line_end - pos);

        std::stringstream ss;
        ss << std::hex << size_str;
        size_t chunk_size;
        ss >> chunk_size;

        if (chunk_size == 0)
            break;

        std::string::size_type data_start = line_end + 2;
        if (data_start + chunk_size > chunked.size())
            break;

        result.append(chunked, data_start, chunk_size);

        pos = data_start + chunk_size + 2;
    }

    return result;
}

HttpRequest parseRequest(const std::string& raw) {
    HttpRequest req;

    std::string::size_type header_end = raw.find("\r\n\r\n");
    std::string header_part;
    std::string body_part;

    if (header_end != std::string::npos) {
        header_part = raw.substr(0, header_end);
        body_part = raw.substr(header_end + 4);
    } else {
        header_part = raw;
    }

    std::istringstream stream(header_part);
    std::string line;

    if (!std::getline(stream, line))
        return req;
    if (!line.empty() && line[line.size() - 1] == '\r')
        line.erase(line.size() - 1);
    {
        std::istringstream ls(line);
        if (!(ls >> req.method >> req.path >> req.version))
            return req;
    }

    while (std::getline(stream, line)) {
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);
        if (line.empty())
            break;

        std::string::size_type colon = line.find(':');
        if (colon == std::string::npos)
            continue;

        std::string name = line.substr(0, colon);
        std::string value = line.substr(colon + 1);
        std::string::size_type start = value.find_first_not_of(" \t");
        if (start != std::string::npos)
            value = value.substr(start);
        else
            value = "";
        req.headers[name] = value;
    }

    std::map<std::string, std::string>::const_iterator te = req.headers.find("Transfer-Encoding");
    if (te != req.headers.end() && te->second.find("chunked") != std::string::npos)
        req.body = unchunkBody(body_part);
    else
        req.body = body_part;

    req.valid = true;
    return req;
}

bool isChunkedComplete(const std::string& buffer, size_t body_start) {
    size_t pos = body_start;

    while (pos < buffer.size()) {
        std::string::size_type line_end = buffer.find("\r\n", pos);
        if (line_end == std::string::npos)
            return false;

        std::string size_str = buffer.substr(pos, line_end - pos);
        std::stringstream ss;
        ss << std::hex << size_str;
        size_t chunk_size;
        ss >> chunk_size;

        if (chunk_size == 0)
            return true;

        size_t data_start = line_end + 2;
        size_t next = data_start + chunk_size + 2;
        if (next > buffer.size())
            return false;

        pos = next;
    }
    return false;
}