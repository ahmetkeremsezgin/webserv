#include "Server.hpp"
#include "Utils.hpp"

#include <unistd.h>
#include <fstream>
#include <sstream>

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
        if (tokens[i] == "server") {
            ++i;

            if (i >= tokens.size() || tokens[i] != "{")
                throw std::runtime_error("Expected '{' after 'server'");
            ++i;

            ServerConfig server;
            parseServer(tokens, i, server);
            servers.push_back(server);
        }
        else {
            throw std::runtime_error("Unexpected token in global scope: " + tokens[i]);
        }
    }
}

void Server::parseServer(const std::vector<std::string>& tokens, size_t& i, ServerConfig& server) {
    while (i < tokens.size() && tokens[i] != "}") {

        if (tokens[i] == "host") {
            ++i;
            if (i >= tokens.size()) throw std::runtime_error("Expected value after 'host'");
            server.host = tokens[i];
            ++i;
            expectSemicolon(tokens, i);
        }
        else if (tokens[i] == "listen") {
            ++i;
            if (i >= tokens.size()) throw std::runtime_error("Expected value after 'listen'");
            server.port = parsePort(tokens[i]);
            ++i;
            expectSemicolon(tokens, i);
        }
        else if (tokens[i] == "server_name") {
            ++i;
            while (i < tokens.size() && tokens[i] != ";") {
                server.server_names.push_back(tokens[i]);
                ++i;
            }
            expectSemicolon(tokens, i);
        }
        else if (tokens[i] == "client_max_body_size") {
            ++i;
            if (i >= tokens.size()) throw std::runtime_error("Expected value after 'client_max_body_size'");
            server.client_max_body_size = parseSize(tokens[i]);
            ++i;
            expectSemicolon(tokens, i);
        }
        else if (tokens[i] == "error_page") {
            ++i;
            if (i + 1 >= tokens.size()) throw std::runtime_error("Expected code and path after 'error_page'");
            int code = parseErrorCode(tokens[i]);
            ++i;
            std::string path = tokens[i];
            ++i;
            server.error_pages[code] = path;
            expectSemicolon(tokens, i);
        }
        else if (tokens[i] == "location") {
            ++i;
            if (i >= tokens.size()) throw std::runtime_error("Expected path after 'location'");
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
            throw std::runtime_error("Unknown directive in server block: " + tokens[i]);
        }
    }

    if (i >= tokens.size())
        throw std::runtime_error("Expected '}' to close server block");
    ++i;
}

void Server::parseLocation(const std::vector<std::string>& tokens, size_t& i, LocationConfig& location) {
    while (i < tokens.size() && tokens[i] != "}") {

        if (tokens[i] == "root") {
            ++i;
            if (i >= tokens.size()) throw std::runtime_error("Expected value after 'root'");
            location.root = tokens[i];
            ++i;
            expectSemicolon(tokens, i);
        }
        else if (tokens[i] == "index") {
            ++i;
            if (i >= tokens.size()) throw std::runtime_error("Expected value after 'index'");
            location.index = tokens[i];
            ++i;
            expectSemicolon(tokens, i);
        }
        else if (tokens[i] == "allowed_methods") {
            ++i;
            while (i < tokens.size() && tokens[i] != ";") {
                if (tokens[i] != "GET" && tokens[i] != "POST" && tokens[i] != "DELETE")
                    throw std::runtime_error("Invalid method: " + tokens[i]);
                location.allowed_methods.insert(tokens[i]);
                ++i;
            }
            expectSemicolon(tokens, i);
        }
        else if (tokens[i] == "client_max_body_size") {
            ++i;
            if (i >= tokens.size())
                throw std::runtime_error("Expected value after 'client_max_body_size'");
            location.client_max_body_size = parseSize(tokens[i]);
            ++i;
            expectSemicolon(tokens, i);
        }
        else if (tokens[i] == "autoindex") {
            ++i;
            if (i >= tokens.size()) throw std::runtime_error("Expected value after 'autoindex'");
            location.autoindex = parseOnOff(tokens[i]);
            ++i;
            expectSemicolon(tokens, i);
        }
        else if (tokens[i] == "upload_enable") {
            ++i;
            if (i >= tokens.size()) throw std::runtime_error("Expected value after 'upload_enable'");
            location.upload_enable = parseOnOff(tokens[i]);
            ++i;
            expectSemicolon(tokens, i);
        }
        else if (tokens[i] == "upload_store") {
            ++i;
            if (i >= tokens.size()) throw std::runtime_error("Expected value after 'upload_store'");
            location.upload_store = tokens[i];
            ++i;
            expectSemicolon(tokens, i);
        }
        else if (tokens[i] == "redirect") {
            ++i;
            if (i + 1 >= tokens.size()) throw std::runtime_error("Expected code and url after 'redirect'");
            location.redirect_code = parseErrorCode(tokens[i]);
            ++i;
            location.redirect_url = tokens[i];
            ++i;
            expectSemicolon(tokens, i);
        }
        else if (tokens[i] == "cgi") {
            ++i;
            if (i + 1 >= tokens.size()) throw std::runtime_error("Expected extension and path after 'cgi'");
            std::string ext = tokens[i];
            ++i;
            std::string interp = tokens[i];
            ++i;
            location.cgi[ext] = interp;
            expectSemicolon(tokens, i);
        }
        else {
            throw std::runtime_error("Unknown directive in location block: " + tokens[i]);
        }
    }

    if (i >= tokens.size())
        throw std::runtime_error("Expected '}' to close location block");
    ++i;
}

Server::Server(const char *filename) {
    if (!file_is_rdy(filename))
        throw std::runtime_error("Config file is not readable");
    
    std::ifstream file(filename);
    if (!file.is_open())
        throw std::runtime_error("Cannot open config file");
    
    std::stringstream ss;
    ss << file.rdbuf();
    file.close();
    std::string content = ss.str();

    std::vector<std::string> tokens = tokenize(content);

    size_t i = 0;
    parseGlobal(tokens, i);
    setupSockets();
}

const std::vector<ServerConfig>& Server::getServers() const {
    return servers; 
}