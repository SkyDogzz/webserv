#ifndef CONFIG_HPP
#define CONFIG_HPP

#include "ServerConfig.hpp"
#include <vector>

/*
    Config
    ------
    Root configuration object.

    Purpose:
    - Holds all server blocks parsed from the configuration file.
    - Immutable after parsing.
    - Provides global access to server configurations.

    This class contains NO networking or runtime logic.
    It is pure configuration data.
*/

class Config {
public:
    Config(std::string& filename);

    std::vector<ServerConfig> servers;
};

#endif
