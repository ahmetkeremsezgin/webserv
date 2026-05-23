#include "Cgi.hpp"
#include <unistd.h>
#include <sys/wait.h>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>

Cgi::Cgi(const HttpRequest& req, const std::string& scriptPath, const std::string& program)
    : _request(req), _scriptPath(scriptPath), _program(program) {
    initEnv();
}

Cgi::~Cgi() {}

void Cgi::initEnv() {
    _env["GATEWAY_INTERFACE"] = "CGI/1.1";
    _env["SERVER_PROTOCOL"]   = _request.getVersion();
    _env["SERVER_SOFTWARE"]   = "Webserv/1.0";
    
    _env["REQUEST_METHOD"]    = _request.getMethod();
    _env["SCRIPT_FILENAME"]   = _scriptPath;
    
    size_t questionMarkPos = _request.getUri().find('?');
    if (questionMarkPos != std::string::npos) {
        _env["QUERY_STRING"] = _request.getUri().substr(questionMarkPos + 1);
    } else {
        _env["QUERY_STRING"] = "";
    }

    if (_request.getMethod() == "POST") {
        std::stringstream ss;
        ss << _request.getBody().length();
        _env["CONTENT_LENGTH"] = ss.str();
        _env["CONTENT_TYPE"]   = _request.getHeader("Content-Type");
    }

    _env["HTTP_HOST"]       = _request.getHeader("Host");
    _env["HTTP_USER_AGENT"] = _request.getHeader("User-Agent");
}

char** Cgi::getEnvAsCArray() const {
    char** env = new char*[_env.size() + 1];
    int i = 0;
    
    for (std::map<std::string, std::string>::const_iterator it = _env.begin(); it != _env.end(); ++it) {
        std::string env_str = it->first + "=" + it->second;
        env[i] = new char[env_str.length() + 1];
        std::strcpy(env[i], env_str.c_str());
        i++;
    }
    env[i] = NULL;
    return env;
}

std::string Cgi::execute() {
    int pipe_in[2];
    int pipe_out[2];

    if (pipe(pipe_in) < 0 || pipe(pipe_out) < 0) {
        return "HTTP/1.1 500 Internal Server Error\r\n\r\nCGI Pipe Error";
    }

    pid_t pid = fork();
    if (pid < 0) {
        return "HTTP/1.1 500 Internal Server Error\r\n\r\nCGI Fork Error";
    }

    if (pid == 0) {
        close(pipe_in[1]);
        close(pipe_out[0]);
        
        dup2(pipe_in[0], STDIN_FILENO);
        dup2(pipe_out[1], STDOUT_FILENO);
        
        close(pipe_in[0]);
        close(pipe_out[1]);

        char* argv[] = {
            const_cast<char*>(_program.c_str()),
            const_cast<char*>(_scriptPath.c_str()),
            NULL
        };
        char** envp = getEnvAsCArray();

        execve(_program.c_str(), argv, envp);
        
        for (int i = 0; envp[i] != NULL; ++i) {
            delete[] envp[i];
        }
        delete[] envp;
        
        exit(1);
    } 
    else {
        close(pipe_in[0]);
        close(pipe_out[1]);

        if (_request.getMethod() == "POST" && !_request.getBody().empty()) {
            write(pipe_in[1], _request.getBody().c_str(), _request.getBody().length());
        }
        close(pipe_in[1]);

        int status;
        waitpid(pid, &status, 0);

        char buffer[4096];
        std::string cgi_output = "";
        int bytes_read;
        
        while ((bytes_read = read(pipe_out[0], buffer, sizeof(buffer) - 1)) > 0) {
            buffer[bytes_read] = '\0';
            cgi_output += buffer;
        }
        close(pipe_out[0]);

        if (cgi_output.find("HTTP/1.1") != 0 && cgi_output.find("HTTP/1.0") != 0) {
            std::string formatted_output = "HTTP/1.1 200 OK\r\n";
            
            if (cgi_output.find("Content-Type:") == std::string::npos) {
                formatted_output += "Content-Type: text/plain\r\n\r\n";
            }
            formatted_output += cgi_output;
            return formatted_output;
        }

        return cgi_output;
    }
}