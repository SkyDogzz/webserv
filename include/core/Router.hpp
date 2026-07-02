#ifndef ROUTER_HPP
#define ROUTER_HPP

#include "../http/HttpRequest.hpp"
#include "../config/Config.hpp"
#include <map>
#include <set>
#include <string>

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

struct RequestContext {
    const ServerConfig* server;
    const LocationConfig* location;
    std::string root;
    std::string index;
    bool autoindex;
    std::set<std::string> allowed_methods;
    std::map<std::string, std::string> cgi;
    std::string upload_dir;

    RequestContext();
};

class Router {
public:
    explicit Router(const Config& config);

    bool resolve(int listen_port, const HttpRequest& request, RequestContext& context) const;

private:
    const Config& config_;

    const ServerConfig* selectServer(int listen_port, const HttpRequest& request) const;
    const LocationConfig* selectLocation(const ServerConfig& server, const std::string& path) const;
    static std::string normalizePath(const std::string& path);
    static std::string hostWithoutPort(const std::string& host);
};

#endif
