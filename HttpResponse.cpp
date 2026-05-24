#include "HttpResponse.hpp"
#include <sstream>
#include <fstream>
#include <ctime>
#include <cstdlib>
#include <cstdio>
#include <dirent.h>
#include <sys/stat.h>
#include <iostream>
#include "Cgi.hpp"

static std::string cleanUriStr(std::string uri) {
    size_t qPos = uri.find('?');
    if (qPos != std::string::npos) {
        uri = uri.substr(0, qPos);
    }
    size_t endpos = uri.find_last_not_of(" \t\r\n");
    if (endpos != std::string::npos) {
        uri = uri.substr(0, endpos + 1);
    }
    return uri;
}

HttpResponse::HttpResponse(const HttpRequest& req, const Server& config) 
    : _request(req), _serverConfig(config), _statusCode(200), _statusMessage("OK") {
    buildResponse();
}

HttpResponse::~HttpResponse() {}

std::string HttpResponse::getRawResponse() const {
    return _rawResponse;
}

const Location* HttpResponse::matchLocation(const std::string& rawUri) {
    const Location* best_match = NULL;
    size_t longest_match = 0;
    
    std::string uri = cleanUriStr(rawUri);
    std::string currentMethod = _request.getMethod();

    for (size_t i = 0; i < _serverConfig.locations.size(); ++i) {
        std::string locUrl = _serverConfig.locations[i].url;
        size_t endpos = locUrl.find_last_not_of(" \t\r\n");
        if (endpos != std::string::npos) {
            locUrl = locUrl.substr(0, endpos + 1);
        }

        if (locUrl.length() > 1 && locUrl[0] == '*') {
            std::string ext = locUrl.substr(1);
            if (uri.length() >= ext.length() && 
                uri.compare(uri.length() - ext.length(), ext.length(), ext) == 0) {
                
                bool methodOk = false;
                for (size_t m = 0; m < _serverConfig.locations[i].allowedMethods.size(); ++m) {
                    if (_serverConfig.locations[i].allowedMethods[m] == currentMethod) {
                        methodOk = true;
                        break;
                    }
                }

                if (methodOk) {
                    return &_serverConfig.locations[i];
                } else {
                    continue;
                }
            }
            continue; 
        }

        if (uri == locUrl) {
            if (locUrl.length() > longest_match) {
                best_match = &_serverConfig.locations[i];
                longest_match = locUrl.length();
            }
        }
        else if (uri.find(locUrl) == 0) {
            bool isValidPrefix = false;
            if (locUrl == "/" || locUrl[locUrl.length() - 1] == '/') {
                isValidPrefix = true; 
            } else if (uri.length() > locUrl.length() && uri[locUrl.length()] == '/') {
                isValidPrefix = true; 
            }

            if (isValidPrefix && locUrl.length() > longest_match) {
                best_match = &_serverConfig.locations[i];
                longest_match = locUrl.length();
            }
        }
        
        else if (locUrl.length() > 1 && locUrl[locUrl.length() - 1] == '/') {
            std::string locNoSlash = locUrl.substr(0, locUrl.length() - 1);
            if (uri == locNoSlash) {
                if (locUrl.length() > longest_match) {
                    best_match = &_serverConfig.locations[i];
                    longest_match = locUrl.length();
                }
            }
        }
    }

    return best_match;
}

std::string HttpResponse::getContentType(const std::string& filePath) {
    size_t dotPos = filePath.find_last_of(".");
    if (dotPos == std::string::npos) return "text/plain";
    
    std::string ext = filePath.substr(dotPos);
    if (ext == ".html" || ext == ".htm") return "text/html";
    if (ext == ".css") return "text/css";
    if (ext == ".js") return "application/javascript";
    if (ext == ".png") return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".gif") return "image/gif";
    
    return "text/plain";
}

bool HttpResponse::readFile(const std::string& filePath, std::string& content) {
    std::ifstream file(filePath.c_str(), std::ios::in | std::ios::binary);
    if (!file) return false;

    std::ostringstream ss;
    ss << file.rdbuf();
    content = ss.str();
    return true;
}

void HttpResponse::generateErrorResponse(int code) {
    _statusCode = code;
    if (code == 404) _statusMessage = "Not Found";
    else if (code == 405) _statusMessage = "Method Not Allowed";
    else if (code == 403) _statusMessage = "Forbidden";
    else _statusMessage = "Error";

    _contentType = "text/html";
    std::string errorPath = "";

    for (size_t i = 0; i < _serverConfig.errorPages.size(); ++i) {
        if (_serverConfig.errorPages[i].code == code) {
            errorPath = _serverConfig.errorPages[i].path;
            break;
        }
    }

    if (!errorPath.empty() && readFile(errorPath, _body)) {
        return;
    }

    std::ostringstream ss;
    ss << "<html><body><h1>" << _statusCode << " " << _statusMessage << "</h1></body></html>";
    _body = ss.str();
}

