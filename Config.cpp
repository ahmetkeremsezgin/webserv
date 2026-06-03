#include "Config.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cstdlib>
#include <cctype>

// '{', '}' and ';' are structural tokens; they can never stand in for a value.
static bool isDelimiter(const std::string& token) {
    return token == "{" || token == "}" || token == ";";
}

/* -------------------------------------------------------------------------- */
/*  Construction                                                              */
/* -------------------------------------------------------------------------- */

Config::Config(const Config& conf) : _pos(0) { *this = conf; }

Config& Config::operator=(const Config& conf) {
    if (this != &conf)
        servers = conf.servers;
    return *this;
}

Config::Config(std::string file_path) : _pos(0) {
    std::ifstream file(file_path.c_str());
    if (!file.is_open())
        error("could not open configuration file: " + file_path);

    std::string content;
    std::string line;
    while (std::getline(file, line)) {
        std::string::size_type hash = line.find('#'); // strip comments
        if (hash != std::string::npos)
            line.erase(hash);
        content += line + "\n";
    }

    tokenize(content);
    parse();
}

/* -------------------------------------------------------------------------- */
/*  Tokenizer                                                                 */
/* -------------------------------------------------------------------------- */

void Config::tokenize(const std::string& content) {
    std::string spaced;
    for (std::string::size_type i = 0; i < content.size(); ++i) {
        char c = content[i];
        if (c == '{' || c == '}' || c == ';') {
            spaced += ' ';
            spaced += c;
            spaced += ' ';
        } else {
            spaced += c;
        }
    }

    std::istringstream iss(spaced);
    std::string token;
    while (iss >> token)
        _tokens.push_back(token);
}

/* -------------------------------------------------------------------------- */
/*  Token cursor                                                              */
/* -------------------------------------------------------------------------- */

bool Config::eof() const {
    return _pos >= _tokens.size();
}

const std::string& Config::peek() const {
    if (eof())
        error("unexpected end of configuration file");
    return _tokens[_pos];
}

const std::string& Config::advance() {
    const std::string& token = peek();
    ++_pos;
    return token;
}

bool Config::accept(const std::string& token) {
    if (!eof() && _tokens[_pos] == token) {
        ++_pos;
        return true;
    }
    return false;
}

void Config::expect(const std::string& token, const std::string& context) {
    if (eof() || _tokens[_pos] != token)
        error("expected '" + token + "' " + context);
    ++_pos;
}

// Next token must be a real value, not a delimiter or end of file.
const std::string& Config::expectValue(const std::string& directive) {
    if (eof() || isDelimiter(peek()))
        error("'" + directive + "' directive is missing a value");
    return advance();
}

void Config::expectSemicolon(const std::string& directive) {
    if (!accept(";"))
        error("'" + directive + "' directive must end with ';'");
}

/* -------------------------------------------------------------------------- */
/*  Value validation                                                          */
/* -------------------------------------------------------------------------- */

void Config::error(const std::string& message) {
    throw std::runtime_error("Config error: " + message);
}

bool Config::isNumber(const std::string& value) {
    if (value.empty())
        return false;
    for (std::string::size_type i = 0; i < value.size(); ++i)
        if (!std::isdigit(static_cast<unsigned char>(value[i])))
            return false;
    return true;
}

bool Config::toToggle(const std::string& value, const std::string& directive) {
    if (value == "on")
        return true;
    if (value == "off")
        return false;
    error("'" + directive + "' must be 'on' or 'off', got '" + value + "'");
    return false; // unreachable
}

int Config::toPort(const std::string& value) {
    if (!isNumber(value))
        error("listen port must be a number, got '" + value + "'");
    long port = std::atol(value.c_str());
    if (port < 1 || port > 65535)
        error("listen port must be in range 1-65535, got '" + value + "'");
    return static_cast<int>(port);
}

int Config::toErrorCode(const std::string& value) {
    if (!isNumber(value))
        error("error_page code must be a number, got '" + value + "'");
    long code = std::atol(value.c_str());
    if (code < 100 || code > 599)
        error("error_page code must be in range 100-599, got '" + value + "'");
    return static_cast<int>(code);
}

long long Config::toSize(const std::string& value, const std::string& directive) {
    if (!isNumber(value))
        error("'" + directive + "' expects a non-negative number, got '" + value + "'");
    if (value.size() > 18)
        error("'" + directive + "' value is too large: " + value);
    long long result = 0;
    for (std::string::size_type i = 0; i < value.size(); ++i)
        result = result * 10 + (value[i] - '0');
    return result;
}

// "host:port" -> host + port; "port" -> 0.0.0.0 + port.
void Config::splitHostPort(const std::string& value, std::string& host, std::string& port) {
    std::string::size_type colon = value.rfind(':');
    if (colon == std::string::npos) {
        host = "0.0.0.0";
        port = value;
    } else {
        host = value.substr(0, colon);
        port = value.substr(colon + 1);
        if (host.empty())
            host = "0.0.0.0";
    }
}

