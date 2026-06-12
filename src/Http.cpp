#include "Http.hpp"
#include "FileUtils.hpp"

#include <sstream>

std::string stripQuery(const std::string& path) {
    std::string::size_type q = path.find('?');
    if (q == std::string::npos)
        return path;
    return path.substr(0, q);
}

static size_t parseHexSize(const std::string& s) {
    std::stringstream ss;
    ss << std::hex << s;
    size_t value = 0;
    ss >> value;
    return value;
}

std::string unchunkBody(const std::string& chunked) {
    std::string result;
    std::string::size_type pos = 0;

    while (pos < chunked.size()) {
        std::string::size_type line_end = chunked.find("\r\n", pos);
        if (line_end == std::string::npos)
            break;

        size_t chunk_size = parseHexSize(chunked.substr(pos, line_end - pos));
        if (chunk_size == 0)
            break;

        std::string::size_type data_start = line_end + 2;
        if (data_start + chunk_size > chunked.size())
            break;

        result.append(chunked, data_start, chunk_size);
        pos = data_start + chunk_size + 2;
    }

    return result;
}

bool isChunkedComplete(const std::string& buffer, size_t& scan_pos) {
    while (scan_pos < buffer.size()) {
        std::string::size_type line_end = buffer.find("\r\n", scan_pos);
        if (line_end == std::string::npos)
            return false;

        size_t chunk_size = parseHexSize(buffer.substr(scan_pos, line_end - scan_pos));
        if (chunk_size == 0)
            return true;

        size_t next = line_end + 2 + chunk_size + 2;
        if (next > buffer.size())
            return false;

        scan_pos = next;
    }
    return false;
}

HttpRequest parseRequest(const std::string& raw) {
    HttpRequest req;

    std::string::size_type header_end = raw.find("\r\n\r\n");
    std::string header_part;
    std::string body_part;

    if (header_end != std::string::npos) {
        header_part = raw.substr(0, header_end);
        body_part = raw.substr(header_end + 4);
    } else {
        header_part = raw;
    }

    std::istringstream stream(header_part);
    std::string line;

    if (!std::getline(stream, line))
        return req;
    if (!line.empty() && line[line.size() - 1] == '\r')
        line.erase(line.size() - 1);
    {
        std::istringstream ls(line);
        if (!(ls >> req.method >> req.path >> req.version))
            return req;
    }

    while (std::getline(stream, line)) {
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);
        if (line.empty())
            break;

        std::string::size_type colon = line.find(':');
        if (colon == std::string::npos)
            continue;

        std::string name = line.substr(0, colon);
        std::string value = line.substr(colon + 1);
        std::string::size_type start = value.find_first_not_of(" \t");
        value = (start != std::string::npos) ? value.substr(start) : "";
        req.headers[name] = value;
    }

    std::map<std::string, std::string>::const_iterator te = req.headers.find("Transfer-Encoding");
    if (te != req.headers.end() && te->second.find("chunked") != std::string::npos) {
        std::string unchunked = unchunkBody(body_part);
        req.body.swap(unchunked);
    } else {
        req.body.swap(body_part);
    }

    req.valid = true;
    return req;
}

std::string statusText(int code) {
    switch (code) {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 400: return "Bad Request";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 413: return "Payload Too Large";
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        case 504: return "Gateway Timeout";
        default:  return "Unknown";
    }
}

std::string getMimeType(const std::string& path) {
    std::string::size_type dot = path.rfind('.');
    if (dot == std::string::npos)
        return "application/octet-stream";

    std::string ext = path.substr(dot + 1);

    if (ext == "html" || ext == "htm")  return "text/html";
    if (ext == "css")                   return "text/css";
    if (ext == "js")                    return "application/javascript";
    if (ext == "json")                  return "application/json";
    if (ext == "txt")                   return "text/plain";
    if (ext == "jpg" || ext == "jpeg")  return "image/jpeg";
    if (ext == "png")                   return "image/png";
    if (ext == "gif")                   return "image/gif";
    if (ext == "svg")                   return "image/svg+xml";
    if (ext == "ico")                   return "image/x-icon";
    if (ext == "pdf")                   return "application/pdf";

    return "application/octet-stream";
}

std::string buildResponse(int code, const std::string& status,
                          const std::string& contentType, const std::string& body) {
    std::stringstream res;
    res << "HTTP/1.1 " << code << " " << status << "\r\n";
    res << "Content-Type: " << contentType << "\r\n";
    res << "Content-Length: " << body.size() << "\r\n";
    res << "Connection: close\r\n";
    res << "\r\n";
    res << body;
    return res.str();
}

static std::string defaultHtmlBody(int code, const std::string& text) {
    std::stringstream body;
    body << "<html><head><title>" << code << " " << text << "</title></head>";
    body << "<body><center><h1>" << code << " " << text << "</h1></center>";
    body << "<hr><center>webserv</center></body></html>";
    return body.str();
}

std::string buildError(int code, const ServerConfig& server) {
    std::map<int, std::string>::const_iterator it = server.error_pages.find(code);
    if (it != server.error_pages.end()) {
        std::string content;
        if (readFile(it->second, content))
            return buildResponse(code, statusText(code), "text/html", content);
    }

    std::string text = statusText(code);
    return buildResponse(code, text, "text/html", defaultHtmlBody(code, text));
}

std::string buildRedirect(int code, const std::string& location) {
    std::string text = statusText(code);
    std::string body = defaultHtmlBody(code, text);

    std::stringstream res;
    res << "HTTP/1.1 " << code << " " << text << "\r\n";
    res << "Location: " << location << "\r\n";
    res << "Content-Type: text/html\r\n";
    res << "Content-Length: " << body.size() << "\r\n";
    res << "Connection: close\r\n";
    res << "\r\n";
    res << body;
    return res.str();
}

std::string buildCgiResponse(const std::string& cgiOutput) {
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

    std::stringstream head;
    head << "HTTP/1.1 200 OK\r\n";
    if (!headers.empty())
        head << headers << "\r\n";
    head << "Content-Length: " << body.size() << "\r\n";
    head << "Connection: close\r\n";
    head << "\r\n";

    std::string res = head.str();
    res += body;
    return res;
}
