#include "Server.hpp"
#include "Client.hpp"
#include "Config.hpp"
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <iostream>
#include <vector>
#include <map>

struct serverState {
    std::map<int, Server> serverConfigs;
    std::map<int, Client*> clients;
};

ServerRunner::ServerRunner(std::vector<Server> servers) {
    serverState state;
    fd_set readfds;
    fd_set writefds;
    int maxfd = -1;

    for (size_t i = 0; i < servers.size(); ++i) {
        int sfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sfd <= 0) {
            std::cerr << "Socket creation error" << std::endl;
            continue;
        }

        int opt = 1;
        if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof opt) < 0) {
            std::cerr << "setsockopt error" << std::endl;
        }

        struct sockaddr_in serverAddress;
        serverAddress.sin_family = AF_INET;
        serverAddress.sin_port = htons(servers[i].port);
        serverAddress.sin_addr.s_addr = INADDR_ANY;

        if (bind(sfd, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
            std::cerr << "Bind error" << std::endl;
            close(sfd);
            continue;
        }

        if (listen(sfd, 5) < 0) {
            std::cerr << "Listen error" << std::endl;
            close(sfd);
            continue;
        }

        state.serverConfigs[sfd] = servers[i];
        if (sfd > maxfd) {
            maxfd = sfd;
        }
    }

    if (state.serverConfigs.empty()) {
        std::cerr << "No servers could be started" << std::endl;
        return;
    }

    std::cout << "Servers started, waiting for connections..." << std::endl;

    while (true) {
        FD_ZERO(&readfds);
        FD_ZERO(&writefds);
        int current_maxfd = maxfd;

        for (std::map<int, Server>::iterator it = state.serverConfigs.begin(); it != state.serverConfigs.end(); ++it) {
            FD_SET(it->first, &readfds);
            if (it->first > current_maxfd) {
                current_maxfd = it->first;
            }
        }

        for (std::map<int, Client*>::iterator it = state.clients.begin(); it != state.clients.end(); ++it) {
            int client_fd = it->first;
            Client* client_obj = it->second;

            FD_SET(client_fd, &readfds);

            if (client_obj->isResponseReady()) {
                FD_SET(client_fd, &writefds);
            }

            if (client_fd > current_maxfd) {
                current_maxfd = client_fd;
            }
        }

        int activity = select(current_maxfd + 1, &readfds, &writefds, NULL, NULL);
        if (activity < 0) {
            std::cerr << "Select error" << std::endl;
            continue;
        }

        for (std::map<int, Server>::iterator it = state.serverConfigs.begin(); it != state.serverConfigs.end(); ++it) {
            int sfd = it->first;
            if (FD_ISSET(sfd, &readfds)) {
                struct sockaddr_in clientAddress;
                socklen_t addrlen = sizeof(clientAddress);
                int new_client_fd = accept(sfd, (struct sockaddr *)&clientAddress, &addrlen);
                
                if (new_client_fd < 0) {
                    std::cerr << "Accept error" << std::endl;
                    continue;
                }
                
                std::cout << "New connection accepted. Port: " << it->second.port << ", FD: " << new_client_fd << std::endl;
                
                state.clients[new_client_fd] = new Client(new_client_fd, it->second);
            }
        }

        std::map<int, Client*>::iterator it = state.clients.begin();
        while (it != state.clients.end()) {
            int client_fd = it->first;
            Client* client_obj = it->second;
            bool disconnect_client = false;

            if (FD_ISSET(client_fd, &readfds)) {
                if (!client_obj->readData()) {
                    std::cout << "Client disconnected, FD: " << client_fd << std::endl;
                    disconnect_client = true;
                } else if (client_obj->isRequestReady() && !client_obj->isResponseReady()) {
                    client_obj->processRequest();
                }
            }

            if (!disconnect_client && FD_ISSET(client_fd, &writefds)) {
                if (!client_obj->sendData()) {
                    std::cout << "Client task finished or disconnected, FD: " << client_fd << std::endl;
                    disconnect_client = true;
                }
            }

            if (disconnect_client) {
                delete client_obj;
                std::map<int, Client*>::iterator temp_it = it;
                ++it;
                state.clients.erase(temp_it);
            } else {
                ++it;
            }
        }
    }
}