#include "Cgi.hpp"
#include <unistd.h>
#include <sys/wait.h>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <cstdio> 
#include <cctype>

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
    _env["REQUEST_URI"]       = _request.getUri();
    _env["SCRIPT_NAME"]       = _request.getUri();
    _env["PATH_INFO"]         = _request.getUri();
    _env["SCRIPT_FILENAME"]   = _scriptPath;
    _env["PATH_TRANSLATED"]   = _scriptPath;
    
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

    const std::map<std::string, std::string>& headers = _request.getHeaders();
    for (std::map<std::string, std::string>::const_iterator it = headers.begin(); it != headers.end(); ++it) {
        std::string hName = it->first;
        
        for (size_t i = 0; i < hName.length(); ++i) {
            if (hName[i] == '-') {
                hName[i] = '_';
            } else {
                hName[i] = std::toupper(hName[i]);
            }
        }
        
        _env["HTTP_" + hName] = it->second;
    }
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
    FILE* fileIn = tmpfile();
    if (!fileIn) return "HTTP/1.1 500 Internal Server Error\r\n\r\nCGI Input Error";
    int fdIn = fileno(fileIn);

    int pipe_out[2];
    if (pipe(pipe_out) < 0) {
        fclose(fileIn);
        return "HTTP/1.1 500 Internal Server Error\r\n\r\nCGI Pipe Error";
    }

    if (_request.getMethod() == "POST" && !_request.getBody().empty()) {        
        const char* bodyData = _request.getBody().data(); 
        size_t bodyLen = _request.getBody().length();
        size_t totalWritten = 0;
        
        while (totalWritten < bodyLen) {
            ssize_t bytes = write(fdIn, bodyData + totalWritten, bodyLen - totalWritten);
            if (bytes < 0) break;
            totalWritten += bytes;
        }
        lseek(fdIn, 0, SEEK_SET);
    }

    pid_t pid = fork();
    if (pid < 0) {
        return "HTTP/1.1 500 Internal Server Error\r\n\r\nCGI Fork Error";
    }

    if (pid == 0) {
        close(pipe_out[0]);

        dup2(fdIn, STDIN_FILENO);
        dup2(pipe_out[1], STDOUT_FILENO);
        
        close(pipe_out[1]);

        char* argv[] = {
            const_cast<char*>(_program.c_str()),
            const_cast<char*>(_scriptPath.c_str()),
            NULL
        };
        char** envp = getEnvAsCArray();

        execve(_program.c_str(), argv, envp);
        exit(1);
    } 
    else {
        close(pipe_out[1]);        
        char buffer[8192];
        std::string cgi_output;
        cgi_output.reserve(_request.getBody().length() + 1024);
        
        int bytes_read;
        while ((bytes_read = read(pipe_out[0], buffer, sizeof(buffer))) > 0) {
            cgi_output.append(buffer, bytes_read); 
        }
        close(pipe_out[0]);

        int status;
        waitpid(pid, &status, 0);
        fclose(fileIn);


        if (cgi_output.find("HTTP/1.1") == 0 || cgi_output.find("HTTP/1.0") == 0) {
            return cgi_output;
        }

        std::string formatted_output = "HTTP/1.1 200 OK\r\n";
        size_t header_end = cgi_output.find("\r\n\r\n");

        if (header_end != std::string::npos) {
            std::string cgi_headers = cgi_output.substr(0, header_end);
            
            if (cgi_headers.find("Content-Type:") == std::string::npos) {
                formatted_output += "Content-Type: text/plain\r\n";
            }
            
            if (cgi_headers.find("Content-Length:") == std::string::npos) {
                std::stringstream ss;
                ss << (cgi_output.length() - (header_end + 4));
                formatted_output += "Content-Length: " + ss.str() + "\r\n";
            }
            
            formatted_output += cgi_output;
        } else {
            formatted_output += "Content-Type: text/plain\r\n";
            std::stringstream ss;
            ss << cgi_output.length();
            formatted_output += "Content-Length: " + ss.str() + "\r\n\r\n";
            formatted_output += cgi_output;
        }

        return formatted_output;
    }
}