/* -------------------------------------------------------------------------- */
/*  Parser                                                                    */
/* -------------------------------------------------------------------------- */

void Config::parse() {
    while (!eof()) {
        if (accept("server"))
            parseServer();
        else
            error("unexpected token '" + peek() + "' at top level (expected 'server')");
    }
    if (servers.empty())
        error("no server block found in configuration");
}

void Config::parseServer() {
    expect("{", "after 'server'");

    Server srv;
    srv.interface = "0.0.0.0";
    srv.server_fd = -1;
    srv.port = 0;
    srv.max_byte = 0;
    bool hasListen = false;

    while (!eof() && peek() != "}") {
        const std::string directive = advance();

        if (directive == "listen") {
            std::string host, port;
            splitHostPort(expectValue("listen"), host, port);
            srv.interface = host;
            srv.port = toPort(port);
            hasListen = true;
            expectSemicolon("listen");
        }
        else if (directive == "server_name") {
            srv.server_name = expectValue("server_name");
            expectSemicolon("server_name");
        }
        else if (directive == "client_max_body_size") {
            srv.max_byte = toSize(expectValue("client_max_body_size"), "client_max_body_size");
            expectSemicolon("client_max_body_size");
        }
        else if (directive == "error_page") {
            ErrorPages ep;
            ep.code = toErrorCode(expectValue("error_page"));
            ep.path = expectValue("error_page");
            srv.errorPages.push_back(ep);
            expectSemicolon("error_page");
        }
        else if (directive == "location") {
            parseLocation(srv);
        }
        else {
            error("unknown directive '" + directive + "' in server block");
        }
    }

    expect("}", "to close server block");
    if (!hasListen)
        error("server block is missing the required 'listen' directive");
    servers.push_back(srv);
}

void Config::parseLocation(Server& srv) {
    if (eof() || isDelimiter(peek()))
        error("'location' directive is missing a URL path");

    Location loc;
    loc.url = advance();
    loc.autoindex = false;
    loc.upload = false;
    loc.max_byte = srv.max_byte;

    expect("{", "after location '" + loc.url + "'");

    while (!eof() && peek() != "}") {
        const std::string directive = advance();

        if (directive == "allow_methods") {
            while (!eof() && !isDelimiter(peek())) {
                const std::string& method = peek();
                if (method != "GET" && method != "POST" && method != "DELETE")
                    error("allow_methods has an unsupported method '" + method + "' in location '" + loc.url + "'");
                loc.allowedMethods.push_back(method);
                advance();
            }
            if (loc.allowedMethods.empty())
                error("allow_methods needs at least one method in location '" + loc.url + "'");
            expectSemicolon("allow_methods");
        }
        else if (directive == "root") {
            loc.path = expectValue("root");
            expectSemicolon("root");
        }
        else if (directive == "index") {
            loc.index_path = expectValue("index");
            expectSemicolon("index");
        }
        else if (directive == "autoindex") {
            loc.autoindex = toToggle(expectValue("autoindex"), "autoindex");
            expectSemicolon("autoindex");
        }
        else if (directive == "upload") {
            loc.upload = toToggle(expectValue("upload"), "upload");
            expectSemicolon("upload");
        }
        else if (directive == "upload_path") {
            loc.upload_path = expectValue("upload_path");
            expectSemicolon("upload_path");
        }
        else if (directive == "return" || directive == "redirect") {
            loc.redirect = expectValue(directive);
            expectSemicolon(directive);
        }
        else if (directive == "client_max_body_size") {
            loc.max_byte = toSize(expectValue("client_max_body_size"), "client_max_body_size");
            expectSemicolon("client_max_body_size");
        }
        else if (directive == "cgi_ext") {
            const std::string ext = expectValue("cgi_ext");
            if (ext.empty() || ext[0] != '.')
                error("cgi_ext extension must start with '.', got '" + ext + "' in location '" + loc.url + "'");
            loc.cgi_ext[ext] = expectValue("cgi_ext");
            expectSemicolon("cgi_ext");
        }
        else {
            error("unknown directive '" + directive + "' in location '" + loc.url + "'");
        }
    }

    expect("}", "to close location '" + loc.url + "'");

    if (loc.allowedMethods.empty())
        error("location '" + loc.url + "' is missing the 'allow_methods' directive");
    if (loc.path.empty() && loc.redirect.empty())
        error("location '" + loc.url + "' is missing the 'root' directive");
    if (loc.upload && loc.upload_path.empty())
        error("location '" + loc.url + "' has 'upload on' but no 'upload_path'");

    srv.locations.push_back(loc);
}
