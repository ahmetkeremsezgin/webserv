#ifndef HTTPREQUEST_HPP
#define HTTPREQUEST_HPP

#include <string>
#include <map>

class HttpRequest {
private:
    std::string _method;
    std::string _uri;
    std::string _version;
    std::map<std::string, std::string> _headers;
    std::string _body;
    bool _isParsed;

    void parseRequestLine(const std::string& line);
    void parseHeader(const std::string& line);

public:
    HttpRequest();
    ~HttpRequest();

    void parse(const std::string& raw_request);

    std::string getMethod() const;
    std::string getUri() const;
    std::string getVersion() const;
    std::string getHeader(const std::string& key) const;
    const std::string& getBody() const;
    const std::map<std::string, std::string>& getHeaders() const;
    bool isParsed() const;
};

#endif