#include "Server.hpp"

#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <csignal>
#include <sstream>

static const time_t CGI_IDLE_TIMEOUT_S = 10;
static const size_t CGI_PIPE_BUFFER    = 65536;

bool Server::isCgiFd(int fd) const {
    return _cgi_fd_map.find(fd) != _cgi_fd_map.end();
}

static void buildCgiEnv(const HttpRequest& req, const std::string& scriptPath,
                        const std::string& query, std::vector<std::string>& env) {
    std::string url_path = stripQuery(req.path);

    std::string server_name = "localhost";
    std::string server_port = "80";
    std::map<std::string, std::string>::const_iterator host = req.headers.find("Host");
    if (host != req.headers.end()) {
        std::string value = host->second;
        std::string::size_type colon = value.find(':');
        if (colon != std::string::npos) {
            server_name = value.substr(0, colon);
            server_port = value.substr(colon + 1);
        } else {
            server_name = value;
        }
    }

    std::stringstream content_length;
    content_length << req.body.size();

    env.push_back("GATEWAY_INTERFACE=CGI/1.1");
    env.push_back("SERVER_PROTOCOL=HTTP/1.1");
    env.push_back("REDIRECT_STATUS=200");
    env.push_back("REQUEST_METHOD=" + req.method);
    env.push_back("REQUEST_URI=" + url_path);
    env.push_back("SCRIPT_FILENAME=" + scriptPath);
    env.push_back("SCRIPT_NAME=" + url_path);
    env.push_back("PATH_INFO=" + url_path);
    env.push_back("QUERY_STRING=" + query);
    env.push_back("SERVER_NAME=" + server_name);
    env.push_back("SERVER_PORT=" + server_port);
    env.push_back("SERVER_SOFTWARE=webserv");
    env.push_back("CONTENT_LENGTH=" + content_length.str());

    std::map<std::string, std::string>::const_iterator ct = req.headers.find("Content-Type");
    if (ct != req.headers.end())
        env.push_back("CONTENT_TYPE=" + ct->second);

    for (std::map<std::string, std::string>::const_iterator it = req.headers.begin();
         it != req.headers.end(); ++it) {
        std::string env_key = "HTTP_";
        for (size_t k = 0; k < it->first.size(); ++k) {
            char c = it->first[k];
            if (c == '-')
                env_key += '_';
            else if (c >= 'a' && c <= 'z')
                env_key += static_cast<char>(c - 'a' + 'A');
            else
                env_key += c;
        }
        env.push_back(env_key + "=" + it->second);
    }
}

