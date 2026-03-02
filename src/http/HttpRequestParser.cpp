#include "../../include/http/HttpRequestParser.hpp"
#include <iostream>
#include <sstream>
#include <utility>

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
    std::cout << line << std::endl;
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

    std::string key, value;
    key = line.substr(0, posDot);
    std::size_t value_start = posDot + 1;
    if (value_start < line.size() && line[value_start] == ' ')
        ++value_start;
    value = line.substr(value_start);
    request.headers.insert(std::make_pair(key, value));
    return true;
}

void printRequest(HttpRequest& request)
{
    std::cout << "First line" << std::endl;
    std::cout << "Method: \"" << request.method << "\"" << std::endl;
    std::cout << "Path: \"" << request.path << "\"" << std::endl;
    std::cout << "Version: \"" << request.version << "\"" << std::endl;
    std::cout << std::endl;

    std::cout << "Headers" << std::endl;
    std::map<std::string, std::string>::iterator it;
    for (it = request.headers.begin(); it != request.headers.end(); it++) {
        std::cout << it->first << " :" << it->second << std::endl;
    }
    std::cout << std::endl;

    std::cout << "Body:" << std::endl;
    std::cout << request.body << std::endl;
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
        std::string header_line = buffer.substr(start, end - start);
        if (header_line == "" || header_line == "\r") {
            if (end == std::string::npos)
                body_start = buffer.size();
            else
                body_start = end + 1;
            break;
        }
        pushHeader(header_line, request);
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
