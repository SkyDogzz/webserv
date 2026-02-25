#ifndef ROUTER_HPP
#define ROUTER_HPP

#include "../http/HttpRequest.hpp"
#include "../config/Config.hpp"

/*
    Router
    ------
    Responsible for request resolution.

    Purpose:
    - Select correct ServerConfig (virtual host).
    - Match LocationConfig.
    - Determine effective root and behavior.

    Produces a context used by handlers.
*/

class Router {
public:
    Router(const Config& config);
};

#endif
