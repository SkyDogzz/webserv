#ifndef STATICHANDLER_HPP
#define STATICHANDLER_HPP

#include "handlers/IHandler.hpp"
#include <string>

class StaticHandler : public IHandler {
public:
    explicit StaticHandler(const std::string& root);
    virtual ~StaticHandler() { }
    virtual HttpResponse handle(const HttpRequest& request);

private:
    std::string root_;

    std::string buildPath(const std::string& uri) const;
    std::string guessMimeType(const std::string& path) const;
    bool isPathTraversal(const std::string& uri) const;
};

#endif
