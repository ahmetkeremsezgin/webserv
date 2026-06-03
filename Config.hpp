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
    std::string interface;   // listen host (bind address), default 0.0.0.0
    std::string server_name;
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
    std::vector<std::string> _tokens;
    size_t _pos;

    // Reads the file, splits it into tokens and parses every server block.
    void tokenize(const std::string& content);
    void parse();
    void parseServer();
    void parseLocation(Server& server);

    // Token cursor: walk over _tokens while reporting precise errors.
    bool eof() const;
    const std::string& peek() const;
    const std::string& advance();
    bool accept(const std::string& token);
    void expect(const std::string& token, const std::string& context);
    const std::string& expectValue(const std::string& directive);
    void expectSemicolon(const std::string& directive);

    // Value parsing & validation (each throws on a malformed value).
    static void error(const std::string& message);
    static bool isNumber(const std::string& value);
    static bool toToggle(const std::string& value, const std::string& directive);
    static int toPort(const std::string& value);
    static int toErrorCode(const std::string& value);
    static long long toSize(const std::string& value, const std::string& directive);
    static void splitHostPort(const std::string& value, std::string& host, std::string& port);
};

#endif
