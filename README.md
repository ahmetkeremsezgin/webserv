*This project has been created as part of the 42 curriculum by asezgin, saincesu, keezgi.*

# Webserv

A lightweight HTTP/1.1 web server written from scratch in **C++98**, inspired by the behaviour of NGINX.

## Description

Webserv is a non-blocking HTTP server that listens on one or more `host:port`
pairs and serves content based on a configuration file. The goal of the project
is to understand how the HTTP protocol works under the hood — how a client
(usually a web browser) opens a connection, sends a request, and how the server
parses that request, locates a resource, and returns a properly formed response.

In short, the server takes incoming requests arriving over different ports and
routes them to the right resources (files, directories, redirections, or CGI
programs) according to the rules defined in its configuration file, then sends
back the appropriate response and status code.

The entire I/O layer is driven by a **single multiplexing call** (`poll()` /
`epoll` / `kqueue` / `select`), so the server stays non-blocking and can handle
many simultaneous clients without ever hanging on a slow or disconnected one.

## Features

- HTTP/1.1 request parsing and response generation
- Supports the **GET**, **POST**, and **DELETE** methods
- Single-`poll()` (or equivalent) event loop monitoring read and write readiness simultaneously
- Listens on **multiple ports / interfaces** at the same time, each serving different content
- Serves **fully static websites**
- **File uploads** from clients, stored in a configurable location
- **Directory listing** (autoindex) that can be enabled or disabled per route
- Default index file when a directory is requested
- **HTTP redirections** per route
- Configurable **maximum client body size**
- **Custom and default error pages** (built-in fallbacks when none are provided)
- **CGI execution** based on file extension (e.g. `.php`, `.py`), with proper
  environment variables and handling of chunked requests
- Accurate HTTP status codes, comparable to NGINX
- Resilient: stays available under stress and never crashes

## Instructions

### Requirements

- A C++ compiler supporting **C++98** (`c++`, `g++`, or `clang++`)
- `make`

### Build & Run

```bash
# Clone the repository
git clone <this repo>
cd <this repo>

# Compile
make

# Run with a configuration file
./webserv <config file>

# If no configuration file is provided, the server falls back to default.conf
./webserv
```

### Makefile rules

| Rule     | Description                          |
|----------|--------------------------------------|
| `make`   | Build the `webserv` executable       |
| `clean`  | Remove object files                  |
| `fclean` | Remove object files and the binary   |
| `re`     | Rebuild from scratch                 |

## Configuration file

The configuration syntax is loosely inspired by the `server` block of an NGINX
configuration. A server can define multiple `location` blocks that apply rules
to specific URL paths.

> **Note:** The example below shows the kinds of directives the server
> understands. Adjust the exact keywords to match your own parser.

```nginx
server {
    listen 8080;
    server_name example.com;

    client_max_body_size 1048576;

    error_page 404 ./www/errors/404.html;
    error_page 500 ./www/errors/500.html;

    location / {
        root ./www/html;
        index index.html;
        allowed_methods GET POST;
        autoindex off;
    }

    location /anasayfa {
        root ./www/html;
        index index.html;
        allowed_methods GET POST;
        autoindex off;
    }

    location /upload {
        root ./www/uploads;
        allowed_methods POST DELETE;
        upload_enable on;
        upload_store ./www/uploads;
    }

    location /cgi-bin {
        root ./www/cgi;
        allowed_methods GET POST;
        cgi .js /usr/bin/node;
    }
    
    location /old {
        redirect 301 /new;
    }
    location /google {
        redirect 302 http://www.google.com;
    }
}
```

You can define several `server` blocks to listen on multiple ports and serve
different content from a single running instance.

## Testing

The server is compatible with standard web browsers. It can also be tested with
tools such as `curl`, `telnet`, and by comparing its responses against NGINX.

```bash
# Simple GET
curl -v http://localhost:8080/

# Upload a file
curl -X POST --data-binary @file.txt http://localhost:8080/uploads/file.txt

# Delete a resource
curl -X DELETE http://localhost:8080/uploads/file.txt
```

For resilience, the server was stress-tested with custom scripts rather than a
single client program.

## Resources

- [HTTP/1.1 reference](https://http.dev/1.1)
- [NGINX documentation](https://nginx.org/en/docs/)
- [RFC 1945 — HTTP/1.0](https://datatracker.ietf.org/doc/html/rfc1945)
- [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/html/#client-server-background)
- Manual pages: `man listen`, `man bind`, `man accept`, `man socket`

### Use of AI

AI tools were used as a support resource, not as a code generator. Specifically,
they helped us:

- Understand core networking and HTTP concepts (sockets, multiplexing, the
  request/response lifecycle) before implementing them ourselves.
- Set up and reason about **testing environments**, and design **edge-case
  tests** to probe the server's robustness.
- Clarify documentation and explain unfamiliar terms found in the RFCs and man pages.

Every AI-assisted idea was reviewed, tested, and validated with peers, and only
content we fully understand was incorporated into the project.