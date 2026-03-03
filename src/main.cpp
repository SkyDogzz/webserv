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
	if (root) {
		std::cout << "--- Parsed JSON Structure ---" << std::endl;
		root->print();
		std::cout << "\n--- End of Structure ---" << std::endl;
		delete root;
	} else {
		std::cerr << "Parsing failed!" << std::endl;
	}
	return EXIT_SUCCESS;
}
