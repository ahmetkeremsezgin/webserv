#ifndef CGI_HPP
#define CGI_HPP

#include "HttpRequest.hpp"
#include "Config.hpp"
#include <string>
#include <map>

class Cgi {
private:
    HttpRequest _request;
    std::string _scriptPath;
    std::string _program;
    std::map<std::string, std::string> _env;

    void initEnv();
    char** getEnvAsCArray() const;

public:
    Cgi(const HttpRequest& req, const std::string& scriptPath, const std::string& program);
    ~Cgi();

    std::string execute();
};

#endif