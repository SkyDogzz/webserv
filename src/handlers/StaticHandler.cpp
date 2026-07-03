#include "../../include/handlers/StaticHandler.hpp"
#include "../../include/http/HttpStatus.hpp"
#include "../../include/utils/DebugLogger.hpp"
#include "../../include/utils/Utils.hpp"
#include <cerrno>
#include <cstdio>
#include <ctime>
#include <algorithm>
#include <cstring>
#include <dirent.h>
#include <vector>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fstream>
#include <iostream>
#include <sstream>

StaticHandler::StaticHandler(const RequestContext& context)
    : context_(context)
{
}

std::string StaticHandler::buildFilesystemPath(const std::string& uri) const
{
    std::string path = Utils::stripQueryCopy(uri);
    if (path.empty())
        path = "/";
    return Utils::joinPathCopy(context_.root, path);
}

std::string StaticHandler::buildGetPath(const std::string& uri, const std::string& index) const
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

std::string StaticHandler::htmlEscapeCopy(const std::string& value) const
{
    std::string escaped;
    escaped.reserve(value.size());
    for (std::string::size_type i = 0; i < value.size(); ++i) {
        char c = value[i];
        if (c == '&')
            escaped += "&amp;";
        else if (c == '<')
            escaped += "&lt;";
        else if (c == '>')
            escaped += "&gt;";
        else if (c == '\"')
            escaped += "&quot;";
        else
            escaped.push_back(c);
    }
    return escaped;
}

