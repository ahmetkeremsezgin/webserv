#include "Server.hpp"

#include <sys/stat.h>
#include <unistd.h>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cctype>

static bool isReadableFile(const char* filename) {
    struct stat st;
    if (stat(filename, &st) != 0)
        return false;
    if (!S_ISREG(st.st_mode))
        return false;
    return access(filename, R_OK) == 0;
}

static void expectSemicolon(const std::vector<std::string>& tokens, size_t& i) {
    if (i >= tokens.size() || tokens[i] != ";")
        throw std::runtime_error("Expected ';'");
    ++i;
}

static void requireDigits(const std::string& s, const std::string& what) {
    for (size_t k = 0; k < s.size(); ++k)
        if (!std::isdigit(static_cast<unsigned char>(s[k])))
            throw std::runtime_error("Invalid " + what + ": " + s);
}

static int parsePort(const std::string& s) {
    requireDigits(s, "port");

    std::stringstream ss(s);
    long port;
    ss >> port;
    if (port < 1 || port > 65535)
        throw std::runtime_error("Port out of range: " + s);
    return static_cast<int>(port);
}

static size_t parseSize(const std::string& s) {
    requireDigits(s, "size");

    std::stringstream ss(s);
    size_t size;
    ss >> size;
    return size;
}

static int parseHttpCode(const std::string& s) {
    requireDigits(s, "HTTP code");

    std::stringstream ss(s);
    int code;
    ss >> code;
    if (code < 100 || code > 599)
        throw std::runtime_error("HTTP code out of range: " + s);
    return code;
}

static bool parseOnOff(const std::string& s) {
    if (s == "on")
        return true;
    if (s == "off")
        return false;
    throw std::runtime_error("Expected 'on' or 'off', got: " + s);
}

static std::string takeValue(const std::vector<std::string>& tokens, size_t& i,
                             const char* directive) {
    ++i;
    if (i >= tokens.size())
        throw std::runtime_error(std::string("Expected value after '") + directive + "'");
    std::string value = tokens[i];
    ++i;
    expectSemicolon(tokens, i);
    return value;
}

std::vector<std::string> Server::tokenize(const std::string& content) {
    std::vector<std::string> tokens;

    std::string cleaned;
    for (size_t i = 0; i < content.size(); ++i) {
        if (content[i] == '#') {
            while (i < content.size() && content[i] != '\n')
                ++i;
        }
        if (i < content.size())
            cleaned += content[i];
    }

    std::string spaced;
    for (size_t i = 0; i < cleaned.size(); ++i) {
        char c = cleaned[i];
        if (c == ';' || c == '{' || c == '}') {
            spaced += ' ';
            spaced += c;
            spaced += ' ';
        } else {
            spaced += c;
        }
    }

    std::istringstream iss(spaced);
    std::string tok;
    while (iss >> tok)
        tokens.push_back(tok);

    return tokens;
}

void Server::parseGlobal(const std::vector<std::string>& tokens, size_t& i) {
    while (i < tokens.size()) {
        if (tokens[i] != "server")
            throw std::runtime_error("Unexpected token in global scope: " + tokens[i]);
        ++i;

        if (i >= tokens.size() || tokens[i] != "{")
            throw std::runtime_error("Expected '{' after 'server'");
        ++i;

        ServerConfig server;
        parseServer(tokens, i, server);
        _servers.push_back(server);
    }
}

