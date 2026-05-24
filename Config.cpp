#include "Config.hpp"
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <iostream>

Config::Config(const Config& conf) { *this = conf; }
Config& Config::operator=(const Config& conf) {
    if (this != &conf) { servers = conf.servers; }
    return *this;
}

Config::Config(std::string file_path) {
    std::ifstream file(file_path.c_str());
    if (!file.is_open()) {
        std::cerr << "WARNING: The configuration file could not be opened; reverting to default settings." << std::endl;
        return;
    }

    std::string content;
    std::string line;
    
    while (std::getline(file, line)) {
        size_t pos = line.find('#');
        if (pos != std::string::npos) {
            line = line.substr(0, pos);
        }
        content += line + " ";
    }

    std::string formatted_content = "";
    for (size_t i = 0; i < content.length(); ++i) {
        if (content[i] == '{' || content[i] == '}' || content[i] == ';') {
            formatted_content += " ";
            formatted_content += content[i];
            formatted_content += " ";
        } else {
            formatted_content += content[i];
        }
    }

    std::istringstream iss(formatted_content);
    std::vector<std::string> tokens;
    std::string token;
    while (iss >> token) {
        tokens.push_back(token);
    }

    parse(tokens);
}

void Config::parse(std::vector<std::string>& tokens) {
    std::vector<std::string>::iterator it = tokens.begin();
    while (it != tokens.end()) {
        if (*it == "server") {
            ++it;
            if (it != tokens.end() && *it == "{") {
                parseServer(++it, tokens.end());
            }
        } else {
            ++it;
        }
    }
}

void Config::parseServer(std::vector<std::string>::iterator& it, std::vector<std::string>::iterator end) {
    Server srv;
    srv.port = 8080;
    srv.max_byte = 0;

    while (it != end && *it != "}") {
        if (*it == "listen") {
            srv.port = std::atoi((*(++it)).c_str());
            ++it;
        }
        else if (*it == "server_name") {
            srv.interface = *(++it);
            ++it; 
        }
        else if (*it == "client_max_body_size") {
            srv.max_byte = std::atol((*(++it)).c_str());
            ++it;
        }
        else if (*it == "error_page") {
            ErrorPages ep;
            ep.code = std::atoi((*(++it)).c_str());
            ep.path = *(++it);
            srv.errorPages.push_back(ep);
            ++it; 
        }
        else if (*it == "location") {
            parseLocation(srv, ++it, end);
        }
        else {
            ++it;
        }
    }
    servers.push_back(srv);
    
    if (it != end) ++it;
}

void Config::parseLocation(Server& srv, std::vector<std::string>::iterator& it, std::vector<std::string>::iterator end) {
    Location loc;
    loc.url = *it;
    ++it;
    ++it;

    loc.autoindex = false;
    loc.upload = false;
    loc.max_byte = srv.max_byte; 

    while (it != end && *it != "}") {
        if (*it == "allow_methods") {
            ++it;
            while (it != end && *it != ";") {
                loc.allowedMethods.push_back(*it);
                ++it;
            }
        }
        else if (*it == "root") {
            loc.path = *(++it);
            ++it; 
        }
        else if (*it == "index") {
            loc.index_path = *(++it);
            ++it;
        }
        else if (*it == "autoindex") {
            loc.autoindex = (*(++it) == "on");
            ++it;
        }
        else if (*it == "upload") {
            loc.upload = (*(++it) == "on");
            ++it;
        }
        else if (*it == "upload_path") {
            loc.upload_path = *(++it);
            ++it;
        }
        else if (*it == "client_max_body_size") {
            loc.max_byte = std::atol((*(++it)).c_str());
            ++it;
        }
        else if (*it == "cgi_ext") {
            std::string ext = *(++it);
            std::string prog = *(++it);
            loc.cgi_ext[ext] = prog;
            ++it; 
        }
        else {
            ++it;
        }
    }
    srv.locations.push_back(loc);

    if (it != end && *it == "}") {
        ++it;
    }
}