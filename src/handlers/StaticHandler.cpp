#include "../../include/handlers/StaticHandler.hpp"
#include "../../include/http/HttpStatus.hpp"
#include "../../include/utils/DebugLogger.hpp"
#include "../../include/utils/Utils.hpp"
#include <fstream>
#include <iostream>
#include <sstream>

StaticHandler::StaticHandler(const RequestContext& context)
    : context_(context)
{
}

std::string StaticHandler::buildPath(const std::string& uri, const std::string& index) const
{
    std::string path = Utils::stripQueryCopy(uri);
    if (path.empty())
        path = "/";
    if (path[path.size() - 1] == '/')
        path = Utils::joinPathCopy(path, index.empty() ? "index.html" : index);

    return Utils::joinPathCopy(context_.root, path);
}

std::string StaticHandler::guessMimeType(const std::string& path) const
{
    std::size_t dot = path.rfind('.');
    DEBUG_LOG << "find dot ?: " << dot << std::endl;
    if (dot == std::string::npos)
        return "application/octet-stream";
    std::string ext = path.substr(dot + 1);
    DEBUG_LOG << "find ext: " << ext << std::endl;

    if (ext == "html" || ext == "htm")
        return "text/html";
    if (ext == "css")
        return "text/css";
    if (ext == "js")
        return "application/javascript";
    if (ext == "png")
        return "image/png";
    if (ext == "jpg" || ext == "jpeg")
        return "image/jpeg";
    if (ext == "gif")
        return "image/gif";
    if (ext == "txt")
        return "text/plain";
    return "application/octet-stream";
}

HttpResponse StaticHandler::handle(const HttpRequest& request)
{
    HttpResponse response;
    response.status_code = 200;

    if (request.method != "GET" && request.method != "HEAD") {
        return HttpResponse::makeError(405, false);
    }

    if (Utils::hasPathTraversal(request.path)) {
        return HttpResponse::makeError(403, false);
    }

    std::string file_path = buildPath(request.path, context_.index);
    DEBUG_LOG << "try path: \"" << file_path << "\"" << std::endl;
    std::ifstream file(file_path.c_str(), std::ios::in | std::ios::binary);
    if (!file) {
        return HttpResponse::makeError(404, false);
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    std::string body = buffer.str();
    response.headers["Content-Type"] = guessMimeType(file_path);
    DEBUG_LOG << response.headers["Content-Type"] << std::endl;

    std::ostringstream len;
    len << body.size();
    response.headers["Content-Length"] = len.str();
    response.headers["Connection"] = "close";

    if (request.method != "HEAD")
        response.body = body;
    DEBUG_LOG << "request: " << std::endl;
    DEBUG_LOG << response.toString() << std::endl;
    return response;
}
