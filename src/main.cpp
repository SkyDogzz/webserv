#include <cstdlib>
#include <exception>
#include <iostream>

#include "config/Config.hpp"
#include "config/lexer.hpp"
#include "config/parser.hpp"
#include "core/WebServer.hpp"
#include "utils/DebugLogger.hpp"

int main(int argc, const char* argv[])
{
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " config_file.json" << std::endl;
        return EXIT_FAILURE;
    }

    DebugLogger::setEnabled(false);

    try {
        Lexer lexer(argv[1]);
        Parser parser(lexer);
        JsonValue* root = parser.parse();

        if (!JsonValue::isValid(root)) {
            std::cerr << "Parsing failed or JSON structure is invalid" << std::endl;
            delete root;
            return EXIT_FAILURE;
        }

        Config* config = new Config(root);
        delete root;

        WebServer& webServer = WebServer::getInstance();
        webServer.appliConfig(*config);
        webServer.run();
    } catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
