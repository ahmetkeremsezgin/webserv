#include "Server.hpp"
#include "Config.hpp"

#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>


int ServerRunner::OpenSocket(Server servers) {
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(servers.port);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    bind(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress));

    listen(serverSocket, 5);
    int clientSocket = accept(serverSocket, NULL, NULL);
    char buffer[1024] = {0};
    recv(clientSocket, buffer, sizeof(buffer), 0);
    std::cout << "Message from client: " << buffer << std::endl;

    close(serverSocket);
    return 0;
};

ServerRunner::ServerRunner(std::vector<Server> servers) {
    size_t i = 0;
    while (i < servers.size()) {
        OpenSocket(servers[i]);
        i++;
    }
}