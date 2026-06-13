#include "Server.hpp"
#include "FileUtils.hpp"

#include <cstdio>
#include <fstream>

static const std::string* findCgiInterpreter(const LocationConfig& loc,
                                             const std::string& filepath) {
    std::string::size_type dot = filepath.rfind('.');
    if (dot == std::string::npos)
        return NULL;

    std::map<std::string, std::string>::const_iterator it = loc.cgi.find(filepath.substr(dot));
    if (it == loc.cgi.end())
        return NULL;
    return &it->second;
}

std::string Server::handleGet(const LocationConfig& loc, const HttpRequest& req,
                              const ServerConfig& server, int client_fd) {
    std::string cleanPath = stripQuery(req.path);
    std::string filepath = resolvePath(loc, cleanPath);

    const std::string* interpreter = findCgiInterpreter(loc, filepath);
    if (interpreter) {
        startCGI(*interpreter, filepath, req, client_fd);
        return "";
    }

    PathType type = pathType(filepath);

    if (type == PATH_NOT_FOUND)
        return buildError(404, server);

    if (type == PATH_FILE) {
        std::string content;
        if (readFile(filepath, content))
            return buildResponse(200, "OK", getMimeType(filepath), content);
        return buildError(403, server);
    }

    std::string indexPath = filepath;
    if (indexPath[indexPath.size() - 1] != '/')
        indexPath += "/";
    indexPath += (loc.index.empty() ? "index.html" : loc.index);

    std::string content;
    if (pathType(indexPath) == PATH_FILE && readFile(indexPath, content))
        return buildResponse(200, "OK", getMimeType(indexPath), content);

    if (loc.autoindex) {
        std::string listing = generateAutoindex(filepath, cleanPath);
        if (!listing.empty())
            return buildResponse(200, "OK", "text/html", listing);
    }

    return buildError(404, server);
}

std::string Server::handleDelete(const LocationConfig& loc, const HttpRequest& req,
                                 const ServerConfig& server) {
    std::string filepath = resolvePath(loc, req.path);
    PathType type = pathType(filepath);

    if (type == PATH_NOT_FOUND)
        return buildError(404, server);
    if (type == PATH_DIRECTORY)
        return buildError(403, server);
    if (std::remove(filepath.c_str()) != 0)
        return buildError(403, server);

    std::string body = "<html><body><h1>200 OK</h1><p>File deleted.</p></body></html>";
    return buildResponse(200, "OK", "text/html", body);
}

std::string Server::handlePost(const LocationConfig& loc, const HttpRequest& req,
                               const ServerConfig& server, int client_fd) {
    std::string cleanPath = stripQuery(req.path);
    std::string filepath = resolvePath(loc, cleanPath);

    const std::string* interpreter = findCgiInterpreter(loc, filepath);
    if (interpreter) {
        startCGI(*interpreter, filepath, req, client_fd);
        return "";
    }

    if (loc.upload_enable) {
        if (loc.upload_store.empty())
            return buildError(500, server);

        std::string filename = cleanPath;
        std::string::size_type slash = filename.rfind('/');
        if (slash != std::string::npos)
            filename = filename.substr(slash + 1);
        if (filename.empty())
            filename = "upload.dat";

        std::string dest = loc.upload_store;
        if (!dest.empty() && dest[dest.size() - 1] != '/')
            dest += "/";
        dest += filename;

        std::ofstream out(dest.c_str(), std::ios::binary);
        if (!out.is_open())
            return buildError(500, server);
        out.write(req.body.c_str(), req.body.size());

        std::string body = "<html><body><h1>201 Created</h1><p>File uploaded.</p></body></html>";
        return buildResponse(201, "Created", "text/html", body);
    }

    std::string body = "<html><body><h1>200 OK</h1><p>POST received.</p></body></html>";
    return buildResponse(200, "OK", "text/html", body);
}

std::string Server::handleRequest(const HttpRequest& req, const ServerConfig& server,
                                  int client_fd) {
    if (!req.valid)
        return buildError(400, server);

    if (!isPathSafe(req.path))
        return buildError(403, server);

    const LocationConfig* loc = matchLocation(server, req.path);
    if (loc == NULL)
        return buildError(404, server);

    if (loc->redirect_code != 0)
        return buildRedirect(loc->redirect_code, loc->redirect_url);

    if (loc->allowed_methods.find(req.method) == loc->allowed_methods.end())
        return buildError(405, server);

    size_t body_limit = (loc->client_max_body_size > 0)
                        ? loc->client_max_body_size
                        : server.client_max_body_size;
    if (req.body.size() > body_limit)
        return buildError(413, server);

    if (req.method == "GET")
        return handleGet(*loc, req, server, client_fd);
    if (req.method == "POST")
        return handlePost(*loc, req, server, client_fd);
    if (req.method == "DELETE")
        return handleDelete(*loc, req, server);

    return buildError(501, server);
}
