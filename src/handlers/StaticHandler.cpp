#include "../../include/handlers/StaticHandler.hpp"
#include "../../include/http/HttpStatus.hpp"
#include "../../include/utils/DebugLogger.hpp"
#include "../../include/utils/Utils.hpp"
#include <cerrno>
#include <cstdio>
#include <ctime>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <dirent.h>
#include <map>
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

struct MultipartFile {
    std::string filename;
    std::string content;
    bool found;

    MultipartFile()
        : found(false)
    {
    }
};

static std::string stripLineEndCopy(const std::string& line)
{
    if (!line.empty() && line[line.size() - 1] == '\r')
        return line.substr(0, line.size() - 1);
    return line;
}

static std::string unquoteCopy(const std::string& value)
{
    std::string trimmed = Utils::trimCopy(value);
    if (trimmed.size() >= 2 && trimmed[0] == '"' && trimmed[trimmed.size() - 1] == '"') {
        std::string out;
        for (std::string::size_type i = 1; i + 1 < trimmed.size(); ++i) {
            if (trimmed[i] == '\\' && i + 2 < trimmed.size()) {
                ++i;
                out.push_back(trimmed[i]);
            } else {
                out.push_back(trimmed[i]);
            }
        }
        return out;
    }
    return trimmed;
}

static bool extractMultipartBoundary(const std::string& content_type, std::string& boundary)
{
    boundary.clear();
    std::string::size_type pos = 0;
    bool multipart = false;
    while (pos <= content_type.size()) {
        std::string::size_type semi = content_type.find(';', pos);
        std::string part = Utils::trimCopy(content_type.substr(pos, semi == std::string::npos ? std::string::npos : semi - pos));
        std::string lower = Utils::toLowerCopy(part);
        if (pos == 0 && lower == "multipart/form-data")
            multipart = true;
        std::string::size_type eq = part.find('=');
        if (eq != std::string::npos) {
            std::string key = Utils::toLowerCopy(Utils::trimCopy(part.substr(0, eq)));
            if (key == "boundary")
                boundary = unquoteCopy(part.substr(eq + 1));
        }
        if (semi == std::string::npos)
            break;
        pos = semi + 1;
    }
    return multipart && !boundary.empty();
}

static std::map<std::string, std::string> parseMultipartHeaders(const std::string& block)
{
    std::map<std::string, std::string> headers;
    std::string::size_type start = 0;
    while (start <= block.size()) {
        std::string::size_type end = block.find('\n', start);
        std::string line = stripLineEndCopy(block.substr(start, end == std::string::npos ? std::string::npos : end - start));
        std::string::size_type colon = line.find(':');
        if (colon != std::string::npos) {
            std::string key = Utils::toLowerCopy(Utils::trimCopy(line.substr(0, colon)));
            std::string value = Utils::trimCopy(line.substr(colon + 1));
            if (!key.empty())
                headers[key] = value;
        }
        if (end == std::string::npos)
            break;
        start = end + 1;
    }
    return headers;
}

static std::string extractDispositionParam(const std::string& disposition, const std::string& param)
{
    std::string::size_type pos = 0;
    while (pos <= disposition.size()) {
        std::string::size_type semi = disposition.find(';', pos);
        std::string part = Utils::trimCopy(disposition.substr(pos, semi == std::string::npos ? std::string::npos : semi - pos));
        std::string::size_type eq = part.find('=');
        if (eq != std::string::npos) {
            std::string key = Utils::toLowerCopy(Utils::trimCopy(part.substr(0, eq)));
            if (key == param)
                return unquoteCopy(part.substr(eq + 1));
        }
        if (semi == std::string::npos)
            break;
        pos = semi + 1;
    }
    return "";
}

static std::string::size_type findHeaderEnd(const std::string& body, std::string::size_type start, std::size_t& sep_len)
{
    std::string::size_type crlf = body.find("\r\n\r\n", start);
    std::string::size_type lf = body.find("\n\n", start);
    if (crlf == std::string::npos || (lf != std::string::npos && lf < crlf)) {
        sep_len = 2;
        return lf;
    }
    sep_len = 4;
    return crlf;
}

