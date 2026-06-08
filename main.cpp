#include "Server.hpp"
#include <csignal>

volatile sig_atomic_t g_running = 1;

void handleSigint(int sig) {
    (void)sig;
    g_running = 0;
}

int main(int argc, char **argv) {
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, handleSigint);

    if (argc != 2) {
        std::cerr << "Usage: ./webserv <config_file>" << std::endl;
        return 1;
    }
    try {
        Server webserv(argv[1]);
        webserv.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}