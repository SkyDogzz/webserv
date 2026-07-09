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
    LocationConfig()
        : path("")
        , root("")
        , index("")
        , autoindex(false)
        , upload_dir("")
        , redirect_code(0)
        , redirect_url("")
    {}

    std::string path;

    std::set<std::string> allowed_methods;

    std::string root;
    std::string index;

    bool autoindex;

    int redirect_code;
    std::string redirect_url;

    std::map<int, std::string> error_pages;
    std::map<std::string, std::string> cgi; // extension -> interpreter
    std::string upload_dir;

    int         redirect_code;
    std::string redirect_url;
    size_t client_max_body_size;
};

#endif
