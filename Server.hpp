#ifndef SERVER_HPP
#define SERVER_HPP

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <poll.h> 
#include <unistd.h>
#include <ctime>

struct HttpRequest {
    std::string method;
    std::string path;
    std::string version;
    std::map<std::string, std::string> headers;
    std::string body;

    bool        valid;

    HttpRequest() : valid(false) {}
};

struct Client {
    int         fd;
    int         listen_fd; 
    std::string read_buffer;
    std::string write_buffer;
    bool        response_ready;
    time_t      last_activity;
    bool   headers_parsed;
    size_t content_length;
    size_t header_size;
    bool   is_chunked;

    Client() : fd(-1), listen_fd(-1), response_ready(false), last_activity(0),
               headers_parsed(false), content_length(0), header_size(0), is_chunked(false) {}
    Client(int f, int lfd) : fd(f), listen_fd(lfd), response_ready(false),
               last_activity(time(NULL)), headers_parsed(false),
               content_length(0), header_size(0), is_chunked(false) {}
};

struct LocationConfig {
    std::string                 path;
    std::string                 root;
    std::string                 index;
    std::set<std::string>       allowed_methods;
    bool                        autoindex;
    std::string                 upload_store;
    bool                        upload_enable;
    int                         redirect_code;
    std::string                 redirect_url;
    size_t client_max_body_size;
    std::map<std::string, std::string> cgi;

    LocationConfig()
        : autoindex(false),
          upload_enable(false),
          redirect_code(0) {}
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

struct CgiProcess {
    pid_t       pid;
    int         client_fd;

    int         in_fd;
    int         out_fd;

    std::string write_buffer;
    size_t      write_offset;
    std::string output;

    bool        in_done;
    bool        out_done;

    time_t      start_time;

    CgiProcess()
        : pid(-1), client_fd(-1), in_fd(-1), out_fd(-1),
          write_offset(0), in_done(false), out_done(false), start_time(0) {}
};

class Server {
    private:
        std::map<int, CgiProcess> _cgi;
        std::map<int, int>        _cgi_fd_map;
        std::vector<ServerConfig> servers;

        std::vector<std::string> tokenize(const std::string& content);
        void parseGlobal(const std::vector<std::string>& tokens, size_t& i);
        void parseServer(const std::vector<std::string>& tokens, size_t& i, ServerConfig& server);
        void parseLocation(const std::vector<std::string>& tokens, size_t& i, LocationConfig& location);

        std::vector<int>    _listen_fds;
        std::vector<struct pollfd> _pfds;
        std::map<int, int>  _fd_to_server;
        std::map<int, Client> _clients; 
        
        bool isCgiFd(int fd) const;
        void handleCgiWrite(int fd);
        void handleCgiRead(int fd);
        void finishCgi(int client_fd);
        void checkCgiTimeouts();
        void closeCgi(int client_fd);

        void setupSockets();
        int  createListenSocket(const std::string& host, int port);
        void addToPoll(int fd, short events);
        bool isRequestComplete(const std::string& buffer);

        void enablePollOut(int fd);
        void handleClientWrite(int client_fd);

        void checkTimeouts();
    public:
        Server(const char *filename);
        const std::vector<ServerConfig>& getServers() const;

        void run();
        bool isListenFd(int fd) const;
        void acceptConnection(int listen_fd);
        void handleClientData(int client_fd);
        void closeConnection(int fd);

        const ServerConfig& selectServer(int listen_fd, const HttpRequest& req);

        std::string handleRequest(const HttpRequest& req, const ServerConfig& server, int client_fd);
std::string handleGet(const LocationConfig& loc, const HttpRequest& req, const ServerConfig& server, int client_fd);
std::string handlePost(const LocationConfig& loc, const HttpRequest& req, const ServerConfig& server, int client_fd);
std::string handleDelete(const LocationConfig& loc, const HttpRequest& req, const ServerConfig& server, int client_fd);

        void startCGI(const std::string& interpreter, const std::string& scriptPath,const HttpRequest& req, int client_fd);
        void sendErrorToClient(int client_fd, int code);
        ~Server();
};


#endif