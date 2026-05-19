#include "Server.hpp"
#include "Config.hpp"
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <iostream>
#include <vector>

struct clientDetails {
    std::vector<int> serverFds;
    std::vector<int> clientList;
};

int ServerRunner::OpenSocket(std::vector<Server> servers) {
    clientDetails client;

    for (size_t i = 0; i < servers.size(); ++i) {
        int sfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sfd <= 0) {
            std::cerr << "socket creation error" << std::endl;
            continue;
        }

        int opt = 1;
        if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof opt) < 0) {
            std::cerr << "setSocketopt" << std::endl;
        }

        struct sockaddr_in serverAddress;
        serverAddress.sin_family = AF_INET;
        serverAddress.sin_port = htons(servers[i].port);
        serverAddress.sin_addr.s_addr = INADDR_ANY;

        if (bind(sfd, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
            std::cerr << "bind error" << std::endl;
            close(sfd);
            continue;
        }

        if (listen(sfd, 5) < 0) {
            std::cerr << "listen error" << std::endl;
            close(sfd);
            continue;
        }

        client.serverFds.push_back(sfd);
    }

    if (client.serverFds.empty()) {
        std::cerr << "No servers could be started" << std::endl;
        return -1;
    }

    fd_set readfds;
    ssize_t valerad;
    int maxfd;
    int activity;

    while (true) {
        std::cout << "Ready For Connect" << std::endl;
        FD_ZERO(&readfds);
        maxfd = -1;

        for (size_t i = 0; i < client.serverFds.size(); ++i) {
            int sfd = client.serverFds[i];
            FD_SET(sfd, &readfds);
            if (sfd > maxfd) {
                maxfd = sfd;
            }
        }

        std::vector<int>::const_iterator it;
        for (it = client.clientList.begin(); it != client.clientList.end(); ++it) {
            int sd = *it;
            FD_SET(sd, &readfds);
            if (sd > maxfd) {
                maxfd = sd;
            }
        }

        activity = select(maxfd + 1, &readfds, NULL, NULL, NULL);
        if (activity < 0) {
            std::cerr << "Select error" << std::endl;
            continue;
        }

        for (size_t i = 0; i < client.serverFds.size(); ++i) {
            int sfd = client.serverFds[i];
            if (FD_ISSET(sfd, &readfds)) {
                struct sockaddr_in clientAddress;
                socklen_t addrlen = sizeof(clientAddress);
                int new_client_fd = accept(sfd, (struct sockaddr *)&clientAddress, &addrlen);
                
                if (new_client_fd < 0) {
                    std::cerr << "accept error" << std::endl;
                    continue;
                }
                
                std::cout << "New connection accepted on port " << servers[i].port << ", fd: " << new_client_fd << std::endl;
                client.clientList.push_back(new_client_fd);
            }
        }

        char message[1024];
        for (size_t i = 0; i < client.clientList.size(); ++i) {
            int sd = client.clientList[i];
            if (FD_ISSET(sd, &readfds)) {
                memset(message, 0, 1024);
                valerad = read(sd, message, 1023);
                
                if (valerad <= 0) {
                    std::cout << "client disconnect, fd: " << sd << std::endl;
                    close(sd);
                    client.clientList.erase(client.clientList.begin() + i);
                    --i;
                } else {
                    std::cout << "Message: " << message << std::endl;
                }
            }
        }
    }
    return 1;
}

ServerRunner::ServerRunner(std::vector<Server> servers) {
    OpenSocket(servers);
}