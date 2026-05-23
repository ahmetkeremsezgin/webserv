#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <iostream>
#include <vector>
#include <map>
#include <string>

struct ErrorPages {
    int code;
    std::string path;
};

struct Location {
    std::string url;
    std::vector<std::string> allowedMethods;
    long long max_byte;
    std::string redirect;
    std::string path;
    bool autoindex;
    std::string index_path;
    bool upload;
    std::string upload_path;
    std::map<std::string, std::string> cgi_ext;
};

struct Server {
    std::string interface;
    int server_fd;
    int port;
    long long max_byte;
    std::vector<ErrorPages> errorPages;
    std::vector<Location> locations;
};

class Config {
public:
    std::vector<Server> servers;

    Config(const Config& conf);
    Config& operator=(const Config& conf);
    Config(std::string file_path);

private:
    void parse(std::vector<std::string>& tokens);
    void parseServer(std::vector<std::string>::iterator& it, std::vector<std::string>::iterator end);
    void parseLocation(Server& server, std::vector<std::string>::iterator& it, std::vector<std::string>::iterator end);
};

#endif