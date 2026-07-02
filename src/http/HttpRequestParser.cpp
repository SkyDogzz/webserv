#include "../../include/http/HttpRequestParser.hpp"
#include "../../include/utils/DebugLogger.hpp"
#include <cctype>
#include <iostream>
#include <sstream>
#include <utility>

static std::string toLowerCopy(const std::string& value)
{
    std::string lower = value;
    for (std::size_t i = 0; i < lower.size(); ++i)
        lower[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(lower[i])));
    return lower;
}

static bool isValidHeaderName(const std::string& name)
{
    if (name.empty())
        return false;
    for (std::size_t i = 0; i < name.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(name[i]);
        if (!(std::isalnum(c) || c == '-'))
            return false;
    }
    return true;
}

bool validStartLine(HttpRequest& request)
{
    if (request.method != "GET" && request.method != "POST" && request.method != "DELETE")
        return false;
    if (request.version != "HTTP/1.1")
        return false;
    return true;
}

bool parseStartLine(const std::string& line, HttpRequest& request)
{
    DEBUG_LOG << line << std::endl;
    std::string trimmed = line;
    if (!trimmed.empty() && trimmed[trimmed.size() - 1] == '\r')
        trimmed.erase(trimmed.size() - 1);
    std::istringstream iss(trimmed);
    if (!(iss >> request.method >> request.path >> request.version))
        return false;
    std::string extra;
    if (iss >> extra)
        return false;
    if (!validStartLine(request))
        return false;
    return true;
}

bool pushHeader(const std::string& line, HttpRequest& request)
{
    std::size_t posDot = line.find(':');
    if (posDot == std::string::npos)
        return false;

    std::string key = line.substr(0, posDot);
    if (!isValidHeaderName(key))
        return false;

    std::size_t value_start = posDot + 1;
    if (value_start < line.size() && line[value_start] == ' ')
        ++value_start;
    std::string value = line.substr(value_start);
    if (!value.empty() && value[value.size() - 1] == '\r')
        value.erase(value.size() - 1);

    std::string normalized_key = toLowerCopy(key);
    if (normalized_key == "content-length" && request.headers.find(normalized_key) != request.headers.end())
        return false;

    request.headers[normalized_key] = value;
    return true;
}

void printRequest(HttpRequest& request)
{
    DEBUG_LOG << "First line" << std::endl;
    DEBUG_LOG << "Method: \"" << request.method << "\"" << std::endl;
    DEBUG_LOG << "Path: \"" << request.path << "\"" << std::endl;
    DEBUG_LOG << "Version: \"" << request.version << "\"" << std::endl;
    DEBUG_LOG << std::endl;

    DEBUG_LOG << "Headers" << std::endl;
    std::map<std::string, std::string>::iterator it;
    for (it = request.headers.begin(); it != request.headers.end(); it++) {
        DEBUG_LOG << it->first << " :" << it->second << std::endl;
    }
    DEBUG_LOG << std::endl;

    DEBUG_LOG << "Body:" << std::endl;
    DEBUG_LOG << request.body << std::endl;
}

bool HttpRequestParser::parse(const std::string& buffer, HttpRequest& request)
{
    std::size_t pos = buffer.find('\n');
    if (pos == std::string::npos)
        return false;
    std::string line = buffer.substr(0, pos);
    if (!parseStartLine(line, request))
        throw HttpRequestParser::FirstLineInvalidException();
    std::size_t start = pos + 1;
    std::size_t body_start = buffer.size();
    while (start <= buffer.size()) {
        std::size_t end = buffer.find('\n', start);
        std::string header_line = buffer.substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (header_line == "" || header_line == "\r") {
            if (end == std::string::npos)
                body_start = buffer.size();
            else
                body_start = end + 1;
            break;
        }
        if (!pushHeader(header_line, request))
            throw HttpRequestParser::FirstLineInvalidException();
        if (end == std::string::npos) {
            body_start = buffer.size();
            break;
        }
        start = end + 1;
    }
    if (body_start < buffer.size()) {
        request.body = buffer.substr(body_start);
        if (!request.body.empty() && request.body[0] == '\r')
            request.body.erase(0, 1);
    }
    printRequest(request);
    return true;
}
