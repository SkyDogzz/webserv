#ifndef LOCATIONCONFIG_HPP
#define LOCATIONCONFIG_HPP

#include <map>
#include <set>
#include <string>

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
    explicit LocationConfig();
    ~LocationConfig();

    std::string path;

    std::set<std::string> allowed_methods;

    std::string root;
    std::string index;

    bool autoindex;

    std::map<int, std::string> error_pages;
    std::map<std::string, std::string> cgi; // extension -> interpreter
    std::string upload_dir;
};

#endif
