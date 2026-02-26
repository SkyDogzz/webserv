#ifndef LOCATIONCONFIG_HPP
#define LOCATIONCONFIG_HPP

#include <string>
#include <set>
#include <map>

/*
    LocationConfig
    --------------
    Represents a "location /path { }" block.

    Purpose:
    - Route matching based on URL prefix.
    - Method restrictions.
    - Root overrides.
    - Autoindex / CGI / upload configuration.

    Used by the Router to determine how a request is handled.
*/

class LocationConfig {
public:
    std::string path;

    std::set<std::string> allowed_methods;

    std::string root;
    std::string index;

    bool autoindex;

    std::map<std::string, std::string> cgi; // extension -> interpreter
    std::string upload_dir;
};

#endif