static bool parseMultipartFile(const std::string& body, const std::string& boundary, MultipartFile& file,
    std::size_t& part_count)
{
    const std::string delimiter = "--" + boundary;
    std::string::size_type pos = body.find(delimiter);
    part_count = 0;
    file = MultipartFile();
    if (pos == std::string::npos)
        return false;
    pos += delimiter.size();

    while (pos < body.size()) {
        if (body.compare(pos, 2, "--") == 0)
            return true;
        if (body.compare(pos, 2, "\r\n") == 0)
            pos += 2;
        else if (body.compare(pos, 1, "\n") == 0)
            pos += 1;
        else
            return false;

        std::size_t sep_len = 0;
        std::string::size_type header_end = findHeaderEnd(body, pos, sep_len);
        if (header_end == std::string::npos)
            return false;
        std::map<std::string, std::string> headers = parseMultipartHeaders(body.substr(pos, header_end - pos));
        std::string::size_type data_start = header_end + sep_len;
        std::string::size_type next = body.find(delimiter, data_start);
        if (next == std::string::npos)
            return false;

        std::string::size_type data_end = next;
        if (data_end >= 2 && body.compare(data_end - 2, 2, "\r\n") == 0)
            data_end -= 2;
        else if (data_end >= 1 && body.compare(data_end - 1, 1, "\n") == 0)
            data_end -= 1;

        ++part_count;
        std::map<std::string, std::string>::const_iterator disp = headers.find("content-disposition");
        if (!file.found && disp != headers.end()) {
            std::string filename = extractDispositionParam(disp->second, "filename");
            if (!filename.empty()) {
                file.filename = filename;
                file.content = body.substr(data_start, data_end - data_start);
                file.found = true;
            }
        }
        pos = next + delimiter.size();
    }
    return true;
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

std::string StaticHandler::buildRedirectLocation(const std::string& target)
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

std::string StaticHandler::htmlEscapeCopy(const std::string& value)
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

std::string StaticHandler::buildAutoindexBody(const std::string& dir_path, const std::string& uri)
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

std::string StaticHandler::makeUploadFilename(const std::string& hint)
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

std::string StaticHandler::sanitizeUploadFilename(const std::string& filename)
{
    std::string clean = filename;
    std::string::size_type slash = clean.find_last_of("/\\");
    if (slash != std::string::npos)
        clean = clean.substr(slash + 1);

    std::string out;
    for (std::string::size_type i = 0; i < clean.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(clean[i]);
        if (std::isalnum(c) || clean[i] == '.' || clean[i] == '_' || clean[i] == '-')
            out.push_back(clean[i]);
        else
            out.push_back('_');
    }

    if (out.empty() || out == "." || out == ".." || Utils::hasPathTraversal(out))
        return makeUploadFilename(filename);
    return out;
}

std::string StaticHandler::guessMimeType(const std::string& path)
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

    DEBUG_LOG << "upload request method=" << request.method << " uri=" << request.target << std::endl;
    std::map<std::string, std::string>::const_iterator ct = request.headers.find("content-type");
    std::map<std::string, std::string>::const_iterator cl = request.headers.find("content-length");
    DEBUG_LOG << "upload content-type=" << (ct == request.headers.end() ? "" : ct->second) << std::endl;
    DEBUG_LOG << "upload content-length=" << (cl == request.headers.end() ? "" : cl->second)
              << " body-size=" << request.body.size() << std::endl;

    struct stat st;
    if (stat(context_.upload_dir.c_str(), &st) != 0)
        return makeErrorResponse(403);
    if (!S_ISDIR(st.st_mode))
        return makeErrorResponse(403);

    std::string filename;
    std::string content;
    std::string boundary;
    std::size_t part_count = 0;
    bool is_multipart = ct != request.headers.end()
        && Utils::toLowerCopy(Utils::trimCopy(ct->second)).compare(0, 19, "multipart/form-data") == 0;
    if (is_multipart && !extractMultipartBoundary(ct->second, boundary))
        return makeErrorResponse(400);
    if (is_multipart) {
        MultipartFile multipart_file;
        if (!parseMultipartFile(request.body, boundary, multipart_file, part_count))
            return makeErrorResponse(400);
        if (!multipart_file.found)
            return makeErrorResponse(400);
        filename = sanitizeUploadFilename(multipart_file.filename);
        content = multipart_file.content;
    } else {
        filename = makeUploadFilename(request.path);
        content = request.body;
    }

    std::string full_path = Utils::joinPathCopy(context_.upload_dir, filename);
    DEBUG_LOG << "upload boundary=" << boundary << std::endl;
    DEBUG_LOG << "upload multipart-parts=" << part_count << std::endl;
    DEBUG_LOG << "upload filename=" << filename << std::endl;
    DEBUG_LOG << "upload write-path=" << full_path << std::endl;

    std::ofstream file(full_path.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);
    if (!file) {
        if (errno == EACCES || errno == EPERM)
            return makeErrorResponse(403);
        return HttpResponse::makeError(500, false);
    }

    file.write(content.c_str(), content.size());
    if (!file.good())
        return HttpResponse::makeError(500, false);

    std::ostringstream body;
    body << "<html><body><h1>Upload created</h1><p>Saved "
         << htmlEscapeCopy(filename) << " (" << content.size() << " bytes)</p></body></html>";
    HttpResponse response = HttpResponse::makeText(201, body.str(), "text/html", false);
    DEBUG_LOG << "upload response status=201 content-type=text/html content-length=" << body.str().size() << std::endl;
    return response;
}

HttpResponse StaticHandler::handleDelete(const HttpRequest& request) const
{
    if (Utils::hasPathTraversal(request.path))
        return makeErrorResponse(403);

    std::string file_path;
    if (!context_.upload_dir.empty() && context_.location != NULL
        && request.path.compare(0, context_.location->path.size(), context_.location->path) == 0) {
        std::string relative = request.path.substr(context_.location->path.size());
        if (relative.empty())
            relative = "/";
        file_path = Utils::joinPathCopy(context_.upload_dir, relative);
    } else {
        file_path = buildFilesystemPath(request.path);
    }
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