void Server::startCGI(const std::string& interpreter, const std::string& scriptPath,
                      const HttpRequest& req, int client_fd) {
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

        std::string script = scriptPath;
        if (!interpreter.empty() && interpreter[0] == '/') {
            std::string::size_type slash = script.rfind('/');
            if (slash != std::string::npos && slash > 0) {
                std::string dir = script.substr(0, slash);
                if (chdir(dir.c_str()) == 0)
                    script = script.substr(slash + 1);
            }
        }

        std::vector<std::string> env_strings;
        buildCgiEnv(req, script, query, env_strings);

        std::vector<char*> envp;
        for (size_t i = 0; i < env_strings.size(); ++i)
            envp.push_back(const_cast<char*>(env_strings[i].c_str()));
        envp.push_back(NULL);

        char* argv[] = {
            const_cast<char*>(interpreter.c_str()),
            const_cast<char*>(script.c_str()),
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
    cgi.in_fd = pipe_in[1];
    cgi.out_fd = pipe_out[0];
    cgi.write_buffer = req.body;
    cgi.last_activity = time(NULL);

    _cgi[client_fd] = cgi;

    if (req.body.empty()) {
        close(cgi.in_fd);
        _cgi[client_fd].in_fd = -1;
    } else {
        addToPoll(cgi.in_fd, POLLOUT);
        _cgi_fd_map[cgi.in_fd] = client_fd;
    }

    addToPoll(cgi.out_fd, POLLIN);
    _cgi_fd_map[cgi.out_fd] = client_fd;
}

void Server::closeCgiPipe(int fd) {
    if (fd == -1)
        return;
    _cgi_fd_map.erase(fd);
    removeFromPoll(fd);
    close(fd);
    _closed_fds.insert(fd);
}

void Server::handleCgiWrite(int fd) {
    std::map<int, int>::iterator mit = _cgi_fd_map.find(fd);
    if (mit == _cgi_fd_map.end() || _cgi.find(mit->second) == _cgi.end()) {
        closeCgiPipe(fd);
        return;
    }
    CgiProcess& cgi = _cgi[mit->second];

    size_t remaining = cgi.write_buffer.size() - cgi.write_offset;
    ssize_t w = write(fd, cgi.write_buffer.c_str() + cgi.write_offset, remaining);

    if (w <= 0) {
        closeCgiPipe(fd);
        cgi.in_fd = -1;
        return;
    }

    cgi.write_offset += w;
    cgi.last_activity = time(NULL);

    if (cgi.write_offset >= cgi.write_buffer.size()) {
        closeCgiPipe(fd);
        cgi.in_fd = -1;
    }
}

void Server::handleCgiRead(int fd) {
    std::map<int, int>::iterator mit = _cgi_fd_map.find(fd);
    if (mit == _cgi_fd_map.end() || _cgi.find(mit->second) == _cgi.end()) {
        closeCgiPipe(fd);
        return;
    }

    int client_fd = mit->second;
    CgiProcess& cgi = _cgi[client_fd];

    if (fd == cgi.in_fd) {
        closeCgiPipe(fd);
        cgi.in_fd = -1;
        return;
    }

    char buffer[CGI_PIPE_BUFFER];
    ssize_t n = read(fd, buffer, sizeof(buffer));

    if (n > 0) {
        cgi.output.append(buffer, n);
        cgi.last_activity = time(NULL);
        return;
    }

    closeCgiPipe(fd);
    cgi.out_fd = -1;
    finishCgi(client_fd);
}

void Server::finishCgi(int client_fd) {
    std::map<int, CgiProcess>::iterator it = _cgi.find(client_fd);
    if (it == _cgi.end())
        return;
    CgiProcess& cgi = it->second;

    if (cgi.in_fd != -1) {
        closeCgiPipe(cgi.in_fd);
        cgi.in_fd = -1;
    }

    int status;
    if (waitpid(cgi.pid, &status, WNOHANG) == 0) {
        kill(cgi.pid, SIGKILL);
        waitpid(cgi.pid, &status, 0);
    }

    std::string response = buildCgiResponse(cgi.output);
    _cgi.erase(client_fd);

    queueResponse(client_fd, response);
}

void Server::closeCgi(int client_fd) {
    std::map<int, CgiProcess>::iterator it = _cgi.find(client_fd);
    if (it == _cgi.end())
        return;

    CgiProcess& cgi = it->second;
    closeCgiPipe(cgi.in_fd);
    closeCgiPipe(cgi.out_fd);

    if (cgi.pid > 0) {
        kill(cgi.pid, SIGKILL);
        waitpid(cgi.pid, NULL, 0);
    }

    _cgi.erase(client_fd);
}

void Server::checkCgiTimeouts() {
    time_t now = time(NULL);

    std::vector<int> timed_out;
    for (std::map<int, CgiProcess>::iterator it = _cgi.begin();
         it != _cgi.end(); ++it) {
        if (now - it->second.last_activity > CGI_IDLE_TIMEOUT_S)
            timed_out.push_back(it->first);
    }

    for (size_t i = 0; i < timed_out.size(); ++i) {
        closeCgi(timed_out[i]);
        sendErrorToClient(timed_out[i], 504);
    }
}

void Server::sendErrorToClient(int client_fd, int code) {
    std::string response = buildError(code, _servers[0]);
    queueResponse(client_fd, response);
}
