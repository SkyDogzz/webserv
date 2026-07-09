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

    std::string buildGetPath(const std::string& uri, const std::string& index) const;
    std::string buildFilesystemPath(const std::string& uri) const;
    static std::string guessMimeType(const std::string& path);
    static std::string buildRedirectLocation(const std::string& target);
    static std::string buildAutoindexBody(const std::string& dir_path, const std::string& uri);
    static std::string htmlEscapeCopy(const std::string& value);
    static std::string buildUploadLocation(const std::string& request_path, const std::string& filename);
    HttpResponse makeErrorResponse(int status_code) const;
    HttpResponse handleGetOrHead(const HttpRequest& request) const;
    HttpResponse handlePost(const HttpRequest& request) const;
    HttpResponse handleDelete(const HttpRequest& request) const;
    static std::string makeUploadFilename(const std::string& hint);
};

#endif