std::string StaticHandler::buildAutoindexBody(const std::string& dir_path, const std::string& uri) const
{
    DIR* dir = opendir(dir_path.c_str());
    if (dir == NULL)
        return "";

    std::vector<std::string> entries;
    for (;;) {
        errno = 0;
        struct dirent* ent = readdir(dir);
        if (ent == NULL)
            break;
        if (std::strcmp(ent->d_name, ".") == 0 || std::strcmp(ent->d_name, "..") == 0)
            continue;
        entries.push_back(ent->d_name);
    }
    closedir(dir);

    std::sort(entries.begin(), entries.end());

    std::ostringstream out;
    out << "<html><head><title>Index of " << htmlEscapeCopy(uri) << "</title></head><body>";
    out << "<h1>Index of " << htmlEscapeCopy(uri) << "</h1><ul>";
    for (std::vector<std::string>::const_iterator it = entries.begin(); it != entries.end(); ++it) {
        std::string entry_path = Utils::joinPathCopy(dir_path, *it);
        std::string href = Utils::joinPathCopy(uri, *it);
        struct stat st;
        bool is_dir = (stat(entry_path.c_str(), &st) == 0 && S_ISDIR(st.st_mode));
        if (is_dir && href[href.size() - 1] != '/')
            href += "/";
        out << "<li><a href=\"" << htmlEscapeCopy(href) << "\">" << htmlEscapeCopy(*it);
        if (is_dir)
            out << "/";
        out << "</a></li>";
    }
    out << "</ul></body></html>";
    return out.str();
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

std::string StaticHandler::makeUploadFilename(const std::string& hint) const
{
    static unsigned long counter = 0;
    std::ostringstream out;
    out << "upload_" << static_cast<long>(::getpid()) << "_" << static_cast<long>(::time(NULL)) << "_"
        << counter++;

    std::size_t dot = hint.rfind('.');
    if (dot != std::string::npos && dot + 1 < hint.size()) {
        std::string ext = hint.substr(dot);
        if (ext.size() <= 16)
            out << ext;
    }
    return out.str();
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
    if (request.method == "GET" || request.method == "HEAD")
        return handleGetOrHead(request);
    if (request.method == "POST")
        return handlePost(request);
    if (request.method == "DELETE")
        return handleDelete(request);
    return makeErrorResponse(405);
}

HttpResponse StaticHandler::handleGetOrHead(const HttpRequest& request) const
{
    HttpResponse response;
    response.status_code = 200;

    if (Utils::hasPathTraversal(request.path))
        return makeErrorResponse(403);

    if (!context_.redirect_url.empty()) {
        int redirect_code = context_.redirect_code != 0 ? context_.redirect_code : 301;
        return HttpResponse::makeRedirect(redirect_code, context_.redirect_url, false);
    }

    std::string base_path = buildFilesystemPath(request.path);
    struct stat st;
    if (stat(base_path.c_str(), &st) != 0) {
        if (errno == EACCES || errno == EPERM)
            return makeErrorResponse(403);
        return makeErrorResponse(404);
    }

    if (S_ISDIR(st.st_mode)) {
        if (!request.path.empty() && request.path[request.path.size() - 1] != '/')
            return HttpResponse::makeRedirect(301, buildRedirectLocation(request.target), false);

        std::string index_path = Utils::joinPathCopy(base_path, context_.index.empty() ? "index.html" : context_.index);
        DEBUG_LOG << "try index path: \"" << index_path << "\"" << std::endl;
        if (stat(index_path.c_str(), &st) == 0 && S_ISREG(st.st_mode)) {
            std::ifstream file(index_path.c_str(), std::ios::in | std::ios::binary);
            if (!file) {
                if (errno == EACCES || errno == EPERM)
                    return makeErrorResponse(403);
                return makeErrorResponse(404);
            }

            std::ostringstream buffer;
            buffer << file.rdbuf();
            std::string body = buffer.str();
            response.headers["Content-Type"] = guessMimeType(index_path);

            std::ostringstream len;
            len << body.size();
            response.headers["Content-Length"] = len.str();

            if (request.method != "HEAD")
                response.body = body;
            return response;
        }

        if (!context_.autoindex)
            return makeErrorResponse(403);

        std::string body = buildAutoindexBody(base_path, request.path);
        if (body.empty())
            return makeErrorResponse(403);
        response.headers["Content-Type"] = "text/html";
        std::ostringstream len;
        len << body.size();
        response.headers["Content-Length"] = len.str();
        if (request.method != "HEAD")
            response.body = body;
        return response;
    }

    if (!S_ISREG(st.st_mode))
        return makeErrorResponse(403);

    std::ifstream file(base_path.c_str(), std::ios::in | std::ios::binary);
    if (!file) {
        if (errno == EACCES || errno == EPERM)
            return makeErrorResponse(403);
        return makeErrorResponse(404);
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    std::string body = buffer.str();
    response.headers["Content-Type"] = guessMimeType(base_path);

    std::ostringstream len;
    len << body.size();
    response.headers["Content-Length"] = len.str();

    if (request.method != "HEAD")
        response.body = body;
    return response;
}

HttpResponse StaticHandler::handlePost(const HttpRequest& request) const
{
    if (context_.upload_dir.empty())
        return HttpResponse::makeError(501, false);
    if (Utils::hasPathTraversal(request.path))
        return makeErrorResponse(403);

    struct stat st;
    if (stat(context_.upload_dir.c_str(), &st) != 0)
        return makeErrorResponse(403);
    if (!S_ISDIR(st.st_mode))
        return makeErrorResponse(403);

    std::string filename = makeUploadFilename(request.path);
    std::string full_path = Utils::joinPathCopy(context_.upload_dir, filename);
    std::ofstream file(full_path.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);
    if (!file) {
        if (errno == EACCES || errno == EPERM)
            return makeErrorResponse(403);
        return HttpResponse::makeError(500, false);
    }

    file.write(request.body.c_str(), request.body.size());
    if (!file.good())
        return HttpResponse::makeError(500, false);

    HttpResponse response = HttpResponse::makeText(201, "<html><body><h1>Created</h1></body></html>", "text/html", false);
    response.headers["Location"] = Utils::joinPathCopy(request.path, filename);
    return response;
}

HttpResponse StaticHandler::handleDelete(const HttpRequest& request) const
{
    if (Utils::hasPathTraversal(request.path))
        return makeErrorResponse(403);

    std::string file_path = buildFilesystemPath(request.path);
    struct stat st;
    if (stat(file_path.c_str(), &st) != 0)
        return makeErrorResponse(404);
    if (S_ISDIR(st.st_mode))
        return makeErrorResponse(403);
    if (unlink(file_path.c_str()) != 0) {
        if (errno == ENOENT)
            return makeErrorResponse(404);
        if (errno == EACCES || errno == EPERM)
            return makeErrorResponse(403);
        return HttpResponse::makeError(500, false);
    }

    HttpResponse response;
    response.status_code = 204;
    response.headers["Connection"] = "close";
    response.headers["Content-Length"] = "0";
    return response;
}
