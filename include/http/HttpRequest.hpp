#ifndef HTTPREQUEST_HPP
#define HTTPREQUEST_HPP

#include <map>
#include <string>

/*
    HttpRequest
    -----------
    Parsed representation of an HTTP request.

    Contains:
    - Method
    - Path
    - Version
    - Headers
    - Body

    Pure data object.
*/

class HttpRequest {
public:
    HttpRequest()
        : keep_alive(false)
    {
    }

    std::string method;
    std::string target;
    std::string path;
    std::string query;
    std::string version;
    std::map<std::string, std::string> headers;
    std::string body;
    bool keep_alive;
};

#endif
