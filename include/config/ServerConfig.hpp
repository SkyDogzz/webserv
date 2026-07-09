#ifndef SERVERCONFIG_HPP
#define SERVERCONFIG_HPP

#include <string>
#include <vector>
#include <map>
#include "LocationConfig.hpp"

/*
    ServerConfig
    ------------
    Represents one "server { }" block.

    Purpose:
    - Defines host/port to listen on.
    - Holds server-wide settings (root, index, limits).
    - Contains all location blocks.
    - Used at runtime to resolve requests.

    This is data only. No socket logic.
*/

class ServerConfig {
public:
    ServerConfig()
        : host("0.0.0.0")
        , port(80)
        , server_name("")
        , root("")
        , index("")
        , client_max_body_size(1048576) // 1MB default
    {}

    std::string host;
    int         port;
    std::string server_name;
    std::string root;
    std::string index;

    size_t client_max_body_size;

    std::map<int, std::string> error_pages;
    std::vector<LocationConfig> locations;
};

#endif
