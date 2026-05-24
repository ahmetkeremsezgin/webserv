#include "HttpRequest.hpp"
#include <sstream>
#include <iostream>
#include <cstdlib>

HttpRequest::HttpRequest() : _isParsed(false) {}

HttpRequest::~HttpRequest() {}

std::string HttpRequest::getMethod() const { return _method; }
std::string HttpRequest::getUri() const { return _uri; }
std::string HttpRequest::getVersion() const { return _version; }
const std::string& HttpRequest::getBody() const { return _body; }
bool HttpRequest::isParsed() const { return _isParsed; }
const std::map<std::string, std::string>& HttpRequest::getHeaders() const {
    return _headers;
}
std::string HttpRequest::getHeader(const std::string& key) const {
    std::map<std::string, std::string>::const_iterator it = _headers.find(key);
    if (it != _headers.end()) {
        return it->second;
    }
    return "";
}

void HttpRequest::parseRequestLine(const std::string& line) {
    std::istringstream stream(line);
    stream >> _method >> _uri >> _version;
}

void HttpRequest::parseHeader(const std::string& line) {
    size_t colon_pos = line.find(':');
    if (colon_pos != std::string::npos) {
        std::string key = line.substr(0, colon_pos);
        std::string value = line.substr(colon_pos + 1);
        
        size_t start = value.find_first_not_of(" \t");
        if (start != std::string::npos) {
            value = value.substr(start);
        }
        
        if (!value.empty() && value[value.length() - 1] == '\r') {
            value.erase(value.length() - 1);
        }
        
        _headers[key] = value;
    }
}

void HttpRequest::parse(const std::string& raw_request) {
    size_t header_end = raw_request.find("\r\n\r\n");
    if (header_end == std::string::npos) {
        return;
    }

    std::string header_part = raw_request.substr(0, header_end);
    std::istringstream stream(header_part);
    std::string line;
    int line_number = 0;

    while (std::getline(stream, line)) {
        if (!line.empty() && line[line.length() - 1] == '\r') {
            line.erase(line.length() - 1);
        }

        if (line_number == 0) {
            parseRequestLine(line);
        } else {
            parseHeader(line);
        }
        line_number++;
    }

if (raw_request.length() > header_end + 4) {
        size_t body_start = header_end + 4;
        size_t body_length = raw_request.length() - body_start;
        
        if (getHeader("Transfer-Encoding") == "chunked") {
            size_t pos = body_start;
            while (pos < raw_request.length()) {
                size_t crlf_pos = raw_request.find("\r\n", pos);
                if (crlf_pos == std::string::npos) break;
                
                std::string hexStr = raw_request.substr(pos, crlf_pos - pos);
                size_t chunkSize = std::strtoul(hexStr.c_str(), NULL, 16);
                
                if (chunkSize == 0) break;
            
                pos = crlf_pos + 2;
                if (pos + chunkSize <= raw_request.length()) {
                    _body.append(raw_request, pos, chunkSize);
                }
                pos += chunkSize + 2;
            }
        } 
        else {
            std::string cl_str = getHeader("Content-Length");
            if (!cl_str.empty()) {
                size_t expected_length = std::strtoul(cl_str.c_str(), NULL, 10);
                _body.reserve(expected_length > body_length ? expected_length : body_length);
            } else {
                _body.reserve(body_length);
            }
            _body.assign(raw_request, body_start, body_length);
        }
    }

    if (!_method.empty() && !_uri.empty()) {
        _isParsed = true;
    }
}