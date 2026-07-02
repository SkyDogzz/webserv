#include "../../include/core/Router.hpp"
#include "../../include/utils/Utils.hpp"

RequestContext::RequestContext()
    : server(NULL)
    , location(NULL)
    , root(".")
    , index("index.html")
    , autoindex(false)
{
}

Router::Router(const Config& config)
    : config_(config)
{
}

std::string Router::hostWithoutPort(const std::string& host)
{
    std::string value = host;
    std::string::size_type colon = value.find(':');
    if (colon != std::string::npos)
        value.erase(colon);
    return Utils::toLowerCopy(value);
}

const ServerConfig* Router::selectServer(int listen_port, const HttpRequest& request) const
{
    if (config_.servers.empty())
        return NULL;

    const std::string request_host = hostWithoutPort(
        request.headers.find("host") != request.headers.end() ? request.headers.find("host")->second : "");

    const ServerConfig* default_server = NULL;
    for (std::vector<ServerConfig>::const_iterator it = config_.servers.begin(); it != config_.servers.end(); ++it) {
        if (it->port != listen_port)
            continue;
        if (default_server == NULL)
            default_server = &(*it);
        if (!request_host.empty() && !it->server_name.empty() && hostWithoutPort(it->server_name) == request_host)
            return &(*it);
    }

    if (default_server != NULL)
        return default_server;
    return &config_.servers.front();
}

const LocationConfig* Router::selectLocation(const ServerConfig& server, const std::string& path) const
{
    const LocationConfig* best = NULL;
    std::size_t best_len = 0;
    for (std::vector<LocationConfig>::const_iterator it = server.locations.begin(); it != server.locations.end(); ++it) {
        if (it->path.empty())
            continue;
        if (path.compare(0, it->path.size(), it->path) != 0)
            continue;
        if (it->path.size() < best_len)
            continue;
        if (it->path.size() == path.size() && path != it->path)
            continue;
        best = &(*it);
        best_len = it->path.size();
    }
    return best;
}

bool Router::resolve(int listen_port, const HttpRequest& request, RequestContext& context) const
{
    const ServerConfig* server = selectServer(listen_port, request);
    if (server == NULL)
        return false;

    context = RequestContext();
    context.server = server;
    context.root = server->root;
    context.index = server->index;

    const LocationConfig* location = selectLocation(*server, Utils::normalizePathCopy(request.path));
    if (location != NULL) {
        context.location = location;
        if (!location->root.empty())
            context.root = location->root;
        if (!location->index.empty())
            context.index = location->index;
        context.autoindex = location->autoindex;
        context.allowed_methods = location->allowed_methods;
        context.cgi = location->cgi;
        context.error_pages = server->error_pages;
        for (std::map<int, std::string>::const_iterator it = location->error_pages.begin();
             it != location->error_pages.end(); ++it) {
            context.error_pages[it->first] = it->second;
        }
        context.upload_dir = location->upload_dir;
    } else {
        context.error_pages = server->error_pages;
    }
    return true;
}