void Server::parseServer(const std::vector<std::string>& tokens, size_t& i, ServerConfig& server) {
    while (i < tokens.size() && tokens[i] != "}") {
        const std::string& directive = tokens[i];

        if (directive == "host") {
            server.host = takeValue(tokens, i, "host");
        }
        else if (directive == "listen") {
            server.port = parsePort(takeValue(tokens, i, "listen"));
        }
        else if (directive == "server_name") {
            ++i;
            while (i < tokens.size() && tokens[i] != ";") {
                server.server_names.push_back(tokens[i]);
                ++i;
            }
            expectSemicolon(tokens, i);
        }
        else if (directive == "client_max_body_size") {
            server.client_max_body_size = parseSize(takeValue(tokens, i, "client_max_body_size"));
        }
        else if (directive == "error_page") {
            ++i;
            if (i + 1 >= tokens.size())
                throw std::runtime_error("Expected code and path after 'error_page'");
            int code = parseHttpCode(tokens[i]);
            ++i;
            server.error_pages[code] = tokens[i];
            ++i;
            expectSemicolon(tokens, i);
        }
        else if (directive == "location") {
            ++i;
            if (i >= tokens.size())
                throw std::runtime_error("Expected path after 'location'");
            LocationConfig location;
            location.path = tokens[i];
            ++i;
            if (i >= tokens.size() || tokens[i] != "{")
                throw std::runtime_error("Expected '{' after location path");
            ++i;
            parseLocation(tokens, i, location);
            server.locations.push_back(location);
        }
        else {
            throw std::runtime_error("Unknown directive in server block: " + directive);
        }
    }

    if (i >= tokens.size())
        throw std::runtime_error("Expected '}' to close server block");
    ++i;
}

void Server::parseLocation(const std::vector<std::string>& tokens, size_t& i, LocationConfig& location) {
    while (i < tokens.size() && tokens[i] != "}") {
        const std::string& directive = tokens[i];

        if (directive == "root") {
            location.root = takeValue(tokens, i, "root");
        }
        else if (directive == "index") {
            location.index = takeValue(tokens, i, "index");
        }
        else if (directive == "allowed_methods") {
            ++i;
            while (i < tokens.size() && tokens[i] != ";") {
                if (tokens[i] != "GET" && tokens[i] != "POST" && tokens[i] != "DELETE")
                    throw std::runtime_error("Invalid method: " + tokens[i]);
                location.allowed_methods.insert(tokens[i]);
                ++i;
            }
            expectSemicolon(tokens, i);
        }
        else if (directive == "client_max_body_size") {
            location.client_max_body_size = parseSize(takeValue(tokens, i, "client_max_body_size"));
        }
        else if (directive == "autoindex") {
            location.autoindex = parseOnOff(takeValue(tokens, i, "autoindex"));
        }
        else if (directive == "upload_enable") {
            location.upload_enable = parseOnOff(takeValue(tokens, i, "upload_enable"));
        }
        else if (directive == "upload_store") {
            location.upload_store = takeValue(tokens, i, "upload_store");
        }
        else if (directive == "redirect") {
            ++i;
            if (i + 1 >= tokens.size())
                throw std::runtime_error("Expected code and url after 'redirect'");
            location.redirect_code = parseHttpCode(tokens[i]);
            ++i;
            location.redirect_url = tokens[i];
            ++i;
            expectSemicolon(tokens, i);
        }
        else if (directive == "cgi") {
            ++i;
            if (i + 1 >= tokens.size())
                throw std::runtime_error("Expected extension and path after 'cgi'");
            std::string ext = tokens[i];
            ++i;
            location.cgi[ext] = tokens[i];
            ++i;
            expectSemicolon(tokens, i);
        }
        else {
            throw std::runtime_error("Unknown directive in location block: " + directive);
        }
    }

    if (i >= tokens.size())
        throw std::runtime_error("Expected '}' to close location block");
    ++i;
}

Server::Server(const char* filename) {
    if (!isReadableFile(filename))
        throw std::runtime_error("Config file is not readable");

    std::ifstream file(filename);
    if (!file.is_open())
        throw std::runtime_error("Cannot open config file");

    std::stringstream ss;
    ss << file.rdbuf();
    std::string content = ss.str();

    std::vector<std::string> tokens = tokenize(content);

    size_t i = 0;
    parseGlobal(tokens, i);
    setupSockets();
}
