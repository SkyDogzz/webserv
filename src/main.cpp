#include <cstdlib>
#include <iostream>

#include "../include/config/Config.hpp"
#include "../src/config/lexer.hpp"
#include "../include/core/WebServer.hpp"

int main(int argc, char* argv[])
{
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " config_file" << std::endl;
        return EXIT_FAILURE;
    }

    std::string filename(argv[1]);
    Config config = Config(filename);
	
	Token tok;
	Lexer lexer(argv[1]);
while ((tok = lexer.getNextToken()).type != TOK_EOF)
    {
        std::cout << "Token Type: " << tok.type 
                  << " | Value: [" << tok.value << "]" << std::endl;
        if (tok.type == TOK_ERROR) break;
    }
    WebServer& webServer = WebServer::getInstance();
    webServer.appliConfig(config);

    webServer.run();

    (void)argc;
    (void)argv;
    return EXIT_SUCCESS;
}
