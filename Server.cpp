#include "Server.hpp"
#include "Config.hpp"
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

int ServerRunner::OpenSocket(Server servers) {
    (void) servers;

        
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(8080);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    bind(serverSocket, (struct sockaddr*)&serverAddress,
         sizeof(serverAddress));

    listen(serverSocket, 5);
    while (1) {
         int clientSocket
        = accept(serverSocket, NULL, NULL);

    // recieving data
    char buffer[1024] = { 0 };
    recv(clientSocket, buffer, sizeof(buffer), 0);
    std::cout << "Message from client: " << buffer
              << std::endl;
    }
    close(serverSocket);
    return 1;
};

ServerRunner::ServerRunner(std::vector<Server> servers) {
    size_t i = 0;
    while (i < servers.size()) {
        OpenSocket(servers[i]);
        i++;
    }
}