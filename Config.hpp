#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <iostream>
#include <vector>

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
};


struct Server {
    std::string interface;
    int port;
    long long max_byte;
    std::vector<ErrorPages> errorPages;
    std::vector<Location> locations;
};


class Config {
    public:
        std::vector<Server> servers;
        
        Config();
        Config(const Config& conf);
        Config& operator=(const Config& conf);
        Config(std::string file_path);
};

#endif