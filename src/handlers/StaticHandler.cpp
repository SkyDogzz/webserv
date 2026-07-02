#include "../../include/handlers/StaticHandler.hpp"
#include "../../include/http/HttpStatus.hpp"
#include "../../include/utils/DebugLogger.hpp"
#include "../../include/utils/Utils.hpp"
#include <sys/stat.h>
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

std::string StaticHandler::buildRedirectLocation(const std::string& target) const
{
    std::string location = target;
    std::string query;
    std::string::size_type qpos = location.find('?');
    if (qpos != std::string::npos) {
        query = location.substr(qpos);
        location.erase(qpos);
    }
    if (location.empty())
        location = "/";
    if (location[location.size() - 1] != '/')
        location += '/';
    return location + query;
}

HttpResponse StaticHandler::makeErrorResponse(int status_code) const
{
    HttpResponse response = HttpResponse::makeError(status_code, false);
    if (status_code == 405) {
        if (context_.allowed_methods.empty())
            response.headers["Allow"] = "GET, HEAD";
        else {
            std::ostringstream allow;
            bool first = true;
            for (std::set<std::string>::const_iterator it = context_.allowed_methods.begin();
                 it != context_.allowed_methods.end(); ++it) {
                if (!first)
                    allow << ", ";
                allow << *it;
                first = false;
            }
            response.headers["Allow"] = allow.str();
        }
    }
    std::map<int, std::string>::const_iterator it = context_.error_pages.find(status_code);
    if (it != context_.error_pages.end()) {
        std::ifstream file(it->second.c_str(), std::ios::in | std::ios::binary);
        if (file) {
            std::ostringstream buffer;
            buffer << file.rdbuf();
            response.body = buffer.str();
            response.headers["Content-Type"] = "text/html";
            std::ostringstream len;
            len << response.body.size();
            response.headers["Content-Length"] = len.str();
        }
    }
    return response;
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
    if (ext == "json")
        return "application/json";
    if (ext == "xml")
        return "application/xml";
    if (ext == "svg")
        return "image/svg+xml";
    if (ext == "pdf")
        return "application/pdf";
    if (ext == "ico")
        return "image/x-icon";
    if (ext == "webp")
        return "image/webp";
    if (ext == "md")
        return "text/markdown";
    return "application/octet-stream";
}

HttpResponse StaticHandler::handle(const HttpRequest& request)
{
    HttpResponse response;
    response.status_code = 200;

    if (request.method != "GET" && request.method != "HEAD") {
        return makeErrorResponse(405);
    }

    if (Utils::hasPathTraversal(request.path)) {
        return makeErrorResponse(403);
    }

    if (!context_.redirect_url.empty()) {
        int redirect_code = context_.redirect_code != 0 ? context_.redirect_code : 301;
        return HttpResponse::makeRedirect(redirect_code, context_.redirect_url, false);
    }

    std::string base_path = Utils::joinPathCopy(context_.root, request.path);
    struct stat st;
    if (stat(base_path.c_str(), &st) == 0 && S_ISDIR(st.st_mode) && !request.path.empty()
        && request.path[request.path.size() - 1] != '/') {
        return HttpResponse::makeRedirect(301, buildRedirectLocation(request.target), false);
    }

    std::string file_path = buildPath(request.path, context_.index);
    DEBUG_LOG << "try path: \"" << file_path << "\"" << std::endl;
    if (stat(file_path.c_str(), &st) != 0 || !S_ISREG(st.st_mode)) {
        return makeErrorResponse(404);
    }

    std::ifstream file(file_path.c_str(), std::ios::in | std::ios::binary);
    if (!file) {
        return makeErrorResponse(404);
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
