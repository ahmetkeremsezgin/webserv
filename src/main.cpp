#include "Server.hpp"

#include <csignal>
#include <iostream>

volatile sig_atomic_t g_running = 1;

static void handleSigint(int sig) {
    (void)sig;
    g_running = 0;
}

int main(int argc, char** argv) {
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, handleSigint);

    if (argc > 2) {
        std::cerr << "Usage: ./webserv [configuration file]" << std::endl;
        return 1;
    }
    const char* config = (argc == 2) ? argv[1] : "default.conf";

    try {
        Server webserv(config);
        webserv.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
