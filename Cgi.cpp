#include <unistd.h>
#include <sys/wait.h>
#include <cstdlib>
#include <fcntl.h>
#include <sstream>
#include "Server.hpp"
#include "Utils.hpp"

void Server::startCGI(const std::string& interpreter, const std::string& scriptPath,
                      const HttpRequest& req, int client_fd) {
    std::cout << "[startCGI] client_fd=" << client_fd
              << " body=" << req.body.size() << std::endl;

    std::string query;
    std::string::size_type qpos = req.path.find('?');
    if (qpos != std::string::npos)
        query = req.path.substr(qpos + 1);

    int pipe_in[2];
    int pipe_out[2];

    if (pipe(pipe_in) == -1) {
        sendErrorToClient(client_fd, 500);
        return;
    }
    if (pipe(pipe_out) == -1) {
        close(pipe_in[0]); close(pipe_in[1]);
        sendErrorToClient(client_fd, 500);
        return;
    }

    pid_t pid = fork();
    if (pid == -1) {
        close(pipe_in[0]); close(pipe_in[1]);
        close(pipe_out[0]); close(pipe_out[1]);
        sendErrorToClient(client_fd, 500);
        return;
    }

    if (pid == 0) {
        dup2(pipe_in[0], STDIN_FILENO);
        dup2(pipe_out[1], STDOUT_FILENO);
        close(pipe_in[0]);  close(pipe_in[1]);
        close(pipe_out[0]); close(pipe_out[1]);

        std::string url_path = req.path;
        std::string::size_type q = url_path.find('?');
        if (q != std::string::npos)
            url_path = url_path.substr(0, q);

        std::string server_name = "localhost";
        std::string server_port = "80";
        std::map<std::string, std::string>::const_iterator hh = req.headers.find("Host");
        if (hh != req.headers.end()) {
            std::string hv = hh->second;
            std::string::size_type c = hv.find(':');
            if (c != std::string::npos) {
                server_name = hv.substr(0, c);
                server_port = hv.substr(c + 1);
            } else {
                server_name = hv;
            }
        }

        std::stringstream cl_ss;
        cl_ss << req.body.size();

        std::vector<std::string> env_strings;
        env_strings.push_back("GATEWAY_INTERFACE=CGI/1.1");
        env_strings.push_back("SERVER_PROTOCOL=HTTP/1.1");
        env_strings.push_back("REDIRECT_STATUS=200");
        env_strings.push_back("REQUEST_METHOD=" + req.method);
        env_strings.push_back("REQUEST_URI=" + url_path);
        env_strings.push_back("SCRIPT_FILENAME=" + scriptPath);
        env_strings.push_back("SCRIPT_NAME=" + url_path);
        env_strings.push_back("PATH_INFO=" + url_path);
        env_strings.push_back("QUERY_STRING=" + query);
        env_strings.push_back("SERVER_NAME=" + server_name);
        env_strings.push_back("SERVER_PORT=" + server_port);
        env_strings.push_back("SERVER_SOFTWARE=webserv");
        env_strings.push_back("CONTENT_LENGTH=" + cl_ss.str());

        std::map<std::string, std::string>::const_iterator ct = req.headers.find("Content-Type");
        if (ct != req.headers.end())
            env_strings.push_back("CONTENT_TYPE=" + ct->second);

        for (std::map<std::string, std::string>::const_iterator hit = req.headers.begin();
             hit != req.headers.end(); ++hit) {
            std::string key = hit->first;
            std::string env_key = "HTTP_";
            for (size_t k = 0; k < key.size(); ++k) {
                char c = key[k];
                if (c == '-') env_key += '_';
                else if (c >= 'a' && c <= 'z') env_key += (char)(c - 'a' + 'A');
                else env_key += c;
            }
            env_strings.push_back(env_key + "=" + hit->second);
        }

        std::vector<char*> envp;
        for (size_t i = 0; i < env_strings.size(); ++i)
            envp.push_back(const_cast<char*>(env_strings[i].c_str()));
        envp.push_back(NULL);

        char* argv[] = {
            const_cast<char*>(interpreter.c_str()),
            const_cast<char*>(scriptPath.c_str()),
            NULL
        };

        execve(interpreter.c_str(), argv, &envp[0]);
        _exit(1);
    }

    close(pipe_in[0]);
    close(pipe_out[1]);

    fcntl(pipe_in[1], F_SETFL, O_NONBLOCK);
    fcntl(pipe_out[0], F_SETFL, O_NONBLOCK);

    CgiProcess cgi;
    cgi.pid = pid;
    cgi.client_fd = client_fd;
    cgi.in_fd = pipe_in[1];
    cgi.out_fd = pipe_out[0];
    cgi.write_buffer = req.body;
    cgi.write_offset = 0;
    cgi.in_done = req.body.empty();
    cgi.out_done = false;
    cgi.start_time = time(NULL);

    std::cout << "[startCGI] EBEVEYN client_fd=" << client_fd
              << " in_fd=" << cgi.in_fd << " out_fd=" << cgi.out_fd << std::endl;

    _cgi[client_fd] = cgi;

    if (!cgi.in_done) {
        addToPoll(cgi.in_fd, POLLOUT);
        _cgi_fd_map[cgi.in_fd] = client_fd;
    } else {
        close(cgi.in_fd);
        _cgi[client_fd].in_fd = -1;
    }

    addToPoll(cgi.out_fd, POLLIN);
    _cgi_fd_map[cgi.out_fd] = client_fd;
}

std::string buildCgiResponse(const std::string& cgiOutput) {
    std::cout << "[CGI RESP] total=" << cgiOutput.size() << std::endl;

    std::string headers;
    std::string body;

    std::string::size_type sep = cgiOutput.find("\r\n\r\n");
    std::string::size_type sep_len = 4;
    if (sep == std::string::npos) {
        sep = cgiOutput.find("\n\n");
        sep_len = 2;
    }

    if (sep != std::string::npos) {
        headers = cgiOutput.substr(0, sep);
        body = cgiOutput.substr(sep + sep_len);
    } else {
        body = cgiOutput;
    }

    std::stringstream res;
    res << "HTTP/1.1 200 OK\r\n";
    if (!headers.empty())
        res << headers << "\r\n";
    res << "Content-Length: " << body.size() << "\r\n";
    res << "Connection: close\r\n";
    res << "\r\n";
    res << body;

    return res.str();
}

void Server::sendErrorToClient(int client_fd, int code) {
    std::map<int, Client>::iterator it = _clients.find(client_fd);
    if (it == _clients.end())
        return;

    const ServerConfig& server = servers[0];

    it->second.write_buffer = buildError(code, server);
    it->second.response_ready = true;
    enablePollOut(client_fd);
}