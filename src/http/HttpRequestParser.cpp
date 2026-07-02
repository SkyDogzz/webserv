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

static bool parseChunkSizeLine(const std::string& line, std::size_t& chunk_size)
{
    std::size_t pos = 0;
    chunk_size = 0;

    while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t'))
        ++pos;
    if (pos == line.size())
        return false;

    for (; pos < line.size(); ++pos) {
        char c = line[pos];
        if (c == ';')
            return true;
        if (c >= '0' && c <= '9') {
            chunk_size = chunk_size * 16 + static_cast<std::size_t>(c - '0');
        } else if (c >= 'a' && c <= 'f') {
            chunk_size = chunk_size * 16 + static_cast<std::size_t>(10 + c - 'a');
        } else if (c >= 'A' && c <= 'F') {
            chunk_size = chunk_size * 16 + static_cast<std::size_t>(10 + c - 'A');
        } else if (c == ' ' || c == '\t') {
            while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t'))
                ++pos;
            return pos == line.size() || line[pos] == ';';
        } else {
            return false;
        }
    }
    return true;
}

static bool consumeChunkedBody(const std::string& body, std::string* decoded_body)
{
    std::size_t pos = 0;

    while (true) {
        std::size_t line_end = body.find('\n', pos);
        if (line_end == std::string::npos)
            return false;

        std::string size_line = body.substr(pos, line_end - pos);
        if (!size_line.empty() && size_line[size_line.size() - 1] == '\r')
            size_line.erase(size_line.size() - 1);

        std::size_t chunk_size = 0;
        if (!parseChunkSizeLine(size_line, chunk_size))
            return false;

        pos = line_end + 1;
        if (body.size() < pos + chunk_size)
            return false;

        if (decoded_body != NULL && chunk_size > 0)
            decoded_body->append(body, pos, chunk_size);

        pos += chunk_size;
        if (pos >= body.size())
            return false;

        if (body[pos] == '\r') {
            if (pos + 1 >= body.size())
                return false;
            if (body[pos + 1] != '\n')
                return false;
            pos += 2;
        } else if (body[pos] == '\n') {
            ++pos;
        } else {
            return false;
        }

        if (chunk_size == 0) {
            while (true) {
                std::size_t trailer_end = body.find('\n', pos);
                if (trailer_end == std::string::npos)
                    return false;
                std::string trailer_line = body.substr(pos, trailer_end - pos);
                if (!trailer_line.empty() && trailer_line[trailer_line.size() - 1] == '\r')
                    trailer_line.erase(trailer_line.size() - 1);
                pos = trailer_end + 1;
                if (trailer_line.empty())
                    return pos == body.size();
            }
        }
    }
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
    if (normalized_key == "transfer-encoding" && request.headers.find(normalized_key) != request.headers.end())
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
    std::map<std::string, std::string>::const_iterator te = request.headers.find("transfer-encoding");
    if (te != request.headers.end()) {
        if (toLowerCopy(te->second) != "chunked")
            throw HttpRequestParser::FirstLineInvalidException();
        if (request.headers.find("content-length") != request.headers.end())
            throw HttpRequestParser::FirstLineInvalidException();
        std::string decoded_body;
        if (!consumeChunkedBody(request.body, &decoded_body))
            throw HttpRequestParser::FirstLineInvalidException();
        request.body = decoded_body;
    }
    printRequest(request);
    return true;
}
