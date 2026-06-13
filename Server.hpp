#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>
#include <vector>
#include <map>
#include <set>
#include <poll.h>
#include <ctime>

#include "Config.hpp"
#include "Http.hpp"

struct Client {
    int         listen_fd;
    std::string read_buffer;
    std::string write_buffer;
    size_t      write_offset;
    bool        response_ready;
    time_t      last_activity;

    bool        headers_parsed;
    size_t      header_size;
    size_t      content_length;
    bool        is_chunked;
    size_t      chunk_scan_pos;

    Client()
        : listen_fd(-1), write_offset(0), response_ready(false),
          last_activity(0), headers_parsed(false), header_size(0),
          content_length(0), is_chunked(false), chunk_scan_pos(0) {}

    explicit Client(int lfd)
        : listen_fd(lfd), write_offset(0), response_ready(false),
          last_activity(time(NULL)), headers_parsed(false), header_size(0),
          content_length(0), is_chunked(false), chunk_scan_pos(0) {}
};

struct CgiProcess {
    pid_t       pid;
    int         in_fd;
    int         out_fd;
    std::string write_buffer;
    size_t      write_offset;
    std::string output;
    time_t      last_activity;

    CgiProcess()
        : pid(-1), in_fd(-1), out_fd(-1), write_offset(0), last_activity(0) {}
};

class Server {
    public:
        Server(const char* filename);
        ~Server();

        void run();

    private:
        std::vector<ServerConfig> _servers;

        std::vector<std::string> tokenize(const std::string& content);
        void parseGlobal(const std::vector<std::string>& tokens, size_t& i);
        void parseServer(const std::vector<std::string>& tokens, size_t& i, ServerConfig& server);
        void parseLocation(const std::vector<std::string>& tokens, size_t& i, LocationConfig& location);

        std::vector<int>           _listen_fds;
        std::vector<struct pollfd> _pfds;
        std::map<int, int>         _fd_to_server;
        std::map<int, Client>      _clients;
        std::set<int>              _closed_fds;

        void setupSockets();
        int  createListenSocket(const std::string& host, int port);
        void addToPoll(int fd, short events);
        void removeFromPoll(int fd);
        void enablePollOut(int fd);
        bool isListenFd(int fd) const;

        void acceptConnection(int listen_fd);
        void handleClientData(int client_fd);
        void handleClientWrite(int client_fd);
        void closeConnection(int fd);
        void queueResponse(int client_fd, std::string& response);
        void checkTimeouts();

        const ServerConfig& selectServer(int listen_fd, const HttpRequest& req);

        std::string handleRequest(const HttpRequest& req, const ServerConfig& server, int client_fd);
        std::string handleGet(const LocationConfig& loc, const HttpRequest& req, const ServerConfig& server, int client_fd);
        std::string handlePost(const LocationConfig& loc, const HttpRequest& req, const ServerConfig& server, int client_fd);
        std::string handleDelete(const LocationConfig& loc, const HttpRequest& req, const ServerConfig& server);

        std::map<int, CgiProcess> _cgi;
        std::map<int, int>        _cgi_fd_map;

        bool isCgiFd(int fd) const;
        void startCGI(const std::string& interpreter, const std::string& scriptPath,
                      const HttpRequest& req, int client_fd);
        void handleCgiWrite(int fd);
        void handleCgiRead(int fd);
        void finishCgi(int client_fd);
        void closeCgi(int client_fd);
        void closeCgiPipe(int fd);
        void checkCgiTimeouts();
        void sendErrorToClient(int client_fd, int code);
};

#endif