void HttpResponse::handleGet(const Location* loc) {
    std::string targetPath = loc->path; 
    std::string uri = cleanUriStr(_request.getUri());

    std::string subPath = uri;
    if (loc->url.length() > 0 && loc->url[0] != '*' && uri.find(loc->url) == 0) {
        subPath = uri.substr(loc->url.length());
    }

    if (!subPath.empty()) {
        if (targetPath[targetPath.length() - 1] == '/' && subPath[0] == '/') {
            targetPath += subPath.substr(1);
        } else if (targetPath[targetPath.length() - 1] != '/' && subPath[0] != '/') {
            targetPath += "/" + subPath;
        } else {
            targetPath += subPath;
        }
    }
    
    if (isDirectory(targetPath)) {
        std::string indexPath = targetPath;
        if (indexPath[indexPath.length() - 1] != '/') {
            indexPath += "/";
        }
        indexPath += loc->index_path;

        if (!loc->index_path.empty()) {
            if (executeCgiIfMatch(indexPath, loc)) return;
            
            if (readFile(indexPath, _body)) {
                _statusCode = 200;
                _statusMessage = "OK";
                _contentType = getContentType(indexPath);
                return;
            }
        }

        if (loc->autoindex) {
            handleAutoindex(targetPath, uri);
        } else {
            if (!loc->index_path.empty()) {
                generateErrorResponse(404);
            } else {
                generateErrorResponse(403);
            }
        }
    } 
    else {
        if (executeCgiIfMatch(targetPath, loc)) return;

        if (readFile(targetPath, _body)) {
            _statusCode = 200;
            _statusMessage = "OK";
            _contentType = getContentType(targetPath);
        } else {
            generateErrorResponse(404);
        }
    }
}

void HttpResponse::buildResponse() {
    const Location* loc = matchLocation(_request.getUri());

    if (!loc) {
        generateErrorResponse(404);
    } else {
        bool methodAllowed = false;
        for (size_t i = 0; i < loc->allowedMethods.size(); ++i) {
            if (loc->allowedMethods[i] == _request.getMethod()) {
                methodAllowed = true;
                break;
            }
        }

        if (!methodAllowed) {
            generateErrorResponse(405);
        } else {
            if (_request.getMethod() == "GET" || _request.getMethod() == "HEAD") {
                handleGet(loc);
            } 
            else if (_request.getMethod() == "POST") {
                handlePost(loc);
            }
            else if (_request.getMethod() == "DELETE") {
                handleDelete(loc);
            }
            else {
                generateErrorResponse(405);
            }
        }
    }

    if (!_rawResponse.empty()) {
        return; 
    }

std::ostringstream responseStream;
    responseStream << "HTTP/1.1 " << _statusCode << " " << _statusMessage << "\r\n";
    responseStream << "Content-Type: " << _contentType << "\r\n";
    responseStream << "Content-Length: " << _body.length() << "\r\n";
    responseStream << "Connection: close\r\n";
    responseStream << "\r\n";
    
    if (_request.getMethod() != "HEAD") {
        responseStream << _body;
    }

    _rawResponse = responseStream.str();
}
void HttpResponse::handlePost(const Location* loc) {
    std::string targetPath = loc->path;
    std::string uri = cleanUriStr(_request.getUri());

    std::string subPath = uri;
    if (loc->url.length() > 0 && loc->url[0] != '*' && uri.find(loc->url) == 0) {
        subPath = uri.substr(loc->url.length());
    }

    if (!subPath.empty()) {
        if (targetPath[targetPath.length() - 1] == '/' && subPath[0] == '/') {
            targetPath += subPath.substr(1);
        } else if (targetPath[targetPath.length() - 1] != '/' && subPath[0] != '/') {
            targetPath += "/" + subPath;
        } else {
            targetPath += subPath;
        }
    }

    if (executeCgiIfMatch(targetPath, loc)) return;

    if (loc->max_byte > 0 && _request.getBody().length() > (size_t)loc->max_byte) {
        generateErrorResponse(413);
        return;
    }

    if (!loc->upload) {
        _statusCode = 200;
        _statusMessage = "OK";
        _contentType = "text/plain";
        _body = "POST request accepted successfully.";
        return;
    }

    std::string uploadDir = loc->upload_path;
    if (uploadDir.empty()) uploadDir = "/tmp/www/uploads";

    std::string body = _request.getBody();
    std::string filename = "";
    std::string fileContent = "";

    std::string contentType = _request.getHeader("Content-Type");
    std::string boundary = "";
    size_t boundaryPos = contentType.find("boundary=");
    
    if (boundaryPos != std::string::npos) {
        boundary = "--" + contentType.substr(boundaryPos + 9);
        size_t startBoundary = body.find(boundary);
        if (startBoundary != std::string::npos) {
            size_t filenamePos = body.find("filename=\"", startBoundary);
            if (filenamePos != std::string::npos) {
                filenamePos += 10;
                size_t filenameEnd = body.find("\"", filenamePos);
                if (filenameEnd != std::string::npos) {
                    filename = body.substr(filenamePos, filenameEnd - filenamePos);
                }
            }
            size_t dataStart = body.find("\r\n\r\n", startBoundary);
            if (dataStart != std::string::npos) {
                dataStart += 4;
                std::string endBoundary = "\r\n" + boundary;
                size_t dataEnd = body.find(endBoundary, dataStart);
                if (dataEnd != std::string::npos) fileContent = body.substr(dataStart, dataEnd - dataStart);
                else fileContent = body.substr(dataStart);
            }
        }
    }

    if (filename.empty()) {
        std::ostringstream fallbackName;
        fallbackName << "raw_post_" << time(NULL) << ".txt";
        filename = fallbackName.str();
        fileContent = body;
    }

    std::string fullPath = uploadDir + "/" + filename;
    std::ofstream outFile(fullPath.c_str(), std::ios::out | std::ios::binary);
    
    if (!outFile.is_open()) {
        generateErrorResponse(500);
        return;
    }

    outFile.write(fileContent.c_str(), fileContent.length());
    outFile.close();

    _statusCode = 201;
    _statusMessage = "Created";
    _contentType = "text/html";
    _body = "<html><body><h2>Dosya Basariyla Yuklendi!</h2></body></html>";
}

