#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <string>
#include <vector>
#include <map>
#include <set>

struct LocationConfig {
    std::string                        path;
    std::string                        root;
    std::string                        index;
    std::set<std::string>              allowed_methods;
    bool                               autoindex;
    std::string                        upload_store;
    bool                               upload_enable;
    int                                redirect_code;
    std::string                        redirect_url;
    size_t                             client_max_body_size;
    std::map<std::string, std::string> cgi;

    LocationConfig()
        : autoindex(false),
          upload_enable(false),
          redirect_code(0),
          client_max_body_size(0) {}
};

struct ServerConfig {
    std::string                 host;
    int                         port;
    std::vector<std::string>    server_names;
    size_t                      client_max_body_size;
    std::map<int, std::string>  error_pages;
    std::vector<LocationConfig> locations;

    ServerConfig()
        : host("0.0.0.0"),
          port(-1),
          client_max_body_size(1048576) {}
};

#endif
