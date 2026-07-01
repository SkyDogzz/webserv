#include "../../include/handlers/StaticHandler.hpp"
#include "../../include/http/HttpStatus.hpp"
#include "../../include/utils/DebugLogger.hpp"
#include <fstream>
#include <iostream>
#include <sstream>

StaticHandler::StaticHandler(const std::string& root)
    : root_(root)
{
}

bool StaticHandler::isPathTraversal(const std::string& uri) const { return uri.find("..") != std::string::npos; }

std::string StaticHandler::buildPath(const std::string& uri) const
{
    std::string path = uri;
    std::size_t query_pos = path.find('?');
    if (query_pos != std::string::npos)
        path = path.substr(0, query_pos);
    if (!path.empty() && path[0] == '/')
        path.erase(0, 1);
    if (path.empty() || path[path.size() - 1] == '/')
        path += "index.html";

    if (!root_.empty() && root_[root_.size() - 1] == '/')
        return root_ + path;
    return root_ + "/" + path;
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
        response.status_code = 405;
        response.body = httpReasonPhrase(response.status_code);
        response.headers["Content-Length"] = "18";
        response.headers["Content-Type"] = "text/plain";
        return response;
    }

    if (isPathTraversal(request.path)) {
        response.status_code = 403;
        response.body = httpReasonPhrase(response.status_code);
        response.headers["Content-Length"] = "9";
        response.headers["Content-Type"] = "text/plain";
        return response;
    }

    std::string file_path = buildPath(request.path);
    DEBUG_LOG << "try path: \"" << file_path << "\"" << std::endl;
    std::ifstream file(file_path.c_str(), std::ios::in | std::ios::binary);
    if (!file) {
        response.status_code = 404;
        response.body = httpReasonPhrase(response.status_code);
        response.headers["Content-Length"] = "9";
        response.headers["Content-Type"] = "text/plain";
        return response;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    std::string body = buffer.str();
    response.headers["Content-Type"] = guessMimeType(file_path);
    DEBUG_LOG << response.headers["Content-Type"] << std::endl;

    std::ostringstream len;
    len << body.size();
    response.headers["Content-Length"] = len.str();

    if (request.method != "HEAD")
        response.body = body;
    DEBUG_LOG << "request: " << std::endl;
    DEBUG_LOG << response.toString() << std::endl;
    return response;
}
