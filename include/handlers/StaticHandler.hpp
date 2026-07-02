#ifndef STATICHANDLER_HPP
#define STATICHANDLER_HPP

#include "handlers/IHandler.hpp"
#include "../core/Router.hpp"
#include <string>

class StaticHandler : public IHandler {
public:
    explicit StaticHandler(const RequestContext& context);
    virtual ~StaticHandler() { }
    virtual HttpResponse handle(const HttpRequest& request);

private:
    RequestContext context_;

    std::string buildPath(const std::string& uri, const std::string& index) const;
    std::string guessMimeType(const std::string& path) const;
};

#endif
