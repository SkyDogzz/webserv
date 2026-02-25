#include <cstdlib>
#include <iostream>

#include "../include/config/Config.hpp"
#include "../include/core/WebServer.hpp"

int main(int argc, char* argv[])
{
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " config_file" << std::endl;
        return EXIT_FAILURE;
    }

    std::string filename(argv[1]);
    Config config = Config(filename);

    WebServer& webServer = WebServer::getInstance();
    webServer.appliConfig(config);

    webServer.run();

    (void)argc;
    (void)argv;
    return EXIT_SUCCESS;
}
