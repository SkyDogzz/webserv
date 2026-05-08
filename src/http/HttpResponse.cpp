#include "../../include/http/HttpResponse.hpp"

std::string HttpResponse::toString() const
{
    std::ostringstream out;
    bool keep_alive = false;
    std::string contentType = "text/plain";
    std::map<std::string, std::string>::const_iterator it = headers.find("keep-alive");
    if (it != headers.end()) {
        if (it->second == "keep-alive")
            keep_alive = true;
    }
    it = headers.find("Content-Type");
    if (it != headers.end()) {
        contentType = it->second;
    }

    out << "HTTP/1.1 " << status_code << " ";
    if (status_code == 200)
        out << "OK";
    else if (status_code == 400)
        out << "Bad Request";
    else if (status_code == 404)
        out << "Not Found";
    else
        out << "Error";
    out << "\r\n";
    out << "Content-Type: " << contentType << "\r\n";
    out << "Content-Length: " << body.size() << "\r\n";
    out << "Connection: " << (keep_alive ? "keep-alive" : "close") << "\r\n";
    out << "\r\n";
    out << body;
    return out.str();
}