void HttpResponse::handleDelete(const Location* loc) {
    std::string targetPath = loc->path;
    std::string uri = cleanUriStr(_request.getUri());

    std::string subPath = uri;
    if (loc->url.length() > 0 && loc->url[0] != '*' && uri.find(loc->url) == 0) {
        subPath = uri.substr(loc->url.length());
    }

    if (!subPath.empty()) {
        if (targetPath[targetPath.length() - 1] == '/' && subPath[0] == '/') {
            targetPath += subPath.substr(1);
        } else if (targetPath[targetPath.length() - 1] != '/' && subPath[0] != '/') {
            targetPath += "/" + subPath;
        } else {
            targetPath += subPath;
        }
    }

    if (std::remove(targetPath.c_str()) == 0) {
        _statusCode = 200;
        _statusMessage = "OK";
        _contentType = "text/html";
        _body = "<html><body><h2>Silme Islemi Basarili!</h2></body></html>";
    } else {
        generateErrorResponse(404);
    }
}

bool HttpResponse::isDirectory(const std::string& path) {
    struct stat statbuf;
    if (stat(path.c_str(), &statbuf) != 0) return false;
    return S_ISDIR(statbuf.st_mode);
}

void HttpResponse::handleAutoindex(const std::string& targetPath, const std::string& uri) {
    DIR *dir;
    struct dirent *ent;
    
    if ((dir = opendir(targetPath.c_str())) != NULL) {
        std::ostringstream html;
        html << "<html><head><title>Index of " << uri << "</title></head><body>";
        html << "<h1>Index of " << uri << "</h1><hr><pre>";

        while ((ent = readdir(dir)) != NULL) {
            std::string filename = ent->d_name;
            html << "<a href=\"" << uri;
            if (uri.length() > 0 && uri[uri.length() - 1] != '/') html << "/";
            html << filename << "\">" << filename << "</a><br>";
        }
        closedir(dir);
        html << "</pre><hr></body></html>";

        _statusCode = 200;
        _statusMessage = "OK";
        _contentType = "text/html";
        _body = html.str();
    } else {
        generateErrorResponse(403);
    }
}

bool HttpResponse::executeCgiIfMatch(const std::string& targetPath, const Location* loc) {
    size_t dotPos = targetPath.find_last_of(".");
    if (dotPos == std::string::npos) return false;
    
    std::string ext = targetPath.substr(dotPos);
    std::map<std::string, std::string>::const_iterator it = loc->cgi_ext.find(ext);
    
    if (it != loc->cgi_ext.end()) {
        std::string program = it->second;
        Cgi cgi(_request, targetPath, program);
        _rawResponse = cgi.execute(); 
        return true;
    }
    return false;
}