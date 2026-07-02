#include "../../include/http/HttpResponse.hpp"
#include "../../include/http/HttpStatus.hpp"
#include <sstream>

static std::string buildDefaultErrorBody(int status_code)
{
    std::ostringstream out;
    out << "<html><head><title>" << status_code << " " << httpReasonPhrase(status_code)
        << "</title></head><body><h1>" << status_code << " " << httpReasonPhrase(status_code)
        << "</h1></body></html>";
    return out.str();
}

HttpResponse HttpResponse::makeText(int status_code, const std::string& body, const std::string& content_type,
    bool keep_alive)
{
    HttpResponse response;
    response.status_code = status_code;
    response.body = body;
    response.headers["Content-Type"] = content_type;
    response.headers["Connection"] = keep_alive ? "keep-alive" : "close";
    return response;
}

HttpResponse HttpResponse::makeError(int status_code, bool keep_alive)
{
    return makeText(status_code, buildDefaultErrorBody(status_code), "text/html", keep_alive);
}

std::string HttpResponse::toString() const
{
    std::ostringstream out;
    std::map<std::string, std::string>::const_iterator it;

    out << "HTTP/1.1 " << status_code << " " << httpReasonPhrase(status_code) << "\r\n";

    bool has_content_type = false;
    bool has_content_length = false;
    bool has_connection = false;
    for (it = headers.begin(); it != headers.end(); ++it) {
        out << it->first << ": " << it->second << "\r\n";
        if (it->first == "Content-Type")
            has_content_type = true;
        else if (it->first == "Content-Length")
            has_content_length = true;
        else if (it->first == "Connection")
            has_connection = true;
    }

    if (!has_content_type)
        out << "Content-Type: text/plain\r\n";
    if (!has_content_length)
        out << "Content-Length: " << body.size() << "\r\n";
    if (!has_connection)
        out << "Connection: close\r\n";

    out << "\r\n";
    out << body;
    return out.str();
}
