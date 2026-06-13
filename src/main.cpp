#include <cstdlib>
#include <iostream>

#include "../include/config/Config.hpp"
#include "../src/config/lexer.hpp"
#include "../include/core/WebServer.hpp"
#include "../src/config/parser.hpp"
#include "../src/config/JsonValue.hpp"

int main(int argc, char* argv[])
{
	if (argc != 2) {
		std::cerr << "Usage: " << argv[0] << " config_file" << std::endl;
		return EXIT_FAILURE;
	}
	Lexer lexer(argv[1]);
	Parser parser(lexer);

	std::cout << "--- Starting Parsing ---" << std::endl;
	JsonValue* root = parser.parse();

	if (!JsonValue::isValid(root)) {
		std::cerr << "Parsing failed or JSON structure is invalid!" << std::endl;
		delete root;
		return EXIT_FAILURE;
	}

	try {
		Config* config = new Config(root);
		delete root;

		std::cout << "Parsed " << config->servers.size() << " server(s):" << std::endl;
		for (size_t i = 0; i < config->servers.size(); ++i) {
			std::cout << "  Server #" << i << ": " << config->servers[i].server_name 
			          << " listening on " << config->servers[i].host << ":" 
			          << config->servers[i].port << std::endl;
		}

		WebServer& webServer = WebServer::getInstance();
		webServer._config = config;
		webServer.run();
	} catch (std::exception& e) {
		std::cerr << "Unknown exception throwed: " << e.what() << std::endl;
		return EXIT_FAILURE;
	}

    (void)argc;
    (void)argv;
    return EXIT_SUCCESS;
}